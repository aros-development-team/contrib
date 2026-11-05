#!/usr/bin/env python3
"""Exercise the actual plain styled-run helper, including allocation failure."""
from pathlib import Path
import subprocess
import tempfile

source = (Path(__file__).resolve().parents[1] / 'stdconclass.c').read_text()
start = source.index('static void stdcon_native_span(')
end = source.index('/* VT/xterm autowrap', start)
helper = source[start:end]
code = r'''
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
typedef unsigned char UBYTE;
typedef unsigned long ULONG;
typedef long LONG;
typedef int BOOL;
typedef const unsigned char *CONST_STRPTR;
#define STDCON_TAB_COLUMNS 32768
#define CON_TXTFLAGS_BOLD 2
#define CON_TXTFLAGS_ITALIC 4
#define MEMF_ANY 0
struct Library { int unused; };
struct Font { int tf_Baseline; };
struct RastPort { struct Font *Font; LONG x,y; };
struct stdcondata { UBYTE *styleRun; ULONG styleLength,styleCapacity;
 LONG styleX,styleY,styleEnd; struct Library *scd_GfxBase; };
typedef struct { struct RastPort rp; LONG x,y,max; UBYTE cu_TxFlags,cu_FgPen,cu_BgPen;
 ULONG fgColor,underlineStyle,underlineColor; } Object;
#define RASTPORT(o) (&(o)->rp)
#define CP_X(o) ((o)->x)
#define CP_Y(o) ((o)->y)
#define CHAR_XMAX(o) ((o)->max)
#define CU(o) (o)
#define ICU(o) (o)
#define XRSIZE 8
#define YRSIZE 16
static int fail,allocs,frees,decorations;
static ULONG drawn;
static LONG drawnX;
static unsigned char bytes[32768];
static void *AllocVec(ULONG n,int flags) { allocs++;return fail?NULL:malloc(n); }
static void FreeVec(void *p) { if(p)frees++;free(p); }
#define CopyMem(a,b,n) memcpy(b,a,n)
static void Move(struct RastPort *p,LONG x,LONG y){p->x=x;p->y=y;}
static void stdcon_text(struct stdcondata*d,struct RastPort*p,CONST_STRPTR s,ULONG n){
 assert(n<=sizeof(bytes));memcpy(bytes,s,n);drawn=n;drawnX=p->x;p->x+=n*8;
}
#define ConsoleNGFaintPen(o,g,f,p,c) 77
static void decoration(struct RastPort*p,LONG x,LONG y,ULONG w,ULONG h,int fg,int bg){
 assert(x==drawnX&&w==drawn*8&&h==16&&fg==77&&bg==42);decorations++;
}
#define ConsoleNGStrike(g,p,f,x,y,w,h,fg,bg) decoration(p,x,y,w,h,fg,bg)
#define ConsoleNGStyledUnderline(g,p,f,x,y,w,h,fg,bg,s) decoration(p,x,y,w,h,fg,bg)
#define ConsoleNGColorUnderline(o,g,p,f,x,y,w,h,s,c) decoration(p,x,y,w,h,77,42)
'''+helper+r'''
int main(void){
 struct Font font={11};Object o={.rp={.Font=&font},.x=8,.y=16,.max=79,.cu_BgPen=42};
 unsigned cases=0;
 for(int plain=0;plain<2;plain++)for(int flags=0;flags<256;flags++){
  struct stdcondata d={0};o.x=8;o.cu_TxFlags=flags;
  int active=plain&&(flags&6);decorations=0;
  stdcon_native_span(&d,&o,plain,(CONST_STRPTR)"g",1);
  o.x+=8;stdcon_native_span(&d,&o,plain,(CONST_STRPTR)"jpy",3);
  assert(drawn==(active?4:3)&&drawnX==(active?8:16));
  assert(!memcmp(bytes,active?"gjpy":"jpy",drawn));assert(decorations==6);
  assert(d.styleLength==(active?4:0));FreeVec(d.styleRun);cases++;
 }
 struct stdcondata d={0};o.cu_TxFlags=6;o.x=8;fail=1;
 stdcon_native_span(&d,&o,1,(CONST_STRPTR)"g",1);
 assert(!d.styleLength&&drawn==1);fail=0;o.x=16;
 stdcon_native_span(&d,&o,1,(CONST_STRPTR)"p",1);assert(d.styleLength==1&&drawnX==16);
 o.x=0;stdcon_native_span(&d,&o,1,(CONST_STRPTR)"y",1);assert(drawn==1&&drawnX==0);
 o.x=8;o.y+=16;stdcon_native_span(&d,&o,1,(CONST_STRPTR)"j",1);assert(drawn==1);
 o.max=159;o.x=16;fail=1;stdcon_native_span(&d,&o,1,(CONST_STRPTR)"q",1);assert(drawn==1);
 fail=0;o.x=24;stdcon_native_span(&d,&o,1,(CONST_STRPTR)"g",1);assert(drawn==1&&d.styleCapacity==160);
 o.max=32768;o.x=32;stdcon_native_span(&d,&o,1,(CONST_STRPTR)"p",1);assert(drawn==1&&!d.styleLength);
 FreeVec(d.styleRun);
 assert(allocs>frees);printf("Plain styled run PASS %u style/route cases plus allocation and geometry boundaries\n",cases);
}
'''
# Rendering commands and layout/erase entry points must invalidate the run.
dispatch = source[source.index('static VOID stdcon_docommand('):]
assert 'if (msg->Command != C_ASCII && msg->Command != C_ASCII_STRING)\n        data->styleLength = 0;' in dispatch
for name in ['stdcon_clearcell', 'stdcon_newwindowsize']:
    body = source[source.index('static VOID '+name+'('):]
    assert 'data->styleLength = 0;' in body[:650]
with tempfile.TemporaryDirectory() as directory:
    path = Path(directory)
    (path/'test.c').write_text(code)
    subprocess.run(['cc','-Wall','-Werror','-Wno-unused-variable','-fsanitize=address,undefined',str(path/'test.c'),'-o',str(path/'test')],check=True)
    subprocess.run([str(path/'test')],check=True)
