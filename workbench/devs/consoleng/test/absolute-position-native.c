/* Public ConUnit coordinates after fragmented absolute-position sequences. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <devices/conunit.h>
#include <stdio.h>
#include <string.h>
static struct IOStdReq*io;
static int put(const char*s){io->io_Command=CMD_WRITE;io->io_Data=(APTR)s;io->io_Length=strlen(s);return !DoIO((struct IORequest*)io)&&io->io_Actual==strlen(s);}
static int split(const char*s){char byte[2]={0};for(unsigned i=0;i<strlen(s);i++){byte[0]=s[i];if(!put(byte))return 0;}return 1;}
#define CHECK(c) do{if(!(c)){printf("POSITION_FAIL line=%d kind=%d hidden=%d case=%d\n",__LINE__,kind,hidden,test);fflush(stdout);Delay(500);goto out;}}while(0)
int main(void){int rc=20,opened=0,kind=0,hidden=0,test=0,count=0;struct Window*w=NULL;struct MsgPort*p=CreateMsgPort();io=p?(void*)CreateIORequest(p,sizeof(*io)):NULL;CHECK(io);
 for(kind=0;kind<2;kind++){
 w=OpenWindowTags(NULL,WA_Left,30,WA_Top,30,WA_Width,720,WA_Height,450,WA_Title,(IPTR)"ConsoleNG absolute cursor positioning",WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_SizeGadget,TRUE,WA_SizeBRight,TRUE,WA_MinWidth,200,WA_MinHeight,160,WA_MaxWidth,-1,WA_MaxHeight,-1,TAG_DONE);CHECK(w);
 io->io_Data=w;io->io_Length=sizeof(*w);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",kind?CONU_CHARMAP:CONU_STANDARD,(struct IORequest*)io,0));opened=1;struct ConUnit*u=(void*)io->io_Unit;
 const char*seq[]={"\x1b[H","\x1b[0;0H","\x1b[65535;65535H","\x1b[32768;32768H","\x1b[5;6H","\x1b[65535G","\x1b[0G","\x1b[10d","\x1b[65535d","\x1b[0d","\x1b[10`","\x1b[65535`","\x1b[;f","\x1b[5;f","\x1b[;6f","\x1b[000f","\x1b[999999999999999999H"};
 /* Each case starts at row3,column4; -1 denotes the relevant screen edge. */
 const int xs[]={0,0,-1,-1,5,-1,0,3,3,3,9,-1,0,0,5,0,0};
 const int ys[]={0,0,-1,-1,4,2,2,9,-1,0,2,2,0,4,0,0,-1};
 for(hidden=0;hidden<2;hidden++){
  CHECK(put(hidden?"\x1b[>9h":"\x1b[>9l"));
  for(test=0;test<17;test++){
   CHECK(put("\x1b[3;4H"));CHECK(split(seq[test]));int x=xs[test]<0?u->cu_XMax:xs[test],y=ys[test]<0?u->cu_YMax:ys[test];
   if(u->cu_XCP!=x||u->cu_YCP!=y)printf("POSITION_MISMATCH actual=%d,%d expected=%d,%d\n",u->cu_XCP,u->cu_YCP,x,y);
   CHECK(u->cu_XCP==x&&u->cu_XCCP==x&&u->cu_YCP==y&&u->cu_YCCP==y);count++;
  }
  printf("POSITION_GROUP kind=%d hidden=%d PASS\n",kind,hidden);fflush(stdout);
 }
 CHECK(put("\x1b[>9l\x1b[?25l\x1b]50;topaz,8\x07\x1b[38;2;230;230;230m\x1b[48;2;25;60;80m\x1b[2J\x1b[HPASS: absolute positioning, wide coordinates and omitted fields.\r\nVisible and hidden consoles agree; 68 native cases passed.\r\n\x1b[5dRow five via VPA\x1b[7;1fRow seven via HVP\x1b[9;1HColumn ten:\x1b[10`X"));
 if(kind){printf("POSITION_PASS cases=%d\n",count);fflush(stdout);Delay(600);}
 CloseDevice((struct IORequest*)io);opened=0;CloseWindow(w);w=NULL;
 }rc=0;
out:if(opened)CloseDevice((struct IORequest*)io);if(w)CloseWindow(w);if(io)DeleteIORequest((struct IORequest*)io);if(p)DeleteMsgPort(p);printf("POSITION_SCRIPT rc=%d\n",rc);return rc;
}
