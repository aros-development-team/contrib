/* Private owned unit: cursor paint must preserve caller raster state and pixels. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/cybergraphics.h>
#include <dos/dosextens.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../console_gcc.h"
#include "../consoleif.h"
/* This fixture opens its window before it has a device instance. */
#undef IntuitionBase
struct Device *ConsoleDevice;
struct Library *CyberGfxBase;
static Object *unit;
static struct IOStdReq *io;
static int put(const char *s){io->io_Command=CMD_WRITE;io->io_Data=(APTR)s;io->io_Length=strlen(s);return !DoIO((struct IORequest*)io)&&io->io_Actual==strlen(s);}
static int reply(const char*expected){char got[64]={0};int n=strlen(expected);io->io_Command=CMD_READ;io->io_Data=got;io->io_Length=n;SendIO((struct IORequest*)io);for(int i=0;i<100;i++){if(CheckIO((struct IORequest*)io)){WaitIO((struct IORequest*)io);int ok=!io->io_Error&&io->io_Actual==n&&!memcmp(got,expected,n);if(!ok){printf("CURCOLOR_REPLY actual=%lu expected=%d bytes=",(unsigned long)io->io_Actual,n);for(unsigned j=0;j<io->io_Actual&&j<sizeof(got);j++)printf("%02x",(unsigned char)got[j]);printf("\n");fflush(stdout);}return ok;}Delay(1);}AbortIO((struct IORequest*)io);WaitIO((struct IORequest*)io);return 0;}
static ULONG pixels(void){struct ConUnit *u=CU(unit);ULONG hash=2166136261UL;for(int y=0;y<3*u->cu_YRSize;y++)for(int x=0;x<20*u->cu_XRSize;x++)hash=(hash^ReadRGBPixel(u->cu_Window->RPort,u->cu_XROrigin+x,u->cu_YROrigin+y))*16777619UL;return hash;}
#define CHECK(c) do{if(!(c)){printf("CURCOLOR_FAIL line=%d check=%s\n",__LINE__,#c);fflush(stdout);goto out;}}while(0)
int main(int argc,char**argv){int kind=argc>1?atoi(argv[1]):1;int rc=20,cases=0,opened=0;struct ConsoleBase *base=NULL;struct Window*w=NULL;struct MsgPort*port=CreateMsgPort();io=port?(void*)CreateIORequest(port,sizeof(*io)):NULL;CHECK(io);
 CyberGfxBase=OpenLibrary((CONST_STRPTR)"cybergraphics.library",0);CHECK(CyberGfxBase);
 w=OpenWindowTags(NULL,WA_Left,30,WA_Top,30,WA_Width,730,WA_Height,470,WA_Title,(IPTR)"ConsoleNG independent cursor colors",WA_Activate,TRUE,WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_SizeGadget,TRUE,WA_SizeBRight,TRUE,TAG_DONE);CHECK(w);io->io_Data=w;io->io_Length=sizeof(*w);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",kind,(struct IORequest*)io,0));opened=1;unit=(Object*)io->io_Unit;base=(void*)io->io_Device;ConsoleDevice=io->io_Device;ActivateWindow(w);Delay(20);
 CHECK(put("\x1b[?25l\x1b]11;#000000\7\x1b[0m\x1b[2J\x1b[H\x1b[2 q"));
 for(int split=0;split<2;split++){
  const char*set="\x1b]12;#12a4e8\7";if(split){char b[2]={0};while(*set){b[0]=*set++;CHECK(put(b));}}else CHECK(put(set));
  CHECK(put("\x1b]12;?\7"));CHECK(reply("\x1b]12;rgb:1212/A4A4/E8E8\7"));
  CHECK(put("\x1b[?25h"));CHECK((ReadRGBPixel(w->RPort,CU(unit)->cu_XROrigin+3,CU(unit)->cu_YROrigin+3)&0xffffff)==0x12a4e8);CHECK(put("\x1b[?25l"));
  CHECK(put("\x1b]12;#xyz\7\x1b]12;?\x1b\\"));CHECK(reply("\x1b]12;rgb:1212/A4A4/E8E8\x1b\\"));
 }
 CHECK(put("\x1b%G\x1b]50;DejaVuSansMono,16\7\x1b[2 q\x1b[2J\x1b[HCursor paint restoration\r\n0123456789 ABCDEFGHIJ\x1b[2;5H\x1b[?25h"));Delay(10);
 for(int color=0;color<2;color++)for(int style=2;style<=6;style+=2)for(int active=0;active<2;active++)for(int mode=0;mode<8;mode++){
  CHECK(put(color?"\x1b]12;#fa4379\7":"\x1b]12;#12a4e8\7"));
  char seq[24];snprintf(seq,sizeof(seq),"\x1b[%d q",style);CHECK(put(seq));
  struct RastPort *rp=CU(unit)->cu_Window->RPort;
  static const UWORD pattern[]={0x1111,0xEEEE};
  ObtainSemaphore(&base->consoleTaskLock);
  struct InputEvent event={0};event.ie_Class=active?IECLASS_ACTIVEWINDOW:IECLASS_INACTIVEWINDOW;Console_HandleGadgets(unit,&event);
  Console_UnRenderCursor(unit);
  ULONG before=pixels();const UWORD *oldpattern=rp->AreaPtrn;BYTE oldsize=rp->AreaPtSz;UBYTE oldmode=rp->DrawMode;
  rp->AreaPtrn=pattern;rp->AreaPtSz=1;SetDrMd(rp,mode);
  Console_RenderCursor(unit);
  int state1=rp->AreaPtrn==pattern&&rp->AreaPtSz==1&&rp->DrawMode==mode;
  ULONG painted=pixels();Console_UnRenderCursor(unit);
  int state2=rp->AreaPtrn==pattern&&rp->AreaPtSz==1&&rp->DrawMode==mode;
  ULONG after=pixels();rp->AreaPtrn=oldpattern;rp->AreaPtSz=oldsize;SetDrMd(rp,oldmode);Console_RenderCursor(unit);
  ReleaseSemaphore(&base->consoleTaskLock);
  CHECK(state1&&state2);CHECK(painted!=before);CHECK(after==before);cases++;
 }
 CHECK(put("\x1b]112\7\x1b]10;#ffffff;#000000;#12a4e8\7\x1b]12;?\7"));CHECK(reply("\x1b]12;rgb:1212/A4A4/E8E8\7"));
 CHECK(put("\x1b[?25l\x1b[2J\x1b[HPASS: cursor raster state and exact pixel restoration\r\n\r\n96 native combinations: block / underline / bar\r\nActive solid and inactive stipple\r\nEight incoming drawing modes, custom caller pattern\r\nDraw / erase restores the original pixels\r\nCaller DrawMode and area pattern remain unchanged\r\n\r\nColored cursor: gjpqy Style\x1b[9;17H\x1b[2 q\x1b[?25h"));
 printf("CURCOLOR_PASS unit=%d cases=%d\n",kind,cases);fflush(stdout);Delay(500);rc=0;
 out:if(opened)CloseDevice((struct IORequest*)io);if(w)CloseWindow(w);if(io)DeleteIORequest((struct IORequest*)io);if(port)DeleteMsgPort(port);if(CyberGfxBase)CloseLibrary(CyberGfxBase);printf("CURCOLOR_RUN rc=%d\n",rc);return rc;
}
