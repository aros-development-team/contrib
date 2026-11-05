/* Actual device fallback, cell containment, retained redraw and font changes. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/cybergraphics.h>
#include <devices/conunit.h>
#include "consoleng.h"
#include <stdio.h>
#include <string.h>
struct Library *CyberGfxBase;
static struct IOStdReq *io;static struct Window*w;static struct ConUnit*u;
static int put(const char*s){io->io_Command=CMD_WRITE;io->io_Data=(APTR)s;io->io_Length=strlen(s);return !DoIO((struct IORequest*)io)&&io->io_Actual==strlen(s);}
static int req(struct IOStdReq*q,UWORD cmd,void*p,ULONG n){q->io_Command=cmd;q->io_Data=p;q->io_Length=n;return !DoIO((struct IORequest*)q)&&q->io_Actual==n;}
static ULONG hashcell(void){ULONG h=2166136261u;for(int y=0;y<u->cu_YRSize;y++)for(int x=0;x<2*u->cu_XRSize;x++)h=(h^ReadRGBPixel(w->RPort,u->cu_XROrigin+x,u->cu_YROrigin+y))*16777619u;return h;}
#define CHECK(c) do{if(!(c)){printf("EMOJIPREFS_FAIL line=%d font=%d glyph=%d\n",__LINE__,f,g);fflush(stdout);Delay(500);goto out;}}while(0)
int main(void){int rc=20,opened=0,f=0,g=0,cases=0,controlOpened=0;struct IOStdReq*control=NULL;struct ConsolengEmojiPrefs saved,policy={12,1,1};struct MsgPort*p=CreateMsgPort();io=p?(void*)CreateIORequest(p,sizeof(*io)):NULL;CHECK(io);CyberGfxBase=OpenLibrary((CONST_STRPTR)"cybergraphics.library",0);CHECK(CyberGfxBase);
w=OpenWindowTags(NULL,WA_Left,30,WA_Top,30,WA_Width,730,WA_Height,470,WA_Title,(IPTR)"ConsoleNG native Google emoji fallback",WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_SizeGadget,TRUE,WA_SizeBRight,TRUE,WA_MinWidth,300,WA_MinHeight,200,WA_MaxWidth,-1,WA_MaxHeight,-1,TAG_DONE);CHECK(w);
io->io_Data=w;io->io_Length=sizeof(*w);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",CONU_CHARMAP,(struct IORequest*)io,0));opened=1;u=(void*)io->io_Unit;
control=(void*)CreateIORequest(p,sizeof(*control));CHECK(control);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",CONU_LIBRARY,(struct IORequest*)control,0));controlOpened=1;
CHECK(req(control,CD_GETDEFAULTEMOJIPREFS,&saved,sizeof(saved)));
CHECK(req(io,CD_GETEMOJIPREFS,&policy,sizeof(policy))&&policy.ep_Enabled==1);
policy.ep_Enabled=2;CHECK(!req(io,CD_SETEMOJIPREFS,&policy,sizeof(policy))&&io->io_Error==CONSOLENG_ERR_BADPREFS&&io->io_Actual==0);
CHECK(req(io,CD_GETEMOJIPREFS,&policy,sizeof(policy))&&policy.ep_Enabled==1);
policy.ep_Enabled=0;CHECK(req(control,CD_SETDEFAULTEMOJIPREFS,&policy,sizeof(policy)));
CHECK(req(io,CD_GETEMOJIPREFS,&policy,sizeof(policy))&&policy.ep_Enabled==1);
struct ConsolengThemeProfile theme;CHECK(req(control,CD_GETTHEMEPROFILE,&theme,sizeof(theme)));CHECK(req(control,CD_SETTHEMEPROFILE,&theme,sizeof(theme)));
CHECK(req(control,CD_GETDEFAULTEMOJIPREFS,&policy,sizeof(policy))&&policy.ep_Enabled==0);
struct Window*w2=OpenWindowTags(NULL,WA_Left,80,WA_Top,80,WA_Width,300,WA_Height,180,TAG_DONE);CHECK(w2);
struct IOStdReq*q=(void*)CreateIORequest(p,sizeof(*q));CHECK(q);q->io_Data=w2;q->io_Length=sizeof(*w2);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",CONU_CHARMAP,(struct IORequest*)q,0));
CHECK(req(q,CD_GETEMOJIPREFS,&policy,sizeof(policy))&&policy.ep_Enabled==0);CloseDevice((struct IORequest*)q);DeleteIORequest((struct IORequest*)q);CloseWindow(w2);
CHECK(req(control,CD_SETDEFAULTEMOJIPREFS,&saved,sizeof(saved)));
const char*fonts[]={"topaz,8","DejaVuSansMono.font,16","DejaVuSansMono.font,24","DejaVuSansMono.font,32"};
const char*glyphs[]={"\xf0\x9f\x98\x80","\xf0\x9f\x9a\x80","\xf0\x9f\x90\xb1","\xf0\x9f\x8c\x8d","\xf0\x9f\x8e\x89"};
for(f=0;f<4;f++)for(g=0;g<5;g++){
char seq[100];snprintf(seq,sizeof(seq),"\x1b%%G\x1b[?25l\x1b]50;%s\x07\x1b[0;48;2;25;60;80m\x1b[2J\x1b[H",fonts[f]);CHECK(put(seq));CHECK(u->cu_YRSize==(f?8+f*8:8));CHECK(put(glyphs[g]));CHECK(u->cu_XCCP==2);
int color=0;for(int y=0;y<u->cu_YRSize;y++)for(int x=0;x<2*u->cu_XRSize;x++){ULONG rgb=ReadRGBPixel(w->RPort,u->cu_XROrigin+x,u->cu_YROrigin+y);if(rgb!=0x193c50&&((rgb>>16)&255)!=(rgb&255))color++;}CHECK(color>3);
for(int y=0;y<2*u->cu_YRSize;y++)for(int x=0;x<4*u->cu_XRSize;x++)if(y>=u->cu_YRSize||x>=2*u->cu_XRSize)CHECK(ReadRGBPixel(w->RPort,u->cu_XROrigin+x,u->cu_YROrigin+y)==0x193c50);
ULONG before=hashcell();
policy.ep_Enabled=0;CHECK(req(io,CD_SETEMOJIPREFS,&policy,sizeof(policy)));CHECK(hashcell()!=before);
ULONG disabled=hashcell();CHECK(put("\x1b[2J\x1b[H"));CHECK(put(glyphs[g]));CHECK(hashcell()==disabled);
policy.ep_Enabled=1;CHECK(req(io,CD_SETEMOJIPREFS,&policy,sizeof(policy)));CHECK(hashcell()==before);CHECK(put("\x1b[>9h\x1b[>9l"));CHECK(hashcell()==before);
SizeWindow(w,-40,0);Delay(10);CHECK(hashcell()==before);SizeWindow(w,40,0);Delay(10);CHECK(hashcell()==before);
CHECK(put("\x1b[2J\x1b[H\x1b[8m"));CHECK(put(glyphs[g]));
for(int phase=0;phase<2;phase++){for(int y=0;y<u->cu_YRSize;y++)for(int x=0;x<2*u->cu_XRSize;x++)CHECK(ReadRGBPixel(w->RPort,u->cu_XROrigin+x,u->cu_YROrigin+y)==0x193c50);CHECK(put("\x1b[>9h\x1b[>9l"));}
printf("EMOJIPREFS_CASE font=%d glyph=%d colored=%d PASS\n",f,g,color);fflush(stdout);cases++;
}
CHECK(put("\x1b[0;48;2;25;60;80m\x1b]50;DejaVuSansMono.font,24\x07\x1b[2J\x1b[HConsoleNG: emoji preferences\r\n\r\n"));
for(g=0;g<5;g++){CHECK(put(glyphs[g]));CHECK(put("  "));}
CHECK(put("\r\n\r\n20 toggle / redraw checks PASS\r\nTopaz 8 and DejaVu 16 / 24 / 32\r\n\r\nGoogle Noto Color Emoji"));printf("EMOJIPREFS_PASS cases=%d\n",cases);fflush(stdout);Delay(600);rc=0;
out:if(controlOpened){req(control,CD_SETDEFAULTEMOJIPREFS,&saved,sizeof(saved));CloseDevice((struct IORequest*)control);}if(control)DeleteIORequest((struct IORequest*)control);if(opened)CloseDevice((struct IORequest*)io);if(w)CloseWindow(w);if(io)DeleteIORequest((struct IORequest*)io);if(p)DeleteMsgPort(p);if(CyberGfxBase)CloseLibrary(CyberGfxBase);printf("EMOJIPREFS_SCRIPT rc=%d\n",rc);return rc;}
