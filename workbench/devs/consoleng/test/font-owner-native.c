/* Shared baseline must outlive units and be restored before owned-font close. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <graphics/text.h>
#include <devices/conunit.h>
#include <stdio.h>
#include <string.h>
static int put(struct IOStdReq *io,const char *s) {
 io->io_Command=CMD_WRITE;io->io_Data=(APTR)s;io->io_Length=strlen(s);
 return !DoIO((struct IORequest*)io);
}
#define CHECK(x) do{if(!(x)){printf("FONT_OWNER_FAIL line=%d\n",__LINE__);goto out;}}while(0)
int main(void) {
 int rc=20,opened[3]={0};struct Window *w=NULL;struct TextFont *original=NULL;
 struct MsgPort *p=CreateMsgPort();struct IOStdReq *io[3]={NULL};
 CHECK(p);
 for(int i=0;i<3;i++){io[i]=(void*)CreateIORequest(p,sizeof(*io[i]));CHECK(io[i]);}
 w=OpenWindowTags(NULL,WA_Left,30,WA_Top,30,WA_Width,730,WA_Height,500,
 WA_Title,(IPTR)"ConsoleNG shared fonts and caller-font restoration",WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_SizeGadget,TRUE,
 WA_MinWidth,200,WA_MinHeight,120,WA_MaxWidth,~0UL,WA_MaxHeight,~0UL,WA_Activate,TRUE,TAG_DONE);CHECK(w);
 original=w->RPort->Font;
 for(int i=0;i<3;i++){
  io[i]->io_Data=w;io[i]->io_Length=sizeof(*w);
  CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",CONU_CHARMAP,(struct IORequest*)io[i],0));opened[i]=1;
  CHECK(w->RPort->Font==original);
  CHECK(((struct ConUnit*)io[i]->io_Unit)->cu_YRSize==original->tf_YSize);
  if(i==0)CHECK(put(io[i],"\x1b]50;DejaVuSansMono,12\x07"));
  CHECK(put(io[i],"\x1b[>9h"));
 }
 CHECK(put(io[0],"\x1b[>9l"));CHECK(w->RPort->Font->tf_YSize==12);
 CHECK(put(io[0],"\x1b[>9h"));CHECK(put(io[1],"\x1b[>9l"));CHECK(w->RPort->Font==original);
 CloseDevice((struct IORequest*)io[0]);opened[0]=0;CHECK(w->RPort->Font==original);
 CHECK(put(io[1],"\x1b[>9h"));CHECK(put(io[2],"\x1b[>9l\x1b]50;DejaVuSansMono,16\x07"));
 CHECK(w->RPort->Font->tf_YSize==16);
 CHECK(put(io[2],"\x1b[>9h"));CHECK(put(io[1],"\x1b[>9l"));CHECK(w->RPort->Font==original);
 CHECK(put(io[2],"\x1b]50;DejaVuSansMono,18\x07"));
 CHECK(w->RPort->Font==original);CHECK(((struct ConUnit*)io[2]->io_Unit)->cu_YRSize==18);
 CHECK(put(io[1],"\x1b[>9h"));CHECK(put(io[2],"\x1b[>9l"));CHECK(w->RPort->Font->tf_YSize==18);
 CHECK(put(io[2],"\x1b[>9h"));CHECK(put(io[1],"\x1b[>9l"));CHECK(w->RPort->Font==original);
 puts("FONT_OWNER_HIDDEN_PASS");
 CloseDevice((struct IORequest*)io[2]);opened[2]=0;CHECK(w->RPort->Font==original);
 CHECK(put(io[1],"\x1b]50;DejaVuSansMono,14\x07\x1b[2J\x1b[HPASS: default tabs restore the caller font.\r\nPASS: closing hidden tabs preserves the visible font.\r\nTesting last visible unit close next.\r\n"));
 CHECK(w->RPort->Font->tf_YSize==14);
 puts("FONT_OWNER_READY");fflush(stdout);Delay(50*10);
 CloseDevice((struct IORequest*)io[1]);opened[1]=0;
 CHECK(w->RPort->Font==original);
 /* The last detach removed the registry entry; a fresh group still borrows
    the caller baseline, never the released 14px font. */
 io[0]->io_Data=w;io[0]->io_Length=sizeof(*w);
 CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",CONU_CHARMAP,(struct IORequest*)io[0],0));opened[0]=1;
 CHECK(w->RPort->Font==original);
 CloseDevice((struct IORequest*)io[0]);opened[0]=0;CHECK(w->RPort->Font==original);
 /* Device-created windows survive their first unit when another borrows it. */
 io[0]->io_Data=NULL;io[0]->io_Length=0;
 CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",CONU_CHARMAP,(struct IORequest*)io[0],0));opened[0]=1;
 struct Window *owned=((struct ConUnit*)io[0]->io_Unit)->cu_Window;
 CHECK(put(io[0],"\x1b[>9h"));
 io[1]->io_Data=owned;io[1]->io_Length=sizeof(*owned);
 CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",CONU_CHARMAP,(struct IORequest*)io[1],0));opened[1]=1;
 CloseDevice((struct IORequest*)io[0]);opened[0]=0;
 CHECK(put(io[1],"\x1b]50;DejaVuSansMono,16\x07Surviving unit still owns this window.\r\n"));
 CHECK(owned->RPort->Font->tf_YSize==16);
 CloseDevice((struct IORequest*)io[1]);opened[1]=0;
 puts("FONT_OWNED_WINDOW_PASS");
 puts("FONT_OWNER_PASS");rc=0;
out:
 if(w&&original)SetFont(w->RPort,original); /* failure cleanup only */
 for(int i=2;i>=0;i--)if(opened[i])CloseDevice((struct IORequest*)io[i]);
 if(w)CloseWindow(w);
 for(int i=0;i<3;i++)if(io[i])DeleteIORequest((struct IORequest*)io[i]);
 if(p)DeleteMsgPort(p);
 printf("FONT_OWNER_SCRIPT rc=%d\n",rc);return rc;
}
