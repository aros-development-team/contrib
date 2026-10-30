/* Native ED0/ED1 wide-boundary visual fixture, two independent device units. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <devices/conunit.h>
#include <intuition/intuition.h>
#include <stdio.h>
#include <string.h>
int main(void) {
 struct Window *w[2]={0};struct MsgPort *p[2]={0};struct IOStdReq *io[2]={0};int opened[2]={0},rc=0;
 const char *page[2]={
  "\x1b%G\x1b]50;DejaVuSansMono,14\x07\x1b[2J\x1b[H"
  "ED0: erase from second half through end of screen\r\n\r\n"
  "Expect: A \r\n\r\n"
  "Actual: A☕BC\x1b[5;11H\x1b[J",
  "\x1b%G\x1b]50;DejaVuSansMono,14\x07\x1b[2J\x1b[H"
  "THIS HEADER MUST DISAPPEAR\r\n\r\n"
  "Actual: A☕BC\r\n"
  "           BC\r\n\r\n"
  "ED1: erase through first half from screen start\r\n"
  "The two BC rows above must occupy identical columns."
  "\x1b[3;10H\x1b[1J"
 };
 for(int n=0;n<2;n++) {
  w[n]=OpenWindowTags(NULL,WA_Left,20,WA_Top,30+n*265,WA_Width,750,WA_Height,250,
   WA_Title,(IPTR)(n?"ConsoleNG - ED1 wide boundary":"ConsoleNG - ED0 wide boundary"),
   WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_SizeGadget,TRUE,WA_SizeBRight,TRUE,
   WA_MinWidth,160,WA_MinHeight,80,WA_MaxWidth,-1,WA_MaxHeight,-1,WA_Activate,TRUE,TAG_DONE);
  if(!w[n]){rc=20;goto done;}
  p[n]=CreateMsgPort();io[n]=p[n]?(struct IOStdReq*)CreateIORequest(p[n],sizeof(*io[n])):NULL;
  if(!io[n]){rc=20;goto done;}
  io[n]->io_Data=w[n];io[n]->io_Length=sizeof(*w[n]);
  if(OpenDevice((CONST_STRPTR)"consoleng.device",CONU_CHARMAP,(struct IORequest*)io[n],0)){rc=20;goto done;}opened[n]=1;
  io[n]->io_Command=CMD_WRITE;io[n]->io_Data=(APTR)page[n];io[n]->io_Length=strlen(page[n]);
  if(DoIO((struct IORequest*)io[n]))rc=20;
  printf("ED%d_WRITE error=%ld actual=%lu\n",n,(long)io[n]->io_Error,(unsigned long)io[n]->io_Actual);fflush(stdout);
 }
 Delay(250);
 for(int n=0;n<2;n++)SizeWindow(w[n],10,0);
 Delay(50);
 for(int n=0;n<2;n++)SizeWindow(w[n],-10,0);
 puts("ED_RESIZED");fflush(stdout);Delay(1500);
done:
 for(int n=1;n>=0;n--){if(opened[n])CloseDevice((struct IORequest*)io[n]);if(io[n])DeleteIORequest((struct IORequest*)io[n]);if(p[n])DeleteMsgPort(p[n]);if(w[n])CloseWindow(w[n]);}
 return rc;
}
