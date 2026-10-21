/* Compile with cursor-colored-helper.inc extracted from stdconclass.c.
 * Native graphics and palette allocation; independent planar bitmap acceptance. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/cybergraphics.h>
#include <cybergraphx/cybergraphics.h>
#include <graphics/view.h>
#include <stdio.h>
#include <string.h>
#define CON_TXTFLAGS_REVERSED 8
struct stdcondata {
 struct Library *scd_GfxBase,*cursorCyberBase;struct ColorMap *cm;
 LONG cursorLeft,cursorTop,cursorRight,cursorBottom,cursorPen;
 BOOL cursorpainted,cursorColored,cursorIndexed,cursorPatterned;
 ULONG cursorRGB,cursorstyle,cursorSavedWidth,cursorSavedHeight;
 struct BitMap *cursorSaved;
};
typedef struct {struct RastPort rp;ULONG cu_TxFlags,cu_FgPen,cu_BgPen;} TestConsole;
#define Object TestConsole
#define RASTPORT(o) (&(o)->rp)
#define CU(o) (o)
static LONG stdcon_bestpen(struct stdcondata *d,UBYTE r,UBYTE g,UBYTE b){return ObtainBestPenA(d->cm,r*0x01010101UL,g*0x01010101UL,b*0x01010101UL,NULL);}
static ULONG stdcon_penrgb(struct stdcondata *d,ULONG p){ULONG rgb[3];GetRGB32(d->cm,p,1,rgb);return (rgb[0]>>24)<<16|(rgb[1]>>24)<<8|(rgb[2]>>24);}
#include "cursor-colored-helper.inc"
#define CHECK(c) do{if(!(c)){printf("PLANAR_FAIL line=%d check=%s\n",__LINE__,#c);fflush(stdout);goto out;}}while(0)
int main(void){
 int rc=20,cases=0;struct BitMap *bitmap=NULL;struct ColorMap *cm=NULL;struct stdcondata d={0};
 for(int depth=1;depth<=8;depth*=2){
  bitmap=AllocBitMap(32,32,depth,BMF_CLEAR|BMF_STANDARD,NULL);CHECK(bitmap);CHECK(GetBitMapAttr(bitmap,BMA_DEPTH)==depth);
  cm=GetColorMap(1<<depth);CHECK(cm);struct RasInfo ri={0};ri.BitMap=bitmap;struct ViewPort vp={0};vp.RasInfo=&ri;vp.ColorMap=cm;CHECK(!AttachPalExtra(cm,&vp));
  TestConsole o={0};InitRastPort(&o.rp);o.rp.BitMap=bitmap;o.cu_FgPen=1;o.cu_BgPen=0;
  for(int style=0;style<7;style++)for(int pattern=0;pattern<2;pattern++)for(int reverse=0;reverse<2;reverse++){
   SetDrMd(&o.rp,JAM1);SetWriteMask(&o.rp,255);
   for(int y=0;y<32;y++)for(int x=0;x<32;x++){SetAPen(&o.rp,(x+y)&((1<<depth)-1));WritePixel(&o.rp,x,y);}
   d=(struct stdcondata){.scd_GfxBase=(struct Library*)GfxBase,.cm=cm,.cursorLeft=8,.cursorTop=8,.cursorRight=15,.cursorBottom=15,.cursorRGB=0x0112a4e8,.cursorstyle=style,.cursorPatterned=pattern};
   o.cu_TxFlags=reverse?8:0;SetAPen(&o.rp,1);SetDrMd(&o.rp,COMPLEMENT);SetWriteMask(&o.rp,0x55);
   UWORD freeBefore=cm->PalExtra->pe_NFree;CHECK(stdcon_coloredcursor(&o,&d)&&d.cursorIndexed);CHECK(o.rp.FgPen==1&&o.rp.DrawMode==COMPLEMENT&&o.rp.Mask==0x55);
   for(int y=0;y<32;y++)for(int x=0;x<32;x++){
    ULONG expected=(x+y)&((1<<depth)-1);
    if(x>=8&&x<=15&&y>=8&&y<=15&&(!pattern||!((x+y)&1)))expected=style<=2?expected^(reverse?1:0)^d.cursorPen:d.cursorPen;
    CHECK(ReadPixel(&o.rp,x,y)==expected);
   }
   d.cursorpainted=TRUE;CHECK(stdcon_coloredcursor(&o,&d)&&!d.cursorIndexed&&cm->PalExtra->pe_NFree==freeBefore);
   for(int y=0;y<32;y++)for(int x=0;x<32;x++)CHECK(ReadPixel(&o.rp,x,y)==((x+y)&((1<<depth)-1)));
   FreeBitMap(d.cursorSaved);d.cursorSaved=NULL;cases++;
  }
  FreeColorMap(cm);cm=NULL;FreeBitMap(bitmap);bitmap=NULL;
 }
 printf("PLANAR_PASS cases=%d depths=1,2,4,8 exact restoration, pen lifetime, drawing state\n",cases);rc=0;
out:
 if(d.cursorIndexed&&cm)ReleasePen(cm,d.cursorPen);
 if(d.cursorSaved)FreeBitMap(d.cursorSaved);
 if(cm)FreeColorMap(cm);
 if(bitmap)FreeBitMap(bitmap);
 printf("PLANAR_RUN rc=%d\n",rc);return rc;
}
