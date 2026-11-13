/* Native-byte and UTF-8 Latin-1 must have identical immediate/retained pixels. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/cybergraphics.h>
#include <devices/conunit.h>
#include <stdio.h>
#include <string.h>
struct Library *CyberGfxBase;
static struct IOStdReq*io;static struct Window*w;static struct ConUnit*u;
static int put(const void*s,ULONG n){io->io_Command=CMD_WRITE;io->io_Data=(APTR)s;io->io_Length=n;return !DoIO((struct IORequest*)io)&&io->io_Actual==n;}
static int text(const char*s){return put(s,strlen(s));}
static ULONG pixels(void){ULONG h=2166136261u;for(int y=0;y<u->cu_YRSize;y++)for(int x=0;x<4*u->cu_XRSize;x++)h=(h^ReadRGBPixel(w->RPort,u->cu_XROrigin+x,u->cu_YROrigin+y))*16777619u;return h;}
#define CHECK(c) do{if(!(c)){printf("LATIN1_FAIL line=%d font=%d glyph=%d style=%d split=%d check=%s\n",__LINE__,f,g,style,fragment,#c);fflush(stdout);Delay(500);goto out;}}while(0)
int main(void){int rc=20,opened=0,f=0,g=0,style=0,fragment=0,cases=0;struct MsgPort*p=CreateMsgPort();io=p?(void*)CreateIORequest(p,sizeof(*io)):NULL;CHECK(io);CyberGfxBase=OpenLibrary((CONST_STRPTR)"cybergraphics.library",0);CHECK(CyberGfxBase);
w=OpenWindowTags(NULL,WA_Left,30,WA_Top,30,WA_Width,730,WA_Height,470,WA_Title,(IPTR)"ConsoleNG Latin-1 native/UTF-8 equivalence",WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_SizeGadget,TRUE,WA_SizeBRight,TRUE,WA_MinWidth,300,WA_MinHeight,200,WA_MaxWidth,-1,WA_MaxHeight,-1,TAG_DONE);CHECK(w);
io->io_Data=w;io->io_Length=sizeof(*w);CHECK(!OpenDevice((CONST_STRPTR)"LATIN1:Devs/consoleng.device",CONU_CHARMAP,(struct IORequest*)io,0));opened=1;u=(void*)io->io_Unit;
const char*fonts[]={"topaz,8","DejaVuSansMono.font,16","DejaVuSansMono.font,24","DejaVuSansMono.font,32"};const UBYTE glyphs[]={0xa3,0xe9,0xfc,0xc4,0xdf,0xff};const char*styles[]={"0","1","2","3","4","7","8","21;9"};
for(f=0;f<4;f++){
 char font[80];snprintf(font,sizeof(font),"\x1b[?25l\x1b]50;%s\7",fonts[f]);CHECK(text(font));
 for(g=0;g<6;g++)for(style=0;style<8;style++)for(fragment=0;fragment<2;fragment++){
  char reset[100];snprintf(reset,sizeof(reset),"\x1b[0;38;2;230;240;245;48;2;25;60;80m\x1b[%sm\x1b[2J\x1b[H",styles[style]);
  CHECK(text("\x1b%@"));CHECK(text(reset));CHECK(put(glyphs+g,1));CHECK(u->cu_XCCP==1);ULONG expected=pixels();
  CHECK(text("\x1b[>9h\x1b[>9l"));CHECK(pixels()==expected);
  CHECK(text("\x1b%G"));CHECK(text(reset));UBYTE enc[]={0xc0|(glyphs[g]>>6),0x80|(glyphs[g]&63)};
  if(fragment){CHECK(put(enc,1));CHECK(u->cu_XCCP==0);CHECK(put(enc+1,1));}else CHECK(put(enc,2));
  CHECK(u->cu_XCCP==1);CHECK(pixels()==expected);CHECK(text("\x1b[>9h\x1b[>9l"));CHECK(pixels()==expected);cases++;
 }
 ULONG expected=pixels();SizeWindow(w,-40,0);Delay(10);CHECK(pixels()==expected);SizeWindow(w,40,0);Delay(10);CHECK(pixels()==expected);
 printf("LATIN1_FONT_PASS font=%d cases=%d\n",f,cases);fflush(stdout);
}
CHECK(text("\x1b%G\x1b[0;38;2;230;240;245;48;2;25;60;80m\x1b]50;DejaVuSansMono.font,24\7\x1b[2J\x1b[HLatin-1: matching native and UTF-8\r\n\r\n\xc2\xa3  \xc3\xa9  \xc3\xbc  \xc3\x84  \xc3\x9f  \xc3\xbf\r\n\r\n384 native pixel comparisons PASS\r\nFour fonts; eight text styles\r\nWhole and bytewise UTF-8\r\nImmediate / retained / resize agreement"));printf("LATIN1_PASS cases=%d\n",cases);fflush(stdout);Delay(600);rc=0;
out:if(opened)CloseDevice((struct IORequest*)io);if(w)CloseWindow(w);if(io)DeleteIORequest((struct IORequest*)io);if(p)DeleteMsgPort(p);if(CyberGfxBase)CloseLibrary(CyberGfxBase);printf("LATIN1_SCRIPT rc=%d\n",rc);return rc;}
