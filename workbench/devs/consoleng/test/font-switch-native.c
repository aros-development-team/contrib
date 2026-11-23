/* Bounded OSC50/glyph-cache replay. Caller restores its original font before
 * close: shared-window font restoration is a separate, still-open gate. */
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
 int rc=20,opened=0;struct Window *w=NULL;struct TextFont *original=NULL;
 struct MsgPort *p=CreateMsgPort();struct IOStdReq *io=p?(void*)CreateIORequest(p,sizeof(*io)):NULL;
 if(!io)goto out;
 w=OpenWindowTags(NULL,WA_Left,30,WA_Top,30,WA_Width,730,WA_Height,500,
 WA_Title,(IPTR)"ConsoleNG font switching - native glyph replay",WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_SizeGadget,TRUE,
 WA_MinWidth,200,WA_MinHeight,120,WA_MaxWidth,~0UL,WA_MaxHeight,~0UL,WA_Activate,TRUE,TAG_DONE);
 if(!w)goto out;
 original=w->RPort->Font;io->io_Data=w;io->io_Length=sizeof(*w);
 if(OpenDevice((CONST_STRPTR)"consoleng.device",CONU_CHARMAP,(struct IORequest*)io,0))goto out;
 opened=1;
 for(int i=0;i<40;i++) {
  char text[220];int size=(i&1)?16:12;
  snprintf(text,sizeof(text),"\x1b]50;DejaVuSansMono,%d\x07\x1b[2J\x1b[HFont switch %d / 40 - requested %d pixels\r\nUnicode: \xe2\x94\x8c\xe2\x94\x80\xe2\x94\x90  \xce\xa9  \xe2\x86\x92\r\n",size,i+1,size);
  if(!put(io,text))goto out;
  struct ConUnit *u=(void*)io->io_Unit;
  if(u->cu_XRSize!=w->RPort->Font->tf_XSize || u->cu_YRSize!=w->RPort->Font->tf_YSize || u->cu_YRSize!=size)goto out;
 }
 if(!put(io,"\r\nPASS: 40 alternating font sizes with Unicode output.\r\n"))goto out;
 puts("FONT_SWITCH_PASS cycles=40");fflush(stdout);Delay(50*12);
 SizeWindow(w,-100,-80);Delay(50*2);
 struct ConUnit *u=(void*)io->io_Unit;
 if(w->Width!=630||w->Height!=420||u->cu_XRExtant>=w->Width-w->BorderRight||u->cu_YRExtant>=w->Height-w->BorderBottom)goto out;
 if(!put(io,"\r\nPASS: resized window, grid stays inside the borders.\r\n"))goto out;
 puts("FONT_RESIZE_PASS width=630 height=420");fflush(stdout);Delay(50*12);rc=0;
out:
 if(w&&original)SetFont(w->RPort,original);
 if(opened)CloseDevice((struct IORequest*)io);
 if(w)CloseWindow(w);
 if(io)DeleteIORequest((struct IORequest*)io);
 if(p)DeleteMsgPort(p);
 printf("FONT_SCRIPT rc=%d\n",rc);return rc;
}
