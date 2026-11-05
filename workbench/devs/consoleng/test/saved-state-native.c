/* Native saved cursor/rendition, hidden state, repeated save and redraw. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <devices/conunit.h>
#include <stdio.h>
#include <string.h>
static struct IOStdReq*io;
static int put(const char*s){io->io_Command=CMD_WRITE;io->io_Data=(APTR)s;io->io_Length=strlen(s);return !DoIO((struct IORequest*)io)&&io->io_Actual==strlen(s);}
static int split(const char*s){char b[2]={0};while(*s){b[0]=*s++;if(!put(b))return 0;}return 1;}
static ULONG pixels(struct Window*w,struct ConUnit*u){ULONG h=2166136261UL;for(int y=u->cu_YROrigin;y<=u->cu_YRExtant;y++)for(int x=u->cu_XROrigin;x<=u->cu_XRExtant;x++)h=(h^(ULONG)ReadPixel(w->RPort,x,y))*16777619UL;return h;}
static int equal(struct Window*w,struct ConUnit*u){int ink=0;LONG bg=ReadPixel(w->RPort,u->cu_XROrigin+7*u->cu_XRSize-1,u->cu_YROrigin+u->cu_YRSize);
 for(int y=0;y<u->cu_YRSize;y++)for(int x=0;x<4*u->cu_XRSize;x++){LONG a=ReadPixel(w->RPort,u->cu_XROrigin+3*u->cu_XRSize+x,u->cu_YROrigin+u->cu_YRSize+y),b=ReadPixel(w->RPort,u->cu_XROrigin+3*u->cu_XRSize+x,u->cu_YROrigin+7*u->cu_YRSize+y);if(a!=b)return 0;if(a!=bg)ink=1;}return ink;}
#define CHECK(c) do{if(!(c)){printf("SAVED_FAIL line=%d kind=%d savehidden=%d restorehidden=%d repeat=%d\n",__LINE__,kind,savehidden,restorehidden,repeat);fflush(stdout);Delay(500);goto out;}}while(0)
int main(void){int rc=20,opened=0,kind=0,savehidden=0,restorehidden=0,repeat=0,count=0;struct Window*w=NULL;struct MsgPort*p=CreateMsgPort();io=p?(void*)CreateIORequest(p,sizeof(*io)):NULL;CHECK(io);
 const char*theme="\x1b[0;1m\x1b[38;2;230;220;180m\x1b[48;2;25;60;80m";
 for(kind=0;kind<2;kind++){
 w=OpenWindowTags(NULL,WA_Left,30,WA_Top,30,WA_Width,720,WA_Height,450,WA_Title,(IPTR)"ConsoleNG saved cursor and logical color regression",WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_SizeGadget,TRUE,WA_SizeBRight,TRUE,WA_MinWidth,200,WA_MinHeight,160,WA_MaxWidth,-1,WA_MaxHeight,-1,TAG_DONE);CHECK(w);
 io->io_Data=w;io->io_Length=sizeof(*w);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",kind?CONU_CHARMAP:CONU_STANDARD,(struct IORequest*)io,0));opened=1;struct ConUnit*u=(void*)io->io_Unit;
 CHECK(put("\x1b[?25l\x1b]50;topaz,8\x07"));
 for(savehidden=0;savehidden<2;savehidden++)for(restorehidden=0;restorehidden<2;restorehidden++)for(repeat=0;repeat<2;repeat++){
 CHECK(put("\x1b[>9l"));CHECK(put(theme));CHECK(put("\x1b[2J\x1b[2;4H"));int fg=u->cu_FgPen,bg=u->cu_BgPen,tx=u->cu_TxFlags;
 CHECK(put(savehidden?"\x1b[>9h":"\x1b[>9l"));ULONG before=pixels(w,u);CHECK(split("\x1b" "7"));CHECK(pixels(w,u)==before);
 CHECK(put("\x1b[>9l\x1b[0m\x1b[38;2;150;30;60m\x1b[48;2;90;100;30m\x1b[5;9H"));
 CHECK(put(restorehidden?"\x1b[>9h":"\x1b[>9l"));before=pixels(w,u);CHECK(split("\x1b" "8"));CHECK(pixels(w,u)==before);
 CHECK(u->cu_XCP==3&&u->cu_XCCP==3&&u->cu_YCP==1&&u->cu_YCCP==1);CHECK(u->cu_FgPen==fg&&u->cu_BgPen==bg&&u->cu_TxFlags==tx);
 if(repeat){CHECK(split("\x1b" "7"));CHECK(put("\x1b[38;2;150;30;60m\x1b[48;2;90;100;30m\x1b[6;7H"));CHECK(split("\x1b" "8"));CHECK(u->cu_FgPen==fg&&u->cu_BgPen==bg);}
 CHECK(put("\x1b[>9lABCD"));CHECK(put(theme));CHECK(put("\x1b[8;4HABCD"));CHECK(equal(w,u));
 if(kind){int width=w->Width;SizeWindow(w,16,0);Delay(30);CHECK(w->Width==width+16);CHECK(equal(w,u));SizeWindow(w,-16,0);Delay(30);CHECK(w->Width==width);CHECK(equal(w,u));}
 count++;printf("SAVED_CASE kind=%d savehidden=%d restorehidden=%d repeat=%d PASS\n",kind,savehidden,restorehidden,repeat);fflush(stdout);
 }
 CHECK(put(theme));CHECK(put("\x1b[2J\x1b[HPASS: saved cursor and logical colors restore together.\r\nVisible and hidden saves / restores agree.\r\nRepeated saves retain colors; resize redraw matches reference.\r\n16 native combinations passed."));
 if(kind){printf("SAVED_PASS cases=%d\n",count);fflush(stdout);Delay(600);}CloseDevice((struct IORequest*)io);opened=0;CloseWindow(w);w=NULL;
 }rc=0;
out:if(opened)CloseDevice((struct IORequest*)io);if(w)CloseWindow(w);if(io)DeleteIORequest((struct IORequest*)io);if(p)DeleteMsgPort(p);printf("SAVED_SCRIPT rc=%d\n",rc);return rc;}
