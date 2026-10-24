// Prelude -- Nintendo Switch homebrew for the Nextendo Network.
// Copyright (C) 2026 Nextendo Network
//
// Licensed under the PolyForm Shield License 1.0.0.
//
// You may use, modify and distribute this software for any purpose EXCEPT providing a product
// that competes with Nextendo Network, or with any product Nextendo Network provides using it.
//
// See LICENSE.md for the full terms, or <https://polyformproject.org/licenses/shield/1.0.0>.
//
// Required Notice: Copyright 2026 Nextendo Network

// ============================================================
//  Nextendo .nro -- auto-update via GitHub Releases API.
//  Checks https://api.github.com/repositories/1292515097/releases/latest
//  for the latest version tag, compares semver with NEXTENDO_VERSION_*,
//  and downloads the .nro asset if a newer version is available.
// ============================================================
#include <switch.h>
#include <string.h>
#include <strings.h>   // strcasecmp (extension .nro insensible a la casse)
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>     // trace du motif exact d'un echec d'ecriture
#include <unistd.h>

#include "nextendo_update.h"
#include "nextendo_update_safe.h"
#include "nextendo_net.h"
#include "nextendo_apply.h"   // nextendo_trace : diagnostic de l'updater depuis la carte
#include "audio.h"            // la BGM tient un FILE* ouvert sur le romfs (cf. releaseRomfs)

// GitHub API for latest release
#define GH_API_HOST  "api.github.com"
#define GH_API_PATH  "/repositories/1292515097/releases/latest"
#define GH_API_PORT  443

// --- Chemin du .nro : celui qu'on EXECUTE, pas un chemin devine. ---
//  L'updater ecrivait TOUJOURS dans sdmc:/switch/nextendo.nro. Pour tous ceux dont le
//  Prelude vit ailleurs (autre nom, sous-dossier, racine de la carte), la mise a jour
//  deposait donc un fichier EN PLUS : l'ancien restait celui qu'ils lancaient, et la
//  carte se retrouvait avec deux versions. C'est la cause des DEUX rapports recus —
//  « l'updater ne marche pas » (on relance l'ancien) et « il devrait remplacer Prelude »
//  (les deux sont conservees). Le seul utilisateur pour qui ca marchait etait celui dont
//  le chemin coincidait avec la constante.
//  hbmenu passe le chemin REEL du .nro dans argv[0] : on ecrit LA, donc le fichier
//  remplace est exactement celui que l'utilisateur vient de lancer.
#define LEGACY_NRO_FILE "sdmc:/switch/nextendo.nro"
#define LEGACY_TMP_FILE "sdmc:/switch/nextendo.nro.new"

static char g_self_nro[512] = {0};
static char g_self_tmp[520] = {0};

void nextendo_update_set_self_path(const char *argv0) {
    // Sans argv exploitable on garde le chemin historique : mieux vaut l'ancien
    // comportement qu'une ecriture a un endroit invente.
    if (!argv0 || !*argv0) return;
    size_t n = strlen(argv0);
    if (n < 5 || n >= sizeof(g_self_nro)) return;
    if (strcasecmp(argv0 + n - 4, ".nro") != 0) return;
    if (strncmp(argv0, "sdmc:/", 6) == 0)
        snprintf(g_self_nro, sizeof(g_self_nro), "%s", argv0);
    else if (argv0[0] == '/')
        snprintf(g_self_nro, sizeof(g_self_nro), "sdmc:%s", argv0);
    else
        return;
    // Le fichier temporaire va A COTE de la cible : rename() ne traverse alors aucune
    // frontiere et le .new ne traine pas dans un dossier qui n'est pas le sien.
    snprintf(g_self_tmp, sizeof(g_self_tmp), "%s.new", g_self_nro);
}

static const char *nroPath(void) { return g_self_nro[0] ? g_self_nro : LEGACY_NRO_FILE; }
static const char *nroTmp(void)  { return g_self_tmp[0] ? g_self_tmp : LEGACY_TMP_FILE; }

// --- Relais de progression. ---
//  net_https_get_to_file ne connait que ce que le serveur annonce ; si Content-Length
//  manque il rapporte total=0 et la barre resterait plate pendant 17 Mo. On retombe
//  alors sur la taille que l'API GitHub nous a donnee, qu'on a de toute facon deja.
static nextendo_progress_fn g_progress_cb    = NULL;
static long                 g_progress_total = 0;

static void progressRelay(long received, long total) {
    if (total <= 0) total = g_progress_total;
    if (g_progress_cb) g_progress_cb(NUP_PHASE_DOWNLOAD, received, total);
}

