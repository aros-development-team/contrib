/* Native OSC1 rejection preserves actual title, cursor, font and pixels. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/cybergraphics.h>
#include <devices/conunit.h>
#include <stdio.h>
#include <string.h>
struct Library *CyberGfxBase;
static struct IOStdReq *io;static struct Window *w;static struct ConUnit *u;
static int put(const char *s){io->io_Command=CMD_WRITE;io->io_Data=(APTR)s;io->io_Length=strlen(s);return !DoIO((struct IORequest*)io)&&io->io_Actual==strlen(s);}
static int split(const char *s){char b[2]={0};while(*s){b[0]=*s++;if(!put(b))return 0;}return 1;}
static ULONG pixels(void){ULONG h=2166136261UL;for(int y=0;y<3*u->cu_YRSize;y++)for(int x=0;x<20*u->cu_XRSize;x++)h=(h^ReadRGBPixel(w->RPort,u->cu_XROrigin+x,u->cu_YROrigin+y))*16777619UL;return h;}
#define CHECK(c) do{if(!(c)){printf("OSC1_FAIL line=%d check=%s kind=%d fragment=%d style=%d code=%d\n",__LINE__,#c,kind,fragment,style,code);fflush(stdout);Delay(500);goto out;}}while(0)
int main(void){int rc=20,opened=0,kind=0,fragment=0,style=0,code=0,cases=0;struct MsgPort *p=CreateMsgPort();io=p?(void*)CreateIORequest(p,sizeof(*io)):NULL;CHECK(io);CyberGfxBase=OpenLibrary((CONST_STRPTR)"cybergraphics.library",0);CHECK(CyberGfxBase);
for(kind=0;kind<2;kind++){
 w=OpenWindowTags(NULL,WA_Left,30,WA_Top,30,WA_Width,720,WA_Height,450,WA_Title,(IPTR)"ConsoleNG icon title isolation",WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_SizeGadget,TRUE,WA_SizeBRight,TRUE,WA_MinWidth,200,WA_MinHeight,160,WA_MaxWidth,-1,WA_MaxHeight,-1,TAG_DONE);CHECK(w);
 io->io_Data=w;io->io_Length=sizeof(*w);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",kind?CONU_CHARMAP:CONU_STANDARD,(struct IORequest*)io,0));opened=1;u=(void*)io->io_Unit;
 CHECK(put("\x1b%G\x1b[?25l\x1b]50;DejaVuSansMono,16\7"));


 for(fragment=0;fragment<2;fragment++)for(style=0;style<2;style++)for(code=0;code<6;code++){
  CHECK(put(style?"\x1b%G":"\x1b%@"));
  CHECK(put("\x1b]2;Anchor title\7\x1b[0;37;40m\x1b[2J\x1b[HAnchor"));
  ULONG expected=pixels();WORD x=u->cu_XCP,y=u->cu_YCP;UBYTE flags=u->cu_TxFlags;
  WORD height=u->cu_YRSize;UBYTE fg=u->cu_FgPen,bg=u->cu_BgPen;
  const char *ends[]={"\7","\x1b\\","\x18","\x1a","\x1b[?25l","\x9c"};
  if(style&&code==5)continue;
  for(int hidden=0;hidden<2;hidden++)for(int over=0;over<2;over++){
   CHECK(put(hidden?"\x1b[>9h":"\x1b[>9l"));
   char seq[1024];strcpy(seq,!style&&code==5?"\x9d" "1;":"\x1b]1;");
   if(over){size_t n=strlen(seq);memset(seq+n,'x',800);seq[n+800]=0;}
   else strcat(seq,"Icon title must not replace window title");
   strcat(seq,ends[code]);CHECK(fragment?split(seq):put(seq));
   CHECK(!strcmp((char*)w->Title,"Anchor title"));
   CHECK(pixels()==expected&&u->cu_XCP==x&&u->cu_YCP==y&&u->cu_TxFlags==flags);
   CHECK(u->cu_YRSize==height&&u->cu_FgPen==fg&&u->cu_BgPen==bg);
   CHECK(put("\x1b[>9l"));CHECK(pixels()==expected&&!strcmp((char*)w->Title,"Anchor title"));
   cases++;
  }
  CHECK(put("Z"));CHECK(u->cu_XCP==x+1&&u->cu_YCP==y);
 }
 CHECK(put("\x1b[0m\x1b[2J\x1b[HPASS: icon title requests leave the console unchanged\r\n\r\nTitle, font, colors, cursor and pixels preserved\r\nBEL, ST, CAN, SUB and new escape recovery\r\nWhole and bytewise input; standard and retained\r\nVisible/hidden, oversized payloads and legacy C1"));
 printf("OSC1_UNIT_PASS kind=%d cases=%d\n",kind,cases);fflush(stdout);Delay(150);CloseDevice((struct IORequest*)io);opened=0;CloseWindow(w);w=NULL;
}
printf("OSC1_PASS cases=%d\n",cases);rc=0;
out:if(opened)CloseDevice((struct IORequest*)io);if(w)CloseWindow(w);if(CyberGfxBase)CloseLibrary(CyberGfxBase);if(io)DeleteIORequest((struct IORequest*)io);if(p)DeleteMsgPort(p);printf("OSC1_RUN rc=%d\n",rc);return rc;}
