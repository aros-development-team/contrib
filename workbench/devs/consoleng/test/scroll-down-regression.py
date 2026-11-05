#!/usr/bin/env python3
"""Execute retained line/scroll dispatch: screen origin, bounds, failure and hidden guard."""
from pathlib import Path
import tempfile,subprocess
r=Path(__file__).resolve().parents[1];s=(r/'charmapconclass.c').read_text();a=s.index('    case C_INSERT_LINE:',s.index('static VOID charmapcon_docommand'));b=s.index('    case C_SCROLL_UP:',a);branch=s[a:b]
prefix=r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
typedef intptr_t IPTR;typedef long LONG;typedef int Class;
#define FALSE 0
#define C_INSERT_LINE 1
#define C_DELETE_LINE 2
#define C_SCROLL_DOWN 3
struct charmap_line {int row;unsigned long *underlineColors;};
struct data {int select_x_min,select_x_max,select_y_min,select_y_max,select_line_min,select_line_max,active_selection,ignore_drag;};
typedef struct {int y,ymax,xmax,cu_FgPen,cu_BgPen,hidden,top,bottom;struct data d;} Object;
struct msg {int Command;};
#define CHAR_YMAX(o) ((o)->ymax)
#define CON_SCROLL_TOP(o) ((o)->top)
#define CON_SCROLL_BOTTOM(o) ((o)->bottom ? (o)->bottom-1 : CHAR_YMAX(o))
#define CHAR_XMAX(o) ((o)->xmax)
#define YCP (o->y)
#define CU(o) (o)
#define ConsoleDevice NULL
#define CHARMAP_ROLES(o) 0x102
#define CON_IS_OBSCURED(o) ((o)->hidden)
static struct charmap_line rows[64];static int calls,first,last,inserted,refreshed,fail;static IPTR amount;
static struct charmap_line*charmapcon_find_line(Class*c,Object*o,LONG n){assert(n>=0&&n<=o->ymax);rows[n].row=n;return rows+n;}
static int charmap_edit_lines(void*b,struct charmap_line*f,struct charmap_line*l,IPTR n,int ins,int width,int fg,int bg,int roles){calls++;first=f->row;last=l->row;amount=n;inserted=ins;assert(width==80&&fg==3&&bg==5&&roles==0x102);return !fail;}
static void charmapcon_refresh_lines(Class*c,Object*o,int a,int b){assert(a==0&&b==o->ymax);refreshed++;}
'''
wrapper='static void run(Object*o,int cmd,IPTR n){Class*cl=0;struct data*data=&o->d;struct msg m={cmd},*msg=&m;IPTR *params=&n;switch(cmd){'+branch+'}}\n'
main=r'''
int main(void){unsigned cases=0;IPTR counts[]={-1,0,1,2,63,65535,INTPTR_MAX};
 for(int h=1;h<=48;h++)for(int y=0;y<h;y++)for(int cmd=1;cmd<=3;cmd++)for(int hidden=0;hidden<2;hidden++)for(fail=0;fail<2;fail++)for(unsigned i=0;i<sizeof(counts)/sizeof(*counts);i++){
 Object o={.y=y,.ymax=h-1,.xmax=79,.cu_FgPen=3,.cu_BgPen=5,.hidden=hidden,.d={1,7,2,8,3,9,1,1}};struct data old=o.d;calls=refreshed=0;
 run(&o,cmd,counts[i]);int start=cmd==3?0:y;IPTR n=counts[i]>0?counts[i]:1;if(n>h-start)n=h-start;
 assert(calls==1&&first==start&&last==h-1&&amount==n&&inserted==(cmd!=2));assert(o.y==y);
 assert(refreshed==(!fail&&!hidden));if(fail)assert(!memcmp(&old,&o.d,sizeof(old)));else assert(!o.d.active_selection&&!o.d.ignore_drag&&o.d.select_x_min==o.d.select_x_max);
 cases++;
 }
 for(int top=0;top<9;top++)for(int bottom=top+2;bottom<=10;bottom++)for(int y=0;y<12;y++)for(int cmd=1;cmd<=3;cmd++){
  Object o={.y=y,.ymax=11,.xmax=79,.cu_FgPen=3,.cu_BgPen=5,.top=top,.bottom=bottom};calls=refreshed=fail=0;run(&o,cmd,65535);
  int active=cmd==3||(y>=top&&y<bottom);assert(calls==active&&refreshed==active);
  if(active){int start=cmd==3?top:y;assert(first==start&&last==bottom-1&&amount==bottom-start&&inserted==(cmd!=2));}
 }
 printf("Scroll-down dispatch PASS: %u count/origin/hidden/failure combinations\n",cases);
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'t.c';exe=Path(d)/'t';p.write_text(prefix+wrapper+main)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
 p.write_text(prefix+wrapper.replace('case C_SCROLL_DOWN:','')+main)
 subprocess.run(['cc','-Wall','-Werror',str(p),'-o',str(exe)],check=True)
 assert subprocess.run([str(exe)],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL).returncode!=0
