/* Shared-window font ownership gate. Nonzero means the contract is not met.
 * Restore caller font before cleanup so a failed gate does not dereference a
 * closed font through the shared RastPort. */
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
int main(void) {
 int rc=20,opened[2]={0,0},failures=0;struct Window *w=NULL;struct TextFont *original=NULL;
 struct MsgPort *p=CreateMsgPort();struct IOStdReq *io[2]={NULL,NULL};
 if(!p)goto out;
 for(int i=0;i<2;i++){io[i]=(void*)CreateIORequest(p,sizeof(*io[i]));if(!io[i])goto out;}
 w=OpenWindowTags(NULL,WA_Left,30,WA_Top,30,WA_Width,730,WA_Height,500,
 WA_Title,(IPTR)"ConsoleNG shared-window font contract",WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_SizeGadget,TRUE,
 WA_MinWidth,200,WA_MinHeight,120,WA_MaxWidth,~0UL,WA_MaxHeight,~0UL,WA_Activate,TRUE,TAG_DONE);
 if(!w)goto out;
 original=w->RPort->Font;
 for(int i=0;i<2;i++) {
  io[i]->io_Data=w;io[i]->io_Length=sizeof(*w);
  if(OpenDevice((CONST_STRPTR)"consoleng.device",CONU_CHARMAP,(struct IORequest*)io[i],0))goto out;
  opened[i]=1;
  if(!put(io[i],i?"\x1b]50;DejaVuSansMono,16\x07":"\x1b]50;DejaVuSansMono,12\x07"))goto out;
  if(!put(io[i],"\x1b[>9h"))goto out;
 }
 for(int i=0;i<2;i++) {
  if(!put(io[i],"\x1b[>9l"))goto out;
  struct ConUnit *u=(void*)io[i]->io_Unit;
  int wanted=i?16:12,actual=w->RPort->Font->tf_YSize;
  printf("SHARED_FONT_REVEAL unit=%d grid=%d raster=%d expected=%d\n",i,u->cu_YRSize,actual,wanted);
  if(u->cu_YRSize!=wanted||actual!=wanted)failures++;
  if(!put(io[i],"\x1b[>9h"))goto out;
 }
 /* Closing a hidden unit must leave the visible font alone. */
 if(!put(io[0],"\x1b[>9l"))goto out;
 struct TextFont *visible=w->RPort->Font;
 /* Do not close a font still selected in the raster port on broken builds. */
 int wrong=visible->tf_YSize!=12;
 printf("SHARED_FONT_CLOSE_PRECONDITION safe=%d\n",!wrong);
 if(!wrong) {
  CloseDevice((struct IORequest*)io[1]);opened[1]=0;
  if(w->RPort->Font!=visible)failures++;
 } else failures++;
 if(opened[1]) {
  put(io[0],"\x1b[>9h");put(io[1],"\x1b[>9l\x1b[2J\x1b[H");
  put(io[1],failures?"Shared-window font gate FAILED.\r\nRevealed 12px unit retained a 16px raster font.\r\nOwnership repair is still required.\r\n":"Shared-window font gate PASSED.\r\n");
 }
 printf("SHARED_FONT_RESULT failures=%d\n",failures);fflush(stdout);Delay(50*10);
 rc=failures?20:0;
out:
 if(w&&original)SetFont(w->RPort,original);
 for(int i=1;i>=0;i--)if(opened[i])CloseDevice((struct IORequest*)io[i]);
 if(w)CloseWindow(w);
 for(int i=0;i<2;i++)if(io[i])DeleteIORequest((struct IORequest*)io[i]);
 if(p)DeleteMsgPort(p);
 printf("SHARED_FONT_SCRIPT rc=%d\n",rc);return rc;
}
