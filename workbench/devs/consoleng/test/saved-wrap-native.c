/* Native edge save/restore versus freshly emitted reference text. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <devices/conunit.h>
#include <stdio.h>
#include <string.h>
static struct IOStdReq*io;
static int put(const char*s){io->io_Command=CMD_WRITE;io->io_Data=(APTR)s;io->io_Length=strlen(s);return !DoIO((struct IORequest*)io)&&io->io_Actual==strlen(s);}
static int equal(struct Window*w,struct ConUnit*u){int ink=0;LONG bg=ReadPixel(w->RPort,u->cu_XROrigin,u->cu_YROrigin+2*u->cu_YRSize);
 for(int y=0;y<2*u->cu_YRSize;y++)for(int x=0;x<(u->cu_XMax+1)*u->cu_XRSize;x++){LONG a=ReadPixel(w->RPort,u->cu_XROrigin+x,u->cu_YROrigin+y),b=ReadPixel(w->RPort,u->cu_XROrigin+x,u->cu_YROrigin+8*u->cu_YRSize+y);if(a!=b){printf("WRAP_PIXEL x=%d y=%d\n",x,y);return 0;}if(a!=bg)ink=1;}return ink;}
#define CHECK(c) do{if(!(c)){printf("WRAP_FAIL line=%d kind=%d hidden=%d delta=%d repeat=%d\n",__LINE__,kind,hidden,delta,repeat);fflush(stdout);Delay(500);goto out;}}while(0)
int main(void){int rc=20,opened=0,kind=0,hidden=0,delta=0,repeat=0,count=0;struct Window*w=NULL;struct MsgPort*p=CreateMsgPort();io=p?(void*)CreateIORequest(p,sizeof(*io)):NULL;CHECK(io);
 for(kind=0;kind<2;kind++){
 w=OpenWindowTags(NULL,WA_Left,30,WA_Top,30,WA_Width,720,WA_Height,450,WA_Title,(IPTR)"ConsoleNG saved wrap and resize regression",WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_SizeGadget,TRUE,WA_SizeBRight,TRUE,WA_MinWidth,200,WA_MinHeight,160,WA_MaxWidth,-1,WA_MaxHeight,-1,TAG_DONE);CHECK(w);
 io->io_Data=w;io->io_Length=sizeof(*w);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",kind?CONU_CHARMAP:CONU_STANDARD,(struct IORequest*)io,0));opened=1;struct ConUnit*u=(void*)io->io_Unit;
 CHECK(put("\x1b[?25l\x1b]50;topaz,8\x07\x1b[38;2;230;230;230m\x1b[48;2;25;60;80m"));
 for(hidden=0;hidden<2;hidden++)for(delta=kind?-1:0;delta<=(kind?1:0);delta++)for(repeat=0;repeat<2;repeat++){
 CHECK(put("\x1b[>9l\x1b[2J\x1b[H"));int width=u->cu_XMax+1,oldWindow=w->Width;char text[300];CHECK(width<290);memset(text,'A',width);text[width]=0;CHECK(put(text));
 CHECK(put(hidden?"\x1b[>9h":"\x1b[>9l"));CHECK(put("\x1b" "7"));CHECK(put("\x1b[3;4H\x1b" "8"));if(repeat)CHECK(put("\x1b" "7"));
 CHECK(put("\x1b[>9l\x1b[3;4H"));if(delta){SizeWindow(w,delta*16,0);Delay(50);CHECK(w->Width==oldWindow+delta*16&&u->cu_XMax+1==width+delta*2);}
 CHECK(put(hidden?"\x1b[>9h":"\x1b[>9l"));CHECK(put("\x1b" "8"));CHECK(put("\x1b[>9lB"));int newWidth=u->cu_XMax+1,total=width+1,wantx=total%newWidth;if(!wantx)wantx=newWidth-1;
 CHECK(u->cu_XCP==wantx&&u->cu_XCCP==wantx&&u->cu_YCP==(total-1)/newWidth&&u->cu_YCCP==(total-1)/newWidth);
 CHECK(put("\x1b[9;1H"));CHECK(put(text));CHECK(put("B"));CHECK(equal(w,u));count++;
 printf("WRAP_CASE kind=%d hidden=%d delta=%d repeat=%d PASS\n",kind,hidden,delta,repeat);fflush(stdout);
 if(delta){SizeWindow(w,-delta*16,0);Delay(50);CHECK(w->Width==oldWindow);}
 }
 CHECK(put("\x1b[2J\x1b[HPASS: saved right-margin cursor preserves pending wrap.\r\nRestored output matches freshly emitted reference text.\r\nHidden saves and narrow / wide resize pass.\r\n16 native combinations passed."));
 if(kind){printf("WRAP_PASS cases=%d\n",count);fflush(stdout);Delay(600);}CloseDevice((struct IORequest*)io);opened=0;CloseWindow(w);w=NULL;
 }rc=0;
out:if(opened)CloseDevice((struct IORequest*)io);if(w)CloseWindow(w);if(io)DeleteIORequest((struct IORequest*)io);if(p)DeleteMsgPort(p);printf("WRAP_SCRIPT rc=%d\n",rc);return rc;}
