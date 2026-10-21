#!/usr/bin/env python3
"""Actual missing-glyph painter: exact outline, degenerate sizes and cell bounds."""
from pathlib import Path
import subprocess,tempfile
s=(Path(__file__).resolve().parents[1]/'consolengtext.c').read_text()
body=s[s.index('static void ConsolengMissingGlyph('):s.index('void ConsoleNGTextUTF8(')]
code=r'''
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
typedef int32_t LONG;typedef int16_t WORD;typedef uint32_t ULONG;typedef uint8_t UBYTE;
#define JAM1 1
struct Library {int unused;};struct TextFont {int tf_XSize,tf_YSize,tf_Baseline;};
struct RastPort {struct TextFont*Font;int cp_x,cp_y,DrawMode;};
static int cells,w,h,calls;static unsigned char pixels[64][128];
static WORD con_cp_width(ULONG cp){return cells;}
#define SetDrMd(rp,m) ((rp)->DrawMode=(m))
static void RectFill(struct RastPort*rp,int x0,int y0,int x1,int y1){
 assert(rp->DrawMode==JAM1);calls++;
 x0-=rp->cp_x;x1-=rp->cp_x;y0-=rp->cp_y-rp->Font->tf_Baseline;y1-=rp->cp_y-rp->Font->tf_Baseline;
 assert(x0>=0&&y0>=0&&x1>=x0&&y1>=y0&&x1<w&&y1<h);
 for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++)pixels[y][x]=1;
}
'''+body+r'''
int main(void){int cases=0;for(int width=1;width<=64;width++)for(h=1;h<=64;h++)for(cells=1;cells<=2;cells++){
 struct TextFont f={width,h,h-1};struct RastPort rp={&f,-17,23,7};w=width*cells;calls=0;memset(pixels,0,sizeof(pixels));
 ConsolengMissingGlyph(0,&rp,0x10fffd);assert(calls==4&&rp.cp_x==-17&&rp.cp_y==23&&rp.DrawMode==7);
 int insetx=w>2,insety=h>2;
 for(int y=0;y<h;y++)for(int x=0;x<w;x++){
 int inside=x>=insetx&&x<w-insetx&&y>=insety&&y<h-insety;
 int edge=x==insetx||x==w-insetx-1||y==insety||y==h-insety-1;
 assert(pixels[y][x]==(inside&&edge));
 }cases++;
}
struct TextFont f={0,8,6};struct RastPort rp={&f,0,0,5};calls=0;ConsolengMissingGlyph(0,&rp,0);f.tf_XSize=8;f.tf_YSize=0;ConsolengMissingGlyph(0,&rp,0);f.tf_YSize=8;cells=0;ConsolengMissingGlyph(0,&rp,0);rp.Font=0;ConsolengMissingGlyph(0,&rp,0);ConsolengMissingGlyph(0,0,0);assert(!calls&&rp.DrawMode==5);
printf("Missing glyph PASS: %d exact one/two-cell outlines, negative coordinates, state preservation and empty inputs\n",cases);}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'t.c';e=Path(d)/'t';p.write_text(code)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(e)],check=True)
 subprocess.run([str(e)],check=True)
