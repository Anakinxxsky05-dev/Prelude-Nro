"""Replay large GitHub redirects through the actual download function, with mocked SSL."""
from pathlib import Path
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
baseline = "--baseline" in sys.argv
args = [a for a in sys.argv[1:] if a != "--baseline"]
compiler = args[0] if args else "cc"
capture = Path(args[1]).resolve() if len(args) > 1 else None
if baseline:
    source = subprocess.check_output(["git", "-c", "safe.directory=" + str(root), "-C", str(root),
        "show", "1cbd8a054df3b982d9370bc386699d497197f6ee:source/nextendo_net.c"], text=True, encoding="utf-8")
else:
    source = (root / "source/nextendo_net.c").read_text(encoding="utf-8")

def function(name, declaration):
    start = source.index(declaration + name + "(")
    pos = source.index("{", start) + 1
    depth = 1
    while depth:
        depth += (source[pos] == "{") - (source[pos] == "}")
        pos += 1
    return source[start:pos]

prefix = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include "prelude_http.h"
typedef unsigned char u8;
typedef uint32_t u32;
typedef int Result;
typedef int SslContext;
typedef struct { int index; size_t position; } SslConnection;
typedef void (*net_progress_fn)(long,long);
enum {NET_ERR_UNKNOWN=-1,NET_ERR_CONNECT=-2,NET_ERR_SOCKET=-3,NET_ERR_TIMEOUT=-4,NET_ERR_PROTO=-5,NET_ERR_OOM=-6};
#define R_FAILED(r) ((r)!=0)
#define SslVersion_Auto 0
#define SslOptionType_SkipDefaultVerify 0
#define SOL_SOCKET 1
#define SO_RCVTIMEO 2
#define SO_SNDTIMEO 3
Result g_net_ssl_rc;
const char *g_net_download_stage;
char g_net_download_host[256];
static unsigned char *responses[2];
static size_t response_sizes[2], fragment=4096;
static int active=-1;
static const char *resolve_host(const char *h){(void)h;return "127.0.0.1";}
static int tcp_connect(const char *ip,int port){(void)ip;(void)port;assert(++active<2);return 10;}
int socket_option(int fd,int level,int opt,const void*v,size_t n){(void)fd;(void)level;(void)opt;(void)v;(void)n;return 0;}
#define setsockopt socket_option
static int socket_close(int fd){(void)fd;return 0;}
#define close socket_close
static Result sslCreateContext(SslContext *c,int version){(void)version;*c=1;return 0;}
static Result sslContextCreateConnection(SslContext*c,SslConnection*s){(void)c;s->index=active;s->position=0;return 0;}
static int socketSslConnectionSetSocketDescriptor(SslConnection*s,int fd){(void)s;return fd;}
static Result sslConnectionSetHostName(SslConnection*s,const char*h,size_t n){(void)s;(void)h;(void)n;return 0;}
static Result sslConnectionSetOption(SslConnection*s,int o,bool value){(void)s;(void)o;(void)value;return 0;}
static Result sslConnectionSetNextAlpnProto(SslConnection*s,const void*p,size_t n){(void)s;(void)p;(void)n;return 0;}
static Result sslConnectionDoHandshake(SslConnection*s,void*a,void*b,void*c,int n){(void)s;(void)a;(void)b;(void)c;(void)n;return 0;}
static Result sslConnectionWrite(SslConnection*s,const void*p,size_t n,u32*written){(void)s;(void)p;*written=(u32)n;return 0;}
static Result sslConnectionRead(SslConnection*s,void*p,size_t cap,u32*n){size_t left=response_sizes[s->index]-s->position;if(left>cap)left=cap;if(left>fragment)left=fragment;memcpy(p,responses[s->index]+s->position,left);s->position+=left;*n=(u32)left;return 0;}
static void sslConnectionClose(SslConnection*s){(void)s;}
static void sslContextClose(SslContext*s){(void)s;}
'''
main = r'''
int main(int argc,char**argv){
    responses[0]=malloc(80000);responses[1]=malloc(80000);assert(responses[0]&&responses[1]);
    size_t n=(size_t)sprintf((char*)responses[0],"HTTP/1.1 302 Found\r\nLocation: https://release-assets.githubusercontent.com/asset\r\nContent-Security-Policy: ");
    memset(responses[0]+n,'x',10789-n-4);memcpy(responses[0]+10789-4,"\r\n\r\n",4);response_sizes[0]=10789;
    if(argc>1){FILE*f=fopen(argv[1],"rb");assert(f);response_sizes[0]=fread(responses[0],1,80000,f);assert(!ferror(f));fclose(f);}
    n=(size_t)sprintf((char*)responses[1],"HTTP/1.1 200 OK\r\nContent-Length: 6000\r\n\r\n");memset(responses[1]+n,'T',6000);response_sizes[1]=n+6000;
    FILE*out=tmpfile();assert(out);int status=0;
    long result=net_https_get_to_file("github.com","/release/nextendo.nro",out,&status,NULL);
#if EXPECT_BASELINE
    assert(result==-1&&status==0&&active==0);
    puts("CONFIRMED: previous production downloader rejects the 10789-byte GitHub redirect with HTTP=0, SSL=0");
#else
    assert(result==6000&&status==200&&active==1);rewind(out);
    for(int i=0;i<6000;i++){assert(fgetc(out)=='T');}
    assert(fgetc(out)==EOF);
    fclose(out);out=tmpfile();assert(out);active=-1;fragment=32768;
    memset(responses[0],'x',70000);response_sizes[0]=70000;
    assert(net_https_get_to_file("github.com","/release",out,&status,NULL)==-1);
    assert(status==NET_ERR_PROTO&&!strcmp(g_net_download_stage,"HTTP_header_limit"));
    puts("PASS: actual downloader follows large GitHub redirect, preserves body, and rejects oversized headers explicitly");
#endif
    fclose(out);free(responses[0]);free(responses[1]);return 0;
}
'''
with tempfile.TemporaryDirectory(prefix="prelude-download-") as directory:
    folder = Path(directory)
    test = folder / "test.c"
    test.write_text(prefix + function("parse_status", "static int ") + function("extract_location", "static void ") +
                    function("net_https_get_to_file", "long ") + main, encoding="utf-8")
    binary = folder / ("test.exe" if sys.platform == "win32" else "test")
    subprocess.run([compiler,"-std=gnu11","-Wall","-Wextra","-Werror","-DEXPECT_BASELINE="+str(int(baseline)),
                    "-I",str(root/"source"),str(test),str(root/"source/prelude_http.c"),"-o",str(binary)],check=True)
    subprocess.run([str(binary)] + ([str(capture)] if capture else []),check=True)
