/* Private-structure lifecycle discriminator: close with partially drained queues.
 * Client IO is completed before CloseDevice, as Exec requires. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/console.h>
#include <exec/memory.h>
#include <stdio.h>
#include <string.h>
#include "../console_gcc.h"
#include "../consoleif.h"
struct Device *ConsoleDevice;
static char text[65537];
static struct IOStdReq *io;
static int put(const char *s){io->io_Command=CMD_WRITE;io->io_Data=(APTR)s;io->io_Length=strlen(s);return !DoIO((struct IORequest*)io);}
#define CHECK(c) do{if(!(c)){printf("CLOSE_FAIL line=%d\n",__LINE__);goto out;}}while(0)
int main(void){
 int rc=20,open=0,libopen=0;struct Window *w=NULL;struct MsgPort *port=NULL;struct IOStdReq *lib=NULL;
 for(int i=0;i<100&&FindPort((CONST_STRPTR)"ConClip.rendezvous");i++)Delay(1);
 CHECK(!FindPort((CONST_STRPTR)"ConClip.rendezvous"));
 port=CreateMsgPort();CHECK(port);io=(void*)CreateIORequest(port,sizeof(*io));lib=(void*)CreateIORequest(port,sizeof(*lib));CHECK(io&&lib);
 CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",CONU_LIBRARY,(struct IORequest*)lib,0));libopen=1;ConsoleDevice=lib->io_Device;
 memset(text,'x',sizeof(text)-1);CHECK(SetConSnip(text));
 w=OpenWindowTags(NULL,WA_Left,30,WA_Top,35,WA_Width,720,WA_Height,430,WA_Title,(IPTR)"ConsoleNG pending-paste close stress",WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_SizeGadget,TRUE,WA_SizeBRight,TRUE,WA_MinWidth,160,WA_MinHeight,80,WA_MaxWidth,-1,WA_MaxHeight,-1,WA_Activate,TRUE,TAG_DONE);CHECK(w);
 IPTR baseline=0,after=0;
 for(int n=-4;n<128;n++){
  io->io_Data=w;io->io_Length=sizeof(*w);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",CONU_SNIPMAP,(struct IORequest*)io,0));open=1;
  CHECK(put("\x1b[?2004h"));
  if(n>=0){
   struct ConsoleBase *base=(void*)ConsoleDevice;
   ObtainSemaphore(&base->consoleTaskLock);
   for(int k=0;k<3;k++)Console_Paste((Object*)io->io_Unit);
   ReleaseSemaphore(&base->consoleTaskLock);
   UBYTE c;io->io_Command=CMD_READ;io->io_Data=&c;io->io_Length=1;CHECK(!DoIO((struct IORequest*)io)&&io->io_Actual==1&&c==27);
   ObtainSemaphore(&base->consoleTaskLock);
   struct intConUnit *u=ICU(io->io_Unit);ULONG nodes=0,bytes=0;struct intPasteData *p;
   ForeachNode(&u->pasteData,p){nodes++;bytes+=p->pasteBufferSize;}
   ULONG partial=u->pasteBufferPos;
   ReleaseSemaphore(&base->consoleTaskLock);
   CHECK(nodes==3&&bytes==3*(65536+12)&&partial>0&&partial<65536);
  }
  CloseDevice((struct IORequest*)io);open=0;
  if(n==-1){Delay(10);baseline=AvailMem(MEMF_ANY);printf("CLOSE_BASELINE %lu\n",(unsigned long)baseline);}
  if(n>=0&&n%16==15){printf("CLOSE_PROGRESS cycles=%d\n",n+1);fflush(stdout);}
 }
 Delay(10);after=AvailMem(MEMF_ANY);
 printf("CLOSE_MEMORY before=%lu after=%lu delta=%ld\n",(unsigned long)baseline,(unsigned long)after,(long)(baseline-after));fflush(stdout);
 /* Detect the expected ~24 MiB orphaned-queue leak, not a leak-free claim. */
 CHECK((SIPTR)baseline-(SIPTR)after<1024*1024);
 io->io_Data=w;io->io_Length=sizeof(*w);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",CONU_SNIPMAP,(struct IORequest*)io,0));open=1;
 CHECK(put("\x1b%G\x1b]50;topaz,8\x07\x1b[2J\x1b[HPASS: 128 closes with three queued 64 KiB pastes each.\r\nPartially consumed queues released; native memory gate passes.\r\n"));
 puts("CLOSE_PASS");fflush(stdout);Delay(600);rc=0;
out:
 if(open)CloseDevice((struct IORequest*)io);
 if(libopen){SetConSnip(NULL);CloseDevice((struct IORequest*)lib);}
 if(w)CloseWindow(w);if(lib)DeleteIORequest((struct IORequest*)lib);if(io)DeleteIORequest((struct IORequest*)io);if(port)DeleteMsgPort(port);
 printf("CLOSE_SCRIPT rc=%d\n",rc);return rc;
}
