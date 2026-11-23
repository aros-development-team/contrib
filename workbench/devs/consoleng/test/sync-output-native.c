/* Native DEC2026 frame buffering, timer, parser boundary and close replay. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/cybergraphics.h>
#include <devices/conunit.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct Library *CyberGfxBase;
static struct IOStdReq *io,*reader;
static int bytes(const char *s,ULONG n){io->io_Command=CMD_WRITE;io->io_Data=(APTR)s;io->io_Length=n;return !DoIO((struct IORequest*)io)&&io->io_Actual==n;}
static int put(const char *s){return bytes(s,strlen(s));}
static int reply(const char *expected){char got[64];reader->io_Command=CMD_READ;reader->io_Data=got;reader->io_Length=strlen(expected);SendIO((struct IORequest*)reader);for(int i=0;i<100;i++){if(CheckIO((struct IORequest*)reader)){WaitIO((struct IORequest*)reader);return !reader->io_Error&&reader->io_Actual==strlen(expected)&&!memcmp(got,expected,strlen(expected));}Delay(1);}AbortIO((struct IORequest*)reader);WaitIO((struct IORequest*)reader);return 0;}
static ULONG pixels(struct Window *w,struct ConUnit *u){ULONG h=2166136261UL;for(int y=0;y<2*u->cu_YRSize;y++)for(int x=0;x<20*u->cu_XRSize;x++)h=(h^ReadRGBPixel(w->RPort,u->cu_XROrigin+x,u->cu_YROrigin+y))*16777619UL;return h;}
#define CHECK(c) do{if(!(c)){printf("SYNC_FAIL line=%d kind=%d count=%d\n",__LINE__,kind,count);fflush(stdout);goto out;}}while(0)
int main(int argc,char **argv){int rc=20,opened=0,count=0,kind=argc>1?atoi(argv[1]):1;struct Window *w=NULL,*other=NULL;struct MsgPort *port=NULL;char *large=NULL;
 CyberGfxBase=OpenLibrary((CONST_STRPTR)"cybergraphics.library",0);CHECK(CyberGfxBase);port=CreateMsgPort();CHECK(port);io=(void*)CreateIORequest(port,sizeof(*io));reader=(void*)CreateIORequest(port,sizeof(*reader));CHECK(io&&reader);
 w=OpenWindowTags(NULL,WA_Left,30,WA_Top,35,WA_Width,730,WA_Height,460,WA_Title,(IPTR)"ConsoleNG synchronized output",WA_Activate,TRUE,WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_SizeGadget,TRUE,WA_SizeBRight,TRUE,WA_MinWidth,200,WA_MinHeight,100,WA_MaxWidth,-1,WA_MaxHeight,-1,TAG_DONE);CHECK(w);
 io->io_Data=w;io->io_Length=sizeof(*w);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",kind,(struct IORequest*)io,0));opened=1;reader->io_Device=io->io_Device;reader->io_Unit=io->io_Unit;
 struct ConUnit *u=(void*)io->io_Unit;
 CHECK(put("\x1b[?2026$p"));CHECK(reply("\x1b[?2026;2$y"));
 for(int start=0;start<=8;start++)for(int end=0;end<=8;end++){
  CHECK(put("\x1b[?25l\x1b[2J\x1b[HOld frame"));ULONG old=pixels(w,u);UWORD x=u->cu_XCP,y=u->cu_YCP;
  CHECK(bytes("\x1b[?2026h",start));CHECK(bytes("\x1b[?2026h"+start,8-start));CHECK(put("\rNew frame\x1b[?2026$p"));
  CHECK(u->cu_XCP==x&&u->cu_YCP==y&&pixels(w,u)==old);
  CHECK(bytes("\x1b[?2026l",end));CHECK(bytes("\x1b[?2026l"+end,8-end));
  CHECK(pixels(w,u)!=old);CHECK(reply("\x1b[?2026;1$y"));CHECK(put("\x1b[?2026$p"));CHECK(reply("\x1b[?2026;2$y"));count++;
 }
 printf("SYNC_SPLITS_PASS kind=%d count=%d\n",kind,count);fflush(stdout);
 /* Inactive window, no input: timer must release the retained frame. */
 other=OpenWindowTags(NULL,WA_Left,620,WA_Top,520,WA_Width,140,WA_Height,55,WA_Title,(IPTR)"Timer focus",WA_Activate,TRUE,WA_DragBar,TRUE,TAG_DONE);CHECK(other);Delay(2);
 CHECK(put("\x1b[2J\x1b[HBefore timeout"));ULONG old=pixels(w,u);
 CHECK(put("\x1b[?2026h\rAfter timeout"));CHECK(pixels(w,u)==old);Delay(30);CHECK(pixels(w,u)!=old);CHECK(put("\x1b[?2026$p"));CHECK(reply("\x1b[?2026;2$y"));count++;
 /* A timeout can leave a UTF8 prefix for the next ordinary write. */
 CHECK(put("\x1b%G\x1b[2J\x1b[H\x1b[?2026h\xc3"));Delay(30);CHECK(put("\xa9"));CHECK(u->cu_XCP==1);count++;
 /* Cap release leaves the DCS parser live across the 2MiB boundary. */
 large=AllocMem(2UL*1024*1024+8,MEMF_PUBLIC);CHECK(large);memset(large,'x',2UL*1024*1024+8);large[0]=27;large[1]='P';large[2UL*1024*1024-1]=27;memcpy(large+2UL*1024*1024,"\\OK",3);
 CHECK(put("\x1b[2J\x1b[H\x1b[?2026h"));CHECK(bytes(large,2UL*1024*1024+3));CHECK(u->cu_XCP==2);CHECK(put("\x1b[?2026$p"));CHECK(reply("\x1b[?2026;2$y"));count++;
 FreeMem(large,2UL*1024*1024+8);large=NULL;CloseWindow(other);other=NULL;ActivateWindow(w);
 for(int i=0;i<32;i++){
  CHECK(put("\x1b[?2026hDiscard on close"));CloseDevice((struct IORequest*)io);opened=0;
  io->io_Data=w;io->io_Length=sizeof(*w);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",kind,(struct IORequest*)io,0));opened=1;reader->io_Device=io->io_Device;reader->io_Unit=io->io_Unit;CHECK(put("\x1b[?2026$p"));CHECK(reply("\x1b[?2026;2$y"));count++;
 }
 CHECK(put("\x1b[?25l\x1b[2J\x1b[HDEC2026 synchronized output: PASS\r\n\r\n81 start/end fragmentation combinations\r\nInactive-window timeout without input\r\nUTF8 continuation after timeout\r\n2 MiB limit with split DCS terminator\r\n32 close/reopen cycles with a pending timer\r\n\r\nNative console.device interfaces; no terminal wrapper.\r\n"));
 printf("SYNC_NATIVE_PASS kind=%d count=%d\n",kind,count);fflush(stdout);Delay(500);rc=0;
out:
 if(large)FreeMem(large,2UL*1024*1024+8);
 if(opened)CloseDevice((struct IORequest*)io);
 if(other)CloseWindow(other);
 if(w)CloseWindow(w);
 if(reader)DeleteIORequest((struct IORequest*)reader);
 if(io)DeleteIORequest((struct IORequest*)io);
 if(port)DeleteMsgPort(port);
 if(CyberGfxBase)CloseLibrary(CyberGfxBase);
 printf("SYNC_SCRIPT rc=%d\n",rc);return rc;
}
