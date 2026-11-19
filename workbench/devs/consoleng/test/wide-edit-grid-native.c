/* Native ICH/DCH/ECH compared against independently generated cells.
 * Optional "standard" / "standard-ascii" arms test the immediate raster without claiming retained
 * hide/reveal or resize behavior. The default retained arm keeps its hide/reveal and resize checks. */
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
static ULONG pixels(void){ULONG h=2166136261UL;for(int y=0;y<3*u->cu_YRSize;y++)for(int x=0;x<(u->cu_XMax+1)*u->cu_XRSize;x++)h=(h^ReadRGBPixel(w->RPort,u->cu_XROrigin+x,u->cu_YROrigin+y))*16777619UL;return h;}
/* SizeWindow queues work in Intuition and console.device independently.
 * Observe both geometries, then use a completed console command as a render
 * barrier. A fixed five-tick delay can sample an intermediate resize. */
static int resized(int windowWidth,int columns){
 for(int ticks=0;ticks<250;ticks++){
  if(w->Width==windowWidth&&u->cu_XMax+1==columns){
   if(!put("\x1b[?25l"))return 0;
   if(w->Width==windowWidth&&u->cu_XMax+1==columns){
    printf("WIDEEDIT_RESIZE width=%d columns=%d ticks=%d\n",windowWidth,columns,ticks);fflush(stdout);return 1;
   }
  }
  Delay(1);
 }
 printf("WIDEEDIT_RESIZE_TIMEOUT actual_width=%d actual_columns=%d expected_width=%d expected_columns=%d\n",w->Width,u->cu_XMax+1,windowWidth,columns);fflush(stdout);return 0;
}
#define CHECK(c) do{if(!(c)){printf("WIDEEDIT_FAIL line=%d check=%s kind=%d fragment=%d style=%d code=%d\n",__LINE__,#c,kind,fragment,style,code);fflush(stdout);Delay(500);goto out;}}while(0)
int main(int argc,char **argv){int ascii=argc==2&&!strcmp(argv[1],"standard-ascii");int standard=ascii||(argc==2&&!strcmp(argv[1],"standard"));int rc=20,opened=0,kind=0,fragment=0,style=0,code=0,cases=0,mismatches=0;struct MsgPort *p=CreateMsgPort();io=p?(void*)CreateIORequest(p,sizeof(*io)):NULL;CHECK(io);CyberGfxBase=OpenLibrary((CONST_STRPTR)"cybergraphics.library",0);CHECK(CyberGfxBase);
for(kind=standard?0:1;kind<(standard?1:2);kind++){
 w=OpenWindowTags(NULL,WA_Left,30,WA_Top,30,WA_Width,720,WA_Height,450,WA_Title,(IPTR)"ConsoleNG wide character editing",WA_Activate,TRUE,WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_SizeGadget,TRUE,WA_SizeBRight,TRUE,WA_MinWidth,200,WA_MinHeight,160,WA_MaxWidth,-1,WA_MaxHeight,-1,TAG_DONE);CHECK(w);
 io->io_Data=w;io->io_Length=sizeof(*w);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",kind?CONU_CHARMAP:CONU_STANDARD,(struct IORequest*)io,0));opened=1;u=(void*)io->io_Unit;
 CHECK(put("\x1b%G\x1b[?25l\x1b]50;DejaVuSansMono.font,16\7"));


 CHECK(u->cu_XRSize>0&&u->cu_YRSize==16);
 CHECK(put("\x1b[0;38;2;230;240;245;48;2;25;60;80m\x1b[2J\x1b[H"));ULONG blank=pixels();
 CHECK(put("A"));printf("WIDEEDIT_ASCII kind=%d cell=%dx%d cursor=%d fg=%d bg=%d blank=%08lx glyph=%08lx\n",kind,u->cu_XRSize,u->cu_YRSize,u->cu_XCP,u->cu_FgPen,u->cu_BgPen,(unsigned long)blank,(unsigned long)pixels());fflush(stdout);CHECK(pixels()!=blank);
 CHECK(put("\x1b[2J\x1b[H\xe2\x98\x95"));
 printf("WIDEEDIT_GLYPH kind=%d cell=%dx%d cursor=%d blank=%08lx glyph=%08lx\n",kind,u->cu_XRSize,u->cu_YRSize,u->cu_XCP,(unsigned long)blank,(unsigned long)pixels());fflush(stdout);
 CHECK(u->cu_XCP==2);CHECK(pixels()!=blank);Delay(150);
 int width=u->cu_XMax+1;CHECK(width>15&&width<180);
 for(fragment=0;fragment<2;fragment++)for(style=0;style<3;style++)for(code=0;code<48;code++){
  int positions[12]={0,1,2,3,4,5,6,7,8,9,width-2,width-1};
  int amounts[4]={0,1,2,1000};int x=positions[code/4],n=amounts[code%4];
  if(n<1)n=1;
  if(n>width-x)n=width-x;
  unsigned original[180],answer[180];
  for(int i=0;i<width;i++)original[i]=' ';
  original[0]='A';original[1]='B';original[2]=0x2615;original[3]=0xffffffff;
  original[4]='C';original[5]=0x03a9;original[6]='D';original[7]='E';
  original[width-2]=0x2615;original[width-1]=0xffffffff;
  if(ascii){original[2]=original[width-2]='W';original[3]=original[width-1]='X';}
  /* Independent per-cell source mapping; never call device repair helpers. */
  for(int i=0;i<width;i++){
   int source=i;
   if(style==0&&i>=x)source=i<x+n?-1:i-n;
   if(style==1&&i>=x)source=i+n<width?i+n:-1;
   if(style==2&&i>=x&&i<x+n)source=-1;
   answer[i]=source<0?' ':original[source];
  }
  unsigned valid[180];memcpy(valid,answer,sizeof(answer));
  for(int i=0;i<width;i++){
   if(answer[i]==0x2615&&(i+1==width||answer[i+1]!=0xffffffff))valid[i]=' ';
   if(answer[i]==0xffffffff&&(i==0||answer[i-1]!=0x2615))valid[i]=' ';
  }
  char text[1024],cmd[64];int len=0;
  for(int i=0;i<width;i++){
   unsigned cp=original[i];if(cp==0xffffffff)continue;
   if(cp==0x2615){memcpy(text+len,"\xe2\x98\x95",3);len+=3;}
   else if(cp==0x03a9){memcpy(text+len,"\xce\xa9",2);len+=2;}
   else text[len++]=cp;
  }text[len]=0;
  CHECK(put("\x1b[0;38;2;230;240;245;48;2;25;60;80m\x1b[2J\x1b[H"));CHECK(put(text));
  snprintf(cmd,sizeof(cmd),"\x1b[1;%dH\x1b[%d%c",x+1,amounts[code%4],"@PX"[style]);
  CHECK(fragment?split(cmd):put(cmd));CHECK(u->cu_XCP==x&&u->cu_YCP==0);
  ULONG actual=pixels();if(!standard){CHECK(put("\x1b[>9h\x1b[>9l"));CHECK(pixels()==actual);}
  if(!standard && code==0){int oldWidth=w->Width,delta=2*u->cu_XRSize;SizeWindow(w,delta,0);CHECK(resized(oldWidth+delta,width+2));SizeWindow(w,-delta,0);CHECK(resized(oldWidth,width));CHECK(pixels()==actual);}
  len=0;for(int i=0;i<width;i++){
   unsigned cp=valid[i];if(cp==0xffffffff)continue;
   if(cp==0x2615){memcpy(text+len,"\xe2\x98\x95",3);len+=3;}
   else if(cp==0x03a9){memcpy(text+len,"\xce\xa9",2);len+=2;}
   else text[len++]=cp;
  }text[len]=0;
  CHECK(put("\x1b[2J\x1b[H"));CHECK(put(text));ULONG expected=pixels();
 if(expected!=actual){
  printf("WIDEEDIT_MISMATCH kind=%d fragment=%d style=%d code=%d column=%d amount=%d actual=%08lx expected=%08lx\n",kind,fragment,style,code,x,amounts[code%4],(unsigned long)actual,(unsigned long)expected);fflush(stdout);mismatches++;
  if(!standard)CHECK(expected==actual);
 }
 cases++;
 }
 printf("WIDEEDIT_AUDIT kind=%d ascii=%d cases=%d mismatches=%d\n",kind,ascii,cases,mismatches);fflush(stdout);CHECK(!mismatches);
 CHECK(put("\x1b[0m\x1b[2J\x1b[H"));
 CHECK(put(ascii?"PASS: narrow character edit control\r\n\r\nInsert, delete and erase; ordinary single-cell text\r\n":"PASS: wide character edits match reference pixels\r\n\r\nInsert, delete and erase through wide cell halves\r\n"));
 CHECK(put("Zero/default, count and clipped edits; Unicode\r\nWhole and bytewise input; native console"));
 if(!standard)CHECK(put("\r\nHide/reveal and resize preserve edited pixels"));
 printf("WIDEEDIT_UNIT_PASS kind=%d cases=%d\n",kind,cases);fflush(stdout);Delay(150);CloseDevice((struct IORequest*)io);opened=0;CloseWindow(w);w=NULL;
}
printf("WIDEEDIT_PASS cases=%d\n",cases);rc=0;
out:if(opened)CloseDevice((struct IORequest*)io);if(w)CloseWindow(w);if(CyberGfxBase)CloseLibrary(CyberGfxBase);if(io)DeleteIORequest((struct IORequest*)io);if(p)DeleteMsgPort(p);printf("WIDEEDIT_RUN rc=%d\n",rc);return rc;}
