/* Native retained erase colors after SGR, hidden redraw, and resize. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/cybergraphics.h>
#include <devices/conunit.h>
#include <stdio.h>
#include <string.h>
struct Library *CyberGfxBase;static struct IOStdReq*io;static struct Window*w;static struct ConUnit*u;
static int put(const char*s){io->io_Command=CMD_WRITE;io->io_Data=(APTR)s;io->io_Length=strlen(s);return !DoIO((struct IORequest*)io)&&io->io_Actual==strlen(s);}
static int pixels(int display,int mode){for(int y=0;y<=u->cu_YMax;y++)for(int x=0;x<=u->cu_XMax;x++){
 int erased=display?(mode==2||(mode==0&&(y>2||(y==2&&x>=3)))||(mode==1&&(y<2||(y==2&&x<=3)))):y==2&&(mode==2||(mode==0&&x>=3)||(mode==1&&x<=3));
 ULONG expected=erased?0x603020:0x203040;ULONG actual=ReadRGBPixel(w->RPort,u->cu_XROrigin+x*u->cu_XRSize+u->cu_XRSize/2,u->cu_YROrigin+y*u->cu_YRSize+u->cu_YRSize/2);
 if(actual!=expected){printf("ERASE_PIXEL x=%d y=%d got=%lx expected=%lx\n",x,y,(unsigned long)actual,(unsigned long)expected);return 0;}}
 return 1;}
#define CHECK(c) do{if(!(c)){printf("ERASE_FAIL line=%d display=%d mode=%d hidden=%d\n",__LINE__,display,mode,hidden);fflush(stdout);Delay(500);goto out;}}while(0)
int main(void){int rc=20,opened=0,display=0,mode=0,hidden=0,cases=0;struct MsgPort*p=CreateMsgPort();io=p?(void*)CreateIORequest(p,sizeof(*io)):NULL;CHECK(io);CyberGfxBase=OpenLibrary((CONST_STRPTR)"cybergraphics.library",0);CHECK(CyberGfxBase);
w=OpenWindowTags(NULL,WA_Left,30,WA_Top,30,WA_Width,730,WA_Height,470,WA_Title,(IPTR)"ConsoleNG erased backgrounds survive redraw and resize",WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_SizeGadget,TRUE,WA_SizeBRight,TRUE,WA_MinWidth,300,WA_MinHeight,200,WA_MaxWidth,-1,WA_MaxHeight,-1,TAG_DONE);CHECK(w);
io->io_Data=w;io->io_Length=sizeof(*w);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",1,(struct IORequest*)io,0));opened=1;u=(void*)io->io_Unit;
for(display=0;display<2;display++)for(mode=0;mode<3;mode++)for(hidden=0;hidden<2;hidden++){
 char seq[80];CHECK(put("\x1b[?25l\x1b[0;48;2;32;48;64m\x1b[2J\x1b[3;4H\x1b[7;48;2;96;48;32m"));
 if(hidden){CHECK(put("\x1b[>9h"));}
 snprintf(seq,sizeof(seq),"\x1b[%d%c",mode,display?'J':'K');CHECK(put(seq));CHECK(u->cu_YCCP==2&&u->cu_XCCP==3);
 CHECK(put("\x1b[0;48;2;32;96;48m"));if(hidden){CHECK(put("\x1b[>9l"));}CHECK(pixels(display,mode));
 CHECK(put("\x1b[>9h\x1b[>9l"));CHECK(pixels(display,mode));
 SizeWindow(w,-40,0);Delay(12);SizeWindow(w,40,0);Delay(12);CHECK(pixels(display,mode));
 printf("ERASE_CASE display=%d mode=%d hidden=%d PASS\n",display,mode,hidden);fflush(stdout);cases++;
}
CHECK(put("\x1b]50;DejaVuSansMono.font,24\x07\x1b[0;38;2;220;220;220;48;2;32;48;64m\x1b[2J\x1b[HRetained erase colors\r\n\r\n"));
CHECK(put("\x1b[48;2;96;48;32m\x1b[2K\x1b[48;2;32;96;48m\x1b[>9h\x1b[>9l\x1b[5;1H12 erase / redraw / resize checks PASS\r\nBackground survives later color changes."));
printf("ERASE_PASS cases=%d\n",cases);fflush(stdout);Delay(600);rc=0;
out:if(opened)CloseDevice((struct IORequest*)io);if(w)CloseWindow(w);if(io)DeleteIORequest((struct IORequest*)io);if(p)DeleteMsgPort(p);if(CyberGfxBase)CloseLibrary(CyberGfxBase);printf("ERASE_SCRIPT rc=%d\n",rc);return rc;}
