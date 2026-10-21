/* DOS writes: an incomplete UTF-8 character must not eat the following control. */
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
#define CHECK(condition) do{if(!(condition)){printf("UTF8INT_FAIL line=%d kind=%d glyph=%d prefix=%d ctl=%d split=%d check=%s\n",__LINE__,kind,g,prefix,c,fragment,#condition);fflush(stdout);Delay(500);goto out;}}while(0)
int main(void){int rc=20,kind=0,g=0,prefix=0,c=0,fragment=0,cases=0;CyberGfxBase=OpenLibrary((CONST_STRPTR)"cybergraphics.library",0);CHECK(CyberGfxBase);
const char*glyphs[]={"\xc2\xa3","\xe4\xb8\xad","\xf0\x9f\x98\x80"};
const char*controls[]={"\x1b[2;3H","\r","\n","\x1b[31m","\x1b]2;UTF8 interruption test\7","Z"};
const char*reset="\x1b%G\x1b[?25l\x1b]50;DejaVuSansMono.font,16\7\x1b[0;38;2;230;240;245;48;2;25;60;80m\x1b[2J\x1b[H";
for(kind=0;kind<2;kind++){
 struct InfoData info;stream=Open((CONST_STRPTR)"RGCON:",MODE_OLDFILE);CHECK(stream);struct FileHandle*fh=BADDR(stream);CHECK(DoPkt(fh->fh_Type,ACTION_DISK_INFO,(SIPTR)MKBADDR(&info),0,0,0,0));
 struct IOStdReq*io=(void*)(IPTR)info.id_InUse;CHECK(io&&io->io_Unit&&io->io_Device);CHECK(!strcmp((char*)io->io_Device->dd_Library.lib_Node.ln_Name,"consoleng.device"));u=(void*)io->io_Unit;w=u->cu_Window;CHECK(w);
 MoveWindow(w,30-w->LeftEdge,30-w->TopEdge);SizeWindow(w,730-w->Width,470-w->Height);Delay(20);CHECK(SetMode(stream,kind));
 for(g=0;g<3;g++)for(prefix=1;prefix<(int)strlen(glyphs[g]);prefix++)for(c=0;c<6;c++)for(fragment=0;fragment<2;fragment++){
  CHECK(text(reset));CHECK(text("?"));CHECK(text(controls[c]));CHECK(text("Q"));ULONG expected=pixels();WORD x=u->cu_XCCP,y=u->cu_YCCP;UBYTE flags=u->cu_TxFlags;BYTE fg=u->cu_FgPen,bg=u->cu_BgPen;
  CHECK(text("\x1b[>9h\x1b[>9l"));ULONG retained=pixels();
  CHECK(text(reset));CHECK(put(glyphs[g],prefix));CHECK(u->cu_XCCP==0);CHECK(fragment?split(controls[c]):text(controls[c]));CHECK(text("Q"));
  CHECK(u->cu_XCCP==x&&u->cu_YCCP==y&&u->cu_TxFlags==flags&&u->cu_FgPen==fg&&u->cu_BgPen==bg);CHECK(pixels()==expected);if(c==4)CHECK(!strcmp((char*)w->Title,"UTF8 interruption test"));
  CHECK(text("\x1b[>9h\x1b[>9l"));CHECK(pixels()==retained);cases++;
 }
 CHECK(text(reset));CHECK(text("Interrupted UTF-8: DOS streams PASS\r\n\r\nEscape sequences survive partial characters\r\nCursor movement / colors / titles / CR / LF\r\nPrintable bytes preserved\r\nCooked and raw streams; whole and bytewise\r\nExisting truncated-character fallback retained"));
 printf("UTF8INT_UNIT_PASS kind=%d cases=%d\n",kind,cases);fflush(stdout);
 if(kind==1){puts("UTF8INT_SCREENSHOT_READY");fflush(stdout);Delay(600);}
 BOOL closed=Close(stream);stream=0;w=NULL;CHECK(closed);
}
printf("UTF8INT_PASS cases=%d\n",cases);rc=0;
out:if(stream)Close(stream);if(CyberGfxBase)CloseLibrary(CyberGfxBase);printf("UTF8INT_SCRIPT rc=%d\n",rc);return rc;}
