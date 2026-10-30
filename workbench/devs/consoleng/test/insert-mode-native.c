/* Compare native raster output from IRM with independently written expected
 * rows on both standard and retained units; no synthetic rendering. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <devices/conunit.h>
#include <stdio.h>
#include <string.h>
static struct IOStdReq *io;
static int put(const char*s){io->io_Command=CMD_WRITE;io->io_Data=(APTR)s;io->io_Length=strlen(s);return !DoIO((struct IORequest*)io)&&io->io_Actual==strlen(s);}
static int modequery(int enabled){
 const char *set=enabled?"\x1b[4;20h":"\x1b[4;20l";char byte[2]={0};
 for(unsigned n=0;n<strlen(set);n++){byte[0]=set[n];if(!put(byte))return 0;}
 const char *query="\x1b[4$p";
 for(unsigned n=0;n<strlen(query);n++){byte[0]=query[n];if(!put(byte))return 0;}
 char got[7];io->io_Command=CMD_READ;io->io_Data=got;io->io_Length=7;SendIO((struct IORequest*)io);
 for(int n=0;n<100;n++){if(CheckIO((struct IORequest*)io)){WaitIO((struct IORequest*)io);return !io->io_Error&&io->io_Actual==7&&!memcmp(got,enabled?"\x1b[4;1$y":"\x1b[4;2$y",7);}Delay(1);}
 AbortIO((struct IORequest*)io);WaitIO((struct IORequest*)io);return 0;
}
static int equalrows(struct Window*w,struct ConUnit*u,int rows){
 int ink=0;LONG background=ReadPixel(w->RPort,u->cu_XROrigin+(u->cu_XMax+1)*u->cu_XRSize-1,u->cu_YROrigin);
 for(int y=0;y<rows*u->cu_YRSize;y++)for(int x=0;x<(u->cu_XMax+1)*u->cu_XRSize;x++){
  LONG a=ReadPixel(w->RPort,u->cu_XROrigin+x,u->cu_YROrigin+y);
  LONG b=ReadPixel(w->RPort,u->cu_XROrigin+x,u->cu_YROrigin+6*u->cu_YRSize+y);
  if(a!=background)ink=1;
  if(a!=b){printf("INSERT_PIXEL_MISMATCH x=%d y=%d a=%ld b=%ld\n",x,y,(long)a,(long)b);return 0;}
 }if(!ink)puts("INSERT_NO_VISIBLE_INK");return ink;
}
#define CHECK(c) do{if(!(c)){printf("INSERT_FAIL line=%d unit=%d case=%d\n",__LINE__,kind,test);fflush(stdout);Delay(500);goto out;}}while(0)
int main(void){int rc=20,opened=0,kind=0,test=0;struct Window*w=NULL;struct MsgPort*p=CreateMsgPort();
 io=p?(void*)CreateIORequest(p,sizeof(*io)):NULL;CHECK(io);
 for(kind=0;kind<2;kind++){
 w=OpenWindowTags(NULL,WA_Left,30,WA_Top,30,WA_Width,720,WA_Height,450,WA_Title,(IPTR)"ConsoleNG insert mode - native pixel comparison",WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_SizeGadget,TRUE,WA_SizeBRight,TRUE,WA_MinWidth,200,WA_MinHeight,160,WA_MaxWidth,-1,WA_MaxHeight,-1,TAG_DONE);CHECK(w);
 io->io_Data=w;io->io_Length=sizeof(*w);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",kind?CONU_CHARMAP:CONU_STANDARD,(struct IORequest*)io,0));opened=1;
 struct ConUnit*u=(void*)io->io_Unit;CHECK(modequery(0));CHECK(modequery(1));
 printf("INSERT_QUERIES unit=%d PASS\n",kind);
 const char*cases[]={"ABCDEFGH\x1b[5D\x1b[4hxy", "ABCDEFGH\x1b[5D\x1b[4h!\x1b[4l?", "ABCD\x1b[3D\x1b[4h\xe2\x98\x95", "A\xe2\x98\x95" "BC\x1b[1;2H\x1b[4hX"};
 const char*answers[]={"ABCxyDEFGH","ABC!?EFGH","A\xe2\x98\x95" "BCD","AX\xe2\x98\x95" "BC"};
 for(test=0;test<4;test++){
  CHECK(put("\x1b[4l\x1b[?25l\x1b%G\x1b]50;topaz,8\x07\x1b[38;2;230;230;230m\x1b[48;2;25;60;80m\x1b[2J\x1b[H"));CHECK(put(cases[test]));CHECK(put("\x1b[4l\x1b[7;1H"));CHECK(put(answers[test]));CHECK(equalrows(w,u,1));
  if(kind){int oldWidth=w->Width,oldMax=u->cu_XMax;printf("INSERT_GEOM before width=%d xmax=%d origin=%d cell=%d\n",w->Width,u->cu_XMax,u->cu_XROrigin,u->cu_XRSize);SizeWindow(w,16,0);Delay(100);CHECK(w->Width==oldWidth+16&&u->cu_XMax==oldMax+2);CHECK(equalrows(w,u,1));SizeWindow(w,-16,0);Delay(100);CHECK(w->Width==oldWidth&&u->cu_XMax==oldMax);printf("INSERT_GEOM after width=%d xmax=%d origin=%d cell=%d\n",w->Width,u->cu_XMax,u->cu_XROrigin,u->cu_XRSize);CHECK(equalrows(w,u,1));}
  printf("INSERT_CASE unit=%d case=%d PASS\n",kind,test);fflush(stdout);
 }
 /* At the right margin the inserted character must not move other rows. */
 char actual[400],expected[400];int width=u->cu_XMax+1;CHECK(width>4&&width<180);
 memset(actual,'A',width);actual[width]=0;memset(expected,'A',width-1);strcpy(expected+width-1,"XY");
 CHECK(put("\x1b[4l\x1b[2J\x1b[H"));CHECK(put(actual));CHECK(put("\x1b[1D\x1b[4hXY"));
 /* Cursor was at last cell; CUB puts insertion one cell before it. */
 expected[width-2]='X';expected[width-1]='Y';expected[width]=0;
 CHECK(put("\x1b[4l\x1b[7;1H"));CHECK(put(expected));CHECK(equalrows(w,u,2));
 printf("INSERT_MARGIN unit=%d PASS\n",kind);fflush(stdout);
 CHECK(put("\x1b[4l\x1b[2J\x1b[HPASS: native insert / replace mode and wide text.\r\nStandard and retained output matches reference pixels.\r\nResize redraw and split mode queries pass.\r\n\r\n Actual: ABCDEFGH\x1b[5D\x1b[4hxy\x1b[4l\r\n Expect: ABCxyDEFGH\r\n"));
 if(kind){puts("INSERT_PASS comparisons=10");fflush(stdout);Delay(600);}
 CloseDevice((struct IORequest*)io);opened=0;CloseWindow(w);w=NULL;
 }
 rc=0;
out:if(opened)CloseDevice((struct IORequest*)io);if(w)CloseWindow(w);if(io)DeleteIORequest((struct IORequest*)io);if(p)DeleteMsgPort(p);printf("INSERT_SCRIPT rc=%d\n",rc);return rc;
}
