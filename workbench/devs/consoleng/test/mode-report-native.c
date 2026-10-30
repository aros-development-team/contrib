/* Native mode query wire replies, with each query split at every byte. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/cybergraphics.h>
struct Library *CyberGfxBase;
#include <devices/conunit.h>
#include <stdio.h>
#include <string.h>
static struct IOStdReq*io,*reader;
static int writebytes(const char*s,ULONG n){io->io_Command=CMD_WRITE;io->io_Data=(APTR)s;io->io_Length=n;return !DoIO((struct IORequest*)io)&&io->io_Actual==n;}
static int put(const char*s){return writebytes(s,strlen(s));}
static ULONG pixels(struct Window*w,struct ConUnit*u){ULONG h=2166136261UL;for(int y=0;y<2*u->cu_YRSize;y++)for(int x=0;x<12*u->cu_XRSize;x++)h=(h^ReadRGBPixel(w->RPort,u->cu_XROrigin+x,u->cu_YROrigin+y))*16777619UL;return h;}
static int finish(void){for(int n=0;n<100;n++){if(CheckIO((struct IORequest*)reader)){WaitIO((struct IORequest*)reader);return 1;}Delay(1);}AbortIO((struct IORequest*)reader);WaitIO((struct IORequest*)reader);return 0;}
static int query(int priv,ULONG mode,ULONG state,int c1){char request[40],expected[40],got[40];snprintf(request,sizeof(request),"%s%s%lu$p",c1?"\x9b":"\x1b[",priv?"?":"",(unsigned long)mode);snprintf(expected,sizeof(expected),"\x1b[%s%lu;%lu$y",priv?"?":"",(unsigned long)mode,(unsigned long)state);
 for(ULONG i=0;i<strlen(request);i++)if(!writebytes(request+i,1))return 0;
 reader->io_Command=CMD_READ;reader->io_Data=got;reader->io_Length=strlen(expected);SendIO((struct IORequest*)reader);if(!finish())return 0;
 if(reader->io_Error||reader->io_Actual!=strlen(expected)||memcmp(got,expected,strlen(expected))){printf("MODE_MISMATCH mode=%lu state=%lu bytes=%lu\n",(unsigned long)mode,(unsigned long)state,(unsigned long)reader->io_Actual);return 0;}return 1;}
#define CHECK(c) do{if(!(c)){printf("MODE_FAIL line=%d kind=%d\n",__LINE__,kind);fflush(stdout);Delay(500);goto out;}}while(0)
int main(void){int rc=20,opened=0,count=0,kind=0;struct MsgPort*port=NULL;struct Window*w=NULL;
 CyberGfxBase=OpenLibrary((CONST_STRPTR)"cybergraphics.library",0);CHECK(CyberGfxBase);
 port=CreateMsgPort();CHECK(port);io=(void*)CreateIORequest(port,sizeof(*io));reader=(void*)CreateIORequest(port,sizeof(*reader));CHECK(io&&reader);
 const ULONG units[]={CONU_STANDARD,CONU_CHARMAP,CONU_SNIPMAP};
 for(kind=0;kind<3;kind++){
 w=OpenWindowTags(NULL,WA_Left,30,WA_Top,35,WA_Width,720,WA_Height,430,WA_Title,(IPTR)"ConsoleNG mode status queries",WA_Activate,TRUE,WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_SizeGadget,TRUE,WA_SizeBRight,TRUE,WA_MinWidth,160,WA_MinHeight,80,WA_MaxWidth,-1,WA_MaxHeight,-1,TAG_DONE);CHECK(w);
 io->io_Data=w;io->io_Length=sizeof(*w);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",units[kind],(struct IORequest*)io,0));opened=1;reader->io_Device=io->io_Device;reader->io_Unit=io->io_Unit;

 ULONG modes[]={1,6,7,12,25,1000,1002,1003,1004,1005,1006,1007,1049,2004,2031};
 for(int hidden=0;hidden<2;hidden++)for(int utf=0;utf<2;utf++)for(int c1=0;c1<2;c1++){
  printf("MODE_CONTEXT kind=%d hidden=%d utf=%d c1=%d count=%d\n",kind,hidden,utf,c1,count);fflush(stdout);
  CHECK(put(utf?"\x1b%G":"\x1b%@"));CHECK(put(hidden?"\x1b[>9h":"\x1b[>9l"));
  for(unsigned m=0;m<sizeof(modes)/sizeof(*modes);m++)for(int on=0;on<2;on++){
   char set[32];snprintf(set,sizeof(set),"%s?%lu%c",c1?"\x9b":"\x1b[",(unsigned long)modes[m],on?'h':'l');
   for(unsigned i=0;i<strlen(set);i++)CHECK(writebytes(set+i,1));
   CHECK(query(1,modes[m],!kind&&modes[m]==1049?0:on?1:2,c1));count++;
  }
  CHECK(put("\x1b[?1049l"));
  ULONG ansi[]={4,20};for(int m=0;m<2;m++)for(int on=0;on<2;on++){
   char set[32];snprintf(set,sizeof(set),"\x1b[%lu%c",(unsigned long)ansi[m],on?'h':'l');CHECK(put(set));CHECK(query(0,ansi[m],on?1:2,c1));count++;
  }
  ULONG unsupported[]={0,3,5,1042,2025,65535};for(unsigned i=0;i<6;i++){CHECK(query(1,unsupported[i],0,c1));CHECK(query(0,unsupported[i],0,c1));count+=2;}
  CHECK(put("\x1b[?65536$p\x1b[?1;7$p\x1b[?1$q"));CHECK(query(1,2004,1,c1));count++;
  /* Encodings and tracking modes are mutually exclusive, not independent bits. */
  CHECK(put("\x1b[?1005h"));CHECK(query(1,1005,1,c1));CHECK(query(1,1006,2,c1));
  CHECK(put("\x1b[?1006h"));CHECK(query(1,1005,2,c1));CHECK(query(1,1006,1,c1));
  CHECK(put("\x1b[?1000h\x1b[?1002h"));CHECK(query(1,1000,2,c1));CHECK(query(1,1002,1,c1));
  CHECK(put("\x1b[?1000l"));CHECK(query(1,1002,1,c1));count+=7;
  CHECK(put("\x1b[?1002l\x1b[?1005l\x1b[?1006l\x1b[>9l"));
 }

 CHECK(put("\x1b[4l\x1b[?6l\x1b[?1004l\x1b[>9l\x1b[2 q\x1b[?25l\x1b[2J\x1b[HCursor\x1b[H"));Delay(5);
 struct ConUnit*u=(void*)io->io_Unit;ULONG absent=pixels(w,u);
 CHECK(put("\x1b[?25h"));ULONG present=pixels(w,u);CHECK(present!=absent);
 CHECK(put("\x1b[>9h\x1b[?25l"));CHECK(query(1,25,2,0));CHECK(put("\x1b[>9l"));CHECK(pixels(w,u)==absent);
 CHECK(put("\x1b[>9h\x1b[?25h"));CHECK(query(1,25,1,0));CHECK(put("\x1b[>9l"));CHECK(pixels(w,u)==present);count+=2;
 CHECK(put("\x1b[4l\x1b[?6l\x1b[?2004l\x1b[?7;25h\x1b[20h\x1b]50;topaz,8\x07\x1b[2J\x1b[HPASS: mode status follows real settings and screen state.\r\nSplit CSI queries, unsupported modes and unframed replies pass.\r\nVisible/hidden; UTF8/legacy; CSI/C1; mouse mode exclusion.\r\n"));printf("MODE_UNIT_PASS kind=%d queries=%d\n",kind,count);fflush(stdout);Delay(150);
 CloseDevice((struct IORequest*)io);opened=0;CloseWindow(w);w=NULL;
 }
 printf("MODE_PASS queries=%d\n",count);rc=0;
out:
 if(opened)CloseDevice((struct IORequest*)io);if(w)CloseWindow(w);if(reader)DeleteIORequest((struct IORequest*)reader);if(io)DeleteIORequest((struct IORequest*)io);if(port)DeleteMsgPort(port);if(CyberGfxBase)CloseLibrary(CyberGfxBase);printf("MODE_SCRIPT rc=%d\n",rc);return rc;
}