// Trace + motif exact de l'echec. Les trois tentatives de remplacement echouaient
// toutes de la meme facon dans le rapport utilisateur (« impossible d'ecrire sur la
// carte SD ») sans jamais dire POURQUOI : errno distingue un verrou (EBUSY / EACCES)
// d'une carte pleine (ENOSPC) ou d'un chemin absent (ENOENT).
static void traceErr(const char *step) {
    char m[160];
    snprintf(m, sizeof(m), "%s (errno=%d)", step, errno);
    nextendo_trace(m);
}

// --- Liberation du romfs le temps du remplacement. ---
//  romfsInit() (main) ne monte pas un fichier a part : libnx lit argv[0] et garde un
//  handle FS OUVERT sur le .nro en cours pour toute la session, parce que le romfs est
//  lu a la demande (patches .ips, donnees BCAT, bgm). Le fichier que l'updater doit
//  remplacer est donc tenu ouvert par nous-memes : la FS refuse alors l'ouverture en
//  ecriture, remove() et rename() echouent, les trois tentatives tombent d'affilee et
//  l'utilisateur voit « impossible d'ecrire sur la carte SD » avec une mise a jour
//  pourtant deja telechargee et verifiee.
//
//  C'est la vraie cause du rapport d'Andrei depuis 3.3.3 : le build 55 a fait passer la
//  cible de « un chemin fixe » a « le fichier qu'on execute », et c'est precisement ce
//  fichier-la que le romfs tient. Avant 55 ca marchait par accident — on ecrasait un
//  AUTRE fichier que celui qui tournait (d'ou les doublons signales a l'epoque).
//
//  L'audio part en premier : mpg123 detient un FILE* ouvert sur romfs:/bgm.mp3 pendant
//  toute la session. Les polices et les images, elles, sont deja entierement en RAM
//  (ui_init lit dans un buffer puis ferme), donc l'ecran de resultat s'affiche sans
//  romfs monte.
static void releaseRomfs(void) {
    audio_exit();    // ferme le FILE* que mpg123 tient sur romfs:/bgm.mp3
    romfsExit();     // ferme le handle FS sur le .nro courant
    nextendo_trace("59 update: romfs relache (le .nro cible n'est plus ouvert)");
}

// Remonte le romfs — depuis le .nro remplace s'il l'a ete, sinon l'ancien — et relance
// la musique. Non-fatal : une MAJ reussie demande de toute facon de fermer et relancer
// Prelude, mais un ECHEC doit laisser l'app entierement utilisable (changement de mode,
// drapeaux, BCAT lisent tous le romfs).
static bool g_restart_required;
bool nextendo_update_requires_restart(void) { return g_restart_required; }

static void restoreRomfs(void) {
    if (R_FAILED(romfsInit())) { nextendo_trace("69 ERREUR update: romfs non remonte; quitter Prelude"); return; }
    g_restart_required = false;
    audio_init();
}

static bool commitUpdate(void) {
    if (R_FAILED(fsdevCommitDevice("sdmc"))) {
        nextendo_trace("update: SD commit failed");
        return false;
    }
    return true;
}

static char g_download_url[512] = {0};
static long g_download_size = 0;

// Compare two semver triplets. Returns >0 if a>b, <0 if a<b, 0 if equal.
static int semver_cmp(int amaj, int amin, int apatch, int bmaj, int bmin, int bpatch) {
    if (amaj != bmaj) return amaj - bmaj;
    if (amin != bmin) return amin - bmin;
    return apatch - bpatch;
}

NextendoUpdate nextendo_update_check(void) {
    NextendoUpdate u = { false, 0, 0, 0, 0 };
    g_download_url[0] = 0;
    g_download_size = 0;
    Result rc = socketInitializeDefault();
    if (R_FAILED(rc)) { nextendo_trace("update check: socket initialization failed"); return u; }
    rc = sslInitialize(4);
    if (R_FAILED(rc)) { socketExit(); return u; }

    size_t len = 0;
    int status = 0;
    unsigned char *body = net_https_get(GH_API_HOST, GH_API_PATH, &len, &status);
    sslExit();
    socketExit();

    PreludeRelease release;
    if (body && status == 200 && prelude_parse_release((const char *)body, len, &release)) {
        if (semver_cmp(release.major, release.minor, release.patch,
                       NEXTENDO_VERSION_MAJOR, NEXTENDO_VERSION_MINOR, NEXTENDO_VERSION_PATCH) > 0) {
            u.available = true;
            u.maj = release.major; u.min = release.minor; u.patch = release.patch;
            u.size = g_download_size = release.size;
            memcpy(g_download_url, release.url, sizeof(g_download_url));
        }
        nextendo_trace(u.available ? "update check: newer NRO found" : "update check: current version");
    } else {
        char message[96];
        snprintf(message, sizeof(message), "update check failed: HTTP=%d SSL=%08lx bytes=%lu",
                 status, (unsigned long)g_net_ssl_rc, (unsigned long)len);
        nextendo_trace(message);
    }
    free(body);
    return u;
}

