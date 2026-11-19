/* Native retained wide cursors: coverage, exact restoration and half overwrites. */
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
static ULONG original[8192],expected[8192];static int column;
static int put(const char*s){io->io_Command=CMD_WRITE;io->io_Data=(APTR)s;io->io_Length=strlen(s);return !DoIO((struct IORequest*)io)&&io->io_Actual==strlen(s);}
static ULONG pixel(int x,int y){return ReadRGBPixel(w->RPort,u->cu_XROrigin+column*u->cu_XRSize+x,u->cu_YROrigin+2*u->cu_YRSize+y);}
static void capture(ULONG*p){int width=3*u->cu_XRSize;for(int y=0;y<u->cu_YRSize;y++)for(int x=0;x<width;x++)p[y*width+x]=pixel(x,y);}
static int compare(const ULONG*p){int width=3*u->cu_XRSize;for(int y=0;y<u->cu_YRSize;y++)for(int x=0;x<width;x++)if(pixel(x,y)!=p[y*width+x]){printf("WIDECURSOR_PIXEL x=%d y=%d\n",x,y);return 0;}return 1;}
static int covered(int shape){int width=3*u->cu_XRSize,height=u->cu_YRSize;int left=0,right=2*u->cu_XRSize-1,top=0;if(shape==4)top=height-(height/8?height/8:1);if(shape==6)right=(u->cu_XRSize/8?u->cu_XRSize/8:1)-1;for(int y=0;y<height;y++)for(int x=0;x<width;x++){int changed=pixel(x,y)!=original[y*width+x];if(changed!=(x>=left&&x<=right&&y>=top)){printf("WIDECURSOR_MASK x=%d y=%d changed=%d\n",x,y,changed);return 0;}}return 1;}
static int position(int half){char s[40];snprintf(s,sizeof(s),"\x1b[3;%dH",column+half+1);return put(s);}
static int scene(const char*glyph){return put("\x1b[?25l\x1b[0;38;2;230;240;245;48;2;25;60;80m\x1b[2J")&&position(0)&&put(glyph)&&put("Q");}
#define CHECK(c) do{if(!(c)){printf("WIDECURSOR_FAIL line=%d font=%d glyph=%d half=%d shape=%d edge=%d\n",__LINE__,font,glyph,half,shape,edge);fflush(stdout);Delay(500);goto out;}}while(0)
int main(void){int rc=20,opened=0,font=0,glyph=0,half=0,shape=0,edge=0,cases=0;struct MsgPort*p=CreateMsgPort();io=p?(void*)CreateIORequest(p,sizeof(*io)):NULL;CHECK(io);CyberGfxBase=OpenLibrary((CONST_STRPTR)"cybergraphics.library",0);CHECK(CyberGfxBase);
w=OpenWindowTags(NULL,WA_Left,30,WA_Top,30,WA_Width,730,WA_Height,470,WA_Title,(IPTR)"ConsoleNG wide-cell cursor regression",WA_Activate,TRUE,WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_SizeGadget,TRUE,WA_SizeBRight,TRUE,WA_MinWidth,200,WA_MinHeight,160,WA_MaxWidth,-1,WA_MaxHeight,-1,TAG_DONE);CHECK(w);io->io_Data=w;io->io_Length=sizeof(*w);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",CONU_CHARMAP,(struct IORequest*)io,0));opened=1;u=(void*)io->io_Unit;CHECK(put("\x1b%G\x1b[?25l"));ActivateWindow(w);Delay(20);
const char*fonts[]={"\x1b]50;topaz,8\7","\x1b]50;DejaVuSansMono.font,16\7","\x1b]50;DejaVuSansMono.font,24\7"};const char*glyphs[]={"\xe4\xb8\xad","\xf0\x9f\x98\x80"};
for(font=0;font<3;font++){CHECK(put(fonts[font]));for(glyph=0;glyph<2;glyph++)for(half=0;half<2;half++)for(shape=2;shape<=6;shape+=2)for(edge=0;edge<2;edge++){
 column=edge?u->cu_XMax-2:0;CHECK(column>=0);CHECK(scene(glyphs[glyph]));char seq[24];snprintf(seq,sizeof(seq),"\x1b[%d q",shape);CHECK(put(seq));CHECK(position(half));capture(original);
 CHECK(put("\x1b[?25h"));CHECK(covered(shape));CHECK(put("\x1b[?25l"));CHECK(compare(original));
 /* Build the expected half-overwrite with the cursor hidden, then repeat visible. */
 CHECK(position(half));CHECK(put("X"));capture(expected);CHECK(scene(glyphs[glyph]));CHECK(position(half));CHECK(put("\x1b[?25hX\x1b[?25l"));CHECK(compare(expected));
 CHECK(scene(glyphs[glyph]));CHECK(position(half));capture(original);CHECK(put("\x1b[?25h"));
 if(!edge){SizeWindow(w,16,0);Delay(10);SizeWindow(w,-16,0);Delay(10);}
 CHECK(put("\x1b[?25l"));CHECK(compare(original));cases++;
}}
CHECK(put("\x1b[?25l\x1b[0m\x1b[2J\x1b[HPASS: 72 native wide cursor cases\r\n\r\nCJK / emoji; Topaz 8, DejaVu 16 / 24\r\nBlock / underline / bar; either cell half\r\nExact hide/overwrite/resize restoration\r\n\r\nWide cursor: \xe4\xb8\xad  \xf0\x9f\x98\x80\x1b[7;14H\x1b[2 q\x1b[?25h"));printf("WIDECURSOR_PASS cases=%d\n",cases);fflush(stdout);Delay(600);rc=0;
out:if(opened)CloseDevice((struct IORequest*)io);if(w)CloseWindow(w);if(io)DeleteIORequest((struct IORequest*)io);if(p)DeleteMsgPort(p);if(CyberGfxBase)CloseLibrary(CyberGfxBase);printf("WIDECURSOR_SCRIPT rc=%d\n",rc);return rc;}
