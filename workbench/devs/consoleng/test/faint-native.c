/* Faint text versus independently selected RGB reference, including redraw. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/cybergraphics.h>
#include <devices/conunit.h>
#include <graphics/text.h>
#include <stdio.h>
#include <string.h>
struct Library *CyberGfxBase;
static struct IOStdReq*io;static struct Window*w;static struct ConUnit*u;
static int put(const char*s){io->io_Command=CMD_WRITE;io->io_Data=(APTR)s;io->io_Length=strlen(s);return !DoIO((struct IORequest*)io)&&io->io_Actual==strlen(s);}
static int split(const char*s){char b[2]={0};while(*s){b[0]=*s++;if(!put(b))return 0;}return 1;}
static int equal(void){for(int y=0;y<u->cu_YRSize;y++)for(int x=0;x<8*u->cu_XRSize;x++){ULONG a=ReadRGBPixel(w->RPort,u->cu_XROrigin+x,u->cu_YROrigin+u->cu_YRSize+y),b=ReadRGBPixel(w->RPort,u->cu_XROrigin+x,u->cu_YROrigin+4*u->cu_YRSize+y);if(a!=b){printf("FAINT_PIXEL x=%d y=%d actual=%lx reference=%lx\n",x,y,(unsigned long)a,(unsigned long)b);return 0;}}return 1;}
#define CHECK(c) do{if(!(c)){printf("FAINT_FAIL check=%s line=%d kind=%d type=%d bold=%d style=%d utf=%d\n",#c,__LINE__,kind,type,bold,style,utf);fflush(stdout);Delay(500);goto out;}}while(0)
int main(void){int rc=20,opened=0,kind=0,type=0,bold=0,style=0,utf=0,cases=0;struct MsgPort*p=CreateMsgPort();io=p?(void*)CreateIORequest(p,sizeof(*io)):NULL;CHECK(io);CyberGfxBase=OpenLibrary((CONST_STRPTR)"cybergraphics.library",0);CHECK(CyberGfxBase);
const char*fg[]={"\x1b[38;2;200;150;100m","\x1b[39m","\x1b[31m","\x1b[38;5;1m","\x1b[38;5;9m","\x1b[38;5;42m"};
for(kind=0;kind<2;kind++){
 w=OpenWindowTags(NULL,WA_Left,30,WA_Top,30,WA_Width,720,WA_Height,450,WA_Title,(IPTR)"ConsoleNG - faint rendition regression",WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_SizeGadget,TRUE,WA_SizeBRight,TRUE,WA_MinWidth,200,WA_MinHeight,160,WA_MaxWidth,-1,WA_MaxHeight,-1,TAG_DONE);CHECK(w);io->io_Data=w;io->io_Length=sizeof(*w);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",kind?CONU_CHARMAP:CONU_STANDARD,(struct IORequest*)io,0));opened=1;u=(void*)io->io_Unit;
 CHECK(put("\x1b[?25l\x1b]50;topaz,8\7\x1b]10;#c89664\7\x1b]4;1;#c89664;9;#f0c896;42;#c89664\7"));
for(int slot=0;slot<16;slot++){char palette[80];snprintf(palette,sizeof(palette),"\x1b]4;%d;%s\7",slot,slot<8?"#c89664":"#f0c896");CHECK(put(palette));}
for(type=0;type<14;type++)for(bold=0;bold<2;bold++)for(style=0;style<3;style++)for(utf=0;utf<2;utf++){
 const char*text=utf?" A\xce\xa9\xe4\xb8\xad Z ":" AB CD  ";char seq[80];
 int red=200,green=150,blue=100;
 if(type==4&&bold){red=240;green=200;blue=150;}
 if(type<3||(type==3&&!bold)){red=132;green=99;blue=66;}
 CHECK(put(utf?"\x1b%G":"\x1b%@"));CHECK(put("\x1b[0;48;2;32;62;76m\x1b[2J\x1b[4;9m"));if(type<6){CHECK(put(fg[type]));}else{snprintf(seq,sizeof(seq),"\x1b[%d;101m",90+type-6);CHECK(put(seq));}
 if(bold){CHECK(put("\x1b[1m"));}if(style){CHECK(put(style==1?"\x1b[7m":"\x1b[8m"));}
 CHECK(split("\x1b[2m"));CHECK(u->cu_TxFlags&128);UBYTE original=u->cu_FgPen;CHECK(put("\x1b" "7\x1b[0m\x1b" "8"));CHECK(u->cu_TxFlags&128);CHECK((UBYTE)u->cu_FgPen==original);
 CHECK(put("\x1b[2;1H"));CHECK(split(text));CHECK((UBYTE)u->cu_FgPen==original);
 CHECK(split("\x1b[22m"));CHECK(!(u->cu_TxFlags&(128|FSF_BOLD)));CHECK(u->cu_TxFlags&64);
 if(bold){CHECK(put("\x1b[1m"));}
 snprintf(seq,sizeof(seq),"\x1b[38;2;%d;%d;%dm\x1b[5;1H",red,green,blue);CHECK(put(seq));CHECK(put(text));CHECK(equal());
 if(kind){CHECK(put("\x1b[>9h\x1b[>9l"));CHECK(equal());CHECK(put("\x1b[?1049hOTHER\x1b[?1049l"));CHECK(equal());SizeWindow(w,16,0);Delay(5);CHECK(equal());SizeWindow(w,-16,0);Delay(5);CHECK(equal());}
 CHECK(put("\x1b[0m"));CHECK(!u->cu_TxFlags);cases++;
}
 if(kind){CHECK(put("\x1b]50;DejaVuSansMono,16\7\x1b[0;38;2;230;220;190;48;2;20;30;45m\x1b[2J\x1b[HPASS: 336 native faint/reference pixel comparisons.\r\n\r\nNormal:  ConsoleNG text\r\n\x1b[2mFaint:   ConsoleNG text\x1b[22m\r\n\x1b[1;2mBold dim: ConsoleNG text\x1b[22m\r\n\x1b[2;4;9mDim underline and strikethrough\x1b[0m\r\n\r\nRGB/default/ANSI/indexed; reverse and conceal.\r\nLatin-1/Unicode, saved cursor, alternate screen, resize."));printf("FAINT_PASS cases=%d\n",cases);fflush(stdout);Delay(600);}
 CloseDevice((struct IORequest*)io);opened=0;CloseWindow(w);w=NULL;
}rc=0;
out:if(opened)CloseDevice((struct IORequest*)io);if(w)CloseWindow(w);if(io)DeleteIORequest((struct IORequest*)io);if(p)DeleteMsgPort(p);if(CyberGfxBase)CloseLibrary(CyberGfxBase);printf("FAINT_SCRIPT rc=%d\n",rc);return rc;
}
