/* Native DECSTBM: fixed outer rows, edits, hidden tabs and resize reset. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/cybergraphics.h>
#include <devices/conunit.h>
#include <stdio.h>
#include <string.h>
struct Library *CyberGfxBase;
static struct IOStdReq *io;static struct Window*w;static struct ConUnit*u;
static int put(const char*s){io->io_Command=CMD_WRITE;io->io_Data=(APTR)s;io->io_Length=strlen(s);return !DoIO((struct IORequest*)io)&&io->io_Actual==strlen(s);}
static ULONG colors[]={0x203040,0x305070,0x604020,0x206040,0x602050,0x405080,0x806020,0x204060,0x121212};
static int seed(void){char b[160];if(!put("\x1b[r\x1b[?25l\x1b[0m\x1b[2J"))return 0;for(int i=0;i<8;i++){ULONG c=colors[i];snprintf(b,sizeof(b),"\x1b[%d;1H\x1b[48;2;%lu;%lu;%lum\x1b[2K",i+1,(unsigned long)(c>>16),(unsigned long)((c>>8)&255),(unsigned long)(c&255));if(!put(b))return 0;
}return put("\x1b[48;2;18;18;18m\x1b[3;6r");}
static int rows(const int*expected){for(int i=0;i<8;i++){ULONG rgb=ReadRGBPixel(w->RPort,u->cu_XROrigin+20,u->cu_YROrigin+i*u->cu_YRSize+u->cu_YRSize/2);if(rgb!=colors[expected[i]]){printf("REGION_PIXEL row=%d got=%lx want=%lx\n",i,(unsigned long)rgb,(unsigned long)colors[expected[i]]);return 0;}}return 1;}
#define CHECK(c) do{if(!(c)){printf("REGION_FAIL line=%d unit=%d hidden=%d op=%d\n",__LINE__,unit,hidden,op);fflush(stdout);Delay(500);goto out;}}while(0)
int main(void){int rc=20,opened=0,unit=0,hidden=0,op=0,cases=0;struct MsgPort*p=CreateMsgPort();io=p?(void*)CreateIORequest(p,sizeof(*io)):NULL;CHECK(io);CyberGfxBase=OpenLibrary((CONST_STRPTR)"cybergraphics.library",0);CHECK(CyberGfxBase);
w=OpenWindowTags(NULL,WA_Left,30,WA_Top,30,WA_Width,730,WA_Height,470,WA_Title,(IPTR)"ConsoleNG scrolling region: header and footer stay fixed",WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_SizeGadget,TRUE,WA_SizeBRight,TRUE,WA_MinWidth,300,WA_MinHeight,200,WA_MaxWidth,-1,WA_MaxHeight,-1,TAG_DONE);CHECK(w);
const char*commands[]={"\x1b[2S","\x1b[2T","\x1b[4;1H\x1b[L","\x1b[4;1H\x1b[M","\x1b[6;1H\n","\x1b[3;1H\x1bM","\x1b[1;1H\x1b[L","\x1b[8;1H\x1b[M","\x1b[65535S","\x1b[65535T"};
for(unit=0;unit<2;unit++){
 io->io_Data=w;io->io_Length=sizeof(*w);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",unit,(struct IORequest*)io,0));opened=1;u=(void*)io->io_Unit;
 for(hidden=0;hidden<=(unit?1:0);hidden++)for(op=0;op<10;op++){
  CHECK(seed());CHECK(u->cu_YCCP==0&&u->cu_XCCP==0);int expected[]={0,1,2,3,4,5,6,7};int first=2,last=5,n=op<2?2:op>=8?4:1;int down=op==1||op==2||op==5||op==9;
  if(op==2||op==3)first=3;
  if(op!=6&&op!=7){for(int y=first;y<=last;y++){int source=down?y-n:y+n;expected[y]=source<first||source>last?8:source;}}
  if(hidden){CHECK(put("\x1b[>9h"));}
  CHECK(put(commands[op]));
  if(hidden){CHECK(put("\x1b[>9l"));}
  CHECK(rows(expected));
  if(unit){CHECK(put("\x1b[>9h\x1b[>9l"));CHECK(rows(expected));}
  printf("REGION_CASE unit=%d hidden=%d op=%d PASS\n",unit,hidden,op);fflush(stdout);cases++;
 }
 /* Invalid range ignored; split valid range accepted; reset after geometry change. */
 CHECK(seed());CHECK(put("\x1b[5;4r\x1b[6;1H\n"));CHECK(u->cu_YCCP==5);
 CHECK(put("\x1b[2;"));CHECK(put("4r\x1b[4;1H\n"));CHECK(u->cu_YCCP==3);
 CHECK(put("\x1b[r\x1b[6;1H\n"));CHECK(u->cu_YCCP==6);
 CHECK(put("\x1b[3;6r\x1b[6;1H"));SizeWindow(w,0,-16);Delay(15);CHECK(put("\n"));CHECK(u->cu_YCCP==6);SizeWindow(w,0,16);Delay(15);
 CloseDevice((struct IORequest*)io);opened=0;
}
io->io_Data=w;io->io_Length=sizeof(*w);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",1,(struct IORequest*)io,0));opened=1;u=(void*)io->io_Unit;CHECK(seed());CHECK(put("\x1b[r\x1b[1;1H\x1b[37mFixed header\x1b[3;1HScroll region starts here\x1b[6;1HScroll region ends here\x1b[8;1HFixed footer\x1b[10;1H\x1b[40m30 native region checks PASS"));
printf("REGION_PASS cases=%d\n",cases);fflush(stdout);Delay(600);rc=0;
out:if(opened)CloseDevice((struct IORequest*)io);if(w)CloseWindow(w);if(io)DeleteIORequest((struct IORequest*)io);if(p)DeleteMsgPort(p);if(CyberGfxBase)CloseLibrary(CyberGfxBase);printf("REGION_SCRIPT rc=%d\n",rc);return rc;}