// Download and apply the update. Owns socket/SSL initialization and cleanup.
nextendo_update_result nextendo_update_apply(long expectedSize, nextendo_progress_fn onProgress) {
    if (g_download_url[0] == '\0') return NUP_NET_FAIL;
    long expected = expectedSize > 0 ? expectedSize : g_download_size;

    g_progress_cb    = onProgress;
    g_progress_total = expected;

    FILE *f = fopen(nroTmp(), "wb");
    if (!f) {
        mkdir("sdmc:/switch", 0777);
        f = fopen(nroTmp(), "wb");
    }
    if (!f) { traceErr("57 ERREUR update: creation du .new impossible"); return NUP_WRITE_FAIL; }

    Result rc = socketInitializeDefault();
    if (R_FAILED(rc)) { fclose(f); remove(nroTmp()); return NUP_NET_FAIL; }
    rc = sslInitialize(4);
    if (R_FAILED(rc)) { fclose(f); remove(nroTmp()); socketExit(); return NUP_NET_FAIL; }

    char host[256] = {0};
    char path[1024] = {0};
    if (sscanf(g_download_url, "https://%255[^/]%1023s", host, path) < 2) {
        sslExit(); fclose(f); remove(nroTmp()); socketExit(); return NUP_NET_FAIL;
    }

    int status = 0;
    long len = -1;
    for (int attempt = 0; attempt < 2; ++attempt) {
        g_net_ssl_rc = 0;
        len = net_https_get_to_file(host, path, f, &status,
                                   onProgress ? progressRelay : NULL);
        char message[128];
        snprintf(message, sizeof(message), "update download: attempt=%d HTTP=%d SSL=%08lx bytes=%ld",
                 attempt + 1, status, (unsigned long)g_net_ssl_rc, len);
        nextendo_trace(message);
        if (len == -2 || (len >= 0 && status == 200)) break;
        if (attempt == 1 || (status >= 400 && status < 500)) break;
        // Retry only transient network/server errors, restarting the original URL
        // so GitHub can issue a fresh CDN redirect. Never append two responses.
        clearerr(f);
        if (fflush(f) || ftruncate(fileno(f), 0) || fseek(f, 0, SEEK_SET)) {
            len = -2;
            break;
        }
    }
    bool writeOk = !ferror(f);
    if (fclose(f) != 0) writeOk = false;
    sslExit();
    socketExit();

    if (!writeOk || len == -2) { traceErr("58 ERREUR update: ecriture du .new interrompue (carte pleine ?)");
                     remove(nroTmp()); return NUP_WRITE_FAIL; }
    if (len < 0)   { remove(nroTmp()); return NUP_NET_FAIL; }
    if (status != 200 || len < 4096) { remove(nroTmp()); return NUP_NET_FAIL; }
    if (expected > 0 && len != expected) { remove(nroTmp()); return NUP_SIZE_FAIL; }
    if (!commitUpdate()) { remove(nroTmp()); return NUP_WRITE_FAIL; }
    if (!prelude_validate_nro(nroTmp(), expected)) {
        nextendo_trace("update: stored NRO validation failed; installed NRO untouched");
        remove(nroTmp());
        return NUP_SIZE_FAIL;
    }

    char backup[sizeof(g_self_tmp) + 24];
    snprintf(backup, sizeof(backup), "%s.prelude-backup", nroPath());
    if (g_progress_cb) g_progress_cb(NUP_PHASE_INSTALL, 0, expected);
    releaseRomfs();
    g_restart_required = true;
    PreludeInstallResult placed = prelude_install_nro(nroTmp(), nroPath(), backup, commitUpdate);
    if (placed != PRELUDE_INSTALL_OK) {
        traceErr("update: replacement failed; recovery backup retained if rollback failed");
        if (placed == PRELUDE_INSTALL_FAILED) restoreRomfs();
        return NUP_WRITE_FAIL;
    }
    if (g_progress_cb) g_progress_cb(NUP_PHASE_INSTALL, expected, expected);
    nextendo_trace("update: installed; close Prelude before using the new RomFS");
    // Do not mount the NEW resources into OLD code or remove another NRO path.
    return NUP_OK;
}
