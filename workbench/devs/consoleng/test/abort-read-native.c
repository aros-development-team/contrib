/* Native cancellation in command and waiting-read queues, with public IO
 * completion checks. Private lock/list inspection selects each queue phase. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <exec/errors.h>
#include <stdio.h>
#include <string.h>
#include "../console_gcc.h"
#undef IntuitionBase
struct Device *ConsoleDevice;
static struct IOStdReq *io;
static int put(const char*s){io->io_Command=CMD_WRITE;io->io_Data=(APTR)s;io->io_Length=strlen(s);return !DoIO((struct IORequest*)io);}
#define CHECK(c) do{if(!(c)){printf("ABORT_FAIL line=%d cycle=%d\n",__LINE__,cycle);goto out;}}while(0)
int main(void){
 int rc=20,opened=0,cycle=-1;struct Window*w=NULL;struct MsgPort*port=NULL;struct IOStdReq*r[4]={0};UBYTE byte[4];int pending[4]={0};
 port=CreateMsgPort();CHECK(port);io=(void*)CreateIORequest(port,sizeof(*io));CHECK(io);
 w=OpenWindowTags(NULL,WA_Left,30,WA_Top,35,WA_Width,720,WA_Height,430,WA_Title,(IPTR)"ConsoleNG read cancellation",WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_SizeGadget,TRUE,WA_SizeBRight,TRUE,WA_MinWidth,160,WA_MinHeight,80,WA_MaxWidth,-1,WA_MaxHeight,-1,TAG_DONE);CHECK(w);
 io->io_Data=w;io->io_Length=sizeof(*w);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",CONU_SNIPMAP,(struct IORequest*)io,0));opened=1;ConsoleDevice=io->io_Device;
 struct ConsoleBase*base=(void*)ConsoleDevice;
 for(int k=0;k<4;k++){r[k]=(void*)CreateIORequest(port,sizeof(*r[k]));CHECK(r[k]);r[k]->io_Device=io->io_Device;r[k]->io_Unit=io->io_Unit;}
 for(cycle=0;cycle<64;cycle++){
  ObtainSemaphore(&base->consoleTaskLock);
  for(int k=0;k<4;k++){r[k]->io_Command=CMD_READ;r[k]->io_Data=&byte[k];r[k]->io_Length=1;r[k]->io_Actual=999;r[k]->io_Error=17;SendIO((struct IORequest*)r[k]);pending[k]=1;}
  LONG a=AbortIO((struct IORequest*)r[1]),b=AbortIO((struct IORequest*)r[3]);
  ReleaseSemaphore(&base->consoleTaskLock);
  CHECK(a==0&&b==0);CHECK(WaitIO((struct IORequest*)r[1])==IOERR_ABORTED);pending[1]=0;CHECK(WaitIO((struct IORequest*)r[3])==IOERR_ABORTED);pending[3]=0;CHECK(r[1]->io_Actual==0&&r[3]->io_Actual==0);
  int found=0;
  for(int t=0;t<100;t++){Delay(1);ObtainSemaphore(&base->consoleTaskLock);struct Node*n;found=0;ForeachNode(&base->readRequests,n){if(n==(struct Node*)r[0]||n==(struct Node*)r[2])found++;}ReleaseSemaphore(&base->consoleTaskLock);if(found==2)break;}
  CHECK(found==2);CHECK(!AbortIO((struct IORequest*)r[0]));CHECK(AbortIO((struct IORequest*)r[0])!=0);CHECK(WaitIO((struct IORequest*)r[0])==IOERR_ABORTED);pending[0]=0;CHECK(r[0]->io_Actual==0);
  CHECK(put("\x1b[6n"));CHECK(!WaitIO((struct IORequest*)r[2]));pending[2]=0;CHECK(r[2]->io_Actual==1&&byte[2]==0x9b);CHECK(AbortIO((struct IORequest*)r[2])!=0&&r[2]->io_Error==0&&r[2]->io_Actual==1);
  UBYTE rest[4];io->io_Command=CMD_READ;io->io_Data=rest;io->io_Length=4;CHECK(!DoIO((struct IORequest*)io)&&io->io_Actual==4&&!memcmp(rest,"1;1R",4));
  CHECK(put(""));CHECK((io->io_Flags&IOF_QUICK)&&AbortIO((struct IORequest*)io)!=0&&io->io_Error==0);CHECK(!GetMsg(port));
 }
 CHECK(put("\x1b]50;topaz,8\x07\x1b[2J\x1b[HPASS: 64 cycles; 192 queued/waiting reads cancelled.\r\nSibling reads, repeated abort and quick completion preserved.\r\n"));puts("ABORT_PASS cycles=64 cancellations=192");fflush(stdout);Delay(600);rc=0;
out:
 for(int k=0;k<4;k++)if(pending[k]){AbortIO((struct IORequest*)r[k]);WaitIO((struct IORequest*)r[k]);}
 if(opened)CloseDevice((struct IORequest*)io);if(w)CloseWindow(w);for(int k=0;k<4;k++)if(r[k])DeleteIORequest((struct IORequest*)r[k]);if(io)DeleteIORequest((struct IORequest*)io);if(port)DeleteMsgPort(port);
 printf("ABORT_SCRIPT rc=%d\n",rc);return rc;
}
