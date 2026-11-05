#!/usr/bin/env python3
"""Actual extended palette cache: bounded references and live-pair protection."""
from pathlib import Path
import subprocess,tempfile
s=(Path(__file__).resolve().parents[1]/'stdconclass.c').read_text();a=s.index('static VOID stdcon_trimxterm(');helper=s[a:s.index('IPTR stdcon_getpencolor(',a)]
code=r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
typedef void VOID;typedef uint16_t UWORD;typedef uint32_t ULONG;typedef int32_t LONG;
struct Library {int unused;};
struct stdcondata {struct Library*scd_GfxBase;void*cm;LONG xtermpens[256];UWORD xtermCount,xtermNext,xtermLast;};
typedef struct{ULONG fgColor,bgColor;}Object;
#define ICU(o) (o)
#define CONPEN_XTERM(n) (0x100u+(n))
static int refs[256];
#define ReleasePen(cm,p) ((void)GfxBase,assert(refs[p]==1),refs[p]--)
'''+helper+r'''
int main(void){struct stdcondata d={0};Object o={CONPEN_XTERM(17),CONPEN_XTERM(18)};
for(int i=0;i<256;i++)d.xtermpens[i]=-1;
for(int step=0;step<20000;step++){
 unsigned n=16+(step%240),previous=d.xtermLast;int protected=previous>=16&&d.xtermpens[previous]>=0;
 /* Put the previous lookup first in the eviction scan deliberately. */
 if(previous>=16)d.xtermNext=previous-16;
 if(d.xtermpens[n]<0){assert(!refs[n]);refs[n]=1;d.xtermpens[n]=n;stdcon_trimxterm(&o,&d,n);}
 assert(d.xtermpens[n]==(LONG)n&&refs[n]);
 if(protected)assert(d.xtermpens[previous]>=0&&refs[previous]);
 if(step>2)assert(d.xtermpens[17]>=0&&refs[17]&&d.xtermpens[18]>=0&&refs[18]);
 d.xtermLast=n;int live=0;for(int i=16;i<256;i++){assert((d.xtermpens[i]>=0)==!!refs[i]);live+=refs[i];}assert(live==d.xtermCount&&live<=64);
}
for(int i=16;i<256;i++)if(d.xtermpens[i]>=0)refs[i]--;
for(int i=0;i<256;i++){assert(!refs[i]);}
puts("Indexed cache PASS: 20000 lookups,64 bound,active rendition and previous lookup protected,balanced references");}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';b=Path(d)/'test';p.write_text(code)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(b)],check=True);subprocess.run([str(b)],check=True)
 p.write_text(code.replace('victim == data->xtermLast ||',''))
 subprocess.run(['cc','-Wall','-Werror',str(p),'-o',str(b)],check=True)
 assert subprocess.run([str(b)],capture_output=True).returncode!=0
 print('Indexed cache missing-pair protection negative control rejected')
