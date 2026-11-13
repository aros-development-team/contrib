/* Native fragmented tab stops: coordinates and unchanged raster content. */
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
#define CHECK(c) do{if(!(c)){printf("TABS_FAIL line=%d kind=%d hidden=%d case=%d\n",__LINE__,kind,hidden,test);fflush(stdout);Delay(500);goto out;}}while(0)
int main(void){int rc=20,opened=0,kind=0,hidden=0,test=0,count=0;struct Window*w=NULL;struct MsgPort*p=CreateMsgPort();io=p?(void*)CreateIORequest(p,sizeof(*io)):NULL;CHECK(io);
 const char*seq[]={"\t","\x1b[2I","\x1b[65535I","\x1b[65535Z","\x1b[0I","\x1b[3g\t","\x1b[3g\x1b[6G\x1b" "H\x1b[1G\t","\x1b[3g\x1b[6G\x1b" "H\x1b[g\x1b[1G\t","\x1b[3g\x1b[4G\x1b" "H\x1b[12G\x1b" "H\x1b[1G\x1b[2I","\x1b[3g\x1b[4G\x1b" "H\x1b[12G\x1b" "H\x1b[20G\x1b[Z","\x1b[3g\x1b[5G\x1b[0W\x1b[1G\t","\x1b[3g\x1b[5G\x1b[0W\x1b[2W\x1b[1G\t","\x1b[5W\x1b[65535Z","\x1b[81G\t","\x1b[3g\x1b[6G\x1b" "H\x1b[1g\x1b[1G\t","\x1b[1G\x1b[Z"};
 const int xs[]={8,16,-1,0,8,-1,5,-1,11,11,4,-1,0,-1,5,0};
 for(kind=0;kind<2;kind++){
 w=OpenWindowTags(NULL,WA_Left,30,WA_Top,30,WA_Width,720,WA_Height,450,WA_Title,(IPTR)"ConsoleNG tab stops - native raster validation",WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_SizeGadget,TRUE,WA_SizeBRight,TRUE,WA_MinWidth,200,WA_MinHeight,160,WA_MaxWidth,-1,WA_MaxHeight,-1,TAG_DONE);CHECK(w);
 io->io_Data=w;io->io_Length=sizeof(*w);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",kind?CONU_CHARMAP:CONU_STANDARD,(struct IORequest*)io,0));opened=1;struct ConUnit*u=(void*)io->io_Unit;
 CHECK(put("\x1b[?25l\x1b]50;topaz,8\x07\x1b[38;2;230;230;230m\x1b[48;2;25;60;80m\x1b[2J\x1b[HANCHOR: cursor movement must preserve these pixels.\x1b[20;1HBOTTOM ANCHOR"));
 for(hidden=0;hidden<2;hidden++){
 CHECK(put(hidden?"\x1b[>9h":"\x1b[>9l"));ULONG before=pixels(w,u);
 for(test=0;test<16;test++){
 for(int i=0;i<MAXTABS-1;i++)u->cu_TabStops[i]=i*8;u->cu_TabStops[MAXTABS-1]=65535;
 CHECK(put("\x1b[3;4H"));CHECK(split(seq[test]));int x=xs[test]<0?u->cu_XMax:xs[test],y=2;
 if(u->cu_XCP!=x||u->cu_YCP!=y)printf("TABS_MISMATCH actual=%d,%d expected=%d,%d\n",u->cu_XCP,u->cu_YCP,x,y);
 CHECK(u->cu_XCP==x&&u->cu_XCCP==x&&u->cu_YCP==y&&u->cu_YCCP==y);CHECK(pixels(w,u)==before);count++;
 }
 printf("TABS_GROUP kind=%d hidden=%d PASS\n",kind,hidden);fflush(stdout);
 }
 CHECK(put("\x1b[>9l\x1b[2J\x1b[HPASS: native tab set / clear and forward / backward motion.\r\n64 visible / hidden coordinate and raster checks passed.\r\nLarge counts stop at screen edges without wrapping."));
 if(kind){printf("TABS_PASS cases=%d\n",count);fflush(stdout);Delay(600);}
 CloseDevice((struct IORequest*)io);opened=0;CloseWindow(w);w=NULL;
 }rc=0;
out:if(opened)CloseDevice((struct IORequest*)io);if(w)CloseWindow(w);if(io)DeleteIORequest((struct IORequest*)io);if(p)DeleteMsgPort(p);printf("TABS_SCRIPT rc=%d\n",rc);return rc;
}
