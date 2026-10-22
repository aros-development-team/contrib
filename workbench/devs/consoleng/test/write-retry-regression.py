#!/usr/bin/env python3
"""Fault-inject the actual pending-control merge before any parser mutation."""
from pathlib import Path
import subprocess, tempfile
root = Path(__file__).resolve().parents[1]
s = (root / 'support.c').read_text()
a = s.index('    *error = 0;', s.index('ULONG con_parse_output('))
b = s.index('    write_str = orig_write_str', a)
block = s[a:b]
a = s.index('static BOOL csi_incomplete(')
b = s.index('\n\nULONG con_parse_output(', a)
incomplete = s[a:b]
code = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef uint8_t UBYTE; typedef uint16_t UWORD; typedef uint32_t ULONG;
typedef int32_t LONG; typedef char *STRPTR;
#define IOERR_BADLENGTH -4
#define IOERR_ABORTED -2
#define MEMF_ANY 0
#define TRUE 1
#define FALSE 0
#define ESC 27
#define CSI 155
typedef int BOOL;
struct Unit {UBYTE pendingCSI[64];UWORD pendingCSILen;};
static int fail_alloc;static unsigned allocations,copies;
static void *AllocMem(ULONG size,int flags){(void)flags;allocations++;if(fail_alloc)return NULL;assert(size<=65536+64);return malloc(size);}
static void CopyMem(const void*s,void*d,ULONG n){assert(n&&s&&d);copies++;memcpy(d,s,n);}
''' + incomplete + r'''
static ULONG run(struct Unit*icu,STRPTR buf,ULONG towrite,LONG*error,UBYTE **merged){UBYTE*allocbuf=NULL;
''' + block + r'''
*merged=allocbuf;return towrite;}
int main(void){unsigned cases=0;ULONG sizes[]={1,4,31,64,256,32768,65536};UBYTE suffix[65536];memset(suffix,0xa5,sizeof(suffix));
for(unsigned prefix=1;prefix<=64;prefix++)for(unsigned j=0;j<sizeof(sizes)/sizeof(*sizes);j++){
 struct Unit u={0},before;for(unsigned k=0;k<prefix;k++)u.pendingCSI[k]=k+1;u.pendingCSILen=prefix;before=u;
 LONG error=123;UBYTE *merged=NULL;allocations=copies=0;fail_alloc=1;
 assert(run(&u,(char*)suffix,sizes[j],&error,&merged)==0);assert(error==IOERR_ABORTED&&!merged&&allocations==1&&!copies);assert(!memcmp(&u,&before,sizeof(u)));
 fail_alloc=0;error=123;allocations=copies=0;
 assert(run(&u,(char*)suffix,sizes[j],&error,&merged)==prefix+sizes[j]);assert(!error&&u.pendingCSILen==0&&allocations==1&&copies==1+!!sizes[j]);assert(!memcmp(merged,before.pendingCSI,prefix));assert(!memcmp(merged+prefix,suffix,sizes[j]));free(merged);cases++;
}
for(unsigned prefix=0;prefix<=64;prefix++){
 ULONG sizes[]={0x7fffffffU-prefix+1,0x80000000U,0xfffffffeU,0xffffffffU};
 for(unsigned j=0;j<4;j++){struct Unit u={0},before;u.pendingCSILen=prefix;before=u;LONG error=123;UBYTE*merged=NULL;allocations=copies=0;fail_alloc=1;
 assert(run(&u,NULL,sizes[j],&error,&merged)==0);assert(error==IOERR_BADLENGTH&&!allocations&&!copies&&!merged&&!memcmp(&u,&before,sizeof(u)));cases++;}
}
for(unsigned prefix=0;prefix<=64;prefix++){struct Unit u={0},before;u.pendingCSILen=prefix;before=u;LONG error=123;UBYTE*merged=NULL;allocations=copies=0;assert(!run(&u,NULL,0,&error,&merged));assert(!error&&!allocations&&!copies&&!merged&&!memcmp(&u,&before,sizeof(u)));cases++;}
/* The largest supported span is bounded before an allocation attempt. */
for(unsigned prefix=1;prefix<=64;prefix++){struct Unit u={0};u.pendingCSILen=prefix;LONG error=0;UBYTE*merged=NULL;allocations=copies=0;fail_alloc=1;assert(!run(&u,NULL,0x7fffffffU-prefix,&error,&merged));assert(error==IOERR_ABORTED&&allocations==1&&!copies&&u.pendingCSILen==prefix);cases++;}
for(int next=0;next<256;next++){UBYTE bytes[]={27,next,'G'};int expected=(next>='('&&next<='+')||next=='#'||next=='%'||next=='[';assert(csi_incomplete(bytes,1));assert(csi_incomplete(bytes,2)==expected);assert(!csi_incomplete(bytes,3));}
printf("Incomplete escape prefixes PASS: 256 second-byte cases\n");
printf("Pending control merge PASS: %u allocation failure/retry/length cases\n",cases);
}
'''
with tempfile.TemporaryDirectory() as d:
    p=Path(d)/'test.c'; exe=Path(d)/'test'
    for negative in (0,1,2):
        candidate=code.replace('if (!allocbuf)\n', 'icu->pendingCSILen = 0;\n        if (!allocbuf)\n') if negative == 1 else code
        if negative == 2: candidate=candidate.replace(" || buf[1] == '%'", "")
        p.write_text(candidate)
        subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(exe)],check=True)
        result=subprocess.run([str(exe)],capture_output=True)
        assert bool(result.returncode)==bool(negative),result.stderr.decode()
        if not negative: print(result.stdout.decode(),end='')
print('Lost-prefix and missing-percent-prefix negative controls rejected')
