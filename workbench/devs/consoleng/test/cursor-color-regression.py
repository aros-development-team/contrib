#!/usr/bin/env python3
"""Actual colored cursor helper: exact restoration and safe fallback paths."""
from pathlib import Path
import subprocess
import tempfile
s = (Path(__file__).resolve().parents[1]/'stdconclass.c').read_text()
a = s.index('static BOOL stdcon_coloredcursor(')
b = s.index('static VOID stdcon_drawcursor(', a)
code = r'''
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
typedef unsigned char UBYTE;typedef uint32_t ULONG;typedef int32_t LONG;typedef int BOOL;
#define JAM1 1
#define TRUE 1
#define FALSE 0
#define BMA_DEPTH 1
#define BMF_CLEAR 1
#define CYBRMATTR_ISCYBERGFX 1
#define CON_TXTFLAGS_REVERSED 8
struct Library{int unused;};
struct BitMap{ULONG pixels[64*64];int depth,cyber;};
struct RastPort{struct BitMap*BitMap;ULONG untouched;UBYTE FgPen,DrawMode,Mask;};
struct stdcondata{struct Library *scd_GfxBase,*cursorCyberBase;LONG cursorLeft,cursorTop,cursorRight,cursorBottom;
 void *cm;LONG cursorPen;BOOL cursorIndexed;BOOL cursorpainted,cursorColored,cursorPatterned;ULONG cursorRGB,cursorstyle,cursorSavedWidth,cursorSavedHeight;struct BitMap*cursorSaved;};
typedef struct{struct RastPort rp;ULONG cu_TxFlags,cu_FgPen,cu_BgPen;} Object;
#define RASTPORT(o) (&(o)->rp)
#define CU(o) (o)
#define GetCyberMapAttr(b,a) ((b)->cyber)
#define GetBitMapAttr(b,a) ((b)->depth)
static int fail,penrefs,penfail;
static LONG stdcon_bestpen(struct stdcondata*d,UBYTE r,UBYTE g,UBYTE b){if(penfail)return -1;penrefs++;return 3;}
#define ReleasePen(cm,p) do{assert(p==3&&penrefs>0);penrefs--;}while(0)
#define SetWriteMask(rp,m) ((rp)->Mask=(m))
#define SetAPen(rp,p) ((rp)->FgPen=(p))
#define SetDrMd(rp,m) ((rp)->DrawMode=(m))
#define ReadPixel(rp,x,y) ReadRGBPixel(rp,x,y)
#define WritePixel(rp,x,y) WriteRGBPixel(rp,x,y,(rp)->FgPen)
static struct BitMap*AllocBitMap(ULONG w,ULONG h,int depth,int flags,struct BitMap*friend){
 if(fail){return NULL;}assert(w<=64&&h<=64);struct BitMap*b=calloc(1,sizeof(*b));b->depth=depth;b->cyber=1;return b;
}
#define FreeBitMap(b) free(b)
#define InitRastPort(rp) do{memset(rp,0,sizeof(*(rp)));(rp)->Mask=255;}while(0)
static void ClipBlit(struct RastPort*s,int sx,int sy,struct RastPort*d,int dx,int dy,int w,int h,int minterm){
 assert(minterm==0xc0);for(int y=0;y<h;y++)for(int x=0;x<w;x++){ULONG mask=d->BitMap->depth<=8?d->Mask:0xffffffffU;ULONG*pixel=&d->BitMap->pixels[(dy+y)*64+dx+x];*pixel=(*pixel&~mask)|(s->BitMap->pixels[(sy+y)*64+sx+x]&mask);}
}
static ULONG stdcon_penrgb(struct stdcondata*d,ULONG p){return p;}
static ULONG ReadRGBPixel(struct RastPort*p,int x,int y){assert(x>=0&&x<64&&y>=0&&y<64);return p->BitMap->pixels[y*64+x];}
static void WriteRGBPixel(struct RastPort*p,int x,int y,ULONG color){assert(x>=0&&x<64&&y>=0&&y<64);p->BitMap->pixels[y*64+x]=color;}
'''+s[a:b]+r'''
int main(void){
 struct Library lib={0};struct BitMap original={.depth=32,.cyber=1},bitmap;
 for(unsigned i=0;i<4096;i++)original.pixels[i]=0xa0000000|(i*137);
 unsigned cases=0;
 for(int depthIndex=0;depthIndex<3;depthIndex++)for(int style=0;style<7;style++)for(int pattern=0;pattern<2;pattern++)for(int reverse=0;reverse<2;reverse++)for(int size=1;size<=32;size++){
  original.depth=(int[]){15,16,32}[depthIndex];bitmap=original;Object o={{&bitmap,1234},reverse?8:0,0x999999,0x123456};
  struct stdcondata d={.cursorCyberBase=&lib,.cursorLeft=8,.cursorTop=10,.cursorRight=8+size-1,.cursorBottom=10+size-1,.cursorRGB=0x01fa4379,.cursorstyle=style,.cursorPatterned=pattern};
  assert(stdcon_coloredcursor(&o,&d)&&d.cursorColored&&o.rp.untouched==1234);
  for(int y=0;y<64;y++)for(int x=0;x<64;x++){
   ULONG expected=original.pixels[y*64+x];
   if(x>=8&&x<8+size&&y>=10&&y<10+size&&(!pattern||!((x+y)&1)))
    expected=style<=2?expected^(reverse?0x999999:0x123456)^0xfa4379:(expected&0xff000000)|0xfa4379;
   assert(bitmap.pixels[y*64+x]==expected);
  }
  d.cursorpainted=TRUE;d.cursorRGB=0;d.cursorstyle=6;
  assert(stdcon_coloredcursor(&o,&d)&&!d.cursorColored&&!memcmp(&bitmap,&original,sizeof bitmap));
  assert(o.rp.untouched==1234);free(d.cursorSaved);cases++;
 }
 bitmap=original;Object o={{&bitmap,1234},0,0,0};struct stdcondata d={.cursorCyberBase=&lib,.cursorRight=7,.cursorBottom=7,.cursorRGB=0x010000ff};
 fail=1;assert(!stdcon_coloredcursor(&o,&d)&&!memcmp(&bitmap,&original,sizeof bitmap)&&!d.cursorSaved);
 fail=0;bitmap.depth=8;assert(!stdcon_coloredcursor(&o,&d));bitmap.depth=32;bitmap.cyber=0;assert(!stdcon_coloredcursor(&o,&d));bitmap.cyber=1;d.cursorCyberBase=NULL;assert(!stdcon_coloredcursor(&o,&d));
 for(int depth=2;depth<=8;depth++)for(int style=0;style<7;style++)for(int pattern=0;pattern<2;pattern++)for(int reverse=0;reverse<2;reverse++){
  original.depth=depth;original.cyber=0;for(int i=0;i<4096;i++)original.pixels[i]=i&((1<<depth)-1);
  bitmap=original;o=(Object){{&bitmap,1234,5,7,0x55},reverse?8:0,1,2};
  d=(struct stdcondata){.cm=&lib,.cursorLeft=8,.cursorTop=10,.cursorRight=15,.cursorBottom=17,.cursorRGB=0x0100ff00,.cursorstyle=style,.cursorPatterned=pattern};
  assert(stdcon_coloredcursor(&o,&d)&&d.cursorIndexed&&penrefs==1);
  assert(o.rp.FgPen==5&&o.rp.DrawMode==7&&o.rp.Mask==0x55);
  for(int y=0;y<64;y++)for(int x=0;x<64;x++){
   ULONG expected=original.pixels[y*64+x];
   if(x>=8&&x<=15&&y>=10&&y<=17&&(!pattern||!((x+y)&1)))expected=style<=2?expected^(reverse?1:2)^3:3;
   assert(bitmap.pixels[y*64+x]==expected);
  }
  d.cursorpainted=TRUE;assert(stdcon_coloredcursor(&o,&d)&&!penrefs&&!d.cursorIndexed&&!memcmp(&bitmap,&original,sizeof bitmap));
  d.cursorpainted=FALSE;penfail=1;assert(!stdcon_coloredcursor(&o,&d)&&!penrefs&&!memcmp(&bitmap,&original,sizeof bitmap));penfail=0;
  free(d.cursorSaved);cases++;
 }
 bitmap=original;bitmap.depth=1;o=(Object){{&bitmap,1234,5,7,0x55},0,1,0};
 d=(struct stdcondata){.cm=&lib,.cursorRight=7,.cursorBottom=7,.cursorRGB=0x0100ff00};
 assert(!stdcon_coloredcursor(&o,&d)&&!penrefs&&!d.cursorIndexed);free(d.cursorSaved);
 printf("Colored cursor PASS %u shapes/patterns/reversed-background/sizes; exact saved-pixel restore and fallback\n",cases);
}
'''
with tempfile.TemporaryDirectory() as directory:
    p=Path(directory)
    (p/'test.c').write_text(code)
    subprocess.run(['cc','-Wall','-Werror','-Wno-unused-variable','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
    negative=code.replace("SetWriteMask(rp, 0xff);\n        ClipBlit(&saved", "ClipBlit(&saved", 1)
    assert negative != code
    (p/'negative.c').write_text(negative)
    subprocess.run(['cc','-Wall','-Werror','-Wno-unused-variable','-fsanitize=address,undefined',str(p/'negative.c'),'-o',str(p/'negative')],check=True)
    result=subprocess.run([str(p/'negative')],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    assert result.returncode != 0, 'restricted-mask restore regression escaped'
    print('Negative control PASS: restoring through caller write mask rejected')
