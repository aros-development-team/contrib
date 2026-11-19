/* Real DOS cooked/raw streams: underline colors, fragmentation and retained pixels. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/cybergraphics.h>
#include <devices/conunit.h>
#include <dos/dosextens.h>
#include <stdio.h>
#include <string.h>
struct Library *CyberGfxBase;
static struct IOStdReq *io;
static BPTR stream;
static int fragmented;
static struct Window *w;
static struct ConUnit *u;
static ULONG reference[8192];
static int targetRow=2,form=0;
static int put(const char*s){ULONG n=strlen(s);if(!fragmented)return Write(stream,s,n)==(LONG)n;for(ULONG i=0;i<n;i++)if(Write(stream,s+i,1)!=1)return 0;return 1;}
static int split(const char*s){char b[2]={0};while(*s){b[0]=*s++;if(!put(b))return 0;}return 1;}
static ULONG penrgb(UBYTE pen){ULONG rgb[3];GetRGB32(w->WScreen->ViewPort.ColorMap,pen,1,rgb);return ((rgb[0]>>24)<<16)|((rgb[1]>>24)<<8)|(rgb[2]>>24);}
static ULONG pixel(int x,int y){return ReadRGBPixel(w->RPort,u->cu_XROrigin+x,u->cu_YROrigin+targetRow*u->cu_YRSize+y);}
static int verify(int struck,int concealed,ULONG ink){int width=8*u->cu_XRSize,height=u->cu_YRSize;for(int y=0;y<height;y++)for(int x=0;x<width;x++){int period=form==0?4:form==1?2:6, phase=(u->cu_XROrigin+x)%period, row=height-1, active=1; if(form==0){int rise=phase==3?1:phase;row-=rise>=height?height-1:rise;}else if((form==1&&phase)||(form==2&&phase>=4))active=0; if(form==3){row=w->RPort->Font->tf_Baseline+2;if(row>=height)row=height-1;active=1;}if(form==4){row=height-1;active=1;} ULONG expected=struck&&!concealed&&active&&(y==row||(form==4&&y==height-3))?ink:reference[y*width+x];if(pixel(x,y)!=expected){printf("ULCDOS_PIXEL x=%d y=%d got=%lx expected=%lx\n",x,y,(unsigned long)pixel(x,y),(unsigned long)expected);return 0;}}return 1;}
static ULONG hashpixels(void){ULONG h=2166136261u;for(int y=0;y<u->cu_YRSize;y++)for(int x=0;x<8*u->cu_XRSize;x++)h=(h^pixel(x,y))*16777619u;return h;}
#define CHECK(c) do{if(!(c)){printf("ULCDOS_FAIL line=%d kind=%d utf=%d fragmented=%d style=%d font=%d form=%d\n",__LINE__,kind,utf,fragmented,style,font,form);fflush(stdout);Delay(500);goto out;}}while(0)
int main(void){int rc=20,kind=0,utf=0,style=0,cases=0,selectors=0,colorform=0,font=0;
CyberGfxBase=OpenLibrary((CONST_STRPTR)"cybergraphics.library",0);CHECK(CyberGfxBase);
for(kind=0;kind<2;kind++){
 struct InfoData info;
 stream=Open((CONST_STRPTR)"RGCON:",MODE_OLDFILE);CHECK(stream);
 struct FileHandle*fh=BADDR(stream);
 CHECK(DoPkt(fh->fh_Type,ACTION_DISK_INFO,(SIPTR)MKBADDR(&info),0,0,0,0));
 io=(void*)(IPTR)info.id_InUse;CHECK(io&&io->io_Unit&&io->io_Device);
 CHECK(!strcmp((char*)io->io_Device->dd_Library.lib_Node.ln_Name,"consoleng.device"));
 u=(void*)io->io_Unit;w=u->cu_Window;CHECK(w);
 ActivateWindow(w);
 SetWindowTitles(w,(CONST_STRPTR)"ConsoleNG underline colors through DOS streams",(CONST_STRPTR)-1);
 MoveWindow(w,30-w->LeftEdge,30-w->TopEdge);SizeWindow(w,730-w->Width,470-w->Height);Delay(20);
 CHECK(SetMode(stream,kind));fragmented=0;
 CHECK(put("\x1b[?25l"));
for(font=0;font<2;font++){
 CHECK(put(font?"\x1b]50;DejaVuSansMono.font,16\7":"\x1b]50;topaz,8\7"));
for(colorform=0;colorform<2;colorform++)for(utf=0;utf<2;utf++)for(fragmented=0;fragmented<2;fragmented++)for(style=0;style<3;style++)for(form=0;form<5;form++){
 const char *on=form==0?"\x1b[4:3m":form==1?"\x1b[4:4m":form==2?"\x1b[4:5m":form==3?"\x1b[4m":"\x1b[21m", *off="\x1b[24m";
 const char*text=utf?" A\xce\xa9\xe4\xb8\xad Z ":" AB CD  ";
 CHECK(put(utf?"\x1b%G":"\x1b%@"));CHECK(put("\x1b[0;38;2;230;240;245;48;2;25;60;80m\x1b[2J\x1b[H\x1b[7m "));
 ULONG ink=penrgb((UBYTE)u->cu_FgPen);
 CHECK(put("\x1b[27m "));
 CHECK(ink!=penrgb((UBYTE)u->cu_BgPen));
 CHECK(ReadRGBPixel(w->RPort,u->cu_XROrigin,u->cu_YROrigin)!=ReadRGBPixel(w->RPort,u->cu_XROrigin+u->cu_XRSize,u->cu_YROrigin));
 CHECK(put(colorform?"\x1b[38;2;255;80;40;7m ":"\x1b[38;5;1;7m "));
 ULONG sample=ReadRGBPixel(w->RPort,u->cu_XROrigin+2*u->cu_XRSize,u->cu_YROrigin);
 ink=penrgb((UBYTE)u->cu_FgPen);
 if(!utf&&!fragmented&&!style&&!form){printf("ULCDOS_PEN font=%d sample=%lx palette=%lx\n",font,(unsigned long)sample,(unsigned long)ink);fflush(stdout);}
 CHECK(put("\x1b[0;38;2;230;240;245;48;2;25;60;80m"));
 CHECK(put(style==1?"\x1b[7m":style==2?"\x1b[8m":"\x1b[27m"));CHECK(put("\x1b[3;1H"));CHECK(put(text));
 int width=8*u->cu_XRSize,height=u->cu_YRSize;CHECK(width*height<=8192);
 for(int y=0;y<height;y++)for(int x=0;x<width;x++)reference[y*width+x]=pixel(x,y);
 /* Native font underlines may be antialiased. Capture the default-color
  * control before setting an explicit underline color; SGR 59 must restore
  * these exact pixels, not an assumed solid palette entry. */
 CHECK(put(on));CHECK(put("\x1b[3;1H"));CHECK(put(text));ULONG defaultPixels=hashpixels();
 CHECK(put(off));CHECK(put("\x1b[3;1H"));CHECK(put(text));CHECK(verify(0,style==2,ink));
 CHECK(put(colorform?"\x1b[58:2::255:80:40m":"\x1b[58;5;1m"));
 CHECK(fragmented?split(on):put(on));CHECK((u->cu_TxFlags&33)==(form==4?32:1));
 CHECK(put("\x1b" "7\x1b[0m\x1b" "8"));CHECK((u->cu_TxFlags&33)==(form==4?32:1));
 CHECK(put("\x1b[3;1H"));CHECK(fragmented?split(text):put(text));CHECK(verify(1,style==2,ink));
 {CHECK(put("\x1b[1S"));targetRow=1;CHECK(verify(1,style==2,ink));CHECK(put("\x1b[1T"));targetRow=2;CHECK(verify(1,style==2,ink));CHECK(put("\x1b[?1049hOTHER\x1b[?1049l"));CHECK(verify(1,style==2,ink));CHECK(put("\x1b[>9h\x1b[>9l"));CHECK(verify(1,style==2,ink));SizeWindow(w,16,0);Delay(10);CHECK(verify(1,style==2,ink));SizeWindow(w,-16,0);Delay(10);CHECK(verify(1,style==2,ink));}
 CHECK(put("\x1b[59m\x1b[3;1H"));CHECK(put(text));CHECK(hashpixels()==defaultPixels);
 {CHECK(put("\x1b[>9h\x1b[>9l"));CHECK(hashpixels()==defaultPixels);}
 CHECK(fragmented?split(off):put(off));CHECK(!(u->cu_TxFlags&32));CHECK(put("\x1b[3;1H"));CHECK(put(text));CHECK(verify(0,style==2,ink));
 CHECK(put("\x1b[21m\x1b[4m"));CHECK((u->cu_TxFlags&33)==1);CHECK(put("\x1b[21m"));CHECK((u->cu_TxFlags&33)==32);CHECK(put("\x1b[24m"));CHECK(!(u->cu_TxFlags&33));
 CHECK(put("\x1b[4:1m"));CHECK((u->cu_TxFlags&33)==1);CHECK(put("\x1b[4:2m"));CHECK((u->cu_TxFlags&161)==32);CHECK(put("\x1b[4:0m"));CHECK(!(u->cu_TxFlags&33));
 CHECK(put("\x1b[21m\x1b[0m"));CHECK(u->cu_TxFlags==0);
 const char *fallback[]={"\x1b[4:m","\x1b[4:99m","\x1b[4:2:9m","\x1b[4::m","\x1b[4:5:9m","\x1b[4:9999999999999999999m"};
 for(unsigned j=0;j<sizeof(fallback)/sizeof(*fallback);j++){
  CHECK(put(j?"\x1b[0;38;2;230;240;245;48;2;25;60;80;4m":"\x1b[0;38;2;230;240;245;48;2;25;60;80m"));CHECK(put("\x1b[3;1H"));CHECK(put(text));ULONG expected=hashpixels();
  CHECK(put("\x1b[21m"));CHECK(fragmented?split(fallback[j]):put(fallback[j]));CHECK((u->cu_TxFlags&33)==(j?1:0));
  CHECK(put("\x1b[3;1H"));CHECK(put(text));CHECK(hashpixels()==expected);
  {CHECK(put("\x1b[>9h\x1b[>9l"));CHECK(hashpixels()==expected);}
  selectors++;
 }
 CHECK(put("\x1b[0m"));cases++;
}
}
 if(kind){CHECK(put("\x1b%G\x1b[0m\x1b[2J\x1b[HPASS: 480 DOS underline color comparisons.\r\n\r\nNormal text:        ConsoleNG\r\nCurly underline:   \x1b[58;2;220;40;40;4:3mConsoleNG\x1b[24m\r\nDotted Unicode:    \x1b[58;2;40;80;220;4:4mA \xce\xa9 \xe4\xb8\xad Z\x1b[24m\r\nDashed + strike:    \x1b[58;2;20;130;40;4:5;9mConsoleNG\x1b[0m\r\n\r\nCooked / raw DOS streams, Topaz 8 / DejaVu 16.\r\nWhole / bytewise input; retained redraw and resize."));printf("ULCDOS_PASS cases=%d selectors=%d\n",cases,selectors);fflush(stdout);Delay(600);}
 BOOL closed=Close(stream);stream=0;io=NULL;w=NULL;CHECK(closed);
}rc=0;
out:if(stream)Close(stream);if(CyberGfxBase)CloseLibrary(CyberGfxBase);printf("ULCDOS_SCRIPT rc=%d\n",rc);return rc;
}
