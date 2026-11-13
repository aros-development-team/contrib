#!/usr/bin/env python3
"""Real Unicode dispatch and underline painter across glyph providers/cell widths."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'consolengtext.c').read_text();u=(r/'support.c').read_text()
def fn(s,name):
 a=s.index(name);a=s.rfind('\n',0,a)+1;b=s.index('{',a);depth=1;e=b+1
 while depth:depth+=(s[e]=='{')-(s[e]=='}');e+=1
 return s[a:e]
code=r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
typedef int32_t LONG;typedef int16_t WORD;typedef uint32_t ULONG;typedef uint8_t UBYTE;typedef int BOOL;typedef const char *CONST_STRPTR;
#define TRUE 1
#define FALSE 0
#define JAM1 1
#define FSF_UNDERLINED 4
#define CONSOLENG_GLYPH_OK 0
struct Library{int unused;};struct ConsoleBase{int unused;};
struct Font{int tf_XSize,tf_YSize,tf_Baseline,tf_Style;};
struct RastPort{struct Font*Font;int cp_x,cp_y,DrawMode,AlgoStyle;};
static int calls,x0,y0,x1,y1,outline,builtin,missing,latin;
#define SetDrMd(rp,m) ((rp)->DrawMode=(m))
#define Move(rp,x,y) ((rp)->cp_x=(x),(rp)->cp_y=(y))
static void RectFill(struct RastPort*rp,int a,int b,int c,int d){assert(rp->DrawMode==JAM1);calls++;x0=a;y0=b;x1=c;y1=d;}
static void Text(struct RastPort*rp,CONST_STRPTR s,int n){latin++;rp->cp_x+=rp->Font->tf_XSize;}
static BOOL builtin_glyph(struct Library*g,struct RastPort*rp,ULONG cp){if(cp!=0x2801)return FALSE;builtin++;rp->cp_x+=rp->Font->tf_XSize;return TRUE;}
static LONG ConsolengGlyphDraw(struct Library*g,struct ConsoleBase*c,struct RastPort*rp,ULONG cp,LONG x,LONG y,BOOL emoji){outline++;return cp==0x10fffd?1:0;}
static void ConsolengMissingGlyph(struct Library*g,struct RastPort*rp,ULONG cp){missing++;}
'''+fn(u,'WORD con_cp_width(')+'\n'+fn(u,'ULONG con_utf8_encode(')+'\n'+fn(s,'static ULONG ConsoleNGUTF8Next(')+'\n'+fn(s,'static void ConsolengUnderline(')+'\n'+fn(s,'void ConsoleNGTextUTF8(')+r'''
int main(void){int cases=0;
 const ULONG cps[]={0x41,0x3a9,0x4e2d,0x2801,0x1f600,0x10fffd,0x200d};
 for(int w=1;w<=32;w++)for(int h=1;h<=32;h++)for(int b=0;b<h;b++)for(int style=0;style<3;style++)for(unsigned i=0;i<sizeof(cps)/sizeof(cps[0]);i++){
  ULONG cp=cps[i];struct Font f={w,h,b,style==2?FSF_UNDERLINED:0};struct RastPort rp={&f,-19,23,7,style==1?FSF_UNDERLINED:0};
  UBYTE bytes[4];ULONG n=con_utf8_encode(bytes,cp);calls=0;
  ConsoleNGTextUTF8(0,0,&rp,(char*)bytes,n,TRUE);
  int cells=con_cp_width(cp),row=b+1;if(row<h-1)row++;
  int expected=cp>=0x100&&cells>0&&style&&row<h;
  assert(calls==expected&&rp.DrawMode==7&&rp.cp_y==23);
  if(expected){assert(x0==-19&&x1==-19+w*cells-1&&y0==23-b+row&&y1==y0);assert(y0>=23-b&&y1<23-b+h);}
  cases++;
 }
 assert(outline&&builtin&&missing&&latin);
 struct Font f={0,8,6,0};struct RastPort rp={&f,0,0,5,FSF_UNDERLINED};calls=0;
 ConsolengUnderline(0,&rp,0x4e2d,0,0);f.tf_XSize=8;f.tf_YSize=0;ConsolengUnderline(0,&rp,0x4e2d,0,0);rp.Font=0;ConsolengUnderline(0,&rp,0x4e2d,0,0);ConsolengUnderline(0,0,0x4e2d,0,0);assert(!calls&&rp.DrawMode==5);
 printf("Unicode underline PASS: %d dispatch/painter cases, outline/builtin/fallback, one/two/zero cell widths, inherited/style underline, bounds and raster state\n",cases);
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';b=Path(d)/'test';p.write_text(code)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(b)],check=True)
 subprocess.run([str(b)],check=True)
