#!/usr/bin/env python3
"""Render every Braille pattern through the actual builtin glyph dispatcher."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'consolengtext.c').read_text()
start=s.index('static BOOL builtin_glyph(')
end=s.index('{',start)+1;depth=1
while depth:
 depth+=(s[end]=='{')-(s[end]=='}');end+=1
body=s[s.index('#define BX('):end]
prefix=r'''
#include <assert.h>
#include <string.h>
#include <stdint.h>
typedef uint8_t UBYTE; typedef uint16_t UWORD; typedef uint32_t ULONG;
typedef int16_t WORD; typedef int32_t LONG; typedef int BOOL;
#define TRUE 1
#define FALSE 0
#define JAM1 1
struct Library {int dummy;};
struct TextFont {int tf_XSize,tf_YSize,tf_Baseline;};
struct RastPort {struct TextFont *Font;int cp_x,cp_y,DrawMode;};
static unsigned char pixels[64][64];static int width,height;
static void fill(struct RastPort *rp,int x0,int y0,int x1,int y1){
 (void)rp;assert(x0>=3&&y0>=5&&x1<3+width&&y1<5+height);
 for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++)pixels[y][x]=1;
}
#define RectFill(rp,a,b,c,d) fill(rp,a,b,c,d)
#define SetDrMd(rp,m) ((rp)->DrawMode=(m))
#define Move(rp,x,y) ((rp)->cp_x=(x),(rp)->cp_y=(y))
#define Draw(rp,x,y) Move(rp,x,y)
#define SetAfPt(rp,p,n) ((void)(p))
'''
main=r'''
int main(void){
 const int sizes[][2]={{2,4},{5,7},{8,8},{8,16},{9,17},{16,32},{31,47}};
 const int dotrow[]={0,1,2,0,1,2,3,3};
 const int dotcol[]={0,0,0,1,1,1,0,1};
 for(unsigned z=0;z<sizeof(sizes)/sizeof(sizes[0]);z++){
  width=sizes[z][0];height=sizes[z][1];
  struct TextFont f={width,height,height-2};
  for(int mask=0;mask<256;mask++){
   struct RastPort rp={&f,3,5+f.tf_Baseline,7};memset(pixels,0,sizeof(pixels));
   assert(builtin_glyph(0,&rp,0x2800+mask));
   assert(rp.cp_x==3+width&&rp.cp_y==5+f.tf_Baseline&&rp.DrawMode==7);
   for(int bit=0;bit<8;bit++){
    int count=0,r=dotrow[bit],c=dotcol[bit];
    for(int y=5+r*height/4;y<5+(r+1)*height/4;y++)
     for(int x=3+c*width/2;x<3+(c+1)*width/2;x++)count+=pixels[y][x];
    assert((count>0)==!!(mask&(1<<bit)));
   }
  }
  struct RastPort rp={&f,3,5+f.tf_Baseline,7};
  assert(!builtin_glyph(0,&rp,0x27ff)&&!builtin_glyph(0,&rp,0x2900));
  assert(rp.cp_x==3&&rp.DrawMode==7);
 }
 return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';b=Path(d)/'test';p.write_text(prefix+body+main)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(b)],check=True)
 subprocess.run([str(b)],check=True)
print('Braille PASS: all256 patterns at seven cell sizes, dot order, blank, bounds, cursor advance and draw-mode restoration')
