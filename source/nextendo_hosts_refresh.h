// Prelude — Nintendo Switch homebrew for the Nextendo Network.
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
//  Rafraichissement des hosts au demarrage : decisions PURES (aucune E/S, aucune dependance libnx),
//  pour pouvoir les tester sur un PC.
//
//  Probleme : le fichier hosts n'etait ecrit QU'A l'application du mode Nextendo. Une mise a jour de
//  Prelude (qui ajoute un jeu, donc de nouvelles lignes) remplacait le .nro sans toucher aux hosts de
//  la carte : le joueur "a jour" n'avait pas les lignes du nouveau jeu (UCH, UNO...) et devait recevoir
//  le fichier a la main. Ici on decide, pour UN fichier, s'il faut le reecrire.
//
//  Regles de prudence :
//   - jamais un fichier qui n'est pas le notre (pas de signature "NEXTENDO NETWORK") : il appartient
//     a l'utilisateur ;
//   - jamais un fichier absent (on n'en cree pas : c'est le role de l'application du mode) ;
//   - rien a faire si le contenu est deja exactement celui de cette version.
// ============================================================
#ifndef NEXTENDO_HOSTS_REFRESH_H
#define NEXTENDO_HOSTS_REFRESH_H

#include <stddef.h>
#include <string.h>

typedef enum {
    HOSTS_KEEP_NOT_OURS = 0,   // absent, vide ou pas de signature Nextendo : ne pas toucher
    HOSTS_KEEP_CURRENT  = 1,   // identique a ce que cette version ecrirait : rien a faire
    HOSTS_REWRITE       = 2,   // a nous, mais different : reecrire
} HostsRefreshAction;

// La signature est dans l'en-tete : on ne cherche que dans le debut du fichier, pour ne pas
// confondre un fichier d'utilisateur qui citerait ce texte plus bas.
#define NEXTENDO_HOSTS_MARK_WINDOW 512

static inline int nextendo_hosts_has_mark(const char *text, size_t len, const char *mark) {
    size_t markLen = strlen(mark);
    if (markLen == 0 || len < markLen) return 0;
    size_t window = len < NEXTENDO_HOSTS_MARK_WINDOW ? len : NEXTENDO_HOSTS_MARK_WINDOW;
    if (window < markLen) return 0;
    for (size_t i = 0; i + markLen <= window; i++)
        if (memcmp(text + i, mark, markLen) == 0) return 1;
    return 0;
}

// `current` : contenu actuel du fichier (NULL s'il est absent), `expected` : ce que cette version
// ecrirait, `mark` : la signature de nos fichiers.
static inline HostsRefreshAction nextendo_hosts_refresh_action(const char *current, size_t currentLen,
                                                               const char *expected, const char *mark) {
    if (!current || currentLen == 0 || !expected) return HOSTS_KEEP_NOT_OURS;
    if (!nextendo_hosts_has_mark(current, currentLen, mark)) return HOSTS_KEEP_NOT_OURS;
    size_t expectedLen = strlen(expected);
    if (currentLen == expectedLen && memcmp(current, expected, expectedLen) == 0) return HOSTS_KEEP_CURRENT;
    return HOSTS_REWRITE;
}

static inline int nextendo_hosts_mentions(const char *text, const char *needle) {
    return text && strstr(text, needle) != NULL;
}

// L'IP que les fichiers de la carte utilisent deja. g_server_ip repart de la valeur par defaut a chaque
// lancement : sans cette deduction, un joueur sur l'adresse alternative serait renvoye sur celle du VPS.
// Les contenus doivent se terminer par '\0'. L'alternative n'est choisie que si elle est la SEULE presente.
static inline const char *nextendo_hosts_pick_ip(const char *const *contents, size_t count,
                                                 const char *defaultIp, const char *altIp) {
    int hasDefault = 0, hasAlt = 0;
    for (size_t i = 0; i < count; i++) {
        if (nextendo_hosts_mentions(contents[i], defaultIp)) hasDefault = 1;
        if (nextendo_hosts_mentions(contents[i], altIp)) hasAlt = 1;
    }
    return (hasAlt && !hasDefault) ? altIp : defaultIp;
}

#endif // NEXTENDO_HOSTS_REFRESH_H
