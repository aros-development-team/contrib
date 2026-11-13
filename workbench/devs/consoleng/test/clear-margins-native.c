/* Native pixel validation of full-clear residual margins and protected chrome. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <devices/conunit.h>
#include <stdio.h>
#include <string.h>
static struct IOStdReq*io;
static int put(const char*s){io->io_Command=CMD_WRITE;io->io_Data=(APTR)s;io->io_Length=strlen(s);return !DoIO((struct IORequest*)io)&&io->io_Actual==strlen(s);}
static int rightedge(struct Window*w){int right=w->Width-w->BorderRight-1;for(struct Gadget*g=w->FirstGadget;g;g=g->NextGadget){if((g->GadgetType&GTYP_SYSGADGET)||!(g->Activation&GACT_RIGHTBORDER))continue;int edge=g->LeftEdge;if(g->Flags&GFLG_RELRIGHT)edge+=w->Width;if(edge-1<right)right=edge-1;}return right;}
static ULONG chrome(struct Window*w,struct ConUnit*u){ULONG hash=2166136261UL;int right=rightedge(w),bottom=w->Height-w->BorderBottom-1;
 for(int y=0;y<w->Height;y++)for(int x=0;x<w->Width;x++)if(x<u->cu_XROrigin||x>right||y<u->cu_YROrigin||y>bottom)hash=(hash^(ULONG)ReadPixel(w->RPort,x,y))*16777619UL;return hash;}
static int margins(struct Window*w,struct ConUnit*u,LONG color){int right=rightedge(w),bottom=w->Height-w->BorderBottom-1,count=0;
 for(int y=u->cu_YROrigin;y<=bottom;y++)for(int x=u->cu_XROrigin;x<=right;x++)if(x>u->cu_XRExtant||y>u->cu_YRExtant){LONG actual=ReadPixel(w->RPort,x,y);if(actual!=color){printf("MARGIN_MISMATCH x=%d y=%d actual=%ld expected=%ld\n",x,y,(long)actual,(long)color);return 0;}count++;}
 printf("MARGIN_PIXELS width=%d height=%d pixels=%d\n",w->Width,w->Height,count);return count>0;}
#define CHECK(c) do{if(!(c)){printf("MARGIN_FAIL line=%d size=%d phase=%d\n",__LINE__,size,phase);fflush(stdout);Delay(500);goto out;}}while(0)
int main(void){int rc=20,opened=0,size=0,phase=0,cases=0;struct Window*w=NULL;struct MsgPort*p=CreateMsgPort();io=p?(void*)CreateIORequest(p,sizeof(*io)):NULL;CHECK(io);
 w=OpenWindowTags(NULL,WA_Left,30,WA_Top,30,WA_Width,725,WA_Height,451,WA_Title,(IPTR)"ConsoleNG full-clear margins and border protection",WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_SizeGadget,TRUE,WA_SizeBRight,TRUE,WA_MinWidth,200,WA_MinHeight,160,WA_MaxWidth,-1,WA_MaxHeight,-1,TAG_DONE);CHECK(w);
 io->io_Data=w;io->io_Length=sizeof(*w);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",CONU_CHARMAP,(struct IORequest*)io,0));opened=1;struct ConUnit*u=(void*)io->io_Unit;
 CHECK(put("\x1b[?25l\x1b]50;topaz,8\x07\x1b[38;2;230;230;230m"));Delay(20);
 const char*colors[]={"\x1b[48;2;25;60;80m","\x1b[48;2;120;40;60m","\x1b[48;2;40;100;60m"};
 for(size=0;size<4;size++){
  if(size){SizeWindow(w,3,2);Delay(100);}
  for(phase=0;phase<6;phase++){
   ULONG before=chrome(w,u);CHECK(put(colors[phase%3]));CHECK(put(phase<3?"\x1b[2J":"\f"));
   LONG color=ReadPixel(w->RPort,u->cu_XROrigin,u->cu_YROrigin);CHECK(margins(w,u,color));CHECK(chrome(w,u)==before);cases++;
  }
  /* ED3 drops history but must not repaint the visible margin. */
  LONG prior=ReadPixel(w->RPort,u->cu_XROrigin,u->cu_YROrigin);CHECK(put(colors[0]));CHECK(put("\x1b[3J"));CHECK(margins(w,u,prior));cases++;
  /* Hidden clears affect retained state, not the shared window. */
  CHECK(put("\x1b[>9h"));LONG old=ReadPixel(w->RPort,u->cu_XROrigin,u->cu_YRExtant+1);CHECK(put(colors[1]));CHECK(put("\x1b[2J\f"));CHECK(ReadPixel(w->RPort,u->cu_XROrigin,u->cu_YRExtant+1)==old);CHECK(put("\x1b[>9l"));CHECK(margins(w,u,ReadPixel(w->RPort,u->cu_XROrigin,u->cu_YROrigin)));cases++;
 }
 CHECK(put(colors[0]));CHECK(put("\x1b[2J\x1b[HPASS: full clears repaint residual right and bottom pixels.\r\nBorder, scrollbar and resize gadget pixels stay intact.\r\nED3 preserves margins; hidden clears wait for reveal.\r\n"));printf("MARGIN_PASS cases=%d\n",cases);fflush(stdout);Delay(600);rc=0;
out:if(opened)CloseDevice((struct IORequest*)io);if(w)CloseWindow(w);if(io)DeleteIORequest((struct IORequest*)io);if(p)DeleteMsgPort(p);printf("MARGIN_SCRIPT rc=%d\n",rc);return rc;}
