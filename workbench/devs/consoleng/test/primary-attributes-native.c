/* Exact native CMD_READ responses to fragmented status requests. */
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
static int reply(const char*expected){char got[64]={0};int length=strlen(expected);io->io_Command=CMD_READ;io->io_Data=got;io->io_Length=length;SendIO((struct IORequest*)io);
 for(int n=0;n<100;n++){if(CheckIO((struct IORequest*)io)){WaitIO((struct IORequest*)io);if(io->io_Error||io->io_Actual!=length||memcmp(got,expected,length)){printf("PRIMARY_BYTES actual=%lu expected=%d first=%02x\n",(unsigned long)io->io_Actual,length,(unsigned char)got[0]);return 0;}return 1;}Delay(1);}
 AbortIO((struct IORequest*)io);WaitIO((struct IORequest*)io);puts("PRIMARY_TIMEOUT");return 0;}
static ULONG pixels(struct Window*w,struct ConUnit*u){ULONG h=2166136261UL;for(int y=u->cu_YROrigin;y<=u->cu_YRExtant;y++)for(int x=u->cu_XROrigin;x<=u->cu_XRExtant;x++)h=(h^(ULONG)ReadPixel(w->RPort,x,y))*16777619UL;return h;}
#define CHECK(c) do{if(!(c)){printf("PRIMARY_FAIL line=%d kind=%d hidden=%d pos=%d query=%d\n",__LINE__,kind,hidden,pos,query);fflush(stdout);Delay(500);goto out;}}while(0)
int main(void){int rc=20,opened=0,kind=0,hidden=0,pos=0,query=0,count=0;struct Window*w=NULL;struct MsgPort*p=CreateMsgPort();io=p?(void*)CreateIORequest(p,sizeof(*io)):NULL;CHECK(io);
 const char*requests[]={"\x1bZ","\x1b[c","\x1b[0c","\x9b" "c","\x9b" "000c","\x1b[1c\x1b[>1c\x1b[?0c\x1b[0 c\x1b[c"};const char*positions[]={"\x1b[H","\x1b[3;4H","\x1b[65535;65535H"};
 for(kind=0;kind<2;kind++){
 w=OpenWindowTags(NULL,WA_Left,30,WA_Top,30,WA_Width,720,WA_Height,450,WA_Title,(IPTR)"ConsoleNG primary terminal attributes regression",WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_SizeGadget,TRUE,WA_SizeBRight,TRUE,WA_MinWidth,200,WA_MinHeight,160,WA_MaxWidth,-1,WA_MaxHeight,-1,TAG_DONE);CHECK(w);
 io->io_Data=w;io->io_Length=sizeof(*w);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",kind?CONU_CHARMAP:CONU_STANDARD,(struct IORequest*)io,0));opened=1;struct ConUnit*u=(void*)io->io_Unit;
 CHECK(put("\x1b[?25l\x1b]50;topaz,8\x07\x1b[38;2;230;230;230m\x1b[48;2;25;60;80m\x1b[2J\x1b[HPRIMARY ANCHOR: queries must not change these pixels."));
 for(hidden=0;hidden<2;hidden++){
 CHECK(put(hidden?"\x1b[>9h":"\x1b[>9l"));ULONG before=pixels(w,u);
 for(pos=0;pos<3;pos++)for(query=0;query<6;query++){
 CHECK(put(positions[pos]));int x=u->cu_XCP,y=u->cu_YCP;char expected[64];
 strcpy(expected,"\x1b[?6c");
 CHECK(split(requests[query]));CHECK(reply(expected));CHECK(put("\x1b[5n"));CHECK(reply("\x1b[0n"));CHECK(u->cu_XCP==x&&u->cu_YCP==y);CHECK(pixels(w,u)==before);count++;
 }
 printf("PRIMARY_GROUP kind=%d hidden=%d PASS\n",kind,hidden);fflush(stdout);
 }
 CHECK(put("\x1b[>9l\x1b[2J\x1b[HPASS: 72 exact terminal identification replies.\r\nANSI and native CSI framing both pass fragmented requests.\r\nHidden units answer without painting or moving the cursor.\r\nPrivate and unsupported queries do not generate extra replies."));
 if(kind){printf("PRIMARY_PASS cases=%d\n",count);fflush(stdout);Delay(600);}CloseDevice((struct IORequest*)io);opened=0;CloseWindow(w);w=NULL;
 }rc=0;
out:if(opened)CloseDevice((struct IORequest*)io);if(w)CloseWindow(w);if(io)DeleteIORequest((struct IORequest*)io);if(p)DeleteMsgPort(p);printf("PRIMARY_SCRIPT rc=%d\n",rc);return rc;}
