/* Real DOS streams: cooked/raw, whole/bytewise UTF-8, CJK and color emoji. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/cybergraphics.h>
#include <devices/conunit.h>
#include <dos/dosextens.h>
#include <stdio.h>
#include <string.h>
struct Library *CyberGfxBase;
static struct IOStdReq *io;static struct Window*w;static struct ConUnit*u;
static BPTR stream;
static int fragmented;
static int put(const char*s){
 ULONG n=strlen(s);
 if(!fragmented)return Write(stream,s,n)==(LONG)n;
 for(ULONG i=0;i<n;i++)if(Write(stream,s+i,1)!=1)return 0;
 return 1;
}
static ULONG hashcell(void){ULONG h=2166136261u;for(int y=0;y<u->cu_YRSize;y++)for(int x=0;x<2*u->cu_XRSize;x++)h=(h^ReadRGBPixel(w->RPort,u->cu_XROrigin+x,u->cu_YROrigin+y))*16777619u;return h;}
#define CHECK(c) do{if(!(c)){printf("UNICODEDOS_FAIL line=%d kind=%d font=%d glyph=%d\n",__LINE__,kind,f,g);fflush(stdout);Delay(500);goto out;}}while(0)
int main(void){int rc=20,f=0,g=0,cases=0,kind=0;
CyberGfxBase=OpenLibrary((CONST_STRPTR)"cybergraphics.library",0);CHECK(CyberGfxBase);
for(kind=0;kind<4;kind++){
 struct InfoData info;
 stream=Open((CONST_STRPTR)"RGCON:",MODE_OLDFILE);CHECK(stream);
 struct FileHandle*fh=BADDR(stream);
 CHECK(DoPkt(fh->fh_Type,ACTION_DISK_INFO,(SIPTR)MKBADDR(&info),0,0,0,0));
 io=(void*)(IPTR)info.id_InUse;CHECK(io&&io->io_Unit&&io->io_Device);
 CHECK(!strcmp((char*)io->io_Device->dd_Library.lib_Node.ln_Name,"consoleng.device"));
 u=(void*)io->io_Unit;w=u->cu_Window;CHECK(w);
 SetWindowTitles(w,(CONST_STRPTR)"ConsoleNG Unicode through DOS streams",(CONST_STRPTR)-1);
 MoveWindow(w,30-w->LeftEdge,30-w->TopEdge);SizeWindow(w,730-w->Width,470-w->Height);Delay(20);
 CHECK(SetMode(stream,kind&1));fragmented=kind>>1;
const char*fonts[]={"topaz,8","DejaVuSansMono.font,16","DejaVuSansMono.font,24","DejaVuSansMono.font,32"};
const char*glyphs[]={"\xf0\x9f\x98\x80","\xf0\x9f\x9a\x80","\xf0\x9f\x90\xb1","\xf0\x9f\x8c\x8d","\xf0\x9f\x8e\x89","\xe4\xb8\xad","\xe6\x97\xa5","\xe8\xaa\x9e","\xed\x95\x9c","\xea\xb8\x80"};
for(f=0;f<4;f++)for(g=0;g<10;g++){
char seq[100];snprintf(seq,sizeof(seq),"\x1b%%G\x1b[?25l\x1b]50;%s\x07\x1b[0;38;2;230;240;245;48;2;25;60;80m\x1b[2J\x1b[H",fonts[f]);CHECK(put(seq));CHECK(u->cu_YRSize==(f?8+f*8:8));CHECK(put(glyphs[g]));CHECK(u->cu_XCCP==2);
int color=0;for(int y=0;y<u->cu_YRSize;y++)for(int x=0;x<2*u->cu_XRSize;x++){ULONG rgb=ReadRGBPixel(w->RPort,u->cu_XROrigin+x,u->cu_YROrigin+y);if(g<5 ? (rgb!=0x193c50&&rgb!=0xe6f0f5&&((rgb>>16)&255)!=(rgb&255)) : rgb==0xe6f0f5)color++;}CHECK(color>3);
for(int y=0;y<2*u->cu_YRSize;y++)for(int x=0;x<4*u->cu_XRSize;x++)if(y>=u->cu_YRSize||x>=2*u->cu_XRSize)CHECK(ReadRGBPixel(w->RPort,u->cu_XROrigin+x,u->cu_YROrigin+y)==0x193c50);
ULONG before=hashcell();CHECK(put("\x1b[>9h\x1b[>9l"));CHECK(hashcell()==before);
SizeWindow(w,-40,0);Delay(10);CHECK(hashcell()==before);SizeWindow(w,40,0);Delay(10);CHECK(hashcell()==before);
CHECK(put("\x1b[2J\x1b[H\x1b[8m"));CHECK(put(glyphs[g]));
for(int phase=0;phase<2;phase++){for(int y=0;y<u->cu_YRSize;y++)for(int x=0;x<2*u->cu_XRSize;x++)CHECK(ReadRGBPixel(w->RPort,u->cu_XROrigin+x,u->cu_YROrigin+y)==0x193c50);CHECK(put("\x1b[>9h\x1b[>9l"));}
printf("UNICODEDOS_CASE font=%d glyph=%d colored=%d PASS\n",f,g,color);fflush(stdout);cases++;
}
CHECK(put("\x1b[0;38;2;230;240;245;48;2;25;60;80m\x1b]50;DejaVuSansMono.font,24\x07\x1b[2J\x1b[HConsoleNG: Unicode through DOS\r\n\r\n"));
for(g=0;g<10;g++){CHECK(put(glyphs[g]));CHECK(put("  "));}
CHECK(put("\r\n\r\nCooked / raw; whole / bytewise UTF-8\r\nTopaz 8 and DejaVu 16 / 24 / 32\r\n\r\nNoto Color Emoji + Chinese / Japanese / Korean"));printf("UNICODEDOS_UNIT_PASS kind=%d cases=%d\n",kind,cases);fflush(stdout);
if(kind==3){puts("UNICODEDOS_SCREENSHOT_READY");fflush(stdout);Delay(600);}
BOOL closed=Close(stream);stream=0;io=NULL;w=NULL;CHECK(closed);
}
printf("UNICODEDOS_PASS cases=%d\n",cases);fflush(stdout);rc=0;
out:if(stream)Close(stream);if(CyberGfxBase)CloseLibrary(CyberGfxBase);printf("UNICODEDOS_SCRIPT rc=%d\n",rc);return rc;}
