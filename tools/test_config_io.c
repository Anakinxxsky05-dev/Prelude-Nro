#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int fail_seek, fail_tell, fail_read, fail_close, fail_write, fail_rename;
static int rename_count, commit_count, fail_commit;
static int test_seek(FILE *f,long n,int whence) { return fail_seek ? -1 : fseek(f,n,whence); }
static long test_tell(FILE *f) { return fail_tell ? -1 : ftell(f); }
static size_t test_read(void *p,size_t s,size_t n,FILE *f) { return fail_read ? 0 : fread(p,s,n,f); }
static size_t test_write(const void *p,size_t s,size_t n,FILE *f) { return fail_write ? 0 : fwrite(p,s,n,f); }
static int test_close(FILE *f) { int r=fclose(f); return fail_close ? EOF : r; }
static int test_rename(const char *a,const char*b) { return ++rename_count==fail_rename ? -1 : rename(a,b); }
#define fseek test_seek
#define ftell test_tell
#define fread test_read
#define fwrite test_write
#define fclose test_close
#define rename test_rename
#include "../source/prelude_io.c"
#undef fseek
#undef ftell
#undef fread
#undef fwrite
#undef fclose
#undef rename
static const char *path="config-test.ini",*backup="config-test.ini.prelude-backup",*staged="config-test.ini.prelude-new";
static bool commit(void) { return ++commit_count != fail_commit; }
static void reset(void) {
    fail_seek=fail_tell=fail_read=fail_close=fail_write=fail_rename=rename_count=commit_count=fail_commit=0;
    remove(path);remove(backup);remove(staged);
    FILE*f=fopen(path,"wb");assert(f);fputs("[user]\nkeep=yes\n",f);fclose(f);
}
static void original(void) {
    FILE*f=fopen(path,"rb");assert(f);char text[100]={0};fread(text,1,sizeof(text)-1,f);fclose(f);
    assert(!strcmp(text,"[user]\nkeep=yes\n"));
}
int main(void) {
    char *text=NULL;size_t size=0;
    reset(); assert(prelude_read_config(path,&text,&size)); assert(size==16);free(text);
    for (int i=0;i<5;++i) {
        reset();
        if(i==0)fail_seek=1;
        if(i==1)fail_tell=1;
        if(i==2)fail_read=1;
        if(i==3)fail_close=1;
        if(i==4)fail_write=1;
        assert(!prelude_write_config(path,"new configuration",commit));original();
    }
    reset();fail_rename=1;assert(!prelude_write_config(path,"new",commit));original();
    reset();fail_rename=2;assert(!prelude_write_config(path,"new",commit));original();
    for (int i=1;i<=3;++i) {reset();fail_commit=i;assert(!prelude_write_config(path,"new",commit));original();}
    reset();assert(prelude_write_config(path,"new",commit));assert(prelude_read_config(backup,&text,&size));assert(strstr(text,"keep=yes"));free(text);
    assert(prelude_write_config(path,"next",commit));assert(prelude_read_config(path,&text,&size));assert(!strcmp(text,"next"));free(text);
    // Nintendo mode removes generated hosts but the recovery sibling remains.
    // Re-enabling Nextendo must not permanently fail because only that sibling exists.
    assert(remove(path)==0);
    assert(prelude_write_config(path,"enabled again",commit));
    assert(prelude_read_config(path,&text,&size));assert(!strcmp(text,"enabled again"));free(text);
    reset();FILE*f=fopen(path,"wb");assert(f);fwrite("a\0b",1,3,f);fclose(f);assert(!prelude_read_config(path,&text,&size));
    remove(path);remove(backup);remove(staged);assert(prelude_read_config(path,&text,&size));assert(text==NULL&&size==0);
    puts("PASS: config seek/read/close/write/rename/commit failures preserve original settings");
}
