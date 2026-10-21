/* Native Unicode spacing-glyph equivalence to blank console cells. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/cybergraphics.h>
#include <devices/conunit.h>
#include "consoleng.h"
#include <stdio.h>
#include <string.h>
struct Library *CyberGfxBase;
static struct IOStdReq *io;static struct Window *window;static struct ConUnit *unit;
static int put(const char*s){io->io_Command=CMD_WRITE;io->io_Data=(APTR)s;io->io_Length=strlen(s);return !DoIO((struct IORequest*)io)&&io->io_Actual==strlen(s);}
static unsigned reference_differences;
static int equal_cells(int cells,int style){
 int width=unit->cu_XRSize*cells,height=unit->cu_YRSize;
 for(int y=-1;y<=height;y++)for(int x=-1;x<=width;x++){
  ULONG a=ReadRGBPixel(window->RPort,unit->cu_XROrigin+unit->cu_XRSize+x,unit->cu_YROrigin+unit->cu_YRSize+y);
  ULONG b=ReadRGBPixel(window->RPort,unit->cu_XROrigin+unit->cu_XRSize*9+x,unit->cu_YROrigin+unit->cu_YRSize+y);
  /* Native italic Text() paints its expanded background outside the cell.
   * Unicode spacing must instead leave its neighboring cells untouched.
   * Check its raster independently. Outline ASCII also has one-level alpha
   * rounding, so retain reference differences as diagnostics, not an oracle. */
  int inside=x>=0&&x<width&&y>=0&&y<height;
  int underline=unit->cu_YRSize?window->RPort->Font->tf_Baseline+1:0;
  if(underline<height-1)underline++;
  ULONG fg=0xdcdcdc,bg=0x193c50;
  ULONG expected=inside&&(style==1||style==3)?fg:bg;
  if(inside&&style>=2&&y==underline)expected=style==3?bg:fg;
  if(a!=expected){printf("SPACE_EXPECT x=%d y=%d actual=%lx expected=%lx\n",x,y,(unsigned long)a,(unsigned long)expected);return 0;}
  if(inside&&a!=b)reference_differences++;
 }
 return 1;
}
#define CHECK(c) do{if(!(c)){printf("SPACE_FAIL line=%d font=%d glyph=%d style=%d\n",__LINE__,f,g,style);fflush(stdout);Delay(500);goto out;}}while(0)
int main(void){
 int rc=20,opened=0,f=0,g=0,style=0,cases=0;struct MsgPort *port=CreateMsgPort();
 io=port?(void*)CreateIORequest(port,sizeof(*io)):NULL;CHECK(io);
 CyberGfxBase=OpenLibrary((CONST_STRPTR)"cybergraphics.library",0);CHECK(CyberGfxBase);
 window=OpenWindowTags(NULL,WA_Left,30,WA_Top,30,WA_Width,730,WA_Height,470,
  WA_Title,(IPTR)"ConsoleNG Unicode spaces",WA_DragBar,TRUE,WA_DepthGadget,TRUE,
  WA_SizeGadget,TRUE,WA_SizeBRight,TRUE,WA_MinWidth,300,WA_MinHeight,200,
  WA_MaxWidth,-1,WA_MaxHeight,-1,TAG_DONE);CHECK(window);
 io->io_Data=window;io->io_Length=sizeof(*window);
 CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",CONU_CHARMAP,(struct IORequest*)io,0));opened=1;unit=(void*)io->io_Unit;
 const char *fonts[]={"topaz,8","DejaVuSansMono.font,16"};
 const char *styles[]={"","\x1b[7m","\x1b[4m","\x1b[1;3;4;7m"};
 const ULONG spaces[]={0x2000,0x2001,0x2002,0x2003,0x2004,0x2005,0x2006,0x2007,0x2008,0x2009,0x200a,0x202f,0x205f,0x3000};
 for(f=0;f<2;f++)for(style=0;style<4;style++){
  for(g=0;g<14;g++){
   ULONG cp=spaces[g];int cells=cp==0x3000?2:1;char seq[200],utf8[4]={(char)(0xe0|(cp>>12)),(char)(0x80|((cp>>6)&63)),(char)(0x80|(cp&63)),0};
   snprintf(seq,sizeof(seq),"\x1b%%G\x1b[?25l\x1b]50;%s\7\x1b[0;38;2;220;220;220;48;2;25;60;80m\x1b[2J\x1b[2;2H%s",fonts[f],styles[style]);
   CHECK(put(seq));CHECK(put(utf8));CHECK(unit->cu_XCCP==1+cells&&unit->cu_YCCP==1);
   CHECK(put("\x1b[2;10H"));CHECK(put(cells==2?"  ":" "));CHECK(equal_cells(cells,style));
   CHECK(put("\x1b[>9h\x1b[>9l"));CHECK(equal_cells(cells,style));cases++;
  }
  g=13;SizeWindow(window,-40,0);Delay(10);CHECK(equal_cells(2,style));SizeWindow(window,40,0);Delay(10);CHECK(equal_cells(2,style));
 }
 CHECK(put("\x1b[0;38;2;220;220;220;48;2;25;60;80m\x1b]50;DejaVuSansMono.font,16\7\x1b[2J\x1b[HUnicode spaces: no missing-glyph boxes\r\n\r\nEM SPACE:          [\xe2\x80\x83\xe2\x80\x83]\r\nIDEOGRAPHIC SPACE: [\xe3\x80\x80]\r\n\r\n112 native pixel comparisons PASS\r\nNormal, reverse, underline, bold italic underline\r\nImmediate, retained redraw and resize"));
 printf("SPACE_NATIVE_PASS cases=%d ascii_reference_differences=%u\n",cases,reference_differences);fflush(stdout);Delay(500);rc=0;
 out:if(opened)CloseDevice((struct IORequest*)io);if(window)CloseWindow(window);if(io)DeleteIORequest((struct IORequest*)io);if(port)DeleteMsgPort(port);if(CyberGfxBase)CloseLibrary(CyberGfxBase);
 printf("SPACE_SCRIPT rc=%d\n",rc);return rc;
}
