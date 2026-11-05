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
static int reply(const char*expected){char got[64]={0};int length=strlen(expected);io->io_Command=CMD_READ;io->io_Data=got;io->io_Length=length;SendIO((struct IORequest*)io);
 for(int n=0;n<100;n++){if(CheckIO((struct IORequest*)io)){WaitIO((struct IORequest*)io);if(io->io_Error||io->io_Actual!=length||memcmp(got,expected,length)){printf("RESET_BYTES actual=%lu expected=%d first=%02x\n",(unsigned long)io->io_Actual,length,(unsigned char)got[0]);return 0;}return 1;}Delay(1);}
 AbortIO((struct IORequest*)io);WaitIO((struct IORequest*)io);puts("RESET_TIMEOUT");return 0;}


#define CHECK(c) do{if(!(c)){printf("RESET_FAIL line=%d kind=%d hidden=%d alt=%d\n",__LINE__,kind,hidden,alt);fflush(stdout);Delay(500);goto out;}}while(0)
static ULONG hash(struct Window*w,struct ConUnit*u){ULONG h=2166136261UL;for(int y=u->cu_YROrigin;y<u->cu_YROrigin+(u->cu_YMax+1)*u->cu_YRSize;y++)for(int x=u->cu_XROrigin;x<u->cu_XROrigin+(u->cu_XMax+1)*u->cu_XRSize;x++)h=(h^ReadRGBPixel(w->RPort,x,y))*16777619UL;return h;}
int main(void){int rc=20,opened=0,kind=0,hidden=0,alt=0,cases=0;struct Window*w=NULL;struct MsgPort*p=CreateMsgPort();io=p?(void*)CreateIORequest(p,sizeof(*io)):NULL;CHECK(io);CyberGfxBase=OpenLibrary((CONST_STRPTR)"cybergraphics.library",0);CHECK(CyberGfxBase);
w=OpenWindowTags(NULL,WA_Left,30,WA_Top,30,WA_Width,730,WA_Height,470,WA_Title,(IPTR)"ConsoleNG terminal reset",WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_SizeGadget,TRUE,WA_SizeBRight,TRUE,WA_MinWidth,300,WA_MinHeight,200,WA_MaxWidth,-1,WA_MaxHeight,-1,TAG_DONE);CHECK(w);
for(kind=0;kind<2;kind++){
 io->io_Data=w;io->io_Length=sizeof(*w);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",kind,(struct IORequest*)io,0));opened=1;struct ConUnit*u=(void*)io->io_Unit;
 CHECK(put("\x1b]11;#203040\x07\x1b" "c\x1b[?25l"));ULONG blank=hash(w,u);
 for(hidden=0;hidden<2;hidden++)for(alt=0;alt<(kind?2:1);alt++){
  CHECK(put("\x1b[?25l"));for(int n=0;n<80;n++)CHECK(put("old scrollback must disappear\r\n"));
  if(alt)CHECK(put("\x1b[?1049hALTERNATE"));
  CHECK(put("\x1b[3;6r\x1b[?6h\x1b[?7l\x1b[4h\x1b[20h\x1b[?1;1003;1006;1004;2004h\x1b=\x1b[3g\x1b[2;3H\x1b[1;7;31;42m\x1b" "7"));
  if(hidden){CHECK(put("\x1b[>9h"));}ULONG before=hash(w,u);
  CHECK(split("\x1b" "c"));CHECK(u->cu_XCCP==0&&u->cu_YCCP==0&&!u->cu_TxFlags);
  if(hidden)CHECK(hash(w,u)==before);
  int modes[]={1,6,7,25,1003,1006,1004,2004};for(int n=0;n<8;n++){char q[32],a[32];snprintf(q,sizeof(q),"\x1b[?%d$p",modes[n]);snprintf(a,sizeof(a),"\x1b[?%d;%d$y",modes[n],modes[n]==7||modes[n]==25?1:2);CHECK(put(q));CHECK(reply(a));}
  CHECK(put("\x1b[4$p"));CHECK(reply("\x1b[4;2$y"));CHECK(put("\x1b[20$p"));CHECK(reply("\x1b[20;2$y"));
  if(kind){CHECK(put("\x1b[?1049$p"));CHECK(reply("\x1b[?1049;2$y"));}
  CHECK(put("\x1b[48;2;100;20;30m\x1b[>9l\x1b[?25l"));CHECK(hash(w,u)==blank);
  CHECK(put("\t"));CHECK(u->cu_XCCP==8&&u->cu_YCCP==0);
  CHECK(put("\x1b[5;5H\x1b" "8"));CHECK(u->cu_XCCP==0&&u->cu_YCCP==0);
  CHECK(put("\x1b[99999d"));CHECK(u->cu_YCCP==u->cu_YMax);
  if(kind){CHECK(put("\x1b[?1049hNEW ALT\x1b[?1049l\x1b[?25l\x1b[>9h\x1b[>9l"));CHECK(hash(w,u)==blank);}
  printf("RESET_CASE kind=%d hidden=%d alt=%d PASS\n",kind,hidden,alt);fflush(stdout);cases++;
 }
 CloseDevice((struct IORequest*)io);opened=0;
}
io->io_Data=w;io->io_Length=sizeof(*w);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",1,(struct IORequest*)io,0));opened=1;
CHECK(put("\x1b]50;DejaVuSansMono.font,24\x07\x1b]11;#203040\x07\x1b]10;#dddddd\x07\x1b" "c\x1b[?25lConsoleNG terminal reset\r\n\r\nPASS: standard + retained screens\r\nPASS: hidden tabs stay hidden\r\nPASS: alternate screen discarded\r\nPASS: modes, cursor, margins, tab stops\r\n\r\nFont and color preferences preserved."));
printf("RESET_PASS cases=%d\n",cases);fflush(stdout);Delay(600);rc=0;
out:if(opened)CloseDevice((struct IORequest*)io);if(w)CloseWindow(w);if(io)DeleteIORequest((struct IORequest*)io);if(p)DeleteMsgPort(p);if(CyberGfxBase)CloseLibrary(CyberGfxBase);printf("RESET_SCRIPT rc=%d\n",rc);return rc;}
