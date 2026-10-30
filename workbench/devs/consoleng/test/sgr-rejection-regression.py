#!/usr/bin/env python3
"""Unsupported selectors and underline colors preserve unrelated styles and pens."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
s=(r/'consoleclass.c').read_text();a=s.index('                switch (param)');b=s.index('                } /* switch(param) */',a)+len('                }');switch=s[a:b]
a=s.index('                /* Consume extended colors');b=s.index('                /* Bright ANSI:',a);discard=s[a:b]
code=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef unsigned long ULONG;typedef int BOOL;
#define TRUE 1
#define FALSE 0
#define CONPEN_XTERM(n) (0x100+(n))
#define CONPEN_RGB(r,g,b) (0x1000000|((r)<<16)|((g)<<8)|(b))
typedef long IPTR;typedef unsigned char UBYTE;typedef struct {UBYTE cu_TxFlags,cu_FgPen,cu_BgPen,underlineStyle;ULONG underlineColor;} Object;
#define CU(o) (o)
#define ICU(o) (o)
#define CON_TXTFLAGS_BOLD 2
#define CON_TXTFLAGS_FAINT 128
#define CON_TXTFLAGS_ITALIC 4
#define CON_TXTFLAGS_UNDERLINED 1
#define CON_TXTFLAGS_REVERSED 8
#define CON_TXTFLAGS_CONCEALED 16
#define CON_TXTFLAGS_STRUCK 64
#define CON_TXTFLAGS_DOUBLEUNDERLINED 32
#define CONUNIT_PEN_DEFFG 8
#define CONUNIT_PEN_DEFBG 9
static void Console_GetColorPen(Object *o,int key,UBYTE *out){*out=key;}
static void sgr(Object *o,int param){
'''+switch+r'''
}
static void sequence(Object *o,const IPTR *values,UBYTE length){
struct {const IPTR *Params;UBYTE NumParams;} request={values,length},*msg=&request;
for(UBYTE i=0;i<length;i++){IPTR param=values[i];
'''+discard+switch+r'''
}}
int main(void){Object reset={0};reset.underlineColor=0x1012346;sgr(&reset,59);assert(!reset.underlineColor);reset.underlineColor=0x1012346;sgr(&reset,0);assert(!reset.underlineColor);for(int f=0;f<256;f++){Object o={f,1,2};sgr(&o,21);assert(o.cu_TxFlags==((f&~1)|32));sgr(&o,4);assert(o.cu_TxFlags==((f&~32)|1));sgr(&o,24);assert(o.cu_TxFlags==(f&~33));}

IPTR colors[][7]={{58},{58,5},{58,5,1},{58,5,31},{58,2},{58,2,1},{58,2,1,4},{58,2,1,4,31},{58,9},{58,2,255,0,255},{58,5,256}};
UBYTE lengths[]={1,2,3,3,2,3,4,5,2,5,3};
for(int f=0;f<256;f++)for(unsigned k=0;k<sizeof(lengths);k++){
 Object o={f,6,7},before=o;sequence(&o,colors[k],lengths[k]);assert(o.cu_TxFlags==before.cu_TxFlags&&o.cu_FgPen==before.cu_FgPen&&o.cu_BgPen==before.cu_BgPen);
 if(k==2)assert(o.underlineColor==258);
 if(k==7)assert(o.underlineColor==0x1010420);
}
IPTR tail[]={58,2,1,4,31,32,23};Object o={4,6,7};sequence(&o,tail,7);assert(o.cu_TxFlags==0&&o.cu_FgPen==2&&o.cu_BgPen==7);
unsigned cases=0;for(int n=0;n<14;n++)for(int flags=0;flags<256;flags++)for(int fg=0;fg<16;fg++)for(int bg=0;bg<16;bg++){
 Object o={flags,fg,bg},before=o;sgr(&o,n<9?11+n:51+n-9);assert(!memcmp(&o,&before,sizeof(o)));cases++;
}printf("SGR rejection PASS: %u state/pen cases\n",cases);return 0;}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';b=Path(d)/'test'
 for negative in [0,1,2]:
  body=code
  if negative==1:body=body.replace('case 0:', 'case 11: case 0:',1)
  if negative==2:body=body.replace('if (param == 58)', 'if (param == 580)')
  p.write_text(body)
  subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(b)],check=True)
  result=subprocess.run([str(b)],capture_output=True)
  assert bool(result.returncode)==bool(negative),result.stderr.decode()
  if not negative:print(result.stdout.decode(),end='')
print('Unsupported-font reset and underline-color leakage negative controls rejected')
