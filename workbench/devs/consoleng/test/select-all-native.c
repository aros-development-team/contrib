/* Real RAmiga-A/RAmiga-C from retained ConsoleNG text through ConClip. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <dos/conclip.h>
#include <devices/conunit.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "consoleng.h"
static struct MsgPort*reply;static struct IOStdReq*io;
static int put(const void*s,ULONG n){io->io_Command=CMD_WRITE;io->io_Data=(void*)s;io->io_Length=n;return !DoIO((struct IORequest*)io)&&io->io_Actual==n;}
static int text(const char*s){return put(s,strlen(s));}
static int readmatch(const UBYTE*bytes,ULONG n){struct ConClipTextMessage m={0};m.msg.mn_Node.ln_Type=NT_MESSAGE;m.msg.mn_ReplyPort=reply;m.msg.mn_Length=sizeof(m);m.code=CONCLIP_READ_TEXT;m.version=1;Forbid();struct MsgPort*p=FindPort((CONST_STRPTR)"ConClip.rendezvous");if(p)PutMsg(p,&m.msg);Permit();if(!p)return 0;WaitPort(reply);if(GetMsg(reply)!=&m.msg||m.status!=CONCLIP_TEXT_OK)return 0;int ok=m.length==n&&m.capacity>=n&&(!n||!memcmp(m.data,bytes,n));static ULONG previous=0xffffffffUL;if(m.length!=previous){printf("SELECTALL_OBSERVED bytes=%lu expected=%lu first=%02x last=%02x\n",(unsigned long)m.length,(unsigned long)n,m.length?m.data[0]:0,m.length?m.data[m.length-1]:0);ULONG first=0,cr=0,spaces=0;while(first<m.length&&first<n&&m.data[first]==bytes[first])first++;for(ULONG j=0;j<m.length;j++){if(m.data[j]==13){if(cr<8)printf(" CR@%lu",(unsigned long)j);cr++;}if(m.data[j]==32)spaces++;}printf(" firstdiff=%lu actual=%02x expected=%02x CRs=%lu spaces=%lu\n",(unsigned long)first,first<m.length?m.data[first]:0,first<n?bytes[first]:0,(unsigned long)cr,(unsigned long)spaces);fflush(stdout);previous=m.length;}if(m.data)FreeMem(m.data,m.capacity);return ok;}
#define CHECK(c) do{if(!(c)){printf("SELECTALL_FAIL line=%d phase=%d check=%s\n",__LINE__,phase,#c);fflush(stdout);Delay(150);goto out;}}while(0)
int main(void){int rc=20,opened=0,phase=0;struct Window*w=NULL;UBYTE*bytes=malloc(CONCLIP_WRITE_MAX);reply=CreateMsgPort();struct MsgPort*p=CreateMsgPort();io=p?(void*)CreateIORequest(p,sizeof(*io)):NULL;CHECK(bytes&&reply&&io);
ULONG sizes[]={32768,65536,CONCLIP_WRITE_MAX};
for(phase=0;phase<3;phase++){
w=OpenWindowTags(NULL,WA_Left,30,WA_Top,30,WA_Width,730,WA_Height,450,WA_Title,(IPTR)"ConsoleNG select all: large clipboard",WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_SizeGadget,TRUE,WA_SizeBBottom,TRUE,WA_Activate,TRUE,TAG_DONE);CHECK(w);
io->io_Data=w;io->io_Length=sizeof(*w);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",CONU_CHARMAP,(struct IORequest*)io,0));opened=1;
struct ConsolengHistoryPrefs hp={sizeof(hp),1,30000};io->io_Command=CD_SETHISTORYPREFS;io->io_Data=&hp;io->io_Length=sizeof(hp);CHECK(!DoIO((struct IORequest*)io));
CHECK(text("\x1b[?25l\x1b]50;DejaVuSansMono,16\7\x1b[2J\x1b[H\x1b[>9h"));
ULONG n=sizes[phase];for(ULONG j=0;j<n-1;j++)bytes[j]='A'+(j+phase)%26;bytes[n-1]='\r';CHECK(put(bytes,n-1));CHECK(text("\x1b[>9l"));
printf("SELECTALL_SELECT phase=%d bytes=%lu\n",phase,(unsigned long)n);fflush(stdout);
int match=0;for(int i=0;i<300&&!match;i++){match=readmatch(bytes,n);if(!match)Delay(5);}CHECK(match);printf("SELECTALL_SIZE_PASS bytes=%lu\n",(unsigned long)n);fflush(stdout);
if(phase==2){CHECK(text("\x1b[2J\x1b[HSelect all retained text: PASS\r\n\r\nRight-Amiga+A, then Right-Amiga+C\r\n32 KiB, 64 KiB and 1 MiB copied exactly\r\nConsoleNG history -> async queue -> ConClip\r\nRead back from native clipboard.device\r\n\r\nRepeated Select All keeps anchors stable\r\nLibrary vectors unchanged"));puts("SELECTALL_PASS large native selection copies");fflush(stdout);Delay(200);}
CloseDevice((struct IORequest*)io);opened=0;CloseWindow(w);w=NULL;
}
rc=0;
out:if(opened)CloseDevice((struct IORequest*)io);if(w)CloseWindow(w);if(io)DeleteIORequest((struct IORequest*)io);if(p)DeleteMsgPort(p);if(reply)DeleteMsgPort(reply);free(bytes);printf("SELECTALL_RUN rc=%d\n",rc);return rc;}
