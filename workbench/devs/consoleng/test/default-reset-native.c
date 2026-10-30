/* OSC110/111: independent defaults, explicit colors and retained pixels. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/cybergraphics.h>
#include <devices/conunit.h>
#include <stdio.h>
#include <string.h>
#include "consoleng.h"
struct Library *CyberGfxBase;
static struct IOStdReq*io;static struct Window*w;static struct ConUnit*u;
static int put(const char*s){io->io_Command=CMD_WRITE;io->io_Data=(APTR)s;io->io_Length=strlen(s);return !DoIO((struct IORequest*)io)&&io->io_Actual==strlen(s);}
static int send(const char*s,int fragmented){if(!fragmented)return put(s);char b[2]={0};while(*s){b[0]=*s++;if(!put(b))return 0;}return 1;}
static ULONG penrgb(UBYTE pen){ULONG c[3];GetRGB32(w->WScreen->ViewPort.ColorMap,pen,1,c);return ((c[0]>>24)<<16)|((c[1]>>24)<<8)|(c[2]>>24);}
static int colors(ULONG fg,ULONG bg){return penrgb(u->cu_FgPen)==fg&&penrgb(u->cu_BgPen)==bg;}
static int mask(ULONG bits){struct ConsolengColorPrefs p;io->io_Command=CD_GETCOLORPREFS;io->io_Data=&p;io->io_Length=sizeof(p);return !DoIO((struct IORequest*)io)&&(p.cp_Mask&3)==bits;}
static int paint(void){return put("\x1b[0m\x1b[H\x1b[7m        \x1b[27m\r\n        \r\n\x1b[48;2;70;90;110m        \x1b[0m");}
static int pixels(ULONG fg,ULONG bg){ULONG expected[3]={fg,bg,0x465a6e};for(int row=0;row<3;row++)for(int y=0;y<u->cu_YRSize;y++)for(int x=0;x<8*u->cu_XRSize;x++){ULONG got=ReadRGBPixel(w->RPort,u->cu_XROrigin+x,u->cu_YROrigin+row*u->cu_YRSize+y);if(got!=expected[row]){printf("DEFAULTRESET_PIXEL row=%d x=%d y=%d got=%lx expected=%lx\n",row,x,y,(unsigned long)got,(unsigned long)expected[row]);return 0;}}return 1;}
#define CHECK(c) do{if(!(c)){printf("DEFAULTRESET_FAIL line=%d kind=%d split=%d term=%d order=%d hidden=%d\n",__LINE__,kind,fragmented,term,order,hidden);fflush(stdout);Delay(500);goto out;}}while(0)
int main(void){int rc=20,opened=0,kind=0,fragmented=0,term=0,order=0,hidden=0,cases=0;struct MsgPort*p=CreateMsgPort();io=p?(void*)CreateIORequest(p,sizeof(*io)):NULL;CHECK(io);CyberGfxBase=OpenLibrary((CONST_STRPTR)"cybergraphics.library",0);CHECK(CyberGfxBase);
const char*end[]={"\7","\x1b\\","\x9c"};
for(kind=0;kind<2;kind++){
 w=OpenWindowTags(NULL,WA_Left,30,WA_Top,30,WA_Width,720,WA_Height,450,WA_Title,(IPTR)"ConsoleNG - default color reset regression",WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_SizeGadget,TRUE,WA_SizeBRight,TRUE,WA_MinWidth,200,WA_MinHeight,160,WA_MaxWidth,-1,WA_MaxHeight,-1,TAG_DONE);CHECK(w);io->io_Data=w;io->io_Length=sizeof(*w);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",kind?CONU_CHARMAP:CONU_STANDARD,(struct IORequest*)io,0));opened=1;u=(void*)io->io_Unit;
 CHECK(put("\x1b[?25l\x1b]50;topaz,8\7\x1b]110\7\x1b]111\7\x1b[0m"));ULONG originalFg=penrgb(u->cu_FgPen),originalBg=penrgb(u->cu_BgPen);
for(fragmented=0;fragmented<2;fragmented++)for(term=0;term<3;term++)for(order=0;order<2;order++)for(hidden=0;hidden<2;hidden++){
 CHECK(put(term==2?"\x1b%@":"\x1b%G"));
 CHECK(put("\x1b[0m\x1b[2J"));CHECK(paint());CHECK(pixels(originalFg,originalBg));CHECK(put("\x1b" "7"));
 CHECK(put("\x1b]10;#c4a060\7\x1b]11;#203e4c\7"));CHECK(colors(0xc4a060,0x203e4c));CHECK(mask(3));
 if(!kind){CHECK(paint());}CHECK(pixels(0xc4a060,0x203e4c));
 if(hidden)CHECK(put("\x1b[>9h"));
 char seq[40];snprintf(seq,sizeof(seq),"\x1b]%d%s",110+order,end[term]);CHECK(send(seq,fragmented));CHECK(mask(order?1:2));
 ULONG fg=order?0xc4a060:originalFg,bg=order?originalBg:0x203e4c;CHECK(colors(fg,bg));
 CHECK(send(seq,fragmented));CHECK(mask(order?1:2));CHECK(put("\x1b[>9l"));if(!kind){CHECK(paint());}CHECK(pixels(fg,bg));
 CHECK(put("\x1b" "8"));CHECK(colors(fg,bg));
 if(kind){CHECK(put("\x1b[?1049hALTERNATE\x1b[?1049l"));CHECK(colors(fg,bg));CHECK(pixels(fg,bg));}
 snprintf(seq,sizeof(seq),"\x1b]%d%s",111-order,end[term]);CHECK(send(seq,fragmented));CHECK(mask(0));CHECK(colors(originalFg,originalBg));
 if(!kind){CHECK(paint());}CHECK(pixels(originalFg,originalBg));CHECK(put("\x1b" "8"));CHECK(colors(originalFg,originalBg));
 if(kind){SizeWindow(w,16,0);Delay(5);CHECK(pixels(originalFg,originalBg));SizeWindow(w,-16,0);Delay(5);CHECK(pixels(originalFg,originalBg));}
 cases++;
}
 if(kind){CHECK(put("\x1b]50;DejaVuSansMono,16\7\x1b[0m\x1b[2J\x1b[HPASS: 48 native default-color reset combinations.\r\n\r\nOSC 110 restores the foreground.\r\nOSC 111 restores the background.\r\nEach leaves the other default and explicit colors alone.\r\n\r\nWhole / bytewise input; BEL / ST / C1 ST.\r\nSaved cursor, alternate screen, hidden updates and resize."));printf("DEFAULTRESET_PASS cases=%d\n",cases);fflush(stdout);Delay(600);}
 CloseDevice((struct IORequest*)io);opened=0;CloseWindow(w);w=NULL;
}rc=0;
out:if(opened)CloseDevice((struct IORequest*)io);if(w)CloseWindow(w);if(io)DeleteIORequest((struct IORequest*)io);if(p)DeleteMsgPort(p);if(CyberGfxBase)CloseLibrary(CyberGfxBase);printf("DEFAULTRESET_SCRIPT rc=%d\n",rc);return rc;
}
