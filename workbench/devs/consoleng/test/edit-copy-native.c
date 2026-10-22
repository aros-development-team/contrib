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
static int readmatch(const UBYTE*bytes,ULONG n){struct ConClipTextMessage m={0};m.msg.mn_Node.ln_Type=NT_MESSAGE;m.msg.mn_ReplyPort=reply;m.msg.mn_Length=sizeof(m);m.code=CONCLIP_READ_TEXT;m.version=1;Forbid();struct MsgPort*p=FindPort((CONST_STRPTR)"ConClip.rendezvous");if(p)PutMsg(p,&m.msg);Permit();if(!p)return 0;WaitPort(reply);if(GetMsg(reply)!=&m.msg||m.status!=CONCLIP_TEXT_OK)return 0;int ok=m.length==n&&m.capacity>=n&&(!n||!memcmp(m.data,bytes,n));static ULONG previous=0xffffffffUL;if(m.length!=previous){printf("EDITCOPY_OBSERVED bytes=%lu expected=%lu first=%02x last=%02x\n",(unsigned long)m.length,(unsigned long)n,m.length?m.data[0]:0,m.length?m.data[m.length-1]:0);ULONG first=0,cr=0,spaces=0;while(first<m.length&&first<n&&m.data[first]==bytes[first])first++;for(ULONG j=0;j<m.length;j++){if(m.data[j]==13){if(cr<8)printf(" CR@%lu",(unsigned long)j);cr++;}if(m.data[j]==32)spaces++;}printf(" firstdiff=%lu actual=%02x expected=%02x CRs=%lu spaces=%lu\n",(unsigned long)first,first<m.length?m.data[first]:0,first<n?bytes[first]:0,(unsigned long)cr,(unsigned long)spaces);fflush(stdout);previous=m.length;}if(m.data)FreeMem(m.data,m.capacity);return ok;}
#define CHECK(c) do{if(!(c)){printf("EDITCOPY_FAIL line=%d phase=%d check=%s\n",__LINE__,phase,#c);fflush(stdout);Delay(150);goto out;}}while(0)
int main(void){int rc=20,opened=0,phase=0;struct Window*w=NULL;UBYTE*bytes=malloc(CONCLIP_WRITE_MAX);reply=CreateMsgPort();struct MsgPort*p=CreateMsgPort();io=p?(void*)CreateIORequest(p,sizeof(*io)):NULL;CHECK(bytes&&reply&&io);
struct {const char *label,*input,*expected;} cases[]={
 {"insert", "A\r\nZ\x1b[2;1H\x1b[L", "A\r\rZ\r"},
 {"delete", "A\r\nDROP\r\nZ\x1b[2;1H\x1b[M", "A\rZ\r"},
 {"scroll-down", "A\r\nZ\x1b[T", "\rA\rZ\r"},
 {"scroll-up", "A\r\nZ\x1b[S", "A\rZ\r"},
 {"typed-spaces", "A  \r\nZ ", "A  \rZ \r"},
 {"blank-line", "A\r\n\r\nZ", "A\r\rZ\r"},
 {"alternate-insert", "\x1b[?1049hA\r\nZ\x1b[2;1H\x1b[L", "A\r\rZ\r"},
 {"region-insert", "TOP\r\nA\r\nZ\r\nBOTTOM\x1b[2;3r\x1b[2;1H\x1b[L", "TOP\r\rA\rBOTTOM\r"}
};
for(phase=0;phase<16;phase++){
w=OpenWindowTags(NULL,WA_Left,30,WA_Top,30,WA_Width,730,WA_Height,450,WA_Title,(IPTR)"ConsoleNG edited-row clipboard",WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_SizeGadget,TRUE,WA_SizeBBottom,TRUE,WA_Activate,TRUE,TAG_DONE);CHECK(w);
io->io_Data=w;io->io_Length=sizeof(*w);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",CONU_CHARMAP,(struct IORequest*)io,0));opened=1;
struct ConsolengHistoryPrefs hp={sizeof(hp),1,30000};io->io_Command=CD_SETHISTORYPREFS;io->io_Data=&hp;io->io_Length=sizeof(hp);CHECK(!DoIO((struct IORequest*)io));
CHECK(text("\x1b[?25l\x1b]50;DejaVuSansMono,16\7\x1b[2J\x1b[H"));
if(phase&1)CHECK(text("\x1b[>9h"));
CHECK(text(cases[phase/2].input));if(phase&1)CHECK(text("\x1b[>9l"));
ULONG n=strlen(cases[phase/2].expected);memcpy(bytes,cases[phase/2].expected,n);
printf("EDITCOPY_SELECT phase=%d bytes=%lu case=%s hidden=%d\n",phase,(unsigned long)n,cases[phase/2].label,phase&1);fflush(stdout);
int match=0;for(int i=0;i<300&&!match;i++){match=readmatch(bytes,n);if(!match)Delay(5);}CHECK(match);printf("EDITCOPY_SIZE_PASS bytes=%lu\n",(unsigned long)n);fflush(stdout);
if(phase==15){CHECK(text("\x1b[2J\x1b[HEdited-row clipboard: PASS\r\n\r\n16 native keyboard copy cases\r\nInsert/delete rows, scroll up/down, regions\r\nBlank lines and typed spaces preserved\r\nGenerated padding omitted\r\nVisible, hidden and alternate screens\r\n\r\nRight-Amiga+A then Right-Amiga+C"));puts("EDITCOPY_PASS cases=16 edited-row selections");fflush(stdout);Delay(200);}
CloseDevice((struct IORequest*)io);opened=0;CloseWindow(w);w=NULL;
}
rc=0;
out:if(opened)CloseDevice((struct IORequest*)io);if(w)CloseWindow(w);if(io)DeleteIORequest((struct IORequest*)io);if(p)DeleteMsgPort(p);if(reply)DeleteMsgPort(reply);free(bytes);printf("EDITCOPY_RUN rc=%d\n",rc);return rc;}
