#!/usr/bin/env python3
"""Actual DECSCUSR parser and cursor rectangle/pattern restoration."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
def function(s,name,last=False):
 a=s.rindex(name) if last else s.index(name);a=s.rfind('\n',0,a)+1;b=s.index('{',a);d=1;c=b+1
 while d:d+=(s[c]=='{')-(s[c]=='}');c+=1
 return s[a:c]
parser=function((root/'support.c').read_text(),'static BOOL getcursorstylecommand(',True)
draw=function((root/'stdconclass.c').read_text(),'static VOID stdcon_drawcursor(')
prefix=r'''
#include <assert.h>
#include <string.h>
#include <stdio.h>
typedef void VOID;typedef int BOOL;typedef unsigned char UBYTE;typedef signed char BYTE;typedef unsigned short UWORD;typedef unsigned long ULONG;typedef long LONG;typedef long IPTR;
#define TRUE 1
#define FALSE 0
#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))
#define C_NIL 0
#define C_SET_CURSOR_STYLE 1
#define COMPLEMENT 1
#define JAM2 2
struct Library {int unused;};
struct RastPort {const UWORD *AreaPtrn;BYTE AreaPtSz;UBYTE DrawMode;};
struct stdcondata {struct Library *scd_GfxBase;int windowactive,cursorstyle,cursorpainted,cursorQueryReady,cursorPatterned;LONG cursorLeft,cursorTop,cursorRight,cursorBottom;};
typedef struct {struct RastPort rp;int w,h,span;} Object;
#define Console_CursorSpan(o) ((o)->span)
#define RASTPORT(o) (&(o)->rp)
#define CP_X(o) 10
#define CP_Y(o) 20
#define XRSIZE (o->w)
#define YRSIZE (o->h)
static BOOL stdcon_coloredcursor(Object *o,struct stdcondata *d){return FALSE;}
static int mode,rect[4],patterned;
static const UWORD *paintpattern;
#define SetDrMd(rp,m) ((void)GfxBase,mode=(rp)->DrawMode=(m))
#define RectFill(rp,a,b,c,d) (patterned=((rp)->AreaPtrn!=0),paintpattern=(rp)->AreaPtrn,rect[0]=(a),rect[1]=(b),rect[2]=(c),rect[3]=(d))
'''
main=r'''
int main(void){
 for(int style=0;style<100;style++){
  char seq[16];snprintf(seq,sizeof(seq),"%d q",style);
  for(int n=0;n<strlen(seq);n++){
   UBYTE *p=(UBYTE*)seq,count=99;BYTE cmd=99;IPTR value=99;
   assert(!getcursorstylecommand(&cmd,&p,&count,n,&value));
   assert(p==(UBYTE*)seq&&cmd==99&&count==99&&value==99);
  }
  UBYTE *p=(UBYTE*)seq,count=99;BYTE cmd=99;IPTR value=99;
  assert(getcursorstylecommand(&cmd,&p,&count,strlen(seq),&value)&&!*p);
  assert(cmd==(style<=6?C_SET_CURSOR_STYLE:C_NIL));assert(count==(style<=6));
  assert(value==(style<=6?style:99));
 }
 const char *bad[]={"?2 q","2q","2;4 q","2 !q","-1 q"};
 for(int i=0;i<5;i++){UBYTE *p=(UBYTE*)bad[i],count=99;BYTE cmd=99;IPTR value=99;assert(!getcursorstylecommand(&cmd,&p,&count,strlen(bad[i]),&value));}
 UBYTE *p=(UBYTE*)" q",count=99;BYTE cmd=99;IPTR value=99;
 assert(getcursorstylecommand(&cmd,&p,&count,2,&value)&&cmd==1&&value==0);
 for(int w=1;w<=64;w++)for(int h=1;h<=64;h++)for(int style=0;style<7;style++)for(int active=0;active<=1;active++)for(int oldmode=0;oldmode<8;oldmode++){
  static const UWORD pattern[]={7,8};Object o={{pattern,3,oldmode},w,h};struct stdcondata data={0,active,style,0};
  stdcon_drawcursor(&o,&data);assert(mode==oldmode&&o.rp.DrawMode==oldmode&&o.rp.AreaPtrn==pattern&&o.rp.AreaPtSz==3);
  assert(patterned==!active);
  if(!active)assert(paintpattern[0]==0xAAAA&&paintpattern[1]==0x5555);
  assert(data.cursorpainted);
  stdcon_drawcursor(&o,&data);assert(!data.cursorpainted);
  assert(mode==oldmode&&o.rp.AreaPtrn==pattern&&o.rp.AreaPtSz==3);
  assert(rect[0]==10&&rect[3]==20+h-1);
  assert(rect[1]==(style==3||style==4?20+h-MAX(1,h/8):20));
  assert(rect[2]==(style==5||style==6?10+MAX(1,w/8)-1:10+w-1));
 }

 for(int w=1;w<=32;w++)for(int h=1;h<=32;h++)for(int style=0;style<7;style++)for(int active=0;active<2;active++)for(int span=1;span<=3;span++){
  static const UWORD pattern[]={7,8};Object o={{pattern,3,7},w,h,span};
  struct stdcondata data={.windowactive=active,.cursorstyle=style,.cursorQueryReady=1};
  stdcon_drawcursor(&o,&data);
  int left=10-(span==3?w:0),cells=span==1?1:2;
  assert(rect[0]==left&&rect[3]==20+h-1);
  assert(rect[1]==(style==3||style==4?20+h-MAX(1,h/8):20));
  assert(rect[2]==(style==5||style==6?left+MAX(1,w/8)-1:left+cells*w-1));
  int saved[4];memcpy(saved,rect,sizeof saved);int savedpattern=patterned;
  o.w+=5;o.h+=3;o.span=4-span;data.windowactive=!active;data.cursorstyle=(style+3)%7;
  stdcon_drawcursor(&o,&data);
  assert(!data.cursorpainted&&!memcmp(saved,rect,sizeof saved)&&patterned==savedpattern);
  assert(o.rp.DrawMode==7&&o.rp.AreaPtrn==pattern&&o.rp.AreaPtSz==3);
 }
 return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';b=Path(d)/'test'
 for negative in [0,1,2,3]:
  f=draw.replace('data->cursorstyle == 3 || data->cursorstyle == 4','data->cursorstyle == 99') if negative==1 else draw
  if negative==2:f=f.replace('SetDrMd(rp, oldmode);','(void)oldmode; SetDrMd(rp, JAM2);')
  if negative==3:f=f.replace('rp->AreaPtrn = NULL;','/* inherited pattern */')
  p.write_text(prefix+parser+f+main)
  subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(b)],check=True)
  result=subprocess.run([str(b)],capture_output=True)
  assert (result.returncode!=0)==bool(negative),result.stderr.decode()
print('Cursor style PASS: split/default/invalid DEC parser, 458752 geometry/focus/draw-mode cases, paired cursor toggles, 43008 wide-span/changed-geometry restoration cases and three negative controls')
