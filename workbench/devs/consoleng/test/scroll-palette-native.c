/* Retained scrolling blanks: dynamic/default/fixed colors, palette and resize. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/cybergraphics.h>
struct Library *CyberGfxBase;
#include <devices/conunit.h>
#include <stdio.h>
#include <string.h>
static struct IOStdReq*io;static struct Window*w;static struct ConUnit*u;
static int put(const char*s){io->io_Command=CMD_WRITE;io->io_Data=(APTR)s;io->io_Length=strlen(s);return !DoIO((struct IORequest*)io)&&io->io_Actual==strlen(s);}
static int blank(int rows,LONG pen){for(int y=u->cu_YROrigin+(u->cu_YMax+1-rows)*u->cu_YRSize;y<u->cu_YROrigin+(u->cu_YMax+1)*u->cu_YRSize;y++)for(int x=u->cu_XROrigin;x<u->cu_XROrigin+(u->cu_XMax+1)*u->cu_XRSize;x++)if(ReadRGBPixel(w->RPort,x,y)!=pen){printf("SCROLLPALETTE_PIXEL x=%d y=%d got=%ld expected=%ld\n",x,y,(long)ReadRGBPixel(w->RPort,x,y),(long)pen);return 0;}return 1;}
#define CHECK(c) do{if(!(c)){printf("SCROLLPALETTE_FAIL line=%d role=%d hidden=%d delta=%d amount=%d\n",__LINE__,role,hidden,delta,amount);fflush(stdout);Delay(500);goto out;}}while(0)
int main(void){int rc=20,opened=0,role=0,hidden=0,delta=0,amount=0,cases=0;struct MsgPort*p=CreateMsgPort();io=p?(void*)CreateIORequest(p,sizeof(*io)):NULL;CHECK(io);CyberGfxBase=OpenLibrary((CONST_STRPTR)"cybergraphics.library",0);CHECK(CyberGfxBase);
 const char*styles[]={"\x1b[0m","\x1b[0;44m","\x1b[0;48;2;25;60;80m"};
 for(role=0;role<3;role++)for(hidden=0;hidden<2;hidden++)for(delta=-1;delta<=1;delta+=2)for(amount=1;amount<=2;amount++){
 w=OpenWindowTags(NULL,WA_Left,30,WA_Top,30,WA_Width,600,WA_Height,450,WA_Title,(IPTR)"ConsoleNG scrolling palette and resize",WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_SizeGadget,TRUE,WA_SizeBRight,TRUE,WA_MinWidth,200,WA_MinHeight,160,WA_MaxWidth,-1,WA_MaxHeight,-1,TAG_DONE);CHECK(w);
 io->io_Data=w;io->io_Length=sizeof(*w);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",CONU_CHARMAP,(struct IORequest*)io,0));opened=1;u=(void*)io->io_Unit;
 CHECK(put("\x1b[?25l\x1b]50;topaz,8\x07\x1b]11;#193c50\x07\x1b]4;4;#193c50\x07"));CHECK(put(styles[role]));CHECK(put("\x1b[2J\x1b[H"));
 int width=u->cu_XMax+1;char seq[40];snprintf(seq,sizeof(seq),"\x1b[%dS",amount);if(hidden)CHECK(put("\x1b[>9h"));CHECK(put(seq));
 CHECK(put("\x1b]11;#206050\x07\x1b]4;4;#206050\x07\x1b[>9l"));CHECK(put(styles[role]));CHECK(put("\x1b[H\x1b[2K"));LONG pen=ReadRGBPixel(w->RPort,u->cu_XROrigin,u->cu_YROrigin);CHECK(blank(amount,pen));
 CHECK(put("\x1b[41m\x1b[>9h\x1b[>9l"));CHECK(blank(amount,pen));CHECK(put(styles[role]));
 SizeWindow(w,delta*120,0);Delay(50);CHECK(u->cu_XMax+1==width+delta*15);CHECK(put("\x1b[H\x1b[2K"));pen=ReadRGBPixel(w->RPort,u->cu_XROrigin,u->cu_YROrigin);CHECK(blank(amount,pen));
 CHECK(put("\x1b[41m\x1b[>9h\x1b[>9l"));CHECK(blank(amount,pen));
 printf("SCROLLPALETTE_CASE role=%d hidden=%d delta=%d amount=%d PASS\n",role,hidden,delta,amount);fflush(stdout);cases++;
 if(cases==24){CHECK(put("\x1b[0m\x1b[2J\x1b[HPASS: 24 scrolling palette / resize comparisons.\r\nDefault, ANSI and fixed RGB blank backgrounds.\r\nPalette changes, hidden scroll and reveal.\r\nShrink/grow, then SGR changes and redraw."));printf("SCROLLPALETTE_PASS cases=%d\n",cases);fflush(stdout);Delay(600);}
 CloseDevice((struct IORequest*)io);opened=0;CloseWindow(w);w=NULL;
 }rc=0;
 out:if(opened)CloseDevice((struct IORequest*)io);if(w)CloseWindow(w);if(io)DeleteIORequest((struct IORequest*)io);if(p)DeleteMsgPort(p);if(CyberGfxBase)CloseLibrary(CyberGfxBase);printf("SCROLLPALETTE_SCRIPT rc=%d\n",rc);return rc;
}
