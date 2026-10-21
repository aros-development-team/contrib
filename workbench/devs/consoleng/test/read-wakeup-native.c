/* Native read validation plus a paste arriving after CMD_READ is waiting. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/console.h>
#include <exec/errors.h>
#include <stdio.h>
#include <string.h>
#include "../console_gcc.h"
#include "../consoleif.h"
#undef IntuitionBase
struct Device *ConsoleDevice;
static struct IOStdReq *io,*readio;
static char payload[65537],expected[65549],result[65549];
static int put(const char*s){io->io_Command=CMD_WRITE;io->io_Data=(APTR)s;io->io_Length=strlen(s);return !DoIO((struct IORequest*)io);}
static int finish(struct IOStdReq*r){for(int t=0;t<100;t++){if(CheckIO((struct IORequest*)r)){WaitIO((struct IORequest*)r);return 1;}Delay(1);}AbortIO((struct IORequest*)r);WaitIO((struct IORequest*)r);return 0;}
static int zero(struct Unit*u,void*data,ULONG length,int error){readio->io_Unit=u;readio->io_Command=CMD_READ;readio->io_Data=data;readio->io_Length=length;readio->io_Actual=999;readio->io_Error=17;SendIO((struct IORequest*)readio);return finish(readio)&&readio->io_Error==error&&readio->io_Actual==0;}
#define CHECK(c) do{if(!(c)){printf("READ_FAIL line=%d\n",__LINE__);goto out;}}while(0)
int main(void){int rc=20,opened=0;struct Window*w=NULL;struct MsgPort*port=NULL;char canary='!';
 for(int t=0;t<100&&FindPort((CONST_STRPTR)"ConClip.rendezvous");t++)Delay(1);CHECK(!FindPort((CONST_STRPTR)"ConClip.rendezvous"));
 port=CreateMsgPort();CHECK(port);io=(void*)CreateIORequest(port,sizeof(*io));readio=(void*)CreateIORequest(port,sizeof(*readio));CHECK(io&&readio);
 w=OpenWindowTags(NULL,WA_Left,30,WA_Top,35,WA_Width,720,WA_Height,430,WA_Title,(IPTR)"ConsoleNG read validation and paste wakeup",WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_SizeGadget,TRUE,WA_SizeBRight,TRUE,WA_MinWidth,160,WA_MinHeight,80,WA_MaxWidth,-1,WA_MaxHeight,-1,TAG_DONE);CHECK(w);
 io->io_Data=w;io->io_Length=sizeof(*w);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",CONU_SNIPMAP,(struct IORequest*)io,0));opened=1;ConsoleDevice=io->io_Device;readio->io_Device=io->io_Device;struct ConsoleBase*base=(void*)ConsoleDevice;
 CHECK(zero(io->io_Unit,NULL,0,0));CHECK(zero(io->io_Unit,&canary,0,0)&&canary=='!');
 CHECK(zero(NULL,&canary,1,IOERR_BADADDRESS));CHECK(zero((struct Unit*)CONU_LIBRARY,&canary,1,IOERR_BADADDRESS));CHECK(zero(io->io_Unit,NULL,1,IOERR_BADADDRESS));CHECK(zero(NULL,NULL,0,IOERR_BADADDRESS));
 ULONG lengths[]={1,513,65536};
 for(int mode=0;mode<2;mode++)for(int which=0;which<3;which++){
  ULONG n=lengths[which],want=n+(mode?12:0);for(ULONG i=0;i<n;i++)payload[i]='a'+i%26;payload[n]=0;
  if(mode){memcpy(expected,"\x1b[200~",6);memcpy(expected+6,payload,n);memcpy(expected+6+n,"\x1b[201~",6);}else memcpy(expected,payload,n);
  CHECK(put(mode?"\x1b[?2004h":"\x1b[?2004l"));CHECK(SetConSnip(payload));
  readio->io_Unit=io->io_Unit;readio->io_Command=CMD_READ;readio->io_Data=result;readio->io_Length=1;SendIO((struct IORequest*)readio);
  int found=0;for(int t=0;t<100;t++){Delay(1);ObtainSemaphore(&base->consoleTaskLock);struct Node*node;ForeachNode(&base->readRequests,node){if(node==(struct Node*)readio)found=1;}ReleaseSemaphore(&base->consoleTaskLock);if(found)break;}
  if(!found){AbortIO((struct IORequest*)readio);WaitIO((struct IORequest*)readio);CHECK(0);}
  ObtainSemaphore(&base->consoleTaskLock);Console_Paste((Object*)io->io_Unit);ReleaseSemaphore(&base->consoleTaskLock);
  CHECK(finish(readio));CHECK(!readio->io_Error&&readio->io_Actual==1);ULONG used=1;
  CHECK(zero(io->io_Unit,&canary,0,0)&&canary=='!');
  while(used<want){readio->io_Command=CMD_READ;readio->io_Data=result+used;readio->io_Length=want-used;SendIO((struct IORequest*)readio);CHECK(finish(readio));CHECK(!readio->io_Error&&readio->io_Actual&&readio->io_Actual<=want-used);used+=readio->io_Actual;}
  CHECK(!memcmp(result,expected,want));printf("READ_CASE mode=%d payload=%lu bytes=%lu\n",mode,(unsigned long)n,(unsigned long)used);fflush(stdout);
 }
 CHECK(!GetMsg(port));CHECK(put("\x1b]50;topaz,8\x07\x1b[2J\x1b[HPASS: zero and invalid reads complete without queueing.\r\nSix pending reads wake on paste; exact bytes through 64 KiB.\r\n"));puts("READ_PASS");fflush(stdout);Delay(600);rc=0;
out:
 if(opened){SetConSnip(NULL);CloseDevice((struct IORequest*)io);}if(w)CloseWindow(w);if(readio)DeleteIORequest((struct IORequest*)readio);if(io)DeleteIORequest((struct IORequest*)io);if(port)DeleteMsgPort(port);printf("READ_SCRIPT rc=%d\n",rc);return rc;
}
