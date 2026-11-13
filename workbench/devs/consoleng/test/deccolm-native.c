/* DECCOLM clears margins/grid and homes cursor without changing geometry. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/cybergraphics.h>
#include <devices/conunit.h>
#include <stdio.h>
#include <string.h>
#include "../console_gcc.h"
#undef IntuitionBase
struct Library *CyberGfxBase;
static struct IOStdReq *io;static struct Window *w;static struct ConUnit *u;
static int put(const char *s){io->io_Command=CMD_WRITE;io->io_Data=(APTR)s;io->io_Length=strlen(s);return !DoIO((struct IORequest*)io)&&io->io_Actual==strlen(s);}
static int split(const char *s){char b[2]={0};while(*s){b[0]=*s++;if(!put(b))return 0;}return 1;}
static int blank(ULONG bg){for(int y=0;y<(u->cu_YMax+1)*u->cu_YRSize;y++)for(int x=0;x<20*u->cu_XRSize;x++)if(ReadRGBPixel(w->RPort,u->cu_XROrigin+x,u->cu_YROrigin+y)!=bg)return 0;return 1;}
static ULONG pixels(void){ULONG h=2166136261UL;for(int y=0;y<(u->cu_YMax+1)*u->cu_YRSize;y++)for(int x=0;x<20*u->cu_XRSize;x++)h=(h^ReadRGBPixel(w->RPort,u->cu_XROrigin+x,u->cu_YROrigin+y))*16777619UL;return h;}
#define CHECK(c) do{if(!(c)){printf("DECCOLM_FAIL line=%d check=%s kind=%d fragment=%d style=%d code=%d hidden=%d alternate=%d\n",__LINE__,#c,kind,fragment,style,code,hidden,alternate);fflush(stdout);Delay(500);goto out;}}while(0)
int main(void){int rc=20,opened=0,kind=0,fragment=0,style=0,code=0,cases=0,hidden=0,alternate=0;struct MsgPort *p=CreateMsgPort();io=p?(void*)CreateIORequest(p,sizeof(*io)):NULL;CHECK(io);CyberGfxBase=OpenLibrary((CONST_STRPTR)"cybergraphics.library",0);CHECK(CyberGfxBase);
for(kind=0;kind<2;kind++){
 w=OpenWindowTags(NULL,WA_Left,30,WA_Top,30,WA_Width,720,WA_Height,450,WA_Title,(IPTR)"ConsoleNG DECCOLM reset behavior",WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_SizeGadget,TRUE,WA_SizeBRight,TRUE,WA_MinWidth,200,WA_MinHeight,160,WA_MaxWidth,-1,WA_MaxHeight,-1,TAG_DONE);CHECK(w);
 io->io_Data=w;io->io_Length=sizeof(*w);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",kind?CONU_CHARMAP:CONU_STANDARD,(struct IORequest*)io,0));opened=1;u=(void*)io->io_Unit;
 CHECK(put("\x1b%G\x1b[?25l\x1b]50;DejaVuSansMono,16\7"));
 for(fragment=0;fragment<2;fragment++)for(style=0;style<2;style++)for(code=0;code<2;code++)for(hidden=0;hidden<(kind?2:1);hidden++)for(alternate=0;alternate<(kind?2:1);alternate++){
  CHECK(put("\x1b[?6l\x1b[r\x1b[0m\x1b[2J\x1b[HPrimary screen preserved"));ULONG primary=pixels();
  if(alternate)CHECK(put("\x1b[?1049h"));
  CHECK(put("\x1b[?6l\x1b[r\x1b[0;38;2;220;170;90;48;2;20;40;60m\x1b[2J\x1b[H"));
  ULONG bg=ReadRGBPixel(w->RPort,u->cu_XROrigin,u->cu_YROrigin);
  for(int row=1;row<=u->cu_YMax+1;row++){char seq[64];snprintf(seq,sizeof(seq),"\x1b[%d;1HABCDEFGHIJ0123456789",row);CHECK(put(seq));}
  CHECK(!blank(bg));CHECK(put("\x1b[1;3;9m\x1b[3;8r"));CHECK(put(style?"\x1b[?6h":"\x1b[?6l"));CHECK(put("\x1b[4;6H"));
  WORD xmax=u->cu_XMax,ymax=u->cu_YMax,width=w->Width,height=w->Height;UBYTE flags=u->cu_TxFlags;
  ULONG visible=pixels();if(hidden)CHECK(put("\x1b[>9h"));
  CHECK(fragment?split(code?"\x1b[?3h":"\x1b[?3l"):put(code?"\x1b[?3h":"\x1b[?3l"));
  CHECK(u->cu_XCP==0&&u->cu_YCP==0);CHECK(u->cu_XMax==xmax&&u->cu_YMax==ymax&&w->Width==width&&w->Height==height);
  CHECK(ICU((Object*)u)->scrollTop==0&&ICU((Object*)u)->scrollBottom==ymax+1);
  CHECK(!!(ICU((Object*)u)->conFlags&CF_ORIGIN_MODE)==style);CHECK(u->cu_TxFlags==flags);
  if(hidden){CHECK(pixels()==visible);CHECK(put("\x1b[>9l"));}
  CHECK(blank(bg));
  if(alternate){CHECK(put("\x1b[?1049l"));CHECK(pixels()==primary);}
  cases++;
 }
 CHECK(put("\x1b[?6l\x1b[0m\x1b[2J\x1b[HPASS: DECCOLM resets margins and visible grid\r\n\r\nSet and reset, whole and bytewise controls\r\nStandard and retained console units\r\nCursor returns home; origin mode and styles preserved\r\nNative window dimensions remain unchanged\r\nHidden updates preserve pixels until reveal\r\nAlternate-screen resets preserve the primary screen"));
 printf("DECCOLM_UNIT_PASS kind=%d cases=%d\n",kind,cases);fflush(stdout);Delay(150);CloseDevice((struct IORequest*)io);opened=0;CloseWindow(w);w=NULL;
}
printf("DECCOLM_PASS cases=%d\n",cases);rc=0;
out:if(opened)CloseDevice((struct IORequest*)io);if(w)CloseWindow(w);if(CyberGfxBase)CloseLibrary(CyberGfxBase);if(io)DeleteIORequest((struct IORequest*)io);if(p)DeleteMsgPort(p);printf("DECCOLM_RUN rc=%d\n",rc);return rc;}
