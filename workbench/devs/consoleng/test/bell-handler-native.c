/* Native DOS handler BEL: cooked/raw, hidden/visible, bursts and no input. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/cybergraphics.h>
#include <devices/conunit.h>
#include <dos/dosextens.h>
#include <stdio.h>
#include <string.h>
struct Library *CyberGfxBase;
static struct IOStdReq *io;
static BPTR stream;
static struct Window *w;
static struct ConUnit *u;
static struct Screen *screen;
static int put(const char *s){return Write(stream,s,strlen(s))==(LONG)strlen(s);}
static ULONG pixels(void){ULONG h=2166136261UL;for(int y=0;y<3*u->cu_YRSize;y++)for(int x=0;x<20*u->cu_XRSize;x++)h=(h^ReadRGBPixel(w->RPort,u->cu_XROrigin+x,u->cu_YROrigin+y))*16777619UL;return h;}
static int waitbeep(int on){for(int t=0;t<100;t++){if(!!(screen->Flags&BEEPING)==on)return 1;Delay(1);}return 0;}
#define CHECK(c) do{if(!(c)){printf("BELLHANDLER_FAIL line=%d check=%s kind=%d hidden=%d burst=%d\n",__LINE__,#c,kind,hidden,burst);fflush(stdout);Delay(150);goto out;}}while(0)
int main(void){int rc=20,kind=0,hidden=0,burst=0,cases=0;
CyberGfxBase=OpenLibrary((CONST_STRPTR)"cybergraphics.library",0);CHECK(CyberGfxBase);
for(kind=0;kind<2;kind++){
 struct InfoData info;stream=Open((CONST_STRPTR)"RGCON:",MODE_OLDFILE);CHECK(stream);
 struct FileHandle *fh=BADDR(stream);CHECK(DoPkt(fh->fh_Type,ACTION_DISK_INFO,(SIPTR)MKBADDR(&info),0,0,0,0));
 io=(struct IOStdReq*)(IPTR)info.id_InUse;CHECK(io&&io->io_Unit&&io->io_Device);
 CHECK(!strcmp((char*)io->io_Device->dd_Library.lib_Node.ln_Name,"consoleng.device"));
 u=(void*)io->io_Unit;w=u->cu_Window;CHECK(w);screen=w->WScreen;
 SetWindowTitles(w,(CONST_STRPTR)"ConsoleNG DOS cooked/raw visual bell",(CONST_STRPTR)-1);
 MoveWindow(w,30-w->LeftEdge,30-w->TopEdge);SizeWindow(w,720-w->Width,450-w->Height);Delay(20);
 CHECK(SetMode(stream,kind));
 CHECK(put("\x1b[?25l\x1b]50;DejaVuSansMono,16\7DOS handler visual bell\r\nCooked and raw streams preserve text"));
 for(hidden=0;hidden<2;hidden++)for(burst=1;burst<=256;burst*=16){
  char bells[257];memset(bells,7,burst);bells[burst]=0;
  CHECK(waitbeep(0));if(hidden)CHECK(put("\x1b[>9h"));
  ULONG before=pixels();WORD x=u->cu_XCP,y=u->cu_YCP;UBYTE flags=u->cu_TxFlags;
  CHECK(put(bells));CHECK(waitbeep(1));CHECK(pixels()==before);CHECK(u->cu_XCP==x&&u->cu_YCP==y&&u->cu_TxFlags==flags);
  CHECK(waitbeep(0));CHECK(pixels()==before);CHECK(!WaitForChar(stream,100000));if(hidden)CHECK(put("\x1b[>9l"));cases++;
 }
 CHECK(put("\r\n\r\nPASS: native visual bell\r\nDOS Write: cooked/raw, visible/hidden, single/bursts\r\nScreen restores; no stray DOS input"));
 if(kind==1){printf("BELLHANDLER_PASS cases=%d\n",cases);fflush(stdout);for(int i=0;i<30;i++){CHECK(put("\7"));Delay(5);}}
 CHECK(put("\7"));Close(stream);stream=0;w=NULL;io=NULL;CHECK(waitbeep(0));
}
rc=0;
out:if(stream)Close(stream);if(CyberGfxBase)CloseLibrary(CyberGfxBase);printf("BELLHANDLER_RUN rc=%d\n",rc);return rc;}
