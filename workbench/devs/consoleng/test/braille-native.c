/* Native pixel acceptance of every Braille pattern, direct and retained redraw. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <graphics/text.h>
#include <devices/conunit.h>
#include <stdio.h>
#include <string.h>
#include "consoleng.h"
static int req(struct IOStdReq *io,UWORD c,void *p,ULONG n){io->io_Command=c;io->io_Data=p;io->io_Length=n;return !DoIO((struct IORequest*)io)&&io->io_Actual==n;}
static int put(struct IOStdReq *io,const char *s){return req(io,CMD_WRITE,(void*)s,strlen(s));}
static int verify(struct Window *win,struct ConUnit *u){
 const int row[]={0,1,2,0,1,2,3,3},col[]={0,0,0,1,1,1,0,1};
 int w=u->cu_XRSize,h=u->cu_YRSize;
 ULONG bg=ReadPixel(win->RPort,u->cu_XROrigin,u->cu_YROrigin+h);
 for(int mask=0;mask<256;mask++)for(int bit=0;bit<8;bit++){
  int x=u->cu_XROrigin+(mask%16)*2*w,y=u->cu_YROrigin+(1+mask/16)*h,count=0;
  for(int py=y+row[bit]*h/4;py<y+(row[bit]+1)*h/4;py++)
   for(int px=x+col[bit]*w/2;px<x+(col[bit]+1)*w/2;px++)count+=ReadPixel(win->RPort,px,py)!=bg;
  if((count>0)!=!!(mask&(1<<bit))){printf("BRAILLE_PIXEL_FAIL mask=%d bit=%d count=%d\n",mask,bit,count);return 0;}
 }
 return 1;
}
#define CHECK(x) do{if(!(x)){printf("BRAILLE_FAIL line=%d\n",__LINE__);goto out;}}while(0)
int main(void){
 int rc=20,opened=0;struct MsgPort *port=CreateMsgPort();struct IOStdReq *io=NULL;struct Window *w=NULL;
 CHECK(port);io=(void*)CreateIORequest(port,sizeof(*io));CHECK(io);
 w=OpenWindowTags(NULL,WA_Left,30,WA_Top,30,WA_Width,730,WA_Height,520,WA_Title,(IPTR)"ConsoleNG Braille - all 256 patterns",WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_SizeGadget,TRUE,WA_MinWidth,400,WA_MinHeight,400,TAG_DONE);CHECK(w);
 io->io_Data=w;io->io_Length=sizeof(*w);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",CONU_CHARMAP,(struct IORequest*)io,0));opened=1;
 struct ConsolengColorPrefs p={sizeof(p),1,3,{0xffffff,0x203e4c}};CHECK(req(io,CD_SETCOLORPREFS,&p,sizeof(p)));
 for(int phase=0;phase<3;phase++){
  if(phase==1)CHECK(put(io,"\x1b]50;DejaVuSansMono,16\x07"));
  if(phase==2){CHECK(put(io,"\x1b]50;topaz,8\x07"));CHECK(w->RPort->Font->tf_YSize==8 && !strcmp(w->RPort->Font->tf_Message.mn_Node.ln_Name,"topaz.font"));}
  CHECK(put(io,"\x1b%G\x1b[2J\x1b[HAll 256 Braille patterns\r\n"));
  for(int m=0;m<256;m++){
   unsigned cp=0x2800+m;char b[]={0xe0|(cp>>12),0x80|((cp>>6)&63),0x80|(cp&63),' ',0};
   CHECK(put(io,b));if(m%16==15)CHECK(put(io,"\r\n"));
  }
  CHECK(verify(w,(void*)io->io_Unit));
  CHECK(put(io,"\r\nPASS: all 256 patterns, every dot verified.\r\n"));
  printf("BRAILLE_READY phase=%d font=%s height=%u\n",phase,w->RPort->Font->tf_Message.mn_Node.ln_Name,w->RPort->Font->tf_YSize);fflush(stdout);Delay(50*15);
 }
 SizeWindow(w,-80,-30);Delay(50*2);CHECK(verify(w,(void*)io->io_Unit));
 puts("BRAILLE_REDRAW_PASS");puts("BRAILLE_PASS");rc=0;
out:
 if(opened)CloseDevice((struct IORequest*)io);
 if(w)CloseWindow(w);
 if(io)DeleteIORequest((struct IORequest*)io);
 if(port)DeleteMsgPort(port);
 printf("BRAILLE_SCRIPT rc=%d\n",rc);return rc;
}
