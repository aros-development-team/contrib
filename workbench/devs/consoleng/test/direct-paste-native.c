/* Actual native clipboard fallback without ConClip or a DOS handler. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/console.h>
#include <devices/conunit.h>
#include <stdio.h>
#include <string.h>
struct Device *ConsoleDevice;
static struct IOStdReq *io,*reader;
static UBYTE data[65537],expected[65548],received[65548];
static int put(const char *s){io->io_Command=CMD_WRITE;io->io_Data=(APTR)s;io->io_Length=strlen(s);return !DoIO((struct IORequest*)io)&&io->io_Actual==strlen(s);}
static LONG get(UBYTE *out,ULONG length){
 reader->io_Command=CMD_READ;reader->io_Data=out;reader->io_Length=length;SendIO((struct IORequest*)reader);
 for(int i=0;i<500&&!CheckIO((struct IORequest*)reader);i++)Delay(1);
 if(!CheckIO((struct IORequest*)reader)){AbortIO((struct IORequest*)reader);WaitIO((struct IORequest*)reader);return -1;}
 if(WaitIO((struct IORequest*)reader))return -1;return reader->io_Actual;
}
#define CHECK(c) do{if(!(c)){printf("DIRECT_FAIL line=%d\n",__LINE__);goto out;}}while(0)
int main(void){
 int rc=20,opened=0;struct Window *window=NULL;struct MsgPort *port=NULL;
 for(int i=0;i<100&&FindPort((CONST_STRPTR)"ConClip.rendezvous");i++)Delay(1);
 CHECK(!FindPort((CONST_STRPTR)"ConClip.rendezvous"));
 window=OpenWindowTags(NULL,WA_Left,30,WA_Top,35,WA_Width,720,WA_Height,430,
  WA_Title,(IPTR)"ConsoleNG direct clipboard acceptance",WA_DragBar,TRUE,WA_DepthGadget,TRUE,
  WA_SizeGadget,TRUE,WA_SizeBRight,TRUE,WA_MinWidth,160,WA_MinHeight,80,WA_MaxWidth,-1,WA_MaxHeight,-1,WA_Activate,TRUE,TAG_DONE);CHECK(window);
 port=CreateMsgPort();CHECK(port);io=(void*)CreateIORequest(port,sizeof(*io));reader=(void*)CreateIORequest(port,sizeof(*reader));CHECK(io&&reader);
 io->io_Data=window;io->io_Length=sizeof(*window);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",CONU_SNIPMAP,(struct IORequest*)io,0));opened=1;
 ConsoleDevice=(void*)io->io_Device;reader->io_Device=io->io_Device;reader->io_Unit=io->io_Unit;
 CHECK(put("\x1b%G\x1b]50;topaz,8\x07\x1b[2J\x1b[HNative direct clipboard acceptance\r\n"));
 ULONG sizes[]={1,4096,65536};
 for(int mode=0;mode<2;mode++)for(int test=0;test<3;test++){
  ULONG n=sizes[test],length=0,pos=0;
  CHECK(put(mode?"\x1b[?2004h":"\x1b[?2004l"));
  for(ULONG i=0;i<n;i++)data[i]=(i%79==78)?'\n':'a'+i%26;data[n]=0;
  if(n>200){memcpy(data+100,"\x1b[201~\x03",7);memcpy(data+150,"\x9b" "0 v",4);}
  CHECK(SetConSnip(data));
  if(mode){memcpy(expected,"\x1b[200~",6);length=6;}
  for(ULONG i=0;i<n;i++)if(!mode||(data[i]!=27&&data[i]!=3))expected[length++]=data[i];
  if(mode){memcpy(expected+length,"\x1b[201~",6);length+=6;}
  printf("DIRECT_READY mode=%d test=%d bytes=%lu\n",mode,test,(unsigned long)n);fflush(stdout);
  while(pos<length){ULONG want=length-pos;if(want>257)want=257;LONG count=get(received+pos,want);CHECK(count>0);pos+=count;}
  CHECK(!memcmp(received,expected,length));printf("DIRECT_CASE_PASS mode=%d test=%d received=%lu\n",mode,test,(unsigned long)length);fflush(stdout);
 }
 CHECK(put("\x1b[H\x1b[6n"));LONG reply=get(received,6);printf("DIRECT_REPLY bytes=%ld",(long)reply);for(LONG n=0;n<reply;n++)printf(" %02x",received[n]);puts("");
 CHECK(reply==6&&!memcmp(received,"\x1b[1;1R",6));puts("DIRECT_REPLY_UNFRAMED");
 SetConSnip(NULL);SetConSnip(NULL);CHECK(!GetConSnip());puts("DIRECT_CLEAR_PASS");
 CHECK(put("\x1b[?2004l\x1b[2J\x1b[HPASS: direct clipboard plain/bracketed, up to 65536 bytes.\r\nTerminal replies stay unframed; repeated clear passes.\r\n"));
 puts("DIRECT_PASS");fflush(stdout);Delay(600);rc=0;
out:
 if(opened){SetConSnip(NULL);CloseDevice((struct IORequest*)io);}
 if(reader)DeleteIORequest((struct IORequest*)reader);if(io)DeleteIORequest((struct IORequest*)io);if(port)DeleteMsgPort(port);if(window)CloseWindow(window);
 printf("DIRECT_SCRIPT rc=%d\n",rc);return rc;
}
