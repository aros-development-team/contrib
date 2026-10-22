/* Colon color forms must match established semicolon colors. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/cybergraphics.h>
#include <devices/conunit.h>
#include <stdio.h>
#include <string.h>
struct Library *CyberGfxBase;
static struct IOStdReq *io;static struct Window *w;static struct ConUnit *u;
static int put(const char *s){io->io_Command=CMD_WRITE;io->io_Data=(APTR)s;io->io_Length=strlen(s);return !DoIO((struct IORequest*)io)&&io->io_Actual==strlen(s);}
static int split(const char *s){char b[2]={0};while(*s){b[0]=*s++;if(!put(b))return 0;}return 1;}
static ULONG pixels(void){ULONG h=2166136261UL;for(int y=0;y<3*u->cu_YRSize;y++)for(int x=0;x<20*u->cu_XRSize;x++)h=(h^ReadRGBPixel(w->RPort,u->cu_XROrigin+x,u->cu_YROrigin+y))*16777619UL;return h;}
#define CHECK(c) do{if(!(c)){printf("COLONCOLOR_FAIL line=%d check=%s kind=%d fragment=%d style=%d code=%d\n",__LINE__,#c,kind,fragment,style,code);fflush(stdout);Delay(500);goto out;}}while(0)
int main(void){int rc=20,opened=0,kind=0,fragment=0,style=0,code=0,cases=0;struct MsgPort *p=CreateMsgPort();io=p?(void*)CreateIORequest(p,sizeof(*io)):NULL;CHECK(io);CyberGfxBase=OpenLibrary((CONST_STRPTR)"cybergraphics.library",0);CHECK(CyberGfxBase);
for(kind=0;kind<2;kind++){
 w=OpenWindowTags(NULL,WA_Left,30,WA_Top,30,WA_Width,720,WA_Height,450,WA_Title,(IPTR)"ConsoleNG colon color equivalence",WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_SizeGadget,TRUE,WA_SizeBRight,TRUE,WA_MinWidth,200,WA_MinHeight,160,WA_MaxWidth,-1,WA_MaxHeight,-1,TAG_DONE);CHECK(w);
 io->io_Data=w;io->io_Length=sizeof(*w);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",kind?CONU_CHARMAP:CONU_STANDARD,(struct IORequest*)io,0));opened=1;u=(void*)io->io_Unit;
 CHECK(put("\x1b%G\x1b[?25l\x1b]50;DejaVuSansMono,16\7"));
 for(fragment=0;fragment<2;fragment++)for(style=0;style<2;style++)for(code=0;code<8;code++){
  const int indexes[]={0,7,15,16,255};const unsigned colors[]={0x123456,0xff8040,0x112233};char semi[80],colon[80];int selector=style?48:38;
  if(code<5){snprintf(semi,sizeof(semi),"\x1b[%d;5;%dm",selector,indexes[code]);snprintf(colon,sizeof(colon),"\x1b[%d:5:%dm",selector,indexes[code]);}
  else {unsigned c=colors[code-5];snprintf(semi,sizeof(semi),"\x1b[%d;2;%u;%u;%um",selector,c>>16,(c>>8)&255,c&255);snprintf(colon,sizeof(colon),code==5?"\x1b[%d:2:%u:%u:%um":code==6?"\x1b[%d:2::%u:%u:%um":"\x1b[%d:2:0:%u:%u:%um",selector,c>>16,(c>>8)&255,c&255);}
  CHECK(put("\x1b[0;37;40m"));CHECK(put(semi));CHECK(put("\x1b[2J\x1b[HText A\xce\xa9 Z"));ULONG expected=pixels();WORD x=u->cu_XCP,y=u->cu_YCP;
  CHECK(put("\x1b[0;37;40m"));CHECK(fragment?split(colon):put(colon));CHECK(put("\x1b[2J\x1b[HText A\xce\xa9 Z"));CHECK(pixels()==expected);CHECK(u->cu_XCP==x&&u->cu_YCP==y&&u->cu_TxFlags==0);
  CHECK(fragment?split("\x1b[38:2::12::56m\x1b[48:5:256m"):put("\x1b[38:2::12::56m\x1b[48:5:256m"));CHECK(pixels()==expected&&u->cu_TxFlags==0&&u->cu_XCP==x&&u->cu_YCP==y);
  if(kind){CHECK(put("\x1b[>9h\x1b[>9l"));CHECK(pixels()==expected);CHECK(put("\x1b[?1049h\x1b[31;44mOTHER\x1b[?1049l"));CHECK(pixels()==expected);SizeWindow(w,16,0);Delay(5);CHECK(pixels()==expected);SizeWindow(w,-16,0);Delay(5);CHECK(pixels()==expected);}
  cases++;
 }
 CHECK(put("\x1b[0m\x1b[2J\x1b[HPASS: colon colors match semicolon color pixels\r\n\r\nIndexed and RGB foreground/background\r\nRGB with omitted, empty or zero color-space field\r\nWhole and bytewise input; standard and retained\r\nUnicode text, alternate return, hidden redraw, resize"));
 printf("COLONCOLOR_UNIT_PASS kind=%d cases=%d\n",kind,cases);fflush(stdout);Delay(150);CloseDevice((struct IORequest*)io);opened=0;CloseWindow(w);w=NULL;
}
printf("COLONCOLOR_PASS cases=%d\n",cases);rc=0;
out:if(opened)CloseDevice((struct IORequest*)io);if(w)CloseWindow(w);if(CyberGfxBase)CloseLibrary(CyberGfxBase);if(io)DeleteIORequest((struct IORequest*)io);if(p)DeleteMsgPort(p);printf("COLONCOLOR_RUN rc=%d\n",rc);return rc;}
