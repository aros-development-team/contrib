#!/usr/bin/env python3
"""Actual scroll-up dispatch bounds and parameter ownership, including huge counts."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
s=(r/'charmapconclass.c').read_text();a=s.index('    case C_SCROLL_UP:',s.index('static VOID charmapcon_docommand'));b=s.index('    default:',a);branch=s[a:b]
prefix=r'''
#include <assert.h>
#include <stdint.h>
#include <limits.h>
#include <stdio.h>
typedef intptr_t IPTR;typedef long LONG;typedef int Class;typedef void* Msg;
#define C_SCROLL_UP 1
#define FALSE 0
#define CON_SCROLL_TOP(o) 0
#define CON_SCROLL_BOTTOM(o) CHAR_YMAX(o)
#define CON_IS_OBSCURED(o) 0
#define charmapcon_refresh_lines(c,o,a,b) ((void)0)
#define charmap_edit_lines(b,f,l,n,i,w,fg,bg,r) 1
struct P_Console_DoCommand {int MethodID,Command,NumParams;IPTR*Params;};
typedef struct {int ymax;} Object;
struct data {int scrollback_size,select_x_max,select_x_min,select_y_max,select_y_min,select_line_max,select_line_min,active_selection,ignore_drag;};
struct charmap_line {int unused;unsigned long *underlineColors;};
static struct charmap_line row;
static int fail,published;
#define ConsoleDevice 0
#define CHAR_XMAX(o) 79
#define CU(o) (&pens)
#define CHARMAP_ROLES(o) 0x102
static struct {int cu_FgPen,cu_BgPen;} pens={3,5};
static struct charmap_line*charmapcon_find_line(Class*c,Object*o,int y){assert(y==o->ymax);return &row;}
static int charmap_append_blanks(void*b,struct charmap_line*l,IPTR n,int w,int fg,int bg,int roles){assert(l==&row&&n>0&&w==80&&fg==3&&bg==5&&roles==0x102);return !fail;}
#define CHAR_YMAX(o) ((o)->ymax)
static IPTR scrolled,rendered;static int calls;
static void charmap_scroll_up(Class*c,Object*o,IPTR n){scrolled=n;calls++;}
static void DoSuperMethodA(Class*c,Object*o,Msg m){struct P_Console_DoCommand*p=m;assert(p->MethodID==42&&p->Command==1&&p->NumParams==1);rendered=*p->Params;calls++;}
'''
wrapper='static void run(Object*o,IPTR*params){Class*cl=0;struct data d={0},*data=&d;struct P_Console_DoCommand m={42,1,1,params},*msg=&m;switch(msg->Command){'+branch+'default:break;}published=d.scrollback_size;}\n'
main=r'''
int main(void){int cases=0;IPTR counts[]={-1,0,1,2,65535,INTPTR_MAX};
for(fail=0;fail<2;fail++)for(int h=0;h<=100;h++)for(unsigned i=0;i<sizeof(counts)/sizeof(*counts);i++){
Object o={h-1};IPTR n=counts[i],want=n>0?n:1;if(want>h)want=h;calls=0;scrolled=rendered=-1;
run(&o,&n);assert(n==counts[i]);assert(calls==((h&&!fail)?2:0));assert(published==((h&&!fail)?want:0));if(h&&!fail)assert(scrolled==want&&rendered==want);cases++;
}printf("Scroll-up PASS: %d height/count combinations, immutable caller parameters\n",cases);}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'t.c';exe=Path(d)/'t'
 p.write_text(prefix+wrapper+main)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(exe)],check=True);subprocess.run([str(exe)],check=True)
 p.write_text(prefix+wrapper.replace('if (count > available) count = available;','')+main)
 subprocess.run(['cc','-Wall','-Werror',str(p),'-o',str(exe)],check=True)
 assert subprocess.run([str(exe)],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL).returncode!=0
