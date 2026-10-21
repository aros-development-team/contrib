/* Unsupported underline colors must consume payloads without rendition leakage. */
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
#define CHECK(c) do{if(!(c)){printf("SGR58_FAIL line=%d check=%s kind=%d fragment=%d style=%d code=%d\n",__LINE__,#c,kind,fragment,style,code);fflush(stdout);Delay(500);goto out;}}while(0)
int main(void){int rc=20,opened=0,kind=0,fragment=0,style=0,code=0,cases=0;struct MsgPort *p=CreateMsgPort();io=p?(void*)CreateIORequest(p,sizeof(*io)):NULL;CHECK(io);CyberGfxBase=OpenLibrary((CONST_STRPTR)"cybergraphics.library",0);CHECK(CyberGfxBase);
for(kind=0;kind<2;kind++){
 w=OpenWindowTags(NULL,WA_Left,30,WA_Top,30,WA_Width,720,WA_Height,450,WA_Title,(IPTR)"ConsoleNG underline-color isolation",WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_SizeGadget,TRUE,WA_SizeBRight,TRUE,WA_MinWidth,200,WA_MinHeight,160,WA_MaxWidth,-1,WA_MaxHeight,-1,TAG_DONE);CHECK(w);
 io->io_Data=w;io->io_Length=sizeof(*w);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",kind?CONU_CHARMAP:CONU_STANDARD,(struct IORequest*)io,0));opened=1;u=(void*)io->io_Unit;
 CHECK(put("\x1b%G\x1b[?25l\x1b]50;DejaVuSansMono,16\7"));
 for(fragment=0;fragment<2;fragment++)for(style=0;style<2;style++)for(int n=0;n<14;n++){
  const char *values[]={"58","58;5","58;5;1","58;5;31","58;5;256","58;2","58;2;1","58;2;1;4","58;2;1;4;31","58;2;255;0;255","58;9","58:2::1:4:31","58:5:1","59"};
  code=n;char seq[64];snprintf(seq,sizeof(seq),"\x1b[%sm",values[n]);
  CHECK(put("\x1b[0;38;2;220;170;90;48;2;20;40;60m\x1b[2J\x1b[H"));
  CHECK(put(style?"\x1b[1;3;4;7;9m":"\x1b[2m"));CHECK(put("Text A\xce\xa9 Z\x1b[H"));
  ULONG before=pixels();UBYTE flags=u->cu_TxFlags;BYTE fg=u->cu_FgPen,bg=u->cu_BgPen;WORD x=u->cu_XCP,y=u->cu_YCP;
  CHECK(fragment?split(seq):put(seq));CHECK(u->cu_TxFlags==flags&&u->cu_FgPen==fg&&u->cu_BgPen==bg);CHECK(u->cu_XCP==x&&u->cu_YCP==y);CHECK(pixels()==before);
  CHECK(put("Text A\xce\xa9 Z\x1b[H"));CHECK(pixels()==before);cases++;
 }
 CHECK(put("\x1b[0m\x1b[2J\x1b[HPASS: unsupported SGR preserves rendition and pixels\r\n\r\nSGR 58 color payload never becomes unrelated styles\r\nWhole and bytewise control sequences\r\nStandard and retained console units\r\nColors, faint/bold/italic/underline/reverse/strike preserved\r\nCursor position and repeated Unicode text unchanged"));
 printf("SGR58_UNIT_PASS kind=%d cases=%d\n",kind,cases);fflush(stdout);Delay(150);CloseDevice((struct IORequest*)io);opened=0;CloseWindow(w);w=NULL;
}
printf("SGR58_PASS cases=%d\n",cases);rc=0;
out:if(opened)CloseDevice((struct IORequest*)io);if(w)CloseWindow(w);if(CyberGfxBase)CloseLibrary(CyberGfxBase);if(io)DeleteIORequest((struct IORequest*)io);if(p)DeleteMsgPort(p);printf("SGR58_RUN rc=%d\n",rc);return rc;}
