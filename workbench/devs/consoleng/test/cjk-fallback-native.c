/* Actual device fallback, cell containment, retained redraw and font changes. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/cybergraphics.h>
#include <devices/conunit.h>
#include <stdio.h>
#include <string.h>
#include "../console_gcc.h"
#undef IntuitionBase
struct Library *CyberGfxBase;
static struct IOStdReq *io;static struct Window*w;static struct ConUnit*u;
static int put(const char*s){io->io_Command=CMD_WRITE;io->io_Data=(APTR)s;io->io_Length=strlen(s);return !DoIO((struct IORequest*)io)&&io->io_Actual==strlen(s);}
static ULONG hashcell(void){ULONG h=2166136261u;for(int y=0;y<u->cu_YRSize;y++)for(int x=0;x<2*u->cu_XRSize;x++)h=(h^ReadRGBPixel(w->RPort,u->cu_XROrigin+x,u->cu_YROrigin+y))*16777619u;return h;}
#define CHECK(c) do{if(!(c)){printf("CJKFALLBACK_FAIL line=%d font=%d glyph=%d\n",__LINE__,f,g);fflush(stdout);Delay(500);goto out;}}while(0)
int main(void){int rc=20,opened=0,f=0,g=0,cases=0;struct MsgPort*p=CreateMsgPort();io=p?(void*)CreateIORequest(p,sizeof(*io)):NULL;CHECK(io);CyberGfxBase=OpenLibrary((CONST_STRPTR)"cybergraphics.library",0);CHECK(CyberGfxBase);
w=OpenWindowTags(NULL,WA_Left,30,WA_Top,30,WA_Width,730,WA_Height,470,WA_Title,(IPTR)"ConsoleNG native CJK glyph fallback",WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_SizeGadget,TRUE,WA_SizeBRight,TRUE,WA_MinWidth,300,WA_MinHeight,200,WA_MaxWidth,-1,WA_MaxHeight,-1,TAG_DONE);CHECK(w);
io->io_Data=w;io->io_Length=sizeof(*w);CHECK(!OpenDevice((CONST_STRPTR)"CJKF:Devs/consoleng.device",CONU_CHARMAP,(struct IORequest*)io,0));opened=1;u=(void*)io->io_Unit;
const char*fonts[]={"topaz,8","DejaVuSansMono.font,16","DejaVuSansMono.font,24","DejaVuSansMono.font,32"};
const char*glyphs[]={"\xe4\xb8\xad","\xe6\x97\xa5","\xe8\xaa\x9e","\xed\x95\x9c","\xea\xb8\x80"};
for(f=0;f<4;f++)for(g=0;g<5;g++){
char seq[100];snprintf(seq,sizeof(seq),"\x1b%%G\x1b[?25l\x1b]50;%s\x07\x1b[0;38;2;230;240;245;48;2;25;60;80m\x1b[2J\x1b[H",fonts[f]);CHECK(put(seq));CHECK(u->cu_YRSize==(f?8+f*8:8));CHECK(put(glyphs[g]));CHECK(u->cu_XCCP==2);
int color=0;for(int y=0;y<u->cu_YRSize;y++)for(int x=0;x<2*u->cu_XRSize;x++){ULONG rgb=ReadRGBPixel(w->RPort,u->cu_XROrigin+x,u->cu_YROrigin+y);if(rgb==0xe6f0f5)color++;}if(color<=3){struct ConsoleBase *b=(void*)io->io_Device;ULONG ink=0,nonbg=0;for(ULONG i=0;i<b->glyphMaskSize;i++)if(b->glyphMask[i])ink++;for(int y=0;y<u->cu_YRSize;y++)for(int x=0;x<2*u->cu_XRSize;x++){ULONG rgb=ReadRGBPixel(w->RPort,u->cu_XROrigin+x,u->cu_YROrigin+y);if(rgb!=0x193c50){if(nonbg<8)printf("CJK_PIXEL %lx\n",(unsigned long)rgb);nonbg++;}}printf("CJK_STATE failed=%d engine=%p mask=%lu inkbytes=%lu nonbg=%lu drawmode=%d fg=%lu bg=%lu\n",b->cjkFailed,b->cjkEngine,(unsigned long)b->glyphMaskSize,(unsigned long)ink,(unsigned long)nonbg,w->RPort->DrawMode,(unsigned long)w->RPort->FgPen,(unsigned long)w->RPort->BgPen);fflush(stdout);}CHECK(color>3);
for(int y=0;y<2*u->cu_YRSize;y++)for(int x=0;x<4*u->cu_XRSize;x++)if(y>=u->cu_YRSize||x>=2*u->cu_XRSize)CHECK(ReadRGBPixel(w->RPort,u->cu_XROrigin+x,u->cu_YROrigin+y)==0x193c50);
ULONG before=hashcell();CHECK(put("\x1b[>9h\x1b[>9l"));CHECK(hashcell()==before);
SizeWindow(w,-40,0);Delay(10);CHECK(hashcell()==before);SizeWindow(w,40,0);Delay(10);CHECK(hashcell()==before);
CHECK(put("\x1b[2J\x1b[H\x1b[8m"));CHECK(put(glyphs[g]));
for(int phase=0;phase<2;phase++){for(int y=0;y<u->cu_YRSize;y++)for(int x=0;x<2*u->cu_XRSize;x++)CHECK(ReadRGBPixel(w->RPort,u->cu_XROrigin+x,u->cu_YROrigin+y)==0x193c50);CHECK(put("\x1b[>9h\x1b[>9l"));}
printf("CJKFALLBACK_CASE font=%d glyph=%d colored=%d PASS\n",f,g,color);fflush(stdout);cases++;
}
CHECK(put("\x1b[0;38;2;230;240;245;48;2;25;60;80m\x1b]50;DejaVuSansMono.font,24\x07\x1b[2J\x1b[HConsoleNG: native CJK fallback\r\n\r\n"));
for(g=0;g<5;g++){CHECK(put(glyphs[g]));CHECK(put("  "));}
CHECK(put("\r\n\r\n20 cell / redraw / resize checks PASS\r\nTopaz 8 and DejaVu 16 / 24 / 32\r\n\r\nNoto Sans CJK: Chinese / Japanese / Korean"));printf("CJKFALLBACK_PASS cases=%d\n",cases);fflush(stdout);Delay(600);rc=0;
out:if(opened)CloseDevice((struct IORequest*)io);if(w)CloseWindow(w);if(io)DeleteIORequest((struct IORequest*)io);if(p)DeleteMsgPort(p);if(CyberGfxBase)CloseLibrary(CyberGfxBase);printf("CJKFALLBACK_SCRIPT rc=%d\n",rc);return rc;}
