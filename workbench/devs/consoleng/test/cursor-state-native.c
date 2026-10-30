/* Private owned unit: cursor paint must preserve caller raster state and pixels. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/cybergraphics.h>
#include <dos/dosextens.h>
#include <stdio.h>
#include <string.h>
#include "../console_gcc.h"
#include "../consoleif.h"
struct Device *ConsoleDevice;
struct Library *CyberGfxBase;
static Object *unit;
static BPTR stream;
static int put(const char *s){return Write(stream,(APTR)s,strlen(s))==(LONG)strlen(s);}
static ULONG pixels(void){struct ConUnit *u=CU(unit);ULONG hash=2166136261UL;for(int y=0;y<3*u->cu_YRSize;y++)for(int x=0;x<20*u->cu_XRSize;x++)hash=(hash^ReadRGBPixel(u->cu_Window->RPort,u->cu_XROrigin+x,u->cu_YROrigin+y))*16777619UL;return hash;}
#define CHECK(c) do{if(!(c)){printf("CURSTATE_FAIL line=%d check=%s\n",__LINE__,#c);fflush(stdout);goto out;}}while(0)
int main(void){int rc=20,cases=0;struct InfoData info;struct ConsoleBase *base=NULL;
 CyberGfxBase=OpenLibrary((CONST_STRPTR)"cybergraphics.library",0);CHECK(CyberGfxBase);
 stream=Open((CONST_STRPTR)"RGCON:",MODE_OLDFILE);CHECK(stream);
 struct FileHandle *fh=BADDR(stream);CHECK(DoPkt(fh->fh_Type,ACTION_DISK_INFO,(SIPTR)MKBADDR(&info),0,0,0,0));
 struct IOStdReq *io=(void*)(IPTR)info.id_InUse;CHECK(io&&io->io_Unit);
 unit=(Object*)io->io_Unit;base=(void*)io->io_Device;ConsoleDevice=io->io_Device;
 CHECK(put("\x1b%G\x1b]50;DejaVuSansMono,16\7\x1b[2 q\x1b[2J\x1b[HCursor paint restoration\r\n0123456789 ABCDEFGHIJ\x1b[2;5H"));Delay(10);
 for(int style=2;style<=6;style+=2)for(int active=0;active<2;active++)for(int mode=0;mode<8;mode++){
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
 CHECK(put("\x1b[?25l\x1b[2J\x1b[HPASS: cursor raster state and exact pixel restoration\r\n\r\n48 native combinations: block / underline / bar\r\nActive solid and inactive stipple\r\nEight incoming drawing modes, custom caller pattern\r\nDraw / erase restores the original pixels\r\nCaller DrawMode and area pattern remain unchanged"));
 printf("CURSTATE_PASS cases=%d\n",cases);fflush(stdout);Delay(500);rc=0;
 out:if(stream)Close(stream);if(CyberGfxBase)CloseLibrary(CyberGfxBase);printf("CURSTATE_RUN rc=%d\n",rc);return rc;
}
