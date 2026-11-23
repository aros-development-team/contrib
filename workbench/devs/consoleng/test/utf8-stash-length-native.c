/* Real CMD_WRITE lengths spanning signed-WORD boundaries after UTF-8 tails. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/cybergraphics.h>
#include <devices/conunit.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct Library *CyberGfxBase;
static struct IOStdReq *io;static struct Window*w;static struct ConUnit*u;
static int put(const void*s,ULONG n){io->io_Command=CMD_WRITE;io->io_Data=(APTR)s;io->io_Length=n;return !DoIO((struct IORequest*)io)&&io->io_Actual==n;}
static int text(const char*s){return put(s,strlen(s));}
static ULONG pixels(void){ULONG h=2166136261u;for(int y=0;y<u->cu_YRSize;y++)for(int x=0;x<4*u->cu_XRSize;x++)h=(h^ReadRGBPixel(w->RPort,u->cu_XROrigin+x,u->cu_YROrigin+y))*16777619u;return h;}
#define CHECK(c) do{if(!(c)){printf("UTF8LEN_FAIL line=%d glyph=%d prefix=%d size=%lu check=%s\n",__LINE__,g,prefix,(unsigned long)n,#c);fflush(stdout);Delay(500);goto out;}}while(0)
int main(void){int rc=20,opened=0,g=0,prefix=0,cases=0;ULONG n=0;UBYTE*buf=calloc(1048576,1);struct MsgPort*p=CreateMsgPort();io=p?(void*)CreateIORequest(p,sizeof(*io)):NULL;CHECK(buf&&io);
CyberGfxBase=OpenLibrary((CONST_STRPTR)"cybergraphics.library",0);CHECK(CyberGfxBase);
w=OpenWindowTags(NULL,WA_Left,30,WA_Top,30,WA_Width,730,WA_Height,470,WA_Title,(IPTR)"ConsoleNG split UTF-8 large-write regression",WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_SizeGadget,TRUE,WA_SizeBRight,TRUE,WA_MinWidth,300,WA_MinHeight,200,WA_MaxWidth,-1,WA_MaxHeight,-1,TAG_DONE);CHECK(w);
io->io_Data=w;io->io_Length=sizeof(*w);CHECK(!OpenDevice((CONST_STRPTR)"UTF8LEN:Devs/consoleng.device",CONU_CHARMAP,(struct IORequest*)io,0));opened=1;u=(void*)io->io_Unit;
CHECK(text("\x1b%G\x1b[?25l\x1b]50;DejaVuSansMono.font,24\7\x1b[0;38;2;230;240;245;48;2;25;60;80m"));
const char*glyphs[]={"\xc2\xa3","\xe4\xb8\xad","\xf0\x9f\x98\x80"};ULONG sizes[]={3,32767,32768,65535,65536,65537,1048576};
for(g=0;g<3;g++)for(prefix=1;prefix<(int)strlen(glyphs[g]);prefix++)for(unsigned j=0;j<sizeof(sizes)/sizeof(*sizes);j++){
 n=sizes[j];CHECK(text("\x1b[2J\x1b[H"));CHECK(text(glyphs[g]));ULONG expected=pixels();WORD cursor=u->cu_XCCP;
 CHECK(text("\x1b[>9h\x1b[>9l"));ULONG retained=pixels();CHECK(retained==expected);
 CHECK(text("\x1b[2J\x1b[H"));CHECK(put(glyphs[g],prefix));CHECK(u->cu_XCCP==0);
 memset(buf,0,n);memcpy(buf,glyphs[g]+prefix,strlen(glyphs[g])-prefix);CHECK(put(buf,n));CHECK(u->cu_XCCP==cursor);CHECK(pixels()==expected);
 CHECK(text("\x1b[>9h\x1b[>9l"));CHECK(pixels()==retained);cases++;
 printf("UTF8LEN_CASE glyph=%d prefix=%d bytes=%lu PASS\n",g,prefix,(unsigned long)n);fflush(stdout);
}
CHECK(text("\x1b[2J\x1b[HSplit UTF-8: large writes PASS\r\n\r\n\xc2\xa3  \xe4\xb8\xad  \xf0\x9f\x98\x80\r\n\r\n42 native length-boundary cases\r\n32 KiB / 64 KiB / 1 MiB writes\r\nExact glyph pixels and retained redraw\r\nEvery split of 2 / 3 / 4-byte UTF-8"));printf("UTF8LEN_PASS cases=%d\n",cases);fflush(stdout);Delay(600);rc=0;
out:if(opened)CloseDevice((struct IORequest*)io);if(w)CloseWindow(w);if(io)DeleteIORequest((struct IORequest*)io);if(p)DeleteMsgPort(p);if(CyberGfxBase)CloseLibrary(CyberGfxBase);free(buf);printf("UTF8LEN_SCRIPT rc=%d\n",rc);return rc;}
