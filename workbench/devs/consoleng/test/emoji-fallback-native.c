/* Actual device fallback, cell containment, retained redraw and font changes. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/cybergraphics.h>
#include <devices/conunit.h>
#include <stdio.h>
#include <string.h>
struct Library *CyberGfxBase;
static struct IOStdReq *io;static struct Window*w;static struct ConUnit*u;
static int put(const char*s){io->io_Command=CMD_WRITE;io->io_Data=(APTR)s;io->io_Length=strlen(s);return !DoIO((struct IORequest*)io)&&io->io_Actual==strlen(s);}
static ULONG hashcell(void){ULONG h=2166136261u;for(int y=0;y<u->cu_YRSize;y++)for(int x=0;x<2*u->cu_XRSize;x++)h=(h^ReadRGBPixel(w->RPort,u->cu_XROrigin+x,u->cu_YROrigin+y))*16777619u;return h;}
#define CHECK(c) do{if(!(c)){printf("EMOJIFALLBACK_FAIL line=%d font=%d glyph=%d\n",__LINE__,f,g);fflush(stdout);Delay(500);goto out;}}while(0)
int main(void){int rc=20,opened=0,f=0,g=0,cases=0;struct MsgPort*p=CreateMsgPort();io=p?(void*)CreateIORequest(p,sizeof(*io)):NULL;CHECK(io);CyberGfxBase=OpenLibrary((CONST_STRPTR)"cybergraphics.library",0);CHECK(CyberGfxBase);
w=OpenWindowTags(NULL,WA_Left,30,WA_Top,30,WA_Width,730,WA_Height,470,WA_Title,(IPTR)"ConsoleNG native Google emoji fallback",WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_SizeGadget,TRUE,WA_SizeBRight,TRUE,WA_MinWidth,300,WA_MinHeight,200,WA_MaxWidth,-1,WA_MaxHeight,-1,TAG_DONE);CHECK(w);
io->io_Data=w;io->io_Length=sizeof(*w);CHECK(!OpenDevice((CONST_STRPTR)"EMOF:Devs/consoleng.device",CONU_CHARMAP,(struct IORequest*)io,0));opened=1;u=(void*)io->io_Unit;
const char*fonts[]={"topaz,8","DejaVuSansMono.font,16","DejaVuSansMono.font,24","DejaVuSansMono.font,32"};
const char*glyphs[]={"\xf0\x9f\x98\x80","\xf0\x9f\x9a\x80","\xf0\x9f\x90\xb1","\xf0\x9f\x8c\x8d","\xf0\x9f\x8e\x89"};
for(f=0;f<4;f++)for(g=0;g<5;g++){
char seq[100];snprintf(seq,sizeof(seq),"\x1b%%G\x1b[?25l\x1b]50;%s\x07\x1b[0;48;2;25;60;80m\x1b[2J\x1b[H",fonts[f]);CHECK(put(seq));CHECK(u->cu_YRSize==(f?8+f*8:8));CHECK(put(glyphs[g]));CHECK(u->cu_XCCP==2);
int color=0;for(int y=0;y<u->cu_YRSize;y++)for(int x=0;x<2*u->cu_XRSize;x++){ULONG rgb=ReadRGBPixel(w->RPort,u->cu_XROrigin+x,u->cu_YROrigin+y);if(rgb!=0x193c50&&((rgb>>16)&255)!=(rgb&255))color++;}CHECK(color>3);
for(int y=0;y<2*u->cu_YRSize;y++)for(int x=0;x<4*u->cu_XRSize;x++)if(y>=u->cu_YRSize||x>=2*u->cu_XRSize)CHECK(ReadRGBPixel(w->RPort,u->cu_XROrigin+x,u->cu_YROrigin+y)==0x193c50);
ULONG before=hashcell();CHECK(put("\x1b[>9h\x1b[>9l"));CHECK(hashcell()==before);
SizeWindow(w,-40,0);Delay(10);CHECK(hashcell()==before);SizeWindow(w,40,0);Delay(10);CHECK(hashcell()==before);
CHECK(put("\x1b[2J\x1b[H\x1b[8m"));CHECK(put(glyphs[g]));
for(int phase=0;phase<2;phase++){for(int y=0;y<u->cu_YRSize;y++)for(int x=0;x<2*u->cu_XRSize;x++)CHECK(ReadRGBPixel(w->RPort,u->cu_XROrigin+x,u->cu_YROrigin+y)==0x193c50);CHECK(put("\x1b[>9h\x1b[>9l"));}
printf("EMOJIFALLBACK_CASE font=%d glyph=%d colored=%d PASS\n",f,g,color);fflush(stdout);cases++;
}
CHECK(put("\x1b[0;48;2;25;60;80m\x1b]50;DejaVuSansMono.font,24\x07\x1b[2J\x1b[HConsoleNG: native emoji fallback\r\n\r\n"));
for(g=0;g<5;g++){CHECK(put(glyphs[g]));CHECK(put("  "));}
CHECK(put("\r\n\r\n20 cell / redraw / resize checks PASS\r\nTopaz 8 and DejaVu 16 / 24 / 32\r\n\r\nGoogle Noto Color Emoji"));printf("EMOJIFALLBACK_PASS cases=%d\n",cases);fflush(stdout);Delay(600);rc=0;
out:if(opened)CloseDevice((struct IORequest*)io);if(w)CloseWindow(w);if(io)DeleteIORequest((struct IORequest*)io);if(p)DeleteMsgPort(p);if(CyberGfxBase)CloseLibrary(CyberGfxBase);printf("EMOJIFALLBACK_SCRIPT rc=%d\n",rc);return rc;}
