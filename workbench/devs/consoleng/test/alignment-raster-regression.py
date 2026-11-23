#!/usr/bin/env python3
"""Actual DECALN raster helper fills exact cells and restores drawing state."""
from pathlib import Path
import subprocess,tempfile
s=(Path(__file__).resolve().parents[1]/'stdconclass.c').read_text();a=s.index('static VOID stdcon_alignment(');f=s[a:s.index('static VOID stdcon_reset_clear(',a)]
code=r'''
/* This fixture has no plain wide grid; standard-width-regression and native edits cover it. */
#define con_width_clear(g) ((void)0)
#include <assert.h>
#include <string.h>
#include <stdio.h>
typedef void VOID;typedef unsigned char UBYTE;typedef short WORD;typedef long LONG;
#define MIN(a,b) ((a)<(b)?(a):(b))
#define JAM2 2
#define CON_TXTFLAGS_MASK 255
#define CONUNIT_PEN_DEFFG 20
#define CONUNIT_PEN_DEFBG 21
struct Library{int unused;};struct font{int tf_Baseline;};struct RastPort{UBYTE FgPen,BgPen,DrawMode,AlgoStyle;WORD cp_x,cp_y;struct font*Font;};struct stdcondata{struct Library*scd_GfxBase;};typedef struct{struct RastPort rp;int xmax,ymax;}Object;
#define RASTPORT(o) (&(o)->rp)
#define CHAR_XMAX(o) ((o)->xmax)
#define CHAR_YMAX(o) ((o)->ymax)
#define GFX_X(o,x) (10+(x)*8)
#define GFX_Y(o,y) (20+(y)*16)
#define Console_GetColorPen(o,c,p) (*(p)=(c))
#define SetABPenDrMd(p,f,b,m) ((p)->FgPen=(f),(p)->BgPen=(b),(p)->DrawMode=(m),(void)GfxBase)
#define SetSoftStyle(p,s,m) ((p)->AlgoStyle=(s))
#define Move(p,x,y) ((p)->cp_x=(x),(p)->cp_y=(y))
static int width,height,cells;
static void stdcon_text(struct stdcondata*d,struct RastPort*p,const unsigned char*s,LONG n){assert(p->FgPen==20&&p->BgPen==21&&p->DrawMode==2&&!p->AlgoStyle);assert(p->cp_y==20+(cells/width)*16+11&&p->cp_x==10+(cells%width)*8&&n>0&&n<=64&&cells%width+n<=width);while(n--){assert(*s++=='E');cells++;}assert(cells<=width*height);}
'''+f+r'''
int main(void){int widths[]={1,2,63,64,65,640};for(int i=0;i<6;i++)for(height=1;height<20;height++){
 width=widths[i];struct font font={11};Object o={.rp={3,4,5,7,101,103,&font},.xmax=width-1,.ymax=height-1};Object old=o;struct stdcondata d={0};cells=0;stdcon_alignment(&d,&o);assert(cells==width*height&&!memcmp(&o,&old,sizeof(o)));
}puts("DECALN raster PASS: 114 geometries, chunk boundaries, default rendition and complete raster-state restoration");}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'t.c';b=Path(d)/'t';p.write_text(code)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(b)],check=True);subprocess.run([str(b)],check=True)
