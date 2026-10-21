/* Exact pixel comparisons for SGR 21/24 across immediate and retained output. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/cybergraphics.h>
#include <devices/conunit.h>
#include <stdio.h>
#include <string.h>
struct Library *CyberGfxBase;
static struct IOStdReq *io;
static struct Window *w;
static struct ConUnit *u;
static ULONG reference[8192];
static int targetRow=2,form=0;
static int put(const char*s){io->io_Command=CMD_WRITE;io->io_Data=(APTR)s;io->io_Length=strlen(s);return !DoIO((struct IORequest*)io)&&io->io_Actual==strlen(s);}
static int split(const char*s){char b[2]={0};while(*s){b[0]=*s++;if(!put(b))return 0;}return 1;}
static ULONG pixel(int x,int y){return ReadRGBPixel(w->RPort,u->cu_XROrigin+x,u->cu_YROrigin+targetRow*u->cu_YRSize+y);}
static int verify(int struck,int concealed,ULONG ink){int width=8*u->cu_XRSize,height=u->cu_YRSize;for(int y=0;y<height;y++)for(int x=0;x<width;x++){int period=form==0?4:form==1?2:6, phase=(u->cu_XROrigin+x)%period, row=height-1, active=1; if(form==0){int rise=phase==3?1:phase;row-=rise>=height?height-1:rise;}else if((form==1&&phase)||(form==2&&phase>=4))active=0; ULONG expected=struck&&!concealed&&active&&y==row?ink:reference[y*width+x];if(pixel(x,y)!=expected){printf("STYLEUL_PIXEL x=%d y=%d got=%lx expected=%lx\n",x,y,(unsigned long)pixel(x,y),(unsigned long)expected);return 0;}}return 1;}
static ULONG hashpixels(void){ULONG h=2166136261u;for(int y=0;y<u->cu_YRSize;y++)for(int x=0;x<8*u->cu_XRSize;x++)h=(h^pixel(x,y))*16777619u;return h;}
#define CHECK(c) do{if(!(c)){printf("STYLEUL_FAIL line=%d kind=%d utf=%d fragmented=%d style=%d\n",__LINE__,kind,utf,fragmented,style);fflush(stdout);Delay(500);goto out;}}while(0)
int main(void){int rc=20,opened=0,kind=0,utf=0,fragmented=0,style=0,cases=0,selectors=0;struct MsgPort*p=CreateMsgPort();io=p?(void*)CreateIORequest(p,sizeof(*io)):NULL;CHECK(io);CyberGfxBase=OpenLibrary((CONST_STRPTR)"cybergraphics.library",0);CHECK(CyberGfxBase);
for(kind=0;kind<2;kind++){
 w=OpenWindowTags(NULL,WA_Left,30,WA_Top,30,WA_Width,720,WA_Height,450,WA_Title,(IPTR)"ConsoleNG styled underline - native pixel regression",WA_Activate,TRUE,WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_SizeGadget,TRUE,WA_SizeBRight,TRUE,WA_MinWidth,200,WA_MinHeight,160,WA_MaxWidth,-1,WA_MaxHeight,-1,TAG_DONE);CHECK(w);
 io->io_Data=w;io->io_Length=sizeof(*w);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",kind?CONU_CHARMAP:CONU_STANDARD,(struct IORequest*)io,0));opened=1;u=(void*)io->io_Unit;
 CHECK(put("\x1b[?25l\x1b]50;topaz,8\7"));
for(utf=0;utf<2;utf++)for(fragmented=0;fragmented<2;fragmented++)for(style=0;style<3;style++)for(form=0;form<3;form++){
 const char *on=form==0?"\x1b[4:3m":form==1?"\x1b[4:4m":"\x1b[4:5m", *off="\x1b[24m";
 const char*text=utf?" A\xce\xa9\xe4\xb8\xad Z ":" AB CD  ";
 CHECK(put(utf?"\x1b%G":"\x1b%@"));CHECK(put("\x1b[0;38;2;230;240;245;48;2;25;60;80m\x1b[2J\x1b[H\x1b[7m "));
 ULONG ink=ReadRGBPixel(w->RPort,u->cu_XROrigin,u->cu_YROrigin);
 CHECK(put("\x1b[27m "));
 CHECK(ink!=ReadRGBPixel(w->RPort,u->cu_XROrigin+u->cu_XRSize,u->cu_YROrigin));
 if(style==1)ink=ReadRGBPixel(w->RPort,u->cu_XROrigin+u->cu_XRSize,u->cu_YROrigin);
 CHECK(put(style==1?"\x1b[7m":style==2?"\x1b[8m":"\x1b[27m"));CHECK(put("\x1b[3;1H"));CHECK(put(text));
 int width=8*u->cu_XRSize,height=u->cu_YRSize;CHECK(width*height<=8192);
 for(int y=0;y<height;y++)for(int x=0;x<width;x++)reference[y*width+x]=pixel(x,y);
 CHECK(fragmented?split(on):put(on));CHECK((u->cu_TxFlags&33)==1);
 CHECK(put("\x1b" "7\x1b[0m\x1b" "8"));CHECK((u->cu_TxFlags&33)==1);
 CHECK(put("\x1b[3;1H"));CHECK(fragmented?split(text):put(text));CHECK(verify(1,style==2,ink));
 if(kind){CHECK(put("\x1b[1S"));targetRow=1;CHECK(verify(1,style==2,ink));CHECK(put("\x1b[1T"));targetRow=2;CHECK(verify(1,style==2,ink));CHECK(put("\x1b[?1049hOTHER\x1b[?1049l"));CHECK(verify(1,style==2,ink));CHECK(put("\x1b[>9h\x1b[>9l"));CHECK(verify(1,style==2,ink));SizeWindow(w,16,0);Delay(10);CHECK(verify(1,style==2,ink));SizeWindow(w,-16,0);Delay(10);CHECK(verify(1,style==2,ink));}
 CHECK(fragmented?split(off):put(off));CHECK(!(u->cu_TxFlags&32));CHECK(put("\x1b[3;1H"));CHECK(put(text));CHECK(verify(0,style==2,ink));
 CHECK(put("\x1b[21m\x1b[4m"));CHECK((u->cu_TxFlags&33)==1);CHECK(put("\x1b[21m"));CHECK((u->cu_TxFlags&33)==32);CHECK(put("\x1b[24m"));CHECK(!(u->cu_TxFlags&33));
 CHECK(put("\x1b[4:1m"));CHECK((u->cu_TxFlags&33)==1);CHECK(put("\x1b[4:2m"));CHECK((u->cu_TxFlags&161)==32);CHECK(put("\x1b[4:0m"));CHECK(!(u->cu_TxFlags&33));
 CHECK(put("\x1b[21m\x1b[0m"));CHECK(u->cu_TxFlags==0);
 const char *fallback[]={"\x1b[4:m","\x1b[4:99m","\x1b[4:2:9m","\x1b[4::m","\x1b[4:5:9m","\x1b[4:9999999999999999999m"};
 for(unsigned j=0;j<sizeof(fallback)/sizeof(*fallback);j++){
  CHECK(put(j?"\x1b[0;38;2;230;240;245;48;2;25;60;80;4m":"\x1b[0;38;2;230;240;245;48;2;25;60;80m"));CHECK(put("\x1b[3;1H"));CHECK(put(text));ULONG expected=hashpixels();
  CHECK(put("\x1b[21m"));CHECK(fragmented?split(fallback[j]):put(fallback[j]));CHECK((u->cu_TxFlags&33)==(j?1:0));
  CHECK(put("\x1b[3;1H"));CHECK(put(text));CHECK(hashpixels()==expected);
  if(kind){CHECK(put("\x1b[>9h\x1b[>9l"));CHECK(hashpixels()==expected);}
  selectors++;
 }
 CHECK(put("\x1b[0m"));cases++;
}
 if(kind){CHECK(put("\x1b%G\x1b[0m\x1b[2J\x1b[HPASS: 72 native styled underline pixel comparisons.\r\n\r\nNormal text:        ConsoleNG\r\nCurly underline:   \x1b[4:3mConsoleNG\x1b[24m\r\nDotted Unicode:    \x1b[4:4mA \xce\xa9 \xe4\xb8\xad Z\x1b[24m\r\nDashed + strike:    \x1b[4:5;9mConsoleNG\x1b[0m\r\n\r\nSGR underline selectors, reset, saved cursor, reverse and conceal.\r\nBytewise input, retained redraw and window resize."));printf("STYLEUL_PASS cases=%d selectors=%d\n",cases,selectors);fflush(stdout);Delay(600);}
 CloseDevice((struct IORequest*)io);opened=0;CloseWindow(w);w=NULL;
}rc=0;
out:if(opened)CloseDevice((struct IORequest*)io);if(w)CloseWindow(w);if(io)DeleteIORequest((struct IORequest*)io);if(p)DeleteMsgPort(p);if(CyberGfxBase)CloseLibrary(CyberGfxBase);printf("STYLEUL_SCRIPT rc=%d\n",rc);return rc;
}
