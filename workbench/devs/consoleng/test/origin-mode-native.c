/* Native DECOM addressing, reports, cursor save and hidden unit state. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <devices/conunit.h>
#include <stdio.h>
#include <string.h>
static struct IOStdReq*io;
static int put(const char*s){io->io_Command=CMD_WRITE;io->io_Data=(APTR)s;io->io_Length=strlen(s);return !DoIO((struct IORequest*)io)&&io->io_Actual==strlen(s);}
static int split(const char*s){char b[2]={0};while(*s){b[0]=*s++;if(!put(b))return 0;}return 1;}
static int reply(const char*expected){char got[64]={0};int length=strlen(expected);io->io_Command=CMD_READ;io->io_Data=got;io->io_Length=length;SendIO((struct IORequest*)io);
 for(int n=0;n<100;n++){if(CheckIO((struct IORequest*)io)){WaitIO((struct IORequest*)io);if(io->io_Error||io->io_Actual!=length||memcmp(got,expected,length)){printf("ORIGIN_BYTES actual=%lu expected=%d first=%02x\n",(unsigned long)io->io_Actual,length,(unsigned char)got[0]);return 0;}return 1;}Delay(1);}
 AbortIO((struct IORequest*)io);WaitIO((struct IORequest*)io);puts("ORIGIN_TIMEOUT");return 0;}

#define CHECK(c) do{if(!(c)){printf("ORIGIN_FAIL line=%d kind=%d hidden=%d\n",__LINE__,kind,hidden);fflush(stdout);Delay(500);goto out;}}while(0)
int main(void){int rc=20,opened=0,kind=0,hidden=0,cases=0;struct Window*w=NULL;struct MsgPort*p=CreateMsgPort();io=p?(void*)CreateIORequest(p,sizeof(*io)):NULL;CHECK(io);
w=OpenWindowTags(NULL,WA_Left,30,WA_Top,30,WA_Width,730,WA_Height,470,WA_Title,(IPTR)"ConsoleNG origin mode: coordinates relative to scrolling region",WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_SizeGadget,TRUE,WA_SizeBRight,TRUE,WA_MinWidth,300,WA_MinHeight,200,WA_MaxWidth,-1,WA_MaxHeight,-1,TAG_DONE);CHECK(w);
for(kind=0;kind<2;kind++){
 io->io_Data=w;io->io_Length=sizeof(*w);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",kind,(struct IORequest*)io,0));opened=1;struct ConUnit*u=(void*)io->io_Unit;
 CHECK(put("\x1b[?25l\x1b[3;6r\x1b[?6h\x1b" "8"));CHECK(u->cu_YCCP==0);CHECK(put("\x1b[?6$p"));CHECK(reply("\x1b[?6;2$y"));
 for(hidden=0;hidden<2;hidden++){
  CHECK(put(hidden?"\x1b[>9h":"\x1b[>9l"));
  CHECK(put("\x1b[?6l\x1b[3;6r"));CHECK(u->cu_YCCP==0);
  CHECK(split("\x1b[?6h"));CHECK(u->cu_YCCP==2&&u->cu_XCCP==0);
  CHECK(put("\x1b[99999;99999H"));CHECK(u->cu_YCCP==5&&u->cu_XCCP==u->cu_XMax);
  CHECK(put("\x1b[2d\x1b[3G"));CHECK(u->cu_YCCP==3&&u->cu_XCCP==2);
  CHECK(split("\x1b[6n"));CHECK(reply("\x1b[2;3R"));
  CHECK(put("\x9b" "6n"));CHECK(reply("\x9b" "4;3R"));
  CHECK(put("\x1b[?6$p"));CHECK(reply("\x1b[?6;1$y"));cases++;
  CHECK(put("\x1b" "7\x1b[?6l"));CHECK(u->cu_YCCP==0&&u->cu_XCCP==0);
  CHECK(put("\x1b" "8"));CHECK(u->cu_YCCP==3&&u->cu_XCCP==2);
  CHECK(put("\x1b[?6$p"));CHECK(reply("\x1b[?6;1$y"));cases++;
  CHECK(put("\x1b[4;8r"));CHECK(u->cu_YCCP==3&&u->cu_XCCP==0);
  CHECK(put("\x1b[0;0H"));CHECK(u->cu_YCCP==3);
  CHECK(put("\x1b[65535d"));CHECK(u->cu_YCCP==7);
  CHECK(put("\x1b" "8"));CHECK(u->cu_YCCP==3&&u->cu_XCCP==2);
  CHECK(put("\x1b[6n"));CHECK(reply("\x1b[1;3R"));cases++;
  CHECK(put("\x1b[?6l\x1b[2;4H\x1b" "7\x1b[?6h\x1b" "8"));CHECK(u->cu_YCCP==1&&u->cu_XCCP==3);
  CHECK(put("\x1b[?6$p"));CHECK(reply("\x1b[?6;2$y"));cases++;
  if(kind){
   CHECK(put("\x1b[3;6r\x1b[?6h\x1b[2;3H\x1b[?1049h\x1b[?6l\x1b" "7\x1b[?1049l"));
   CHECK(u->cu_YCCP==3&&u->cu_XCCP==2);CHECK(put("\x1b[?6$p"));CHECK(reply("\x1b[?6;1$y"));
   CHECK(put("\x1b[?6l\x1b" "8"));CHECK(u->cu_YCCP==3&&u->cu_XCCP==2);cases++;
  }
  CHECK(put("\x1b[?6h\x1b[r\x1b[H"));CHECK(u->cu_YCCP==0);
  CHECK(put("\x1b[3;6r\x1b[H"));CHECK(u->cu_YCCP==2);cases++;
 }
 CHECK(put("\x1b[>9l\x1b[?6h\x1b[3;6r"));SizeWindow(w,0,-16);Delay(12);CHECK(put("\x1b[H"));CHECK(u->cu_YCCP==0);SizeWindow(w,0,16);Delay(12);
 printf("ORIGIN_UNIT kind=%d PASS\n",kind);fflush(stdout);CloseDevice((struct IORequest*)io);opened=0;
}
io->io_Data=w;io->io_Length=sizeof(*w);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",1,(struct IORequest*)io,0));opened=1;
CHECK(put("\x1b[?25l\x1b]50;DejaVuSansMono.font,24\x07\x1b[38;2;220;220;220;48;2;25;60;80m\x1b[2J\x1b[HFixed header: physical row 1\x1b[9;1HFixed footer: physical row 9\x1b[3;7r\x1b[?6h\x1b[1;1HRegion row 1 = physical row 3\x1b[3;1H22 origin / report / saved-state checks PASS"));
printf("ORIGIN_PASS cases=%d\n",cases);fflush(stdout);Delay(600);rc=0;
out:if(opened)CloseDevice((struct IORequest*)io);if(w)CloseWindow(w);if(io)DeleteIORequest((struct IORequest*)io);if(p)DeleteMsgPort(p);printf("ORIGIN_SCRIPT rc=%d\n",rc);return rc;}
