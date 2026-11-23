#!/usr/bin/env python3
"""Actual explicit-color painter: conceal, independent reverse and raster state."""
from pathlib import Path
import subprocess,tempfile
s=(Path(__file__).resolve().parents[1]/'stdconclass.c').read_text();f=s[s.index('void ConsoleNGColorUnderline('):]
code=r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
typedef uint8_t UBYTE;typedef uint32_t ULONG;typedef int32_t LONG;typedef int Object;
#define CON_TXTFLAGS_UNDERLINED 1
#define CON_TXTFLAGS_REVERSED 8
#define CON_TXTFLAGS_CONCEALED 16
#define CON_TXTFLAGS_DOUBLEUNDERLINED 32
#define JAM1 1
struct Library{int unused;};struct Font{int tf_Baseline;};struct RastPort{struct Font*Font;UBYTE FgPen,DrawMode;};
static int gets,fills,strikes,styles,drawflags,drawstyle,row;static ULONG key;
static void Console_GetColorPen(Object*o,ULONG c,UBYTE*p){key=c;gets++;*p=40;}
static UBYTE ConsoleNGFaintPen(Object*o,struct Library*g,UBYTE f,UBYTE p,ULONG c){assert(c==key&&p==40);return 41;}
static void SetDrMd(struct RastPort*r,UBYTE m){r->DrawMode=m;}
static void SetAPen(struct RastPort*r,UBYTE p){r->FgPen=p;}
static void RectFill(struct RastPort*r,int x,int y,int xx,int yy){assert(x==3&&xx==18&&y==yy&&r->FgPen==41&&r->DrawMode==1);row=y;fills++;}
static void ConsoleNGStrike(struct Library*g,struct RastPort*r,UBYTE f,int x,int y,int w,int h,int fg,int bg){assert(x==3&&y==5&&w==16&&fg==41&&bg==41);drawflags=f;strikes++;}
static void ConsoleNGStyledUnderline(struct Library*g,struct RastPort*r,UBYTE f,int x,int y,int w,int h,int fg,int bg,int style){assert(x==3&&y==5&&w==16&&fg==41&&bg==41);drawflags=f;drawstyle=style;styles++;}
'''+f+r'''
int main(void){int cases=0;
for(int flags=0;flags<256;flags++)for(int style=0;style<4;style++)for(int h=1;h<=32;h++)for(int baseline=-1;baseline<=34;baseline++)for(int color=0;color<2;color++){
 struct Font font={baseline};struct RastPort rp={&font,173,7};gets=fills=strikes=styles=0;
 ConsoleNGColorUnderline(0,0,&rp,flags,3,5,16,h,style,color?0x1012346:0);
 int active=color&&!(flags&16)&&(flags&33);assert(gets==!!active);
 assert(rp.FgPen==173&&rp.DrawMode==7);
 if(active){assert(key==0x1012345);if(flags&32){assert(strikes==1&&drawflags==32&&!fills&&!styles);}else if(style){assert(styles==1&&drawstyle==style&&drawflags==(flags&~8)&&!fills&&!strikes);}else{int want=baseline+2;if(want>=h)want=h-1;if(want<0)want=0;assert(fills==1&&row==5+want&&!styles&&!strikes);}}
 else assert(!fills&&!strikes&&!styles);
 cases++;
}
printf("Underline color painter PASS %d state/style/font cases\n",cases);
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'t.c';b=Path(d)/'t';p.write_text(code)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(b)],check=True)
 subprocess.run([str(b)],check=True)
 p.write_text(code.replace('flags &= ~CON_TXTFLAGS_REVERSED;', 'flags |= CON_TXTFLAGS_REVERSED;'))
 subprocess.run(['cc',str(p),'-o',str(b)],check=True)
 assert subprocess.run([str(b)],capture_output=True).returncode!=0
 print('Reversed explicit-color negative control rejected')
