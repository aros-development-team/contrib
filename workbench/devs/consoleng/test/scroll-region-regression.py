#!/usr/bin/env python3
"""Execute production DECSTBM parser/state and vertical scroll boundaries."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
def fn(s,name):
 a=s.rindex(name);a=s.rfind('\n',0,a)+1;b=s.index('{',a);depth=1;c=b+1
 while depth: depth+=(s[c]=='{')-(s[c]=='}');c+=1
 return s[a:c]
h=(r/'console_gcc.h').read_text();enum='enum'+h.split('enum',1)[1].split('};',1)[0]+'};'
macros='\n'.join(x for x in h.splitlines() if x.startswith('#define CON_SCROLL_'))
functions=fn((r/'support.c').read_text(),'static BOOL getscrollregion(')+fn((r/'stdconclass.c').read_text(),'static BOOL stdcon_scroll_region(')
v=(r/'stdconclass.c').read_text();functions+=fn(v,'static WORD stdcon_relative_coordinate(')+fn(v,'static VOID stdcon_relative_position(')
s=(r/'consoleclass.c').read_text();functions+=fn(s,'static VOID console_up(')+fn(s,'static VOID console_down(')
code=r'''
#define CF_ORIGIN_MODE 2048
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
typedef short WORD;typedef intptr_t IPTR;typedef long LONG;typedef int BOOL;typedef unsigned char UBYTE;typedef signed char BYTE;typedef void VOID;typedef int Class;
#define TRUE 1
#define FALSE 0
#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))
#define CF_WRAP_PENDING 128
#define ICU(o) (o)
#define CHAR_XMIN(o) 0
#define CHAR_XMAX(o) 79
#define CHAR_YMIN(o) 0
#define CHAR_YMAX(o) ((o)->max)
#define XCP (o->x)
#define XCCP (o->xc)
#define YCP (o->y)
#define YCCP (o->yc)
#define EnterFunc(x) ((void)0)
#define ReturnVoid(x) return
#define D(x) ((void)0)
#define CHECK_MODE(o,m) ((o)->autoScroll)
typedef struct {int x,xc,y,yc,max,scrollTop,scrollBottom,conFlags,autoScroll;} Object;
struct P_Console_Up {int Num;};struct P_Console_Down {int Num;};
static int scrolled,command;
#define Console_DoCommand(o,c,n,p) (scrolled=*(p),command=(c))
'''+enum+'\n'+macros+'\n'+functions+r'''
int main(void){unsigned cases=0;
 const char*good[]={"r",";r","0;0r","2;6r",";6r","2;r","999999999999999999999;r"};
 const int expect[][2]={{0,0},{0,0},{0,0},{2,6},{0,6},{2,0},{65535,0}};
 for(int i=0;i<7;i++){UBYTE*p=(void*)good[i],n=99;BYTE c=-1;IPTR v[2]={77,88};assert(getscrollregion(&c,&p,&n,strlen(good[i]),v));assert(c==C_SET_SCROLL_REGION&&n==2&&! *p&&v[0]==expect[i][0]&&v[1]==expect[i][1]);}
 const char*bad[]={"?2;6r","1;2;3r","1:2r","2;6","2;6H"};
 for(int i=0;i<5;i++){UBYTE*p=(void*)bad[i],n=99;BYTE c=-1;IPTR v[2]={77,88};assert(!getscrollregion(&c,&p,&n,strlen(bad[i]),v));assert(p==(void*)bad[i]&&n==99&&c==-1&&v[0]==77&&v[1]==88);}
 for(int h=2;h<32;h++)for(int t=0;t<h+2;t++)for(int b=0;b<h+2;b++){
  Object o={.max=h-1,.scrollTop=0,.scrollBottom=h,.x=4,.xc=4,.y=1,.yc=1,.conFlags=129};Object old=o;IPTR p[]={t,b};int top=t?t:1,bottom=b?b:h;int valid=top<bottom&&bottom<=h;
  assert(stdcon_scroll_region(&o,p)==valid);
  if(!valid)assert(!memcmp(&o,&old,sizeof(o)));else assert(o.scrollTop==top-1&&o.scrollBottom==bottom&&o.x==0&&o.y==0&&o.conFlags==1);
 }
 for(int h=2;h<=24;h++)for(int t=0;t<h-1;t++)for(int b=t+1;b<h;b++)for(int y=0;y<h;y++)for(int direction=0;direction<2;direction++)for(int autoScroll=0;autoScroll<2;autoScroll++){
  Object o={.max=h-1,.scrollTop=t,.scrollBottom=b+1,.y=y,.yc=y,.autoScroll=autoScroll};scrolled=0;command=-1;
  if(direction){struct P_Console_Down m={1};console_down(0,&o,&m);}else{struct P_Console_Up m={1};console_up(0,&o,&m);}
  int limit=direction?(y<=b?b:h-1):(y>=t?t:0);int want=direction?MIN(y+1,limit):MAX(y-1,limit);
  assert(o.y==want&&o.yc==want);int scroll=autoScroll&&(direction?y==b:y==t);
  assert(scrolled==scroll);if(scroll)assert(command==(direction?C_SCROLL_UP:C_SCROLL_DOWN));cases++;
  o.y=o.yc=y;o.conFlags=128;IPTR huge=65535;
  stdcon_relative_position(&o,direction?C_CURSOR_DOWN:C_CURSOR_UP,&huge);
  assert(o.y==limit&&o.yc==limit&&o.conFlags==0);
 }

 printf("Scroll-region PASS: parser/default/invalid state plus %u movement/boundary cases\n",cases);
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'t.c';e=Path(d)/'t';p.write_text(code)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(e)],check=True);subprocess.run([str(e)],check=True)
