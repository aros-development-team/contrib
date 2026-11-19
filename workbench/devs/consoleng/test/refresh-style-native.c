/* First retained run must set style even when all style bits are enabled. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/cybergraphics.h>
#include <devices/conunit.h>
#include <stdio.h>
#include <string.h>
struct Library *CyberGfxBase;
static struct IOStdReq *io;static struct Window *w;static struct ConUnit *u;
static ULONG baseline[65536];
static void snapshot(void){int i=0;for(int y=0;y<3*u->cu_YRSize;y++)for(int x=0;x<20*u->cu_XRSize;x++)baseline[i++]=ReadRGBPixel(w->RPort,u->cu_XROrigin+x,u->cu_YROrigin+y);}
static void differences(void){int i=0,n=0;for(int y=0;y<3*u->cu_YRSize;y++)for(int x=0;x<20*u->cu_XRSize;x++){ULONG c=ReadRGBPixel(w->RPort,u->cu_XROrigin+x,u->cu_YROrigin+y);if(c!=baseline[i]&&n++<12)printf("PIXEL x=%d y=%d before=%08lx after=%08lx\n",x,y,(unsigned long)baseline[i],(unsigned long)c);i++;}printf("PIXEL count=%d\n",n);}
static int put(const char *s){io->io_Command=CMD_WRITE;io->io_Data=(APTR)s;io->io_Length=strlen(s);return !DoIO((struct IORequest*)io)&&io->io_Actual==strlen(s);}
static ULONG pixels(void){ULONG h=2166136261UL;for(int y=0;y<3*u->cu_YRSize;y++)for(int x=0;x<20*u->cu_XRSize;x++)h=(h^ReadRGBPixel(w->RPort,u->cu_XROrigin+x,u->cu_YROrigin+y))*16777619UL;return h;}
#define CHECK(c) do{if(!(c)){printf("REFRESHSTYLE_FAIL line=%d check=%s kind=%d fragment=%d style=%d code=%d\n",__LINE__,#c,kind,fragment,style,code);fflush(stdout);Delay(500);goto out;}}while(0)
int main(void){int rc=20,opened=0,kind=1,fragment=0,style=7,code=0,cases=0;
struct MsgPort *p=CreateMsgPort();io=p?(void*)CreateIORequest(p,sizeof(*io)):NULL;CHECK(io);
CyberGfxBase=OpenLibrary((CONST_STRPTR)"cybergraphics.library",0);CHECK(CyberGfxBase);
for(int test=0;test<8;test++){
 style=7-test;
 w=OpenWindowTags(NULL,WA_Left,30,WA_Top,30,WA_Width,720,WA_Height,450,WA_Title,(IPTR)"ConsoleNG first-run style redraw",WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_SizeGadget,TRUE,WA_SizeBRight,TRUE,WA_MinWidth,200,WA_MinHeight,160,WA_MaxWidth,-1,WA_MaxHeight,-1,TAG_DONE);CHECK(w);
 io->io_Data=w;io->io_Length=sizeof(*w);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",CONU_CHARMAP,(struct IORequest*)io,0));opened=1;u=(void*)io->io_Unit;
 CHECK(put("\x1b%G\x1b[?25l\x1b]50;DejaVuSansMono,16\7"));
 char seq[80];snprintf(seq,sizeof(seq),"\x1b[0;%d;%d;%dm",style&1?1:22,style&2?3:23,style&4?4:24);CHECK(put(seq));
 CHECK(put("Styled A\xce\xa9 Z\r\n\x1b[0mPlain second row"));
 ULONG expected=pixels();snapshot();CHECK(put("\x1b[>9h\x1b[>9l"));if(pixels()!=expected)differences();CHECK(pixels()==expected);
 CHECK(put("\x1b[?1049hOTHER\x1b[?1049l"));CHECK(pixels()==expected);cases++;
 printf("REFRESHSTYLE_CASE style=%d PASS\n",style);fflush(stdout);
 if(test==7){CHECK(put("\r\n\r\nPASS: all eight first-run style combinations\r\nFresh retained console, redraw and alternate return"));printf("REFRESHSTYLE_PASS cases=%d\n",cases);fflush(stdout);Delay(150);}
 CloseDevice((struct IORequest*)io);opened=0;CloseWindow(w);w=NULL;
}
rc=0;
out:if(opened)CloseDevice((struct IORequest*)io);if(w)CloseWindow(w);if(CyberGfxBase)CloseLibrary(CyberGfxBase);if(io)DeleteIORequest((struct IORequest*)io);if(p)DeleteMsgPort(p);printf("REFRESHSTYLE_RUN rc=%d\n",rc);return rc;}
