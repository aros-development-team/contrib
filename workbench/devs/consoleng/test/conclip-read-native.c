/* Real multi-CHRS clipboard snapshots through the private ConClip provider. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/iffparse.h>
#include <proto/intuition.h>
#include <datatypes/textclass.h>
#include <devices/conunit.h>
#include <dos/conclip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct Library *IFFParseBase;
static struct MsgPort *reply;
static int exchange(struct ConClipTextMessage *m)
{
 m->msg.mn_Node.ln_Type=NT_MESSAGE;m->msg.mn_ReplyPort=reply;m->msg.mn_Length=sizeof(*m);
 Forbid();struct MsgPort *p=FindPort((CONST_STRPTR)"ConClip.rendezvous");if(p)PutMsg(p,&m->msg);Permit();
 if(!p)return 0;WaitPort(reply);return GetMsg(reply)==&m->msg;
}
static int write_chunks(const UBYTE *bytes,ULONG chunks,ULONG size)
{
 int ok=0;struct IFFHandle *iff=AllocIFF();if(!iff)return 0;
 iff->iff_Stream=(IPTR)OpenClipboard(0);if(!iff->iff_Stream){FreeIFF(iff);return 0;}
 InitIFFasClip(iff);
 if(!OpenIFF(iff,IFFF_WRITE)){
  if(!PushChunk(iff,ID_FTXT,ID_FORM,IFFSIZE_UNKNOWN)){
   ok=1;
   for(ULONG i=0;i<chunks&&ok;i++){
    if(PushChunk(iff,0,ID_CHRS,IFFSIZE_UNKNOWN)){ok=0;break;}
    if(size&&WriteChunkBytes(iff,(APTR)(bytes+i*size),size)!=(LONG)size)ok=0;
    if(PopChunk(iff))ok=0;
   }
   if(PopChunk(iff))ok=0;
  }
  CloseIFF(iff);
 }
 CloseClipboard((struct ClipboardHandle*)iff->iff_Stream);FreeIFF(iff);return ok;
}
#define CHECK(c) do{if(!(c)){printf("CLIPREAD_FAIL line=%d\n",__LINE__);fflush(stdout);goto out;}}while(0)
int main(void)
{
 int rc=20,cases=0,opened=0;UBYTE *bytes=NULL;struct Window *w=NULL;struct IOStdReq *io=NULL;struct MsgPort *port=NULL;
 IFFParseBase=OpenLibrary((CONST_STRPTR)"iffparse.library",0);CHECK(IFFParseBase);reply=CreateMsgPort();CHECK(reply);
 bytes=malloc(1024*1024);CHECK(bytes);for(ULONG i=0;i<1024*1024;i++)bytes[i]=(UBYTE)(i*37+11);
 const ULONG sizes[]={0,1,17,1024,32768};const ULONG chunks[]={1,2,3,7,32};
 for(unsigned a=0;a<5;a++)for(unsigned b=0;b<5;b++){
  ULONG n=sizes[a]*chunks[b];CHECK(write_chunks(bytes,chunks[b],sizes[a]));
  struct ConClipTextMessage m={0};m.code=CONCLIP_READ_TEXT;m.version=1;CHECK(exchange(&m));
  int good=m.status==CONCLIP_TEXT_OK&&m.data&&m.length==n&&m.capacity>=n+12&&(!n||!memcmp(bytes,m.data,n));
  if(m.status==CONCLIP_TEXT_OK&&m.data){if(good)memset(m.data+n,0,12);FreeMem(m.data,m.capacity);}CHECK(good);
  printf("CLIPREAD_CASE chunks=%lu size=%lu PASS\n",(unsigned long)chunks[b],(unsigned long)sizes[a]);fflush(stdout);cases++;
 }
 CHECK(write_chunks(bytes,0,0));struct ConClipTextMessage absent={0};absent.code=CONCLIP_READ_TEXT;absent.version=1;
 CHECK(exchange(&absent)&&absent.status==CONCLIP_TEXT_ERROR&&!absent.data&&!absent.length&&!absent.capacity);cases++;
 port=CreateMsgPort();CHECK(port);io=(void*)CreateIORequest(port,sizeof(*io));CHECK(io);
 w=OpenWindowTags(NULL,WA_Left,30,WA_Top,30,WA_Width,730,WA_Height,470,WA_Title,(IPTR)"ConsoleNG native clipboard snapshot",WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_SizeGadget,TRUE,WA_SizeBRight,TRUE,WA_MinWidth,300,WA_MinHeight,200,WA_MaxWidth,-1,WA_MaxHeight,-1,TAG_DONE);CHECK(w);
 io->io_Data=w;io->io_Length=sizeof(*w);CHECK(!OpenDevice((CONST_STRPTR)"COPYLRG:Devs/consoleng.device",CONU_CHARMAP,(struct IORequest*)io,0));opened=1;
 const char *screen="\x1b%G\x1b[?25l\x1b]50;DejaVuSansMono.font,20\7\x1b[0;38;2;230;240;245;48;2;25;60;80m\x1b[2J\x1b[HNative clipboard snapshots\r\n\r\n26 multi-chunk / empty-stream cases PASS\r\n1 to 32 chunks; exact bytes through 1 MiB\r\n\r\nNo silently truncated text \xf0\x9f\x8e\x89";
 io->io_Command=CMD_WRITE;io->io_Data=(APTR)screen;io->io_Length=strlen(screen);CHECK(!DoIO((struct IORequest*)io));
 printf("CLIPREAD_PASS cases=%d\n",cases);fflush(stdout);Delay(500);rc=0;
 out:if(opened)CloseDevice((struct IORequest*)io);if(w)CloseWindow(w);if(io)DeleteIORequest((struct IORequest*)io);if(port)DeleteMsgPort(port);free(bytes);if(reply)DeleteMsgPort(reply);if(IFFParseBase)CloseLibrary(IFFParseBase);printf("CLIPREAD_RUN rc=%d\n",rc);return rc;
}
