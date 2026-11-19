/* Actual terminal OSC52 -> ConsoleNG -> ConClip -> clipboard.device. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/console.h>
#include <devices/conunit.h>
#include <dos/conclip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct Device *ConsoleDevice;
static struct IOStdReq *io;
static struct MsgPort *reply;
static int put(const void *s,ULONG n){io->io_Command=CMD_WRITE;io->io_Data=(void*)s;io->io_Length=n;return !DoIO((struct IORequest*)io)&&io->io_Actual==n;}
static int text(const char *s){return put(s,strlen(s));}
static int readmatch(const UBYTE *bytes,ULONG n){
 struct ConClipTextMessage m={0};m.msg.mn_Node.ln_Type=NT_MESSAGE;m.msg.mn_ReplyPort=reply;m.msg.mn_Length=sizeof(m);m.code=CONCLIP_READ_TEXT;m.version=1;
 Forbid();struct MsgPort*p=FindPort((CONST_STRPTR)"ConClip.rendezvous");if(p)PutMsg(p,&m.msg);Permit();if(!p)return 0;
 WaitPort(reply);if(GetMsg(reply)!=&m.msg||m.status!=CONCLIP_TEXT_OK)return 0;
 int ok=m.length==n&&m.capacity>=n&&(!n||!memcmp(m.data,bytes,n));if(m.data)FreeMem(m.data,m.capacity);return ok;
}
static int awaitcopy(const UBYTE*p,ULONG n){for(int i=0;i<100;i++){if(readmatch(p,n))return 1;Delay(2);}return 0;}
static int sendcopy(UBYTE *data,ULONG n,int fragment,int st){
 static const char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
 ULONG size=(n+2)/3*4;UBYTE*encoded=malloc(size+1);if(!encoded)return 0;ULONG out=0;
 for(ULONG i=0;i<n;i+=3){ULONG v=(ULONG)data[i]<<16;if(i+1<n)v|=(ULONG)data[i+1]<<8;if(i+2<n)v|=data[i+2];encoded[out++]=alphabet[v>>18];encoded[out++]=alphabet[(v>>12)&63];encoded[out++]=i+1<n?alphabet[(v>>6)&63]:'=';encoded[out++]=i+2<n?alphabet[v&63]:'=';}
 int ok=text("\x1b]52;c;");for(ULONG i=0;i<size&&ok;){ULONG chunk=fragment?1:4093;if(chunk>size-i)chunk=size-i;ok=put(encoded+i,chunk);i+=chunk;}
 if(ok)ok=text(st?"\x1b\\":"\7");
 free(encoded);return ok;
}
#define CHECK(c) do{if(!(c)){printf("OSC52_FAIL line=%d case=%d kind=%d check=%s\n",__LINE__,cases,kind,#c);fflush(stdout);Delay(150);goto out;}}while(0)
int main(void){
 int rc=20,opened=0,cases=0,kind=0;struct Window*w=NULL;char *before=NULL,*snip=NULL;UBYTE*data=malloc(CONCLIP_WRITE_MAX);
 struct MsgPort*p=CreateMsgPort();reply=CreateMsgPort();io=p?(void*)CreateIORequest(p,sizeof(*io)):NULL;CHECK(data&&io&&reply);
 ULONG sizes[]={1,2,3,32768,65536,1048576,0};
 for(kind=0;kind<2;kind++){
  w=OpenWindowTags(NULL,WA_Left,30,WA_Top,30,WA_Width,730,WA_Height,450,WA_Title,(IPTR)"ConsoleNG terminal clipboard",WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_SizeGadget,TRUE,WA_SizeBBottom,TRUE,TAG_DONE);CHECK(w);
  io->io_Data=w;io->io_Length=sizeof(*w);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",kind?CONU_CHARMAP:CONU_STANDARD,(struct IORequest*)io,0));opened=1;ConsoleDevice=io->io_Device;before=GetConSnip();
  CHECK(text("\x1b%G\x1b[?25l\x1b]50;DejaVuSansMono,16\7"));
  for(unsigned i=0;i<sizeof(sizes)/sizeof(*sizes);i++){
   ULONG n=sizes[i];for(ULONG j=0;j<n;j++)data[j]='A'+(j+i+kind)%26;
   if(kind){CHECK(text("\x1b[>9h"));}
   CHECK(sendcopy(data,n,n<4,i&1));if(kind){CHECK(text("\x1b[>9l"));}
   CHECK(awaitcopy(data,n));
   printf("OSC52_SIZE_PASS kind=%d bytes=%lu\n",kind,(unsigned long)n);fflush(stdout);cases++;
  }
  CHECK(text("\x1b]52;;zqnwn5iA\7"));CHECK(awaitcopy((UBYTE*)"\xce\xa9\xf0\x9f\x98\x80",6));cases++;
  const char *bad[]={"\x1b]52;c;?\7","\x1b]52;c;QR==\7","\x1b]52;c;wIA=\7","\x1b]52;c;7aCA\7","\x1b]52;c;QQ\7","\x1b]52;x;QQ==\7"};
  for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);i++){CHECK(text(bad[i]));Delay(2);CHECK(readmatch((UBYTE*)"\xce\xa9\xf0\x9f\x98\x80",6));cases++;}
  CHECK(text("\x1b]52;p;c2VsZWN0aW9u\7"));snip=GetConSnip();CHECK(snip&&!strcmp(snip,"selection"));FreeVec(snip);snip=NULL;CHECK(readmatch((UBYTE*)"\xce\xa9\xf0\x9f\x98\x80",6));cases++;
  CHECK(text("\x1b]52;s;\7"));snip=GetConSnip();CHECK(!snip);cases++;
  CHECK(text("\x1b[2J\x1b[HOSC 52 terminal clipboard: PASS\r\n\r\nApplication copy through native ConClip\r\nEmpty, short and 1 MiB copies; BEL and ST\r\nUTF-8; bytewise and split payloads\r\nInvalid data leaves clipboard unchanged\r\nCopy-only: clipboard queries denied\r\nLocal selection remains separate\r\nStandard and hidden retained consoles"));
  printf("OSC52_UNIT_PASS kind=%d cases=%d\n",kind,cases);fflush(stdout);Delay(200);
  SetConSnip(before);if(before)FreeVec(before);before=NULL;CloseDevice((struct IORequest*)io);opened=0;CloseWindow(w);w=NULL;
 }
 puts("OSC52_PASS");rc=0;
 out:if(snip)FreeVec(snip);if(opened){SetConSnip(before);if(before)FreeVec(before);CloseDevice((struct IORequest*)io);}if(w)CloseWindow(w);if(io)DeleteIORequest((struct IORequest*)io);if(reply)DeleteMsgPort(reply);if(p)DeleteMsgPort(p);free(data);printf("OSC52_RUN rc=%d cases=%d\n",rc,cases);return rc;
}
