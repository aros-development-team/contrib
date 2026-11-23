/* Mutable extended palette identity across native draws and retained redraws. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/cybergraphics.h>
#include <devices/conunit.h>
#include <stdio.h>
#include <string.h>
struct Library *CyberGfxBase;
static struct IOStdReq*io;static struct Window*w;static struct ConUnit*u;
static int put(const char*s){io->io_Command=CMD_WRITE;io->io_Data=(APTR)s;io->io_Length=strlen(s);return !DoIO((struct IORequest*)io)&&io->io_Actual==strlen(s);}
static int send(const char*s,int fragmented){if(!fragmented)return put(s);char b[2]={0};while(*s){b[0]=*s++;if(!put(b))return 0;}return 1;}
static ULONG penrgb(UBYTE pen){ULONG c[3];GetRGB32(w->WScreen->ViewPort.ColorMap,pen,1,c);return ((c[0]>>24)<<16)|((c[1]>>24)<<8)|(c[2]>>24);}
static int paint(int index)
{
 char s[240];
 snprintf(s,sizeof(s),"\x1b[H\x1b[48;5;%dm        \r\n"
     "\x1b[48;2;35;69;103m        \r\n"
     "\x1b[38;5;%d;7m        \x1b[27;39m\x1b[48;5;%dm",
     index,index,index);
 return put(s);
}
static int pixels(ULONG color)
{
 ULONG expected[3]={color,0x234567,color};
 for(int row=0;row<3;row++)for(int y=0;y<u->cu_YRSize;y++)
 for(int x=0;x<8*u->cu_XRSize;x++){
  ULONG got=ReadRGBPixel(w->RPort,u->cu_XROrigin+x,u->cu_YROrigin+row*u->cu_YRSize+y);
  if(got!=expected[row]){
   printf("PALETTE256_PIXEL row=%d x=%d y=%d got=%lx expected=%lx\n",
       row,x,y,(unsigned long)got,(unsigned long)expected[row]);
   return 0;
  }
 }
 return 1;
}
static int reply(const char*expected){char got[64]={0};int length=strlen(expected);io->io_Command=CMD_READ;io->io_Data=got;io->io_Length=length;SendIO((struct IORequest*)io);
 for(int n=0;n<100;n++){if(CheckIO((struct IORequest*)io)){WaitIO((struct IORequest*)io);return !io->io_Error&&io->io_Actual==length&&!memcmp(got,expected,length);}Delay(1);}
 AbortIO((struct IORequest*)io);WaitIO((struct IORequest*)io);return 0;}
#define CHECK(c) do{if(!(c)){printf("PALETTE256_FAIL line=%d kind=%d split=%d index=%d\n",__LINE__,kind,fragmented,index);fflush(stdout);Delay(500);goto out;}}while(0)
int main(void){int rc=20,opened=0,kind=0,fragmented=0,index=16,cases=0;struct MsgPort*p=CreateMsgPort();io=p?(void*)CreateIORequest(p,sizeof(*io)):NULL;CHECK(io);CyberGfxBase=OpenLibrary((CONST_STRPTR)"cybergraphics.library",0);CHECK(CyberGfxBase);
for(kind=0;kind<2;kind++){
 w=OpenWindowTags(NULL,WA_Left,30,WA_Top,30,WA_Width,720,WA_Height,450,WA_Title,(IPTR)"ConsoleNG - all 256 palette entries",WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_SizeGadget,TRUE,WA_SizeBRight,TRUE,WA_MinWidth,200,WA_MinHeight,160,WA_MaxWidth,-1,WA_MaxHeight,-1,TAG_DONE);CHECK(w);io->io_Data=w;io->io_Length=sizeof(*w);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",kind?CONU_CHARMAP:CONU_STANDARD,(struct IORequest*)io,0));opened=1;u=(void*)io->io_Unit;
 CHECK(put("\x1b[?25l\x1b]50;topaz,8\7\x1b]104\7\x1b[0m\x1b[2J"));
for(fragmented=0;fragmented<2;fragmented++)for(index=16;index<256;index++){
 char seq[160],expected[64];CHECK(paint(index));ULONG original=penrgb(u->cu_BgPen);CHECK(pixels(original));CHECK(put("\x1b" "7"));
 snprintf(seq,sizeof(seq),"\x1b]4;%d;#234567\7",index);CHECK(send(seq,fragmented));CHECK(penrgb(u->cu_BgPen)==0x234567);
 if(!kind){CHECK(paint(index));}CHECK(pixels(0x234567));
 snprintf(seq,sizeof(seq),"\x1b]4;%d;?\7",index);snprintf(expected,sizeof(expected),"\x1b]4;%d;rgb:2323/4545/6767\7",index);CHECK(send(seq,fragmented));CHECK(reply(expected));CHECK(pixels(0x234567));
 if(kind){CHECK(put("\x1b[>9h"));}snprintf(seq,sizeof(seq),"\x1b]4;%d;#a06030\7",index);CHECK(send(seq,fragmented));CHECK(put("\x1b[>9l"));CHECK(penrgb(u->cu_BgPen)==0xa06030);
 if(!kind){CHECK(paint(index));}CHECK(pixels(0xa06030));CHECK(put("\x1b" "8"));CHECK(penrgb(u->cu_BgPen)==0xa06030);
 if(kind&&(index==16||index==127||index==255)){CHECK(put("\x1b[?1049hOTHER\x1b[?1049l"));CHECK(pixels(0xa06030));SizeWindow(w,16,0);Delay(5);CHECK(pixels(0xa06030));SizeWindow(w,-16,0);Delay(5);CHECK(pixels(0xa06030));}
 snprintf(seq,sizeof(seq),"\x1b]104;%d\7",index);CHECK(send(seq,fragmented));CHECK(penrgb(u->cu_BgPen)==original);
 if(!kind){CHECK(paint(index));}CHECK(pixels(original));CHECK(put("\x1b" "8"));CHECK(penrgb(u->cu_BgPen)==original);cases++;
}
 CHECK(put("\x1b]4;16;#234567;127;#234567;255;#234567\7\x1b]104\7"));
 for(index=16;index<256;index++){char seq[40];snprintf(seq,sizeof(seq),"\x1b[48;5;%dm",index);CHECK(put(seq));CHECK(penrgb(u->cu_BgPen)!=0x234567);}
 if(kind){CHECK(put("\x1b]50;DejaVuSansMono,16\7\x1b[0m\x1b[2J\x1b[HPASS: 960 foreground/background palette combinations.\r\n\r\nEvery entry 16-255: set, query, change and reset.\r\nBoth foreground and background follow their palette entry.\r\nExplicit RGB remains unchanged, even when pens alias.\r\nSaved cursor, hidden changes, alternate screen and resize.\r\n\r\nFull palette: "));
 for(index=0;index<256;index++){char seq[50];snprintf(seq,sizeof(seq),"\x1b[48;5;%dm ",index);CHECK(put(seq));if(index%32==31)CHECK(put("\x1b[0m\r\n              "));}CHECK(put("\x1b[0m"));printf("PALETTE256_PASS cases=%d\n",cases);fflush(stdout);Delay(600);}
 CloseDevice((struct IORequest*)io);opened=0;CloseWindow(w);w=NULL;
}rc=0;
out:if(opened)CloseDevice((struct IORequest*)io);if(w)CloseWindow(w);if(io)DeleteIORequest((struct IORequest*)io);if(p)DeleteMsgPort(p);if(CyberGfxBase)CloseLibrary(CyberGfxBase);printf("PALETTE256_SCRIPT rc=%d\n",rc);return rc;
}
