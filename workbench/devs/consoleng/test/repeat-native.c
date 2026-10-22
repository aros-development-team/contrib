/* Native DECOM addressing, reports, cursor save and hidden unit state. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <devices/conunit.h>
#include <proto/graphics.h>
#include <proto/cybergraphics.h>
#include <consoleng.h>
struct Library *CyberGfxBase;
#include <stdio.h>
#include <string.h>
static struct IOStdReq*io;
static int put(const char*s){io->io_Command=CMD_WRITE;io->io_Data=(APTR)s;io->io_Length=strlen(s);return !DoIO((struct IORequest*)io)&&io->io_Actual==strlen(s);}
static int split(const char*s){char b[2]={0};while(*s){b[0]=*s++;if(!put(b))return 0;}return 1;}


#define CHECK(c) do{if(!(c)){printf("REPEAT_FAIL line=%d kind=%d hidden=%d test=%d\n",__LINE__,kind,hidden,test);fflush(stdout);Delay(500);goto out;}}while(0)
static ULONG hash(struct Window*w,struct ConUnit*u){ULONG h=2166136261UL;for(int y=u->cu_YROrigin;y<u->cu_YROrigin+5*u->cu_YRSize;y++)for(int x=u->cu_XROrigin;x<u->cu_XROrigin+(u->cu_XMax+1)*u->cu_XRSize;x++)h=(h^ReadRGBPixel(w->RPort,x,y))*16777619UL;return h;}
static int clear(void){return put("\x1b" "c\x1b[?25l\x1b%G");}
static const char*expected[]={"AAAA","AA","AA","ππππ","中中中","─qqq","q───","A\x1b[31mAAA","abc\x1b[2;1Hccc","Á́́","abcdef\x1b[1;3H\x1b[4hQQQ"};
static const char*actual[]={"A\x1b[3b","A\x1b[b","A\x1b[0b","π\x1b[3b","中\x1b[2b","\x1b(0q\x1b(B\x1b[3b","q\x1b(0\x1b[3b","A\x1b[31m\x1b[3b","abc\x1b[2;1H\x1b[3b","Á\x1b[2b","abcdef\x1b[1;3H\x1b[4hQ\x1b[2b"};
int main(void){int rc=20,opened=0,kind=0,hidden=0,test=0,cases=0;struct Window*w=NULL;struct MsgPort*p=CreateMsgPort();io=p?(void*)CreateIORequest(p,sizeof(*io)):NULL;CHECK(io);CyberGfxBase=OpenLibrary((CONST_STRPTR)"cybergraphics.library",0);CHECK(CyberGfxBase);
w=OpenWindowTags(NULL,WA_Left,30,WA_Top,30,WA_Width,730,WA_Height,470,WA_Title,(IPTR)"ConsoleNG repeat preceding character",WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_SizeGadget,TRUE,WA_SizeBRight,TRUE,WA_MinWidth,300,WA_MinHeight,200,WA_MaxWidth,-1,WA_MaxHeight,-1,TAG_DONE);CHECK(w);
for(kind=0;kind<2;kind++){
 io->io_Data=w;io->io_Length=sizeof(*w);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",kind,(struct IORequest*)io,0));opened=1;struct ConUnit*u=(void*)io->io_Unit;
 CHECK(put("\x1b]50;DejaVuSansMono.font,24\x07\x1b]11;#203040\x07\x1b]10;#dddddd\x07"));CHECK(clear());CHECK(split("\x1b[65535b"));CHECK(u->cu_XCCP==0&&u->cu_YCCP==0);
 for(hidden=0;hidden<(kind?2:1);hidden++)for(test=0;test<13;test++){
  char prefix[64];snprintf(prefix,sizeof(prefix),"\x1b[2;%dH%s",u->cu_XMax-1,test==12?"\x1b[?7l":"");
  CHECK(clear());if(test>=11)CHECK(put(prefix));CHECK(put(test<11?expected[test]:"xxxxxxxxxxxx"));ULONG pixels=hash(w,u);WORD x=u->cu_XCCP,y=u->cu_YCCP;
  CHECK(clear());if(test>=11)CHECK(put(prefix));if(hidden)CHECK(put("\x1b[>9h"));CHECK(split(test<11?actual[test]:"x\x1b[11b"));if(hidden)CHECK(put("\x1b[>9l"));
  CHECK(u->cu_XCCP==x&&u->cu_YCCP==y);CHECK(hash(w,u)==pixels);
  if(kind){CHECK(put("\x1b[>9h\x1b[>9l"));CHECK(hash(w,u)==pixels);}
  printf("REPEAT_CASE kind=%d hidden=%d test=%d PASS\n",kind,hidden,test);fflush(stdout);cases++;
 }
 CHECK(clear());CHECK(put("πππ"));ULONG pixels=hash(w,u);CHECK(clear());CHECK(split("π"));CHECK(put("\x1b%@\x1b[2b"));CHECK(hash(w,u)==pixels);cases++;
 CloseDevice((struct IORequest*)io);opened=0;
}
io->io_Data=w;io->io_Length=sizeof(*w);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",1,(struct IORequest*)io,0));opened=1;
CHECK(put("\x1b]50;DejaVuSansMono.font,24\x07\x1b]11;#203040\x07\x1b]10;#dddddd\x07"));CHECK(clear());
CHECK(put("Repeat preceding character\r\n\r\n\x1b(0lq\x1b[31bk\r\nx\x1b(B  Unicode, colors and wrapping  \x1b(0x\r\nmq\x1b[31bj\x1b(B\r\n\r\nPASS: explicit text matches CSI b\r\nPASS: standard + retained + hidden\r\nPASS: wide text and encoding changes"));
printf("REPEAT_PASS cases=%d\n",cases);fflush(stdout);Delay(600);rc=0;
out:if(opened)CloseDevice((struct IORequest*)io);if(w)CloseWindow(w);if(io)DeleteIORequest((struct IORequest*)io);if(p)DeleteMsgPort(p);if(CyberGfxBase)CloseLibrary(CyberGfxBase);printf("REPEAT_SCRIPT rc=%d\n",rc);return rc;}
