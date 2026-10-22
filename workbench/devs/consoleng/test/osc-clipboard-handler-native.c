/* Native DOS Write -> aux/con-handler -> ConsoleNG -> ConClip/clipboard. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/console.h>
#include <devices/conunit.h>
#include <dos/conclip.h>
#include <dos/dosextens.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct Device *ConsoleDevice;
static struct IOStdReq *io;
static BPTR stream;
static struct MsgPort *reply;
static int put(const void *s,ULONG n){return Write(stream,s,n)==(LONG)n;}
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
#define CHECK(c) do{if(!(c)){printf("OSCDOS_FAIL line=%d case=%d kind=%d check=%s\n",__LINE__,cases,kind,#c);fflush(stdout);Delay(150);goto out;}}while(0)
int main(void){
 int rc=20,cases=0,kind=0,saved=0;struct Window*w=NULL;char *before=NULL,*snip=NULL;UBYTE*data=malloc(CONCLIP_WRITE_MAX);
 reply=CreateMsgPort();CHECK(data&&reply);
 ULONG sizes[]={1,2,3,32768,65536,1048576,0};
 for(kind=0;kind<4;kind++){
  struct InfoData info;stream=Open((CONST_STRPTR)"RGCON:",MODE_OLDFILE);CHECK(stream);
  struct FileHandle*fh=BADDR(stream);CHECK(DoPkt(fh->fh_Type,ACTION_DISK_INFO,(SIPTR)MKBADDR(&info),0,0,0,0));
  io=(void*)(IPTR)info.id_InUse;CHECK(io&&io->io_Unit&&io->io_Device);
  CHECK(!strcmp((char*)io->io_Device->dd_Library.lib_Node.ln_Name,"consoleng.device"));
  struct ConUnit*u=(void*)io->io_Unit;w=u->cu_Window;CHECK(w);ConsoleDevice=io->io_Device;before=GetConSnip();saved=1;
  SetWindowTitles(w,(CONST_STRPTR)"ConsoleNG DOS terminal clipboard",(CONST_STRPTR)-1);
  MoveWindow(w,30-w->LeftEdge,30-w->TopEdge);SizeWindow(w,730-w->Width,450-w->Height);Delay(20);
  CHECK(SetMode(stream,kind&1));CHECK(text("\7"));
  if(kind){CHECK(readmatch((UBYTE*)"\xce\xa9\xf0\x9f\x98\x80",6));}
  CHECK(text("\x1b%G\x1b[?25l\x1b]50;DejaVuSansMono,16\7"));
  for(unsigned i=0;i<sizeof(sizes)/sizeof(*sizes);i++){
   ULONG n=sizes[i];for(ULONG j=0;j<n;j++)data[j]='A'+(j+i+kind)%26;
   if(kind&2){CHECK(text("\x1b[>9h"));}
   CHECK(sendcopy(data,n,n<4,i&1));if(kind&2){CHECK(text("\x1b[>9l"));}
   CHECK(awaitcopy(data,n));
   printf("OSCDOS_SIZE_PASS kind=%d bytes=%lu\n",kind,(unsigned long)n);fflush(stdout);cases++;
  }
  CHECK(text("\x1b]52;;zqnwn5iA\7"));CHECK(awaitcopy((UBYTE*)"\xce\xa9\xf0\x9f\x98\x80",6));cases++;
  const char *bad[]={"\x1b]52;c;?\7","\x1b]52;c;QR==\7","\x1b]52;c;wIA=\7","\x1b]52;c;7aCA\7","\x1b]52;c;QQ\7","\x1b]52;x;QQ==\7"};
  for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);i++){CHECK(text(bad[i]));Delay(2);CHECK(readmatch((UBYTE*)"\xce\xa9\xf0\x9f\x98\x80",6));cases++;}
  CHECK(text("\x1b]52;p;c2VsZWN0aW9u\7"));snip=GetConSnip();CHECK(snip&&!strcmp(snip,"selection"));FreeVec(snip);snip=NULL;CHECK(readmatch((UBYTE*)"\xce\xa9\xf0\x9f\x98\x80",6));cases++;
  CHECK(text("\x1b]52;s;\7"));snip=GetConSnip();CHECK(!snip);cases++;
  CHECK(SetMode(stream,TRUE));CHECK(text("\x1b[H\x1b]52;c;?\7"));CHECK(!WaitForChar(stream,250000));cases++;
  CHECK(text("\x1b[6n"));const char*expected="\x1b[1;1R";char got[8]={0};
  for(unsigned j=0;j<strlen(expected);j++){CHECK(WaitForChar(stream,1000000));CHECK(Read(stream,got+j,1)==1);}
  CHECK(!memcmp(got,expected,strlen(expected)));CHECK(!WaitForChar(stream,10000));CHECK(SetMode(stream,kind&1));cases++;
  CHECK(text("\x1b[2J\x1b[HDOS terminal clipboard: PASS\r\n\r\nApplication copy through native ConClip\r\nEmpty, short and 1 MiB copies; BEL and ST\r\nUTF-8; bytewise and split payloads\r\nInvalid data leaves clipboard unchanged\r\nCopy-only: clipboard queries denied\r\nLocal selection remains separate\r\nCooked/raw DOS Write; visible/hidden consoles"));
  printf("OSCDOS_UNIT_PASS kind=%d cases=%d\n",kind,cases);fflush(stdout);Delay(200);
  CHECK(text("\x1b]52;c;bm"));
  SetConSnip(before);if(before)FreeVec(before);before=NULL;saved=0;BOOL closed=Close(stream);stream=0;CHECK(closed);io=NULL;w=NULL;
 }
 puts("OSCDOS_PASS");rc=0;
 out:if(snip)FreeVec(snip);if(stream){if(saved){SetConSnip(before);if(before)FreeVec(before);}Close(stream);}if(reply)DeleteMsgPort(reply);free(data);printf("OSCDOS_RUN rc=%d cases=%d\n",rc,cases);return rc;
}
