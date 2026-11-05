/* Plain-console width tracking must follow raster lifecycle, not just edits. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/cybergraphics.h>
#include <devices/conunit.h>
#include <stdio.h>
#include <string.h>
struct Library *CyberGfxBase;
static struct IOStdReq *io;
static struct Window *w;
static struct ConUnit *u;
static int put(const char *s){io->io_Command=CMD_WRITE;io->io_Data=(APTR)s;io->io_Length=strlen(s);return !DoIO((struct IORequest*)io)&&io->io_Actual==strlen(s);}
static ULONG pixels(void){ULONG h=2166136261u;for(int y=0;y<3*u->cu_YRSize;y++)for(int x=0;x<8*u->cu_XRSize;x++)h=(h^ReadRGBPixel(w->RPort,u->cu_XROrigin+x,u->cu_YROrigin+y))*16777619u;return h;}
#define WIDE "\xe2\x98\x95"
#define HOME "\x1b[1;3H"
#define RESET "\x1b[0;38;2;230;240;245;48;2;25;60;80m\x1b[4l\x1b[r\x1b[2J\x1b[H"
#define CHECK(c) do{if(!(c)){printf("WIDTHLIFE_FAIL line=%d case=%d\n",__LINE__,test);fflush(stdout);Delay(500);goto out;}}while(0)
int main(void){int rc=20,opened=0,test=0;struct MsgPort *p=CreateMsgPort();
io=p?(void*)CreateIORequest(p,sizeof(*io)):NULL;CHECK(io);CyberGfxBase=OpenLibrary((CONST_STRPTR)"cybergraphics.library",0);CHECK(CyberGfxBase);
w=OpenWindowTags(NULL,WA_Left,30,WA_Top,30,WA_Width,720,WA_Height,450,WA_Title,(IPTR)"ConsoleNG standard wide-cell lifecycle",WA_Activate,TRUE,WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_SizeGadget,TRUE,WA_SizeBRight,TRUE,WA_MinWidth,200,WA_MinHeight,160,WA_MaxWidth,-1,WA_MaxHeight,-1,TAG_DONE);CHECK(w);
io->io_Data=w;io->io_Length=sizeof(*w);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",CONU_STANDARD,(struct IORequest*)io,0));opened=1;u=(void*)io->io_Unit;
CHECK(put("\x1b%G\x1b[?25l\x1b]50;DejaVuSansMono.font,16\7"));CHECK(u->cu_YRSize==16);CHECK(put(RESET));ULONG blank=pixels();CHECK(put(WIDE));CHECK(u->cu_XCP==2&&pixels()!=blank);
static const struct {const char *name,*actual,*expected;} cases[]={
 {"overwrite-tail",HOME WIDE "\x1b[1;4HX","\x1b[1;4HX"},
 {"overwrite-head",HOME WIDE HOME "X",HOME "X"},
 {"erase-tail",HOME WIDE "\x1b[1;4H\x1b[K",""},
 {"erase-head",HOME WIDE HOME "\x1b[1K",""},
 {"display-erase-tail",HOME WIDE "\x1b[1;4H\x1b[J",""},
 {"display-erase-head",HOME WIDE HOME "\x1b[1J",""},
 {"clear-stale-width",HOME WIDE "\x1b[2J\x1b[1;4HX" HOME "\x1b[X","\x1b[1;4HX"},
 {"insert-print-tail",HOME WIDE "AB\x1b[1;4H\x1b[4hX\x1b[4l","\x1b[1;4HX AB"},
 {"scroll-up", "\x1b[2;3H" WIDE "\x1b[S\x1b[1;4H\x1b[X",""},
 {"scroll-down",HOME WIDE "\x1b[T\x1b[2;4H\x1b[X",""},
 {"insert-line",HOME WIDE "\x1b[H\x1b[L\x1b[2;4H\x1b[X",""},
 {"delete-line","\x1b[2;3H" WIDE "\x1b[H\x1b[M\x1b[1;4H\x1b[X",""},
 {"partial-region", "\x1b[2;4r\x1b[3;3H" WIDE "\x1b[S\x1b[2;4H\x1b[X",""},
 {"full-span-scroll",HOME WIDE "\x1b[999S\x1b[1;4HX" HOME "\x1b[X","\x1b[1;4HX"},
 {"same-font-reset",HOME WIDE "\x1b]50;DejaVuSansMono.font,16\7\x1b[1;4HX" HOME "\x1b[X","\x1b[1;4HX"},
 {"failed-font-preserves-width",HOME WIDE "\x1b]50;ConsoleNG-No-Such-Font.font,16\7\x1b[1;4HX","\x1b[1;4HX"},
 {"adjacent-split",HOME WIDE WIDE "\x1b[1;4H\x1b[2P",""},
};
for(test=0;test<(int)(sizeof(cases)/sizeof(*cases));test++){
 CHECK(put(RESET));CHECK(put(cases[test].actual));CHECK(u->cu_YRSize==16);ULONG got=pixels();
 CHECK(put(RESET));CHECK(put(cases[test].expected));ULONG want=pixels();
 printf("WIDTHLIFE_CASE %s got=%08lx expected=%08lx\n",cases[test].name,(unsigned long)got,(unsigned long)want);fflush(stdout);CHECK(got==want);
}
/* A geometric grow preserves existing width positions. */
int oldColumns=u->cu_XMax+1;CHECK(put(RESET HOME WIDE "\x1b[H"));
SizeWindow(w,2*u->cu_XRSize,0);
for(int ticks=0;ticks<250&&u->cu_XMax+1!=oldColumns+2;ticks++)Delay(1);
CHECK(u->cu_XMax+1==oldColumns+2);CHECK(put("\x1b[?25l\x1b[1;4HX"));ULONG grown=pixels();
CHECK(put(RESET "\x1b[1;4HX"));CHECK(pixels()==grown);test++;
/* Shrink through the final wide pair: the remaining head must be blank. */
CHECK(put(RESET));ULONG background=ReadRGBPixel(w->RPort,u->cu_XROrigin,u->cu_YROrigin);
int columns=u->cu_XMax+1;char command[80];snprintf(command,sizeof(command),"\x1b[1;%dH" WIDE "\x1b[H",columns-1);CHECK(put(command));
SizeWindow(w,-u->cu_XRSize,0);
for(int ticks=0;ticks<250&&u->cu_XMax+1!=columns-1;ticks++)Delay(1);
CHECK(u->cu_XMax+1==columns-1);CHECK(put("\x1b[?25l"));
for(int y=0;y<u->cu_YRSize;y++)for(int x=0;x<u->cu_XRSize;x++)CHECK(ReadRGBPixel(w->RPort,u->cu_XROrigin+u->cu_XMax*u->cu_XRSize+x,u->cu_YROrigin+y)==background);
test++;
CHECK(put(RESET "PASS: standard wide-cell lifecycle\r\n\r\nOverwrite, erase, insert mode, scroll and font reset\r\nmatch independently rendered reference pixels.\r\nAdjacent wide glyph fragments do not merge."));
printf("WIDTHLIFE_PASS cases=%d\n",test);fflush(stdout);Delay(500);rc=0;
out:if(opened)CloseDevice((struct IORequest*)io);if(w)CloseWindow(w);if(CyberGfxBase)CloseLibrary(CyberGfxBase);if(io)DeleteIORequest((struct IORequest*)io);if(p)DeleteMsgPort(p);printf("WIDTHLIFE_SCRIPT rc=%d\n",rc);return rc;}
