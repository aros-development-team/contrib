#!/usr/bin/env python3
"""Actual builtin dispatcher keeps intentional Unicode spaces ink-free."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'consolengtext.c').read_text()
a=s.index('static BOOL builtin_glyph(');end=s.index('{',a)+1;depth=1
while depth:depth+=(s[end]=='{')-(s[end]=='}');end+=1
body=s[s.index('#define BX('):end]
prefix=r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
typedef uint8_t UBYTE;typedef uint16_t UWORD;typedef uint32_t ULONG;typedef int16_t WORD;typedef int32_t LONG;typedef int BOOL;
#define TRUE 1
#define FALSE 0
#define JAM1 1
struct Library {int dummy;};struct TextFont {int tf_XSize,tf_YSize,tf_Baseline;};
struct RastPort {struct TextFont*Font;int cp_x,cp_y,DrawMode;};
static unsigned paints;
#define RectFill(rp,a,b,c,d) ((void)(rp),(void)(a),(void)(b),(void)(c),(void)(d),paints++)
#define SetDrMd(rp,m) ((rp)->DrawMode=(m))
#define Move(rp,x,y) ((rp)->cp_x=(x),(rp)->cp_y=(y))
#define Draw(rp,x,y) Move(rp,x,y)
#define SetAfPt(rp,p,n) ((void)(p))
'''
main=r'''
int main(void){unsigned cases=0;const ULONG spaces[]={0x2000,0x2001,0x2002,0x2003,0x2004,0x2005,0x2006,0x2007,0x2008,0x2009,0x200a,0x202f,0x205f,0x3000};
 for(int width=1;width<=32;width++)for(int height=1;height<=32;height++)for(unsigned i=0;i<14;i++){
  struct TextFont f={width,height,height-1};struct RastPort rp={&f,-17,33,7};paints=0;
  assert(builtin_glyph(0,&rp,spaces[i]));assert(!paints&&rp.cp_x==-17+width&&rp.cp_y==33&&rp.DrawMode==7);cases++;
 }
 const ULONG other[]={0x1680,0x1fff,0x200b,0x202e,0x2030,0x205e,0x2060,0x2fff,0x3001};
 for(unsigned i=0;i<sizeof(other)/sizeof(*other);i++){struct TextFont f={8,16,12};struct RastPort rp={&f,5,6,3};paints=0;assert(!builtin_glyph(0,&rp,other[i]));assert(!paints&&rp.cp_x==5&&rp.cp_y==6&&rp.DrawMode==3);}
 printf("Unicode spaces PASS: %u ink-free draws, pen/state preservation, narrow/wide sizing and excluded neighbors\n",cases);
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';b=Path(d)/'test';p.write_text(prefix+body+main)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(b)],check=True);subprocess.run([str(b)],check=True)
 for old,new,label in [('cp <= 0x200a','cp < 0x2000','space fallback box'),('cp <= 0x200a','cp <= 0x200b','zero-width neighbor suppression')]:
  broken=body.replace(old,new);assert broken!=body;p.write_text(prefix+broken+main)
  subprocess.run(['cc','-Wall','-Werror',str(p),'-o',str(b)],check=True)
  assert subprocess.run([str(b)],capture_output=True).returncode!=0,label
  print('Negative control PASS:',label)
