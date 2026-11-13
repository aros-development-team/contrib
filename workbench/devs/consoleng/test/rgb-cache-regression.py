#!/usr/bin/env python3
"""Exercise the production RGB-cache branch with recycled physical pens."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'stdconclass.c').read_text()
branch=s[s.index('    if (CONPEN_ISRGB(idx) && data->cm)'):s.index('    if (CONPEN_ISXTERM(idx) && data->cm)')]
code=r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
typedef int8_t BYTE;typedef uint32_t ULONG;typedef uint16_t UWORD;typedef uint8_t UBYTE;typedef int32_t LONG;
#define STDCON_TCPENS 64
#define TRUE 1
#define CONPEN_ISRGB(n) ((n)>=0x01000000)
#define CONUNIT_PEN_DEFFG 0
#define ICU(o) (o)
struct Library{int unused;};
struct stdcondata {int cm;struct Library *scd_GfxBase;UBYTE *penmap[1];struct{ULONG rgb;LONG pen;}tcpens[64];UWORD tcnext;};
typedef struct{ULONG fgColor,bgColor;}Object;
struct Msg{UBYTE *PenPtr;};
static ULONG rgb[256];static int refs[256],failure;
static LONG stdcon_bestpen(struct stdcondata *data,UBYTE r,UBYTE g,UBYTE b){
 (void)data;if(failure)return -1;ULONG key=0x1000000|(r<<16)|(g<<8)|b;
 for(int i=1;i<256;i++)if(refs[i]&&rgb[i]==key){refs[i]++;return i;}
 for(int i=1;i<256;i++)if(!refs[i]){refs[i]=1;rgb[i]=key;return i;}
 return -1;
}
#define ReleasePen(cm,p) ((void)GfxBase,assert(refs[p]>0),refs[p]--)
static int lookup(struct stdcondata *data,Object*o,ULONG idx,struct Msg*msg){
'''+branch+r'''
 return 0;
}
int main(void){
 UBYTE fallback=0,fg,bg,pen;struct stdcondata d={.cm=1,.penmap={&fallback}};
 Object o={0x1010203,0x1040506};struct Msg m={&fg};assert(lookup(&d,&o,o.fgColor,&m));m.PenPtr=&bg;assert(lookup(&d,&o,o.bgColor,&m));
 for(int i=0;i<10000;i++){
  m.PenPtr=&pen;assert(lookup(&d,&o,0x1100000+i,&m));assert(rgb[pen]==(ULONG)(0x1100000+i));
  assert(refs[fg]&&rgb[fg]==o.fgColor&&refs[bg]&&rgb[bg]==o.bgColor);
 }
 /* Resolve a dormant foreground in the exact slot next to be evicted, then
  * a new background. The foreground must survive the second lookup. */
 for(int i=0;i<64;i++){
  ULONG key=d.tcpens[i].rgb;d.tcnext=i;m.PenPtr=&fg;assert(lookup(&d,&o,key,&m));
  m.PenPtr=&bg;assert(lookup(&d,&o,0x1300000+i,&m));assert(refs[fg]&&rgb[fg]==key);
 }
 failure=1;m.PenPtr=&pen;assert(lookup(&d,&o,0x1ffffff,&m));assert(pen==fallback);
 for(int i=0;i<64;i++){assert(refs[d.tcpens[i].pen]>0);refs[d.tcpens[i].pen]--;}
 for(int i=1;i<256;i++)assert(!refs[i]);
 return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';b=Path(d)/'test';p.write_text(code)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(b)],check=True)
 subprocess.run([str(b)],check=True)
print('RGB cache PASS: 10000 replacements preserve active colors; 64 paired lookups, allocation failure and balanced references')
