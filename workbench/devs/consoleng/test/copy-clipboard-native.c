/* Private-guest only: temporarily substitute a stopped ConClip provider, restore it. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/iffparse.h>
#include <intuition/sghooks.h>
#include <devices/conunit.h>
#include <dos/conclip.h>
#include <stdio.h>
#include <string.h>
struct Library *IFFParseBase;
struct CopyMessage {struct Message msg;struct SGWork *sgw;WORD code;};
static struct Window*w;static struct IOStdReq*io;
static int put(const char*s){io->io_Command=CMD_WRITE;io->io_Data=(void*)s;io->io_Length=strlen(s);return !DoIO((struct IORequest*)io);}
static int clipboard_matches(const char*expected){int match=0;struct IFFHandle*h=AllocIFF();if(!h)return 0;h->iff_Stream=(IPTR)OpenClipboard(0);if(h->iff_Stream){InitIFFasClip(h);if(!OpenIFF(h,IFFF_READ)){StopChunk(h,MAKE_ID('F','T','X','T'),MAKE_ID('C','H','R','S'));if(!ParseIFF(h,IFFPARSE_SCAN)){struct ContextNode*c=CurrentChunk(h);char text[80]={0};if(c&&c->cn_Size<(LONG)sizeof(text)&&c->cn_Size==(LONG)strlen(expected)&&ReadChunkBytes(h,text,c->cn_Size)==c->cn_Size)match=!strcmp(text,expected);}CloseIFF(h);}CloseClipboard((void*)h->iff_Stream);}FreeIFF(h);return match;}
static void select_marker(int phase,int length){struct ConUnit*u=(void*)io->io_Unit;int x=w->LeftEdge+u->cu_XROrigin,y=w->TopEdge+u->cu_YROrigin;printf("COPY_SELECT phase=%d x0=%d y0=%d x1=%d y1=%d\n",phase,x+1,y+u->cu_YRSize/2,x+length*u->cu_XRSize+1,y+u->cu_YRSize/2);fflush(stdout);}
#define CHECK(c) do{if(!(c)){printf("COPY_FAIL line=%d check=%s\n",__LINE__,#c);fflush(stdout);goto out;}}while(0)
int main(void){int rc=20,opened=0,substituted=0;struct MsgPort*original=NULL,*fake=CreateMsgPort(),*p=CreateMsgPort();struct CopyMessage*held=NULL;io=p?(void*)CreateIORequest(p,sizeof(*io)):NULL;CHECK(fake&&io);
IFFParseBase=OpenLibrary((CONST_STRPTR)"iffparse.library",0);CHECK(IFFParseBase);
fake->mp_Node.ln_Name="ConClip.rendezvous";
Forbid();original=FindPort((CONST_STRPTR)fake->mp_Node.ln_Name);if(original)RemPort(original);AddPort(fake);Permit();substituted=1;
for(int phase=0;phase<2;phase++){
w=OpenWindowTags(NULL,WA_Left,30,WA_Top,30,WA_Width,730,WA_Height,450,WA_Title,(IPTR)"ConsoleNG asynchronous selection copy",WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_SizeGadget,TRUE,WA_SizeBBottom,TRUE,WA_Activate,TRUE,TAG_DONE);CHECK(w);
io->io_Data=w;io->io_Length=sizeof(*w);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",CONU_CHARMAP,(struct IORequest*)io,0));opened=1;
CHECK(put("\x1b[?25l\x1b]50;DejaVuSansMono,16\7\x1b[2J\x1b[H"));CHECK(put(phase?"REAL COPY":"ASYNC COPY"));select_marker(phase,phase?9:10);
if(!phase){for(int i=0;i<1500&&!held;i++){held=(void*)GetMsg(fake);if(!held)Delay(1);}CHECK(held);
if(held->code==CONCLIP_WRITE_TEXT){ReplyMsg(&held->msg);held=NULL;for(int i=0;i<500&&!held;i++){held=(void*)GetMsg(fake);if(!held)Delay(1);}CHECK(held);}
printf("COPY_RECEIVED code=%d length=%d\n",held->code,held->sgw->NumChars);fflush(stdout);CHECK(held->code=='C'&&held->sgw->NumChars==10&&!memcmp(held->sgw->WorkBuffer,"ASYNC COPY",10));
CHECK(put("\r\nConsole responds while ConClip withholds reply."));puts("COPY_RESPONSIVE_PASS");fflush(stdout);
CloseDevice((struct IORequest*)io);opened=0;CloseWindow(w);w=NULL;
CHECK(!memcmp(held->sgw->WorkBuffer,"ASYNC COPY",10));ReplyMsg(&held->msg);held=NULL;Delay(10);
Forbid();RemPort(fake);if(original)AddPort(original);Permit();substituted=0;CHECK(original);
}else{int match=0;for(int i=0;i<300&&!match;i++){match=clipboard_matches("REAL COPY");if(!match)Delay(5);}CHECK(match);
CHECK(put("\r\n\r\nPASS: selection copy does not block the console\r\nDelayed reply survives closing its source window\r\nActual ConClip writes the expected FTXT/CHRS bytes\r\n\r\nHost: queue bounds, allocation failures, port race\r\nOptional large-copy protocol; safe legacy fallback"));puts("COPY_PASS delayed reply and real clipboard");fflush(stdout);Delay(200);CloseDevice((struct IORequest*)io);opened=0;CloseWindow(w);w=NULL;
}}
rc=0;
out:if(held)ReplyMsg(&held->msg);if(substituted){Forbid();RemPort(fake);if(original)AddPort(original);Permit();}if(opened)CloseDevice((struct IORequest*)io);if(w)CloseWindow(w);if(io)DeleteIORequest((struct IORequest*)io);if(p)DeleteMsgPort(p);if(fake)DeleteMsgPort(fake);if(IFFParseBase)CloseLibrary(IFFParseBase);printf("COPY_RUN rc=%d\n",rc);return rc;}
