/* Invalid complete UTF-8 encodings become one fallback without changing controls. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/cybergraphics.h>
#include <devices/conunit.h>
#include <dos/dosextens.h>
#include <stdio.h>
#include <string.h>
struct Library *CyberGfxBase;
static BPTR stream;static struct Window*w;static struct ConUnit*u;
static int put(const void*s,ULONG n){return Write(stream,s,n)==(LONG)n;}
static int text(const char*s){return put(s,strlen(s));}
static int split(const char*s){while(*s){if(!put(s++,1))return 0;}return 1;}
static ULONG pixels(void){ULONG h=2166136261u;for(int y=0;y<4*u->cu_YRSize;y++)for(int x=0;x<20*u->cu_XRSize;x++)h=(h^ReadRGBPixel(w->RPort,u->cu_XROrigin+x,u->cu_YROrigin+y))*16777619u;return h;}
#define CHECK(condition) do{if(!(condition)){printf("SCALAR_FAIL line=%d kind=%d glyph=%d style=%d ctl=%d split=%d check=%s\n",__LINE__,kind,g,style,c,fragment,#condition);fflush(stdout);Delay(500);goto out;}}while(0)
int main(void){int rc=20,kind=0,g=0,style=0,c=0,fragment=0,cases=0;CyberGfxBase=OpenLibrary((CONST_STRPTR)"cybergraphics.library",0);CHECK(CyberGfxBase);
const char*bad[]={"\xc0\x80","\xc0\x9b","\xc1\xbf","\xe0\x80\x80","\xe0\x80\x9b","\xe0\x9f\xbf","\xed\xa0\x80","\xed\xbf\xbf","\xf0\x80\x80\x80","\xf0\x8f\xbf\xbf","\xf4\x90\x80\x80","\xf7\xbf\xbf\xbf"};
const char*styles[]={"\x1b[27;28m","\x1b[7m","\x1b[8m"};
const char*control="\x1b[2;3HQ";
const char*reset="\x1b%G\x1b[?25l\x1b]50;DejaVuSansMono.font,16\7\x1b[0;38;2;230;240;245;48;2;25;60;80m\x1b[2J\x1b[H";
for(kind=0;kind<2;kind++){
 struct InfoData info;stream=Open((CONST_STRPTR)"RGCON:",MODE_OLDFILE);CHECK(stream);struct FileHandle*fh=BADDR(stream);CHECK(DoPkt(fh->fh_Type,ACTION_DISK_INFO,(SIPTR)MKBADDR(&info),0,0,0,0));
 struct IOStdReq*io=(void*)(IPTR)info.id_InUse;CHECK(io&&io->io_Unit&&io->io_Device);CHECK(!strcmp((char*)io->io_Device->dd_Library.lib_Node.ln_Name,"consoleng.device"));u=(void*)io->io_Unit;w=u->cu_Window;CHECK(w);
 MoveWindow(w,30-w->LeftEdge,30-w->TopEdge);SizeWindow(w,730-w->Width,470-w->Height);Delay(20);CHECK(SetMode(stream,kind));
 for(g=0;g<12;g++)for(style=0;style<3;style++)for(fragment=0;fragment<2;fragment++){
  CHECK(text(reset));CHECK(text(styles[style]));CHECK(text("?"));CHECK(text(control));ULONG expected=pixels();WORD x=u->cu_XCCP,y=u->cu_YCCP;UBYTE flags=u->cu_TxFlags;BYTE fg=u->cu_FgPen,bg=u->cu_BgPen;
  CHECK(text("\x1b[>9h\x1b[>9l"));CHECK(pixels()==expected);
  CHECK(text(reset));CHECK(text(styles[style]));CHECK(fragment?split(bad[g]):text(bad[g]));CHECK(u->cu_XCCP==1);CHECK(text(control));
  CHECK(u->cu_XCCP==x&&u->cu_YCCP==y&&u->cu_TxFlags==flags&&u->cu_FgPen==fg&&u->cu_BgPen==bg);CHECK(pixels()==expected);
  CHECK(text("\x1b[>9h\x1b[>9l"));CHECK(pixels()==expected);cases++;
 }
 CHECK(text(reset));CHECK(text("Unicode scalar validation: PASS\r\n\r\nOverlong / surrogate / out-of-range encodings\r\nOne fallback; cursor and controls preserved\r\nNormal / reverse / concealed text\r\nCooked and raw streams; whole and bytewise\r\n144 native pixel comparisons"));
 printf("SCALAR_UNIT_PASS kind=%d cases=%d\n",kind,cases);fflush(stdout);
 if(kind==1){puts("SCALAR_SCREENSHOT_READY");fflush(stdout);Delay(600);}
 BOOL closed=Close(stream);stream=0;w=NULL;CHECK(closed);
}
printf("SCALAR_PASS cases=%d\n",cases);rc=0;
out:if(stream)Close(stream);if(CyberGfxBase)CloseLibrary(CyberGfxBase);printf("SCALAR_SCRIPT rc=%d\n",rc);return rc;}
