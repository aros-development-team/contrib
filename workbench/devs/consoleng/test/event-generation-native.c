/* Private queue discriminator: simulate allocator address reuse by targeting
 * the current address with a previous lifetime's generation. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <exec/memory.h>
#include <stdio.h>
#include <string.h>
#include "../console_gcc.h"
static struct IOStdReq *io;
struct Device *ConsoleDevice;
static int put(const char *s){io->io_Command=CMD_WRITE;io->io_Data=(APTR)s;io->io_Length=strlen(s);return !DoIO((struct IORequest*)io);}
#define CHECK(c) do{if(!(c)){printf("GEN_FAIL line=%d\n",__LINE__);goto out;}}while(0)
int main(void){
 int rc=20,open=0,libopen=0;struct Window *w=NULL;struct MsgPort *port=NULL;struct IOStdReq *lib=NULL;
 port=CreateMsgPort();CHECK(port);io=(void*)CreateIORequest(port,sizeof(*io));lib=(void*)CreateIORequest(port,sizeof(*lib));CHECK(io&&lib);
 CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",CONU_LIBRARY,(struct IORequest*)lib,0));libopen=1;ConsoleDevice=lib->io_Device;
 struct ConsoleBase *base=(void*)lib->io_Device;UQUAD previous=0;
 w=OpenWindowTags(NULL,WA_Left,30,WA_Top,35,WA_Width,720,WA_Height,430,WA_Title,(IPTR)"ConsoleNG queued input lifetime",WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_SizeGadget,TRUE,WA_SizeBRight,TRUE,WA_MinWidth,160,WA_MinHeight,80,WA_MaxWidth,-1,WA_MaxHeight,-1,TAG_DONE);CHECK(w);
 for(int n=0;n<65;n++){
  io->io_Data=w;io->io_Length=sizeof(*w);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",CONU_SNIPMAP,(struct IORequest*)io,0));open=1;
  UQUAD current=ICU(io->io_Unit)->inputGeneration;CHECK(current&&current!=previous);
  if(n){
   CHECK(put("\x1b[?1004h"));
   struct cdihMessage *stale=AllocMem(sizeof(*stale),MEMF_PUBLIC|MEMF_CLEAR),*valid=AllocMem(sizeof(*valid),MEMF_PUBLIC|MEMF_CLEAR);
   if(!stale||!valid){if(stale)FreeMem(stale,sizeof(*stale));if(valid)FreeMem(valid,sizeof(*valid));CHECK(0);}
   stale->msg.mn_Length=sizeof(*stale);stale->unit=(Object*)io->io_Unit;stale->unitGeneration=previous;stale->ie.ie_Class=IECLASS_INACTIVEWINDOW;
   valid->msg.mn_Length=sizeof(*valid);valid->unit=(Object*)io->io_Unit;valid->unitGeneration=current;valid->ie.ie_Class=IECLASS_ACTIVEWINDOW;
   ObtainSemaphore(&base->consoleTaskLock);
   PutMsg(base->consIHData.inputPort,(struct Message*)stale);PutMsg(base->consIHData.inputPort,(struct Message*)valid);
   ReleaseSemaphore(&base->consoleTaskLock);
   ULONG count=0;
   for(int wait=0;wait<100;wait++){Delay(1);ObtainSemaphore(&base->consoleTaskLock);count=ICU(io->io_Unit)->numStoredChars;ReleaseSemaphore(&base->consoleTaskLock);if(count)break;}
   CHECK(count==3);UBYTE bytes[3];io->io_Command=CMD_READ;io->io_Data=bytes;io->io_Length=3;CHECK(!DoIO((struct IORequest*)io)&&io->io_Actual==3&&!memcmp(bytes,"\x1b[I",3));
   CHECK(put("\x1b[?1004l"));
  }
  previous=current;
  if(n<64){CloseDevice((struct IORequest*)io);open=0;}
 }
 CHECK(put("\x1b]50;topaz,8\x07\x1b[2J\x1b[HPASS: 64 console lifetimes; stale queued events rejected.\r\nCurrent-generation focus reports delivered exactly.\r\n"));
 puts("GEN_PASS cycles=64");fflush(stdout);Delay(600);rc=0;
out:
 if(open)CloseDevice((struct IORequest*)io);if(libopen)CloseDevice((struct IORequest*)lib);
 if(w)CloseWindow(w);if(io)DeleteIORequest((struct IORequest*)io);if(lib)DeleteIORequest((struct IORequest*)lib);if(port)DeleteMsgPort(port);
 printf("GEN_SCRIPT rc=%d\n",rc);return rc;
}
