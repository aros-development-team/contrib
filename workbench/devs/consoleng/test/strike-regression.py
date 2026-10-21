#!/usr/bin/env python3
"""Actual strike painter: bounds, conceal/reverse and raster-state isolation."""
from pathlib import Path
import subprocess, tempfile
r=Path(__file__).resolve().parents[1]
s=(r/'consolengtext.c').read_text()
a=s.index('void ConsoleNGStrike(');b=s.index('\nstatic void ConsolengUnderline',a)
helper=s[a:b]
parser=(r/"consoleclass.c").read_text()
a=parser.index("                case 9:\n");b=parser.index("                case 22:\n",a)
sgr=parser[a:b]
code=r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
typedef int32_t LONG;typedef uint8_t UBYTE;
#define CON_TXTFLAGS_STRUCK 0x40
#define CON_TXTFLAGS_DOUBLEUNDERLINED 0x20
#define CON_TXTFLAGS_CONCEALED 0x10
#define CON_TXTFLAGS_REVERSED 8
#define JAM1 1
struct Library {int unused;};
struct RastPort {UBYTE FgPen,DrawMode;int cp_x,cp_y;};
static int calls,x0,y0,x1,y1,ink,rows[3];
static void SetAPen(struct RastPort*r,UBYTE p){r->FgPen=p;}
static void SetDrMd(struct RastPort*r,UBYTE m){r->DrawMode=m;}
static void RectFill(struct RastPort*r,int x,int y,int xx,int yy){assert(r->DrawMode==JAM1);assert(calls<3);rows[calls++]=y;x0=x;y0=y;x1=xx;y1=yy;ink=r->FgPen;}
'''+helper+'''\nstruct Unit {UBYTE cu_TxFlags;};\n#define CU(o) (o)\nstatic void sgr(struct Unit*o,int param){switch(param){'''+sgr+'''default:break;}}\n'''+r'''
int main(void){int cases=0;
for(int f=0;f<256;f++){struct Unit u={f};sgr(&u,9);assert(u.cu_TxFlags==(f|64));sgr(&u,29);assert(u.cu_TxFlags==(f&~64));sgr(&u,9);sgr(&u,9);assert(u.cu_TxFlags==(f|64));}
for(int f=0;f<256;f++)for(int w=-1;w<=128;w++)for(int h=-1;h<=48;h++){
 struct RastPort rp={173,7,12,31};calls=0;
 ConsoleNGStrike(0,&rp,f,-17,9,w,h,21,32);
 int active=!(f&16)&&w>0&&h>0;
 int expected=active?!!(f&64)+((f&32)?(h==1?1:2):0):0;
 assert(calls==expected);
 assert(rp.FgPen==173&&rp.DrawMode==7&&rp.cp_x==12&&rp.cp_y==31);
 if(expected){assert(x0==-17&&x1==w-18&&y1==y0);int n=0;if(f&64)assert(rows[n++]==9+(h-1)/2);if(f&32){assert(rows[n++]==9+(h>2?h-3:0));if(h>1)assert(rows[n++]==9+h-1);}assert(y0>=9&&y1<9+h);assert(ink==((f&8)?32:21));}
 cases++;
}
ConsoleNGStrike(0,0,64,0,0,8,8,1,2);
printf("STRIKE PASS %d flag/geometry/state cases\n",cases);
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';binary=Path(d)/'test';p.write_text(code)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(binary)],check=True)
 subprocess.run([str(binary)],check=True)
 p.write_text(code.replace('(flags & CON_TXTFLAGS_CONCEALED)', '0'))
 subprocess.run(['cc',str(p),'-o',str(binary)],check=True)
 result=subprocess.run([str(binary)],capture_output=True)
 assert result.returncode!=0,'conceal negative control escaped'
 print('STRIKE conceal negative control rejected')
