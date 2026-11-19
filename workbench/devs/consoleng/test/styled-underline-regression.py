#!/usr/bin/env python3
"""Production decoration painter: bounds, split runs, conceal and reverse."""
from pathlib import Path
import subprocess, tempfile
src=(Path(__file__).resolve().parents[1]/'consolengtext.c').read_text()
helper=src[src.index('void ConsoleNGStyledUnderline('):]
code=r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
typedef int32_t LONG;typedef uint8_t UBYTE;
#define CON_TXTFLAGS_UNDERLINED 4
#define CON_TXTFLAGS_CONCEALED 16
#define CON_TXTFLAGS_REVERSED 8
#define JAM1 1
struct Library{int dummy;};struct RastPort{UBYTE FgPen,DrawMode;};
static unsigned char pixels[64][256];static int bx,by,bw,bh;
static void SetAPen(struct RastPort*r,UBYTE p){r->FgPen=p;}
static void SetDrMd(struct RastPort*r,UBYTE m){r->DrawMode=m;}
static void RectFill(struct RastPort*r,int x,int y,int xx,int yy){
 assert(r->DrawMode==1&&x==xx&&y==yy&&x>=bx&&x<bx+bw&&y>=by&&y<by+bh);
 pixels[y+8][x+64]=r->FgPen;
}
'''+helper+r'''
int main(void){unsigned count=0;unsigned char expected[64][256];
for(int style=0;style<5;style++)for(int f=0;f<32;f++)for(int h=1;h<=32;h++)for(int x=-12;x<=12;x++){
 struct RastPort rp={173,7};bx=x;by=3;bw=37;bh=h;
 memset(pixels,0,sizeof pixels);
 ConsoleNGStyledUnderline(0,&rp,f,x,by,bw,h,21,32,style);
 assert(rp.FgPen==173&&rp.DrawMode==7);
 for(int col=0;col<bw;col++){
  int active=(f&4)&&!(f&16)&&style>=1&&style<=3;
  int period=style==1?4:style==2?2:6,p=((x+col)%period+period)%period;
  int row=by+h-1,rise=p==3?1:p;
  if(style==1)row-=rise>=h?h-1:rise;
  if(style==2&&p)active=0;if(style==3&&p>=4)active=0;
  for(int y=by;y<by+h;y++)assert(pixels[y+8][x+col+64]==(active&&y==row?((f&8)?32:21):0));
 }
 memcpy(expected,pixels,sizeof pixels);memset(pixels,0,sizeof pixels);
 for(int col=0;col<bw;col++)ConsoleNGStyledUnderline(0,&rp,f,x+col,by,1,h,21,32,style);
 assert(!memcmp(expected,pixels,sizeof pixels));count++;
}
ConsoleNGStyledUnderline(0,0,4,0,0,1,1,1,2,1);
printf("Styled underline PASS %u geometry/style/flag/split cases\n",count);
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';b=Path(d)/'test';p.write_text(code)
 subprocess.run(['cc','-Wall','-Werror','-Wno-misleading-indentation','-fsanitize=address,undefined',str(p),'-o',str(b)],check=True)
 subprocess.run([str(b)],check=True)
 p.write_text(code.replace('(flags & CON_TXTFLAGS_CONCEALED)','0'))
 subprocess.run(['cc',str(p),'-o',str(b)],check=True)
 assert subprocess.run([str(b)],capture_output=True).returncode!=0
 print('Conceal negative control rejected')
