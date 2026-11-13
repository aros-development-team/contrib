#!/usr/bin/env python3
"""Execute standard console region raster dispatch and pen restoration."""
from pathlib import Path
import subprocess,tempfile
s=(Path(__file__).resolve().parents[1]/'stdconclass.c').read_text();a=s.index('    case C_INSERT_LINE:',s.index('static VOID stdcon_docommand('));b=s.index('    case C_SET_TOP_OFFSET:',a);body=s[a:b]
code=r'''
#include <stdint.h>
/* This fixture has no plain wide grid; standard-width-regression and native edits cover it. */
#define con_width_scroll(g,t,b,n) ((void)0)
#include <assert.h>
#include <stdio.h>
typedef intptr_t IPTR;typedef long LONG;typedef unsigned char UBYTE;
#define C_INSERT_LINE 1
#define C_DELETE_LINE 2
#define C_SCROLL_DOWN 3
#define C_SCROLL_UP 4
#define JAM1 0
struct RP {UBYTE FgPen,BgPen,DrawMode;};
typedef struct {int top,bottom,y,cu_BgPen;} Object;
struct Message {int Command;};
#define CON_SCROLL_TOP(o) (o->top)
#define CON_SCROLL_BOTTOM(o) (o->bottom)
#define YCP (o->y)
#define CU(o) (o)
#define GFX_XMIN(o) 10
#define GFX_XMAX(o) 99
#define GFX_Y(o,y) (20+(y)*8)
#define YRSIZE 8
#define Console_UnRenderCursor(o) ((void)0)
#define Console_RenderCursor(o) ((void)0)
#define SetAPen(rp,p) ((rp)->FgPen=(p))
#define SetBPen(rp,p) ((rp)->BgPen=(p))
#define SetDrMd(rp,p) ((rp)->DrawMode=(p))
static int calls,first,last,delta;
static void draw(struct RP*rp,int d,int x1,int y1,int x2,int y2){assert(rp->FgPen==5&&rp->BgPen==5&&rp->DrawMode==JAM1);assert(x1==10&&x2==99);calls++;first=y1;last=y2;delta=d;}
#define RectFill(rp,x1,y1,x2,y2) draw(rp,0,x1,y1,x2,y2)
#define ScrollRaster(rp,dx,dy,x1,y1,x2,y2) draw(rp,dy,x1,y1,x2,y2)
static void run(Object*o,int command,IPTR count){struct RP raster={7,9,3},*rp=&raster;IPTR*params=&count;struct Message message={command},*msg=&message;switch(command){
'''+body+r'''
}assert(rp->FgPen==7&&rp->BgPen==9&&rp->DrawMode==3);}
int main(void){unsigned cases=0;IPTR counts[]={0,1,2,65535,INTPTR_MAX};
for(int top=0;top<10;top++)for(int bottom=top+1;bottom<12;bottom++)for(int y=0;y<14;y++)for(int cmd=1;cmd<=4;cmd++)for(int n=0;n<5;n++){
 Object o={top,bottom,y,5};calls=0;run(&o,cmd,counts[n]);int active=cmd>=3||(y>=top&&y<=bottom);assert(calls==active);
 if(active){int start=cmd>=3?top:y;IPTR amount=counts[n]?counts[n]:1;if(amount>bottom-start+1)amount=bottom-start+1;assert(first==20+start*8&&last==20+(bottom+1)*8-1);assert(delta==(amount==bottom-start+1?0:(cmd==1||cmd==3?-1:1)*amount*8));}cases++;
}printf("Scroll raster PASS: %u range/count/command cases, exact fill pens and restoration\n",cases);}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'t.c';e=Path(d)/'t';p.write_text(code)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(e)],check=True);subprocess.run([str(e)],check=True)
 p.write_text(code.replace('SetBPen(rp, CU(o)->cu_BgPen);',''))
 subprocess.run(['cc','-Wall','-Werror',str(p),'-o',str(e)],check=True)
 assert subprocess.run([str(e)],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL).returncode!=0
