/* Neutral missing-glyph display through the real ConsoleNG device. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/cybergraphics.h>
#include <devices/conunit.h>
#include "consoleng.h"
#include <stdio.h>
#include <string.h>
struct Library *CyberGfxBase;static struct IOStdReq*io;static struct Window*w;static struct ConUnit*u;
static int put(const char*s){io->io_Command=CMD_WRITE;io->io_Data=(APTR)s;io->io_Length=strlen(s);return !DoIO((struct IORequest*)io)&&io->io_Actual==strlen(s);}
static int emoji(ULONG enabled){struct ConsolengEmojiPrefs p={sizeof(p),1,enabled};io->io_Command=CD_SETEMOJIPREFS;io->io_Data=&p;io->io_Length=sizeof(p);return !DoIO((struct IORequest*)io)&&io->io_Actual==sizeof(p);}
static int box(int cells,int reverse){int width=u->cu_XRSize*cells,height=u->cu_YRSize;
for(int y=-1;y<=height;y++)for(int x=-1;x<=width;x++){
ULONG expected=0x193c50;int inside=x>=0&&x<width&&y>=0&&y<height;
if(inside){int edge=(x==1||x==width-2||y==1||y==height-2)&&x>=1&&x<=width-2&&y>=1&&y<=height-2;expected=(edge^reverse)?0xdcdcdc:0x193c50;}
ULONG actual=ReadRGBPixel(w->RPort,u->cu_XROrigin+u->cu_XRSize+x,u->cu_YROrigin+u->cu_YRSize+y);
if(actual!=expected){printf("MISSING_PIXEL x=%d y=%d got=%lx expected=%lx\n",x,y,(unsigned long)actual,(unsigned long)expected);return 0;}}
return 1;}
#define CHECK(c) do{if(!(c)){printf("MISSING_FAIL line=%d font=%d glyph=%d reverse=%d\n",__LINE__,f,g,rev);fflush(stdout);Delay(500);goto out;}}while(0)
int main(void){int rc=20,opened=0,f=0,g=0,rev=0,cases=0;struct MsgPort*p=CreateMsgPort();io=p?(void*)CreateIORequest(p,sizeof(*io)):NULL;CHECK(io);CyberGfxBase=OpenLibrary((CONST_STRPTR)"cybergraphics.library",0);CHECK(CyberGfxBase);
w=OpenWindowTags(NULL,WA_Left,30,WA_Top,30,WA_Width,730,WA_Height,470,WA_Title,(IPTR)"ConsoleNG missing glyphs stay inside their cells",WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_SizeGadget,TRUE,WA_SizeBRight,TRUE,WA_MinWidth,300,WA_MinHeight,200,WA_MaxWidth,-1,WA_MaxHeight,-1,TAG_DONE);CHECK(w);
io->io_Data=w;io->io_Length=sizeof(*w);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",CONU_CHARMAP,(struct IORequest*)io,0));opened=1;u=(void*)io->io_Unit;CHECK(emoji(0));
const char*fonts[]={"topaz,8","DejaVuSansMono.font,16","DejaVuSansMono.font,24","DejaVuSansMono.font,32"};const char*cps[]={"\xf4\x8f\xbf\xbd","\xf0\x9f\x98\x80"};
for(f=0;f<4;f++)for(g=0;g<2;g++)for(rev=0;rev<2;rev++){
char seq[160];snprintf(seq,sizeof(seq),"\x1b%%G\x1b[?25l\x1b]50;%s\x07\x1b[0;38;2;220;220;220;48;2;25;60;80m\x1b[2J\x1b[2;2H",fonts[f]);CHECK(put(seq));CHECK(u->cu_YRSize==(f?8+f*8:8));if(rev)CHECK(put("\x1b[7m"));CHECK(put(cps[g]));CHECK(u->cu_XCCP==g+2&&u->cu_YCCP==1);CHECK(box(g+1,rev));
CHECK(put("\x1b[>9h\x1b[>9l"));CHECK(box(g+1,rev));SizeWindow(w,-40,0);Delay(10);CHECK(box(g+1,rev));SizeWindow(w,40,0);Delay(10);CHECK(box(g+1,rev));
printf("MISSING_CASE font=%d glyph=%d reverse=%d PASS\n",f,g,rev);fflush(stdout);cases++;
}
/* Also qualify real outline, Braille and box-drawing paths, not just missing maps. */
const char*real[]={"\xcf\x80","\xe2\xa3\xbf","\xe2\x94\x80"};ULONG normal[4096];
for(f=1;f<4;f++)for(g=0;g<3;g++){
 char seq[160];snprintf(seq,sizeof(seq),"\x1b]50;%s\x07\x1b[0;38;2;220;220;220;48;2;25;60;80m\x1b[2J\x1b[2;2H",fonts[f]);CHECK(put(seq));CHECK(put(real[g]));
 int ink=0;for(int y=0;y<u->cu_YRSize;y++)for(int x=0;x<u->cu_XRSize;x++){ULONG rgb=ReadRGBPixel(w->RPort,u->cu_XROrigin+u->cu_XRSize+x,u->cu_YROrigin+u->cu_YRSize+y);CHECK(rgb==0xdcdcdc||rgb==0x193c50);normal[y*u->cu_XRSize+x]=rgb;if(rgb==0xdcdcdc)ink++;}CHECK(ink>0);
 CHECK(put("\x1b[2J\x1b[2;2H\x1b[7m"));CHECK(put(real[g]));
 for(int phase=0;phase<4;phase++){
  if(phase==1)CHECK(put("\x1b[>9h\x1b[>9l"));
  if(phase==2){SizeWindow(w,-40,0);Delay(10);}if(phase==3){SizeWindow(w,40,0);Delay(10);}
  for(int y=0;y<u->cu_YRSize;y++)for(int x=0;x<u->cu_XRSize;x++){ULONG expected=normal[y*u->cu_XRSize+x]==0xdcdcdc?0x193c50:0xdcdcdc;CHECK(ReadRGBPixel(w->RPort,u->cu_XROrigin+u->cu_XRSize+x,u->cu_YROrigin+u->cu_YRSize+y)==expected);}
 }
 printf("MISSING_UNICODE_REVERSE font=%d glyph=%d PASS\n",f,g);fflush(stdout);cases++;
}
CHECK(put("\x1b[0;38;2;220;220;220;48;2;25;60;80m\x1b]50;DejaVuSansMono.font,24\x07\x1b[2J\x1b[HMissing glyphs: clear placeholders\r\n\r\nUnsupported character: "));
CHECK(put(cps[0]));CHECK(put("\r\nEmoji fallback off:    "));CHECK(put(cps[1]));CHECK(put("\r\n\r\n25 color / cell / resize checks PASS\r\nNo diagnostic letters in your text."));
printf("MISSING_PASS cases=%d\n",cases);fflush(stdout);Delay(600);rc=0;
out:if(opened)CloseDevice((struct IORequest*)io);if(w)CloseWindow(w);if(io)DeleteIORequest((struct IORequest*)io);if(p)DeleteMsgPort(p);if(CyberGfxBase)CloseLibrary(CyberGfxBase);printf("MISSING_SCRIPT rc=%d\n",rc);return rc;}
