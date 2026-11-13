#!/usr/bin/env python3
"""Actual immediate Latin-1 branch keeps native Text style and decorations."""
from pathlib import Path
import subprocess,tempfile
s=(Path(__file__).resolve().parents[1]/'stdconclass.c').read_text();a=s.index('                    if (cp < 0x100 && cw == 1)');b=s.index('                    data->styleLength = 0;',a);branch=s[a:b]
code=r'''
/* This fixture has no plain wide grid; standard-width-regression and native edits cover it. */
#define stdcon_width_print(d,o,p,n,w,i) ((void)0)
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
typedef uint8_t UBYTE;typedef uint32_t ULONG;typedef const char* CONST_STRPTR;
struct Unit{UBYTE cu_TxFlags,cu_FgPen,cu_BgPen;ULONG fgColor;UBYTE underlineStyle;ULONG underlineColor;};struct Font{int tf_Baseline;};struct RastPort{struct Font*Font;int mode,x,y;};
#define CU(o) (o)
#define ICU(o) (o)
#define CP_X(o) 17
#define CP_Y(o) 23
#define XRSIZE 8
#define YRSIZE 16
static unsigned texts,strikes,advances,inserts,ink;static UBYTE value,seenflags;static int moreseen;
static void stdcon_insert_print(struct Unit*o,struct RastPort*rp,void*g,int n,int insert){assert(n==1);inserts++;}
static void Move(struct RastPort*rp,int x,int y){rp->x=x;rp->y=y;}
#define OCLASS(o) 0
static void stdcon_native_span(void*d,struct Unit*o,int plain,const UBYTE*p,int n){assert(plain&&n==1);value=*p;texts++;strikes++;seenflags=o->cu_TxFlags;ink=77;}
static void stdcon_advance_print(struct Unit*o,int cells,int more){assert(cells==1);advances++;moreseen=more;}
static int run(ULONG cp,int cw,struct Unit*o,int more){UBYTE bytes[4]={0};const UBYTE*p=bytes,*q=bytes+2,*end=bytes+(more?3:2);void*data=0,*GfxBase=0,*cl=0;int insert=1;struct Font font={11};struct RastPort raster={&font,7,0,0},*rp=&raster;
texts=strikes=advances=inserts=0;
do{
'''+branch+r'''
}while(0);
assert(rp->mode==7);return p==q;}
int main(void){unsigned cases=0;
for(unsigned cp=0;cp<512;cp++)for(int cw=0;cw<=2;cw++)for(unsigned flags=0;flags<256;flags++)for(int more=0;more<2;more++){
 struct Unit u={flags,31,42,0x123456};int active=cp<256&&cw==1;
 assert(run(cp,cw,&u,more)==active);assert(texts==active&&strikes==active&&advances==active&&inserts==active);
 if(active)assert(value==cp&&seenflags==flags&&ink==77&&moreseen==more);
 cases++;
}
printf("Latin1 dispatch PASS cases=%u\n",cases);}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'t.c';exe=Path(d)/'t'
 for negative in [0,1]:
  p.write_text(code.replace('if (cp < 0x100 && cw == 1)','if (cp < 0x80 && cw == 1)') if negative else code)
  subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(exe)],check=True)
  result=subprocess.run([str(exe)],capture_output=True);assert bool(result.returncode)==bool(negative),result.stderr.decode()
  if not negative:print(result.stdout.decode(),end='')
print('Latin1 exclusion negative control rejected')
