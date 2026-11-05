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

#define CHECK(c) do{if(!(c)){printf("CHARSET_FAIL line=%d kind=%d hidden=%d utf8=%d slot=%d\n",__LINE__,kind,hidden,utf8,slot);fflush(stdout);Delay(500);goto out;}}while(0)
static ULONG hash(struct Window*w,struct ConUnit*u){ULONG h=2166136261UL;for(int y=u->cu_YROrigin;y<u->cu_YROrigin+4*u->cu_YRSize;y++)for(int x=u->cu_XROrigin;x<u->cu_XROrigin+16*u->cu_XRSize;x++)h=(h^ReadRGBPixel(w->RPort,x,y))*16777619UL;return h;}
static int clear(void){return put("\x1b" "c\x1b[?25l\x1b%G");}
static const char*box="┌────┐\r\n│    │\r\n└────┘";
static const char*dec="lqqqqk\r\nx    x\r\nmqqqqj";
int main(void){int rc=20,opened=0,kind=0,hidden=0,utf8=0,slot=0,cases=0;struct Window*w=NULL;struct MsgPort*p=CreateMsgPort();io=p?(void*)CreateIORequest(p,sizeof(*io)):NULL;CHECK(io);CyberGfxBase=OpenLibrary((CONST_STRPTR)"cybergraphics.library",0);CHECK(CyberGfxBase);
w=OpenWindowTags(NULL,WA_Left,30,WA_Top,30,WA_Width,730,WA_Height,470,WA_Title,(IPTR)"ConsoleNG DEC graphics and Unicode",WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_SizeGadget,TRUE,WA_SizeBRight,TRUE,WA_MinWidth,300,WA_MinHeight,200,WA_MaxWidth,-1,WA_MaxHeight,-1,TAG_DONE);CHECK(w);
for(kind=0;kind<2;kind++){
 io->io_Data=w;io->io_Length=sizeof(*w);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",kind,(struct IORequest*)io,0));opened=1;struct ConUnit*u=(void*)io->io_Unit;
 CHECK(put("\x1b]50;DejaVuSansMono.font,24\x07\x1b]11;#203040\x07\x1b]10;#dddddd\x07"));
 for(hidden=0;hidden<(kind?2:1);hidden++)for(utf8=0;utf8<2;utf8++)for(slot=0;slot<2;slot++){
  CHECK(clear());CHECK(put(box));ULONG expected=hash(w,u);
  CHECK(clear());if(!utf8)CHECK(put("\x1b%@"));if(hidden)CHECK(put("\x1b[>9h"));
  CHECK(split(slot?"\x1b)0\x0e":"\x1b(0"));CHECK(split(dec));
  if(hidden)CHECK(put("\x1b[>9l"));
  CHECK(hash(w,u)==expected);CHECK(u->cu_XCCP==6&&u->cu_YCCP==2);
  if(kind){CHECK(put("\x1b[>9h\x1b[>9l"));CHECK(hash(w,u)==expected);SizeWindow(w,-40,0);Delay(8);SizeWindow(w,40,0);Delay(8);CHECK(hash(w,u)==expected);}
  printf("CHARSET_CASE kind=%d hidden=%d utf8=%d slot=%d PASS\n",kind,hidden,utf8,slot);fflush(stdout);cases++;
 }
 CHECK(clear());CHECK(put(box));ULONG expected=hash(w,u);
 CHECK(clear());CHECK(put("\x1b(0\x1b" "7\x1b(B\x1b" "8"));CHECK(put(dec));CHECK(hash(w,u)==expected);cases++;
 if(kind){CHECK(clear());CHECK(put("\x1b(0\x1b[?1049h\x1b(B\x1b[?1049l"));CHECK(put(dec));CHECK(hash(w,u)==expected);cases++;}
 CHECK(clear());CHECK(split("\x1b(0\x1b(A"));CHECK(put(dec));CHECK(hash(w,u)==expected);cases++;
 CHECK(clear());CHECK(put("lqqk"));expected=hash(w,u);
 CHECK(clear());CHECK(split("\x1b(\x1b(B"));CHECK(put("lqqk"));CHECK(hash(w,u)==expected);cases++;
 CHECK(clear());CHECK(split("\x1b*0\x1b+0"));CHECK(put("lqqk"));CHECK(hash(w,u)==expected);cases++;
 CHECK(put("\x1b(0\x1b)0\x0e"));CHECK(clear());CHECK(put("lqqk"));CHECK(hash(w,u)==expected);cases++;
 CHECK(clear());CHECK(put("─q─"));expected=hash(w,u);
 CHECK(clear());CHECK(put("\x1b)0\x0eq\x0fq\x0eq"));CHECK(hash(w,u)==expected);cases++;
 CloseDevice((struct IORequest*)io);opened=0;
}
io->io_Data=w;io->io_Length=sizeof(*w);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",1,(struct IORequest*)io,0));opened=1;
CHECK(put("\x1b]50;DejaVuSansMono.font,24\x07\x1b]11;#203040\x07\x1b]10;#dddddd\x07"));CHECK(clear());
CHECK(put("DEC line drawing, native ConsoleNG\r\n\r\n\x1b(0lqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqk\r\nx\x1b(B   Same cells as Unicode text   \x1b(0x\r\nmqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqj\x1b(B\r\n\r\nPASS: UTF-8 and classic Latin-1\r\nPASS: fragmented escapes, G0/G1\r\nPASS: saved state, redraw, resize"));
printf("CHARSET_PASS cases=%d\n",cases);fflush(stdout);Delay(600);rc=0;
out:if(opened)CloseDevice((struct IORequest*)io);if(w)CloseWindow(w);if(io)DeleteIORequest((struct IORequest*)io);if(p)DeleteMsgPort(p);if(CyberGfxBase)CloseLibrary(CyberGfxBase);printf("CHARSET_SCRIPT rc=%d\n",rc);return rc;}
