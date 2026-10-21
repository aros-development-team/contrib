/* Empty modern SGR fields must match explicit zero parameters. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/cybergraphics.h>
#include <devices/conunit.h>
#include <stdio.h>
#include <string.h>
struct Library *CyberGfxBase;
static struct IOStdReq *io;static struct Window *w;static struct ConUnit *u;
static ULONG baseline[65536];
static void snapshot(void){int i=0;for(int y=0;y<3*u->cu_YRSize;y++)for(int x=0;x<20*u->cu_XRSize;x++)baseline[i++]=ReadRGBPixel(w->RPort,u->cu_XROrigin+x,u->cu_YROrigin+y);}
static void differences(void){int i=0,n=0;for(int y=0;y<3*u->cu_YRSize;y++)for(int x=0;x<20*u->cu_XRSize;x++){ULONG c=ReadRGBPixel(w->RPort,u->cu_XROrigin+x,u->cu_YROrigin+y);if(c!=baseline[i]&&n++<12)printf("PIXEL x=%d y=%d before=%08lx after=%08lx\n",x,y,(unsigned long)baseline[i],(unsigned long)c);i++;}printf("PIXEL count=%d\n",n);}
static int put(const char *s){io->io_Command=CMD_WRITE;io->io_Data=(APTR)s;io->io_Length=strlen(s);return !DoIO((struct IORequest*)io)&&io->io_Actual==strlen(s);}
static int split(const char *s){char b[2]={0};while(*s){b[0]=*s++;if(!put(b))return 0;}return 1;}
static ULONG pixels(void){ULONG h=2166136261UL;for(int y=0;y<3*u->cu_YRSize;y++)for(int x=0;x<20*u->cu_XRSize;x++)h=(h^ReadRGBPixel(w->RPort,u->cu_XROrigin+x,u->cu_YROrigin+y))*16777619UL;return h;}
#define CHECK(c) do{if(!(c)){printf("SGRDEFAULT_FAIL line=%d check=%s kind=%d fragment=%d style=%d code=%d\n",__LINE__,#c,kind,fragment,style,code);fflush(stdout);Delay(500);goto out;}}while(0)
int main(void){int rc=20,opened=0,kind=0,fragment=0,style=0,code=0,cases=0;struct MsgPort *p=CreateMsgPort();io=p?(void*)CreateIORequest(p,sizeof(*io)):NULL;CHECK(io);CyberGfxBase=OpenLibrary((CONST_STRPTR)"cybergraphics.library",0);CHECK(CyberGfxBase);
for(kind=0;kind<2;kind++){
 w=OpenWindowTags(NULL,WA_Left,30,WA_Top,30,WA_Width,720,WA_Height,450,WA_Title,(IPTR)"ConsoleNG SGR empty defaults",WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_SizeGadget,TRUE,WA_SizeBRight,TRUE,WA_MinWidth,200,WA_MinHeight,160,WA_MaxWidth,-1,WA_MaxHeight,-1,TAG_DONE);CHECK(w);
 io->io_Data=w;io->io_Length=sizeof(*w);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",kind?CONU_CHARMAP:CONU_STANDARD,(struct IORequest*)io,0));opened=1;u=(void*)io->io_Unit;
 CHECK(put("\x1b%G\x1b[?25l\x1b]50;DejaVuSansMono,16\7"));
 for(fragment=0;fragment<2;fragment++)for(style=0;style<8;style++)for(code=0;code<8;code++){

  const char *request[]={";31m","1;;31m","31;m",";;m","38;2;;128;255m","48;2;32;;128m","1;4:2;;32m","38;5;m"};
  const char *explicit[]={"0;31m","1;0;31m","31;0m","0;0;0m","38;2;0;128;255m","48;2;32;0;128m","1;21;0;32m","38;5;0m"};
  char seq[80], initial[80];
  snprintf(initial,sizeof(initial),"\x1b[0;%d;%d;%d;9;33;44m",style&1?1:22,style&2?3:23,style&4?4:24);
  CHECK(put(initial));snprintf(seq,sizeof(seq),"\x1b[%s",explicit[code]);CHECK(put(seq));CHECK(put("\x1b[2J\x1b[HText A\xce\xa9 Z"));
  ULONG expected=pixels();snapshot();
  if(kind){CHECK(put("\x1b[>9h\x1b[>9l"));if(pixels()!=expected){differences();printf("EXPLICIT_REDRAW\n");}CHECK(pixels()==expected);}
  UBYTE flags=u->cu_TxFlags;BYTE fg=u->cu_FgPen,bg=u->cu_BgPen;WORD x=u->cu_XCP,y=u->cu_YCP;
  CHECK(put(initial));snprintf(seq,sizeof(seq),"\x1b[%s",request[code]);CHECK(fragment?split(seq):put(seq));CHECK(put("\x1b[2J\x1b[HText A\xce\xa9 Z"));
  CHECK(pixels()==expected&&u->cu_TxFlags==flags&&u->cu_FgPen==fg&&u->cu_BgPen==bg&&u->cu_XCP==x&&u->cu_YCP==y);
  if(kind){CHECK(put("\x1b[>9h\x1b[>9l"));CHECK(pixels()==expected);
   CHECK(put("\x1b[?1049hOTHER\x1b[?1049l"));
   CHECK(pixels()==expected);}
  cases++;
 }
 CHECK(put("\x1b[0m\x1b[2J\x1b[HPASS: empty SGR fields match explicit zeros\r\n\r\nLeading, trailing and repeated empty parameters\r\nEmpty RGB channels and palette index default to zero\r\nWhole and bytewise input; standard and retained\r\nUnicode, existing styles, alternate return and redraw"));
 printf("SGRDEFAULT_UNIT_PASS kind=%d cases=%d\n",kind,cases);fflush(stdout);Delay(150);CloseDevice((struct IORequest*)io);opened=0;CloseWindow(w);w=NULL;
}
printf("SGRDEFAULT_PASS cases=%d\n",cases);rc=0;
out:if(opened)CloseDevice((struct IORequest*)io);if(w)CloseWindow(w);if(CyberGfxBase)CloseLibrary(CyberGfxBase);if(io)DeleteIORequest((struct IORequest*)io);if(p)DeleteMsgPort(p);printf("SGRDEFAULT_RUN rc=%d\n",rc);return rc;}
