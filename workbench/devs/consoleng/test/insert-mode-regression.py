#!/usr/bin/env python3
"""Run actual ANSI list parser and raster insertion bounds with sanitizers."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
s=(r/'support.c').read_text();a=s.rindex('static BOOL getansimodecommand(');parser=s[a:s.index('/* DECRQM',a)]
s=(r/'stdconclass.c').read_text();a=s.index('static VOID stdcon_insert_print(');raster=s[a:s.index('/*********  xterm',a)]
code=r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>
typedef unsigned char UBYTE;typedef signed char BYTE;typedef uint32_t ULONG;typedef int32_t LONG;typedef int BOOL;typedef void VOID;typedef intptr_t IPTR;
#define TRUE 1
#define FALSE 0
#define C_NIL 0
#define C_SET_LF_MODE 1
#define C_RESET_LF_MODE 2
#define CF_INSERT_MODE 1024
struct Library {int unused;};struct RastPort {UBYTE FgPen;};
typedef struct {ULONG conFlags;int lf,x,y,max;UBYTE cu_BgPen;} Object;
#define ICU(o) (o)
#define CU(o) (o)
#define XCP (o->x)
#define YCP (o->y)
#define XRSIZE 8
#define CHAR_XMAX(o) (o->max)
#define GFX_X(o,x) (10+8*(x))
#define GFX_Y(o,y) (20+16*(y))
#define GFX_XMAX(o) (10+8*((o)->max+1)-1)
#define SetAPen(rp,p) ((rp)->FgPen=(p))
static int calls,dx,rx1,ry1,rx2,ry2;
#define ScrollRaster(rp,x,y,a,b,c,d) (calls++,dx=(x),rx1=(a),ry1=(b),rx2=(c),ry2=(d))
static void Console_DoCommand(Object*o,int c,int n,IPTR*p){assert(n==0&&p==NULL);o->lf=c==C_SET_LF_MODE;}
'''+parser+raster+r'''
int main(void){
 const char*seq[]={"4h","4l","4;20h","4;20l","999;4;20h","65536;4h","999999999999999999999999;4l","20;4;20h"};
 for(unsigned i=0;i<sizeof(seq)/sizeof(*seq);i++){
  for(unsigned split=0;split<strlen(seq[i]);split++){
   Object o={0};UBYTE*p=(UBYTE*)seq[i],n=99;BYTE c=99;
   assert(!getansimodecommand(&c,&p,&n,split,&o));assert(!o.conFlags&&!o.lf&&n==99&&c==99&&p==(UBYTE*)seq[i]);
  }
  Object o={0};UBYTE*p=(UBYTE*)seq[i],n=99;BYTE c=99;
  assert(getansimodecommand(&c,&p,&n,strlen(seq[i]),&o));assert(!*p&&!n&&c==C_NIL);assert(!!(o.conFlags&CF_INSERT_MODE)==(seq[i][strlen(seq[i])-1]=='h'));
 }
 const char*bad[]={"4;20$h","4;20?h","?4h",">4h","4:20h","4q","4;"};
 for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);i++){Object o={0};UBYTE*p=(UBYTE*)bad[i],n=99;BYTE c=99;assert(!getansimodecommand(&c,&p,&n,strlen(bad[i]),&o));assert(!o.conFlags&&!o.lf);}
 for(int width=1;width<=64;width++)for(int x=0;x<width;x++)for(unsigned n=0;n<=65;n++)for(int enabled=0;enabled<2;enabled++){
  Object o={.x=x,.y=3,.max=width-1,.cu_BgPen=5};struct RastPort rp={7};calls=0;stdcon_insert_print(&o,&rp,NULL,n,enabled);assert(rp.FgPen==7);
  assert(calls==(enabled&&n&&n<(unsigned)(width-x)));if(calls){assert(dx==-(int)n*8&&rx1==10+x*8&&rx2==10+width*8-1&&ry1==68&&ry2==83);}
 }
 return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(code)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS: ANSI mode lists, every partial prefix, malformed/overflow isolation; 274560 raster insertion bound cases')
