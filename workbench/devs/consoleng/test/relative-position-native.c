/* Native fragmented cursor motion: coordinates and unchanged raster content. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <devices/conunit.h>
#include <stdio.h>
#include <string.h>
static struct IOStdReq*io;
static int put(const char*s){io->io_Command=CMD_WRITE;io->io_Data=(APTR)s;io->io_Length=strlen(s);return !DoIO((struct IORequest*)io)&&io->io_Actual==strlen(s);}
static int split(const char*s){char b[2]={0};while(*s){b[0]=*s++;if(!put(b))return 0;}return 1;}
static ULONG pixels(struct Window*w,struct ConUnit*u){ULONG h=2166136261UL;for(int y=u->cu_YROrigin;y<=u->cu_YRExtant;y++)for(int x=u->cu_XROrigin;x<=u->cu_XRExtant;x++)h=(h^(ULONG)ReadPixel(w->RPort,x,y))*16777619UL;return h;}
#define CHECK(c) do{if(!(c)){printf("RELATIVE_FAIL line=%d kind=%d hidden=%d case=%d\n",__LINE__,kind,hidden,test);fflush(stdout);Delay(500);goto out;}}while(0)
int main(void){int rc=20,opened=0,kind=0,hidden=0,test=0,count=0;struct Window*w=NULL;struct MsgPort*p=CreateMsgPort();io=p?(void*)CreateIORequest(p,sizeof(*io)):NULL;CHECK(io);
 const char*seq[]={"\x1b[A","\x1b[0A","\x1b[65535A","\x1b[B","\x1b[65535B","\x1b[C","\x1b[65535C","\x1b[D","\x1b[65535D","\x1b[E","\x1b[65535E","\x1b[F","\x1b[65535F","\x1b[2a","\x1b[65535a","\x1b[2e","\x1b[65535e","\b","\r","\x1b[1;1H\b"};
 const int xs[]={3,3,3,3,3,4,-1,2,0,0,0,0,0,5,-1,3,3,2,0,0};
 const int ys[]={1,1,0,3,-1,2,2,2,2,3,-1,1,0,2,2,4,-1,2,2,0};
 for(kind=0;kind<2;kind++){
 w=OpenWindowTags(NULL,WA_Left,30,WA_Top,30,WA_Width,720,WA_Height,450,WA_Title,(IPTR)"ConsoleNG cursor motion - native raster validation",WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_SizeGadget,TRUE,WA_SizeBRight,TRUE,WA_MinWidth,200,WA_MinHeight,160,WA_MaxWidth,-1,WA_MaxHeight,-1,TAG_DONE);CHECK(w);
 io->io_Data=w;io->io_Length=sizeof(*w);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",kind?CONU_CHARMAP:CONU_STANDARD,(struct IORequest*)io,0));opened=1;struct ConUnit*u=(void*)io->io_Unit;
 CHECK(put("\x1b[?25l\x1b]50;topaz,8\x07\x1b[38;2;230;230;230m\x1b[48;2;25;60;80m\x1b[2J\x1b[HANCHOR: cursor movement must preserve these pixels.\x1b[20;1HBOTTOM ANCHOR"));
 for(hidden=0;hidden<2;hidden++){
 CHECK(put(hidden?"\x1b[>9h":"\x1b[>9l"));ULONG before=pixels(w,u);
 for(test=0;test<20;test++){
 CHECK(put("\x1b[3;4H"));CHECK(split(seq[test]));int x=xs[test]<0?u->cu_XMax:xs[test],y=ys[test]<0?u->cu_YMax:ys[test];
 if(u->cu_XCP!=x||u->cu_YCP!=y)printf("RELATIVE_MISMATCH actual=%d,%d expected=%d,%d\n",u->cu_XCP,u->cu_YCP,x,y);
 CHECK(u->cu_XCP==x&&u->cu_XCCP==x&&u->cu_YCP==y&&u->cu_YCCP==y);CHECK(pixels(w,u)==before);count++;
 }
 printf("RELATIVE_GROUP kind=%d hidden=%d PASS\n",kind,hidden);fflush(stdout);
 }
 /* Hidden control characters change state without drawing into the shared window. */
 const char*controls[]={"\x1b[20l\n","\x1b[20h\n","\x1b" "D","\x1b" "E","\x1b" "M","\f"};
 const int cx[]={3,0,3,0,3,0},cy[]={3,3,3,3,1,0};ULONG before=pixels(w,u);
 for(test=0;test<6;test++){CHECK(put("\x1b[3;4H"));CHECK(split(controls[test]));CHECK(u->cu_XCP==cx[test]&&u->cu_XCCP==cx[test]&&u->cu_YCP==cy[test]&&u->cu_YCCP==cy[test]);CHECK(pixels(w,u)==before);count++;}
 CHECK(put("\x1b[>9l\x1b[20l\x1b[2J\x1b[HPASS: cursor commands clamp at every screen edge.\r\nStandard and retained consoles preserve screen pixels.\r\nHidden controls update state without drawing.\r\n92 native coordinate and raster checks passed."));
 if(kind){printf("RELATIVE_PASS cases=%d\n",count);fflush(stdout);Delay(600);}
 CloseDevice((struct IORequest*)io);opened=0;CloseWindow(w);w=NULL;
 }rc=0;
out:if(opened)CloseDevice((struct IORequest*)io);if(w)CloseWindow(w);if(io)DeleteIORequest((struct IORequest*)io);if(p)DeleteMsgPort(p);printf("RELATIVE_SCRIPT rc=%d\n",rc);return rc;
}
