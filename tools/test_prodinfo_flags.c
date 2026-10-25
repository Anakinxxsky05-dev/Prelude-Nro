#include <assert.h>
#include <stdio.h>
#include "../source/prelude_prodinfo.h"

int main(void) {
    const char *key = "blank_prodinfo_emummc";
    assert(prelude_prodinfo_flag(NULL,key)==0);
    assert(prelude_prodinfo_flag("",key)==0);
    assert(prelude_prodinfo_flag("[exosphere]\nblank_prodinfo_emummc=1\n",key)==1);
    assert(prelude_prodinfo_flag("[exosphere]\r\n\tblank_prodinfo_emummc = 1 ; enabled\r\n",key)==1);
    assert(prelude_prodinfo_flag("[exosphere]\n# blank_prodinfo_emummc=1\n; blank_prodinfo_emummc=1\nblank_prodinfo_emummc=0\n",key)==0);
    assert(prelude_prodinfo_flag("[other]\nblank_prodinfo_emummc=1\n",key)==0);
    assert(prelude_prodinfo_flag("[exosphere]\nblank_prodinfo_sysmmc=1\nblank_prodinfo_emummc_extra=1\n",key)==0);
    assert(prelude_prodinfo_flag("[exosphere]\nblank_prodinfo_emummc=1\n[other]\nblank_prodinfo_emummc=0\n",key)==1);
    assert(prelude_prodinfo_flag("[exosphere]\nblank_prodinfo_emummc=invalid\n",key)==-1);
    assert(prelude_prodinfo_flag("[exosphere]\nblank_prodinfo_emummc=10\n",key)==-1);
    assert(prelude_prodinfo_flag("[exosphere]\nblank_prodinfo_emummc=\n",key)==-1);
    assert(prelude_prodinfo_flag("[exosphere]\nblank_prodinfo_emummc=0\nblank_prodinfo_emummc=1\n",key)==-1);
    assert(prelude_prodinfo_flag("[exosphere]\nblank_prodinfo_emummc=1\nblank_prodinfo_emummc=0\n",key)==-1);
    assert(prelude_prodinfo_flag("[exosphere]\nblank_prodinfo_emummc=1\nblank_prodinfo_emummc=1\n",key)==1);
    puts("PASS: PRODINFO saved-state verification handles whitespace, comments, sections, missing/invalid values and conflicting duplicates");
}
