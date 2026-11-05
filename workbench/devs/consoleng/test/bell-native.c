/* Native visual BEL: visible/hidden units, bursts, restoration and disposal. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/cybergraphics.h>
#include <devices/conunit.h>
#include <stdio.h>
#include <string.h>
struct Library *CyberGfxBase;
static struct IOStdReq *io;
static struct Window *w;
static struct ConUnit *u;
static struct Screen *screen;
static int put(const char *s){io->io_Command=CMD_WRITE;io->io_Data=(APTR)s;io->io_Length=strlen(s);return !DoIO((struct IORequest*)io)&&io->io_Actual==strlen(s);}
static ULONG pixels(void){ULONG h=2166136261UL;for(int y=0;y<3*u->cu_YRSize;y++)for(int x=0;x<20*u->cu_XRSize;x++)h=(h^ReadRGBPixel(w->RPort,u->cu_XROrigin+x,u->cu_YROrigin+y))*16777619UL;return h;}
static int waitbeep(int on){for(int t=0;t<100;t++){if(!!(screen->Flags&BEEPING)==on)return 1;Delay(1);}return 0;}
#define CHECK(c) do{if(!(c)){printf("BELL_FAIL line=%d check=%s kind=%d hidden=%d burst=%d\n",__LINE__,#c,kind,hidden,burst);fflush(stdout);Delay(150);goto out;}}while(0)
int main(void){int rc=20,opened=0,kind=0,hidden=0,burst=0,cases=0;int units[]={CONU_STANDARD,CONU_CHARMAP,CONU_SNIPMAP};struct MsgPort *p=CreateMsgPort();io=p?(void*)CreateIORequest(p,sizeof(*io)):NULL;CHECK(io);CyberGfxBase=OpenLibrary((CONST_STRPTR)"cybergraphics.library",0);CHECK(CyberGfxBase);
for(kind=0;kind<3;kind++){
 w=OpenWindowTags(NULL,WA_Left,30,WA_Top,30,WA_Width,720,WA_Height,450,WA_Title,(IPTR)"ConsoleNG native visual bell",WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_SizeGadget,TRUE,WA_SizeBRight,TRUE,WA_MinWidth,200,WA_MinHeight,160,WA_MaxWidth,-1,WA_MaxHeight,-1,TAG_DONE);CHECK(w);screen=w->WScreen;
 io->io_Data=w;io->io_Length=sizeof(*w);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",units[kind],(struct IORequest*)io,0));opened=1;u=(void*)io->io_Unit;
 CHECK(put("\x1b[?25l\x1b]50;DejaVuSansMono,16\7Native visual bell\r\nText and cursor stay unchanged"));
 for(hidden=0;hidden<2;hidden++)for(burst=1;burst<=256;burst*=16){
  char bells[257];memset(bells,7,burst);bells[burst]=0;
  CHECK(waitbeep(0));if(hidden)CHECK(put("\x1b[>9h"));
  ULONG before=pixels();WORD x=u->cu_XCP,y=u->cu_YCP;UBYTE flags=u->cu_TxFlags;
  CHECK(put(bells));CHECK(waitbeep(1));CHECK(pixels()==before);CHECK(u->cu_XCP==x&&u->cu_YCP==y&&u->cu_TxFlags==flags);
  CHECK(waitbeep(0));CHECK(pixels()==before);if(hidden)CHECK(put("\x1b[>9l"));cases++;
 }
 CHECK(put("\r\n\r\nPASS: native visual bell\r\nSingle and burst BEL, visible and hidden units\r\nScreen flash restores; text and cursor unchanged"));
 if(kind==2){printf("BELL_PASS cases=%d\n",cases);fflush(stdout);for(int i=0;i<30;i++){CHECK(put("\7"));Delay(5);}}
 CHECK(put("\7"));CloseDevice((struct IORequest*)io);opened=0;CloseWindow(w);w=NULL;CHECK(waitbeep(0));
}
rc=0;
out:if(opened)CloseDevice((struct IORequest*)io);if(w)CloseWindow(w);if(CyberGfxBase)CloseLibrary(CyberGfxBase);if(io)DeleteIORequest((struct IORequest*)io);if(p)DeleteMsgPort(p);printf("BELL_RUN rc=%d\n",rc);return rc;}
