/* Exact native CMD_READ responses to fragmented status requests. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <devices/conunit.h>
#include <dos/dosextens.h>
#include <stdio.h>
#include <string.h>
static BPTR stream;
static int put(const char*s){LONG n=strlen(s);return Write(stream,s,n)==n;}
static int split(const char*s){char b[2]={0};while(*s){b[0]=*s++;if(!put(b))return 0;}return 1;}
static int reply(const char*expected){char got[64]={0};int length=strlen(expected);
 for(int n=0;n<length;n++){
  if(!WaitForChar(stream,5000000)||Read(stream,got+n,1)!=1){printf("STATUS_TIMEOUT byte=%d\n",n);return 0;}
 }
 if(memcmp(got,expected,length)){printf("STATUS_BYTES first=%02x expected=%02x\n",(unsigned char)got[0],(unsigned char)expected[0]);return 0;}
 return !WaitForChar(stream,10000);
}
static ULONG pixels(struct Window*w,struct ConUnit*u){ULONG h=2166136261UL;for(int y=u->cu_YROrigin;y<=u->cu_YRExtant;y++)for(int x=u->cu_XROrigin;x<=u->cu_XRExtant;x++)h=(h^(ULONG)ReadPixel(w->RPort,x,y))*16777619UL;return h;}
#define CHECK(c) do{if(!(c)){printf("STATUS_FAIL line=%d kind=%d hidden=%d pos=%d query=%d\n",__LINE__,kind,hidden,pos,query);fflush(stdout);Delay(500);goto out;}}while(0)
int main(void){int rc=20,kind=0,hidden=0,pos=0,query=0,count=0;struct Window*w=NULL;struct InfoData info;
 const char*requests[]={"\x1b[5n","\x1b[6n","\x9b" "0005n","\x9b" "0006n","\x9b" "0 q","\x1b[7n\x1b[5;6n\x1b[?6n\x1b[0 q\x1b[5n"};const char*positions[]={"\x1b[H","\x1b[3;4H","\x1b[65535;65535H"};
 for(kind=0;kind<2;kind++){
 stream=Open((CONST_STRPTR)"RGCON:",MODE_OLDFILE);CHECK(stream);struct FileHandle*fh=BADDR(stream);
 CHECK(DoPkt(fh->fh_Type,ACTION_DISK_INFO,(SIPTR)MKBADDR(&info),0,0,0,0));
 struct IOStdReq*io=(void*)(IPTR)info.id_InUse;CHECK(io&&io->io_Unit);struct ConUnit*u=(void*)io->io_Unit;w=u->cu_Window;CHECK(w);
 SetWindowTitles(w,(CONST_STRPTR)"ConsoleNG DOS raw-stream status reports",(CONST_STRPTR)-1);
 MoveWindow(w,30-w->LeftEdge,30-w->TopEdge);SizeWindow(w,720-w->Width,450-w->Height);Delay(50);
 CHECK(SetMode(stream,FALSE));CHECK(SetMode(stream,TRUE));
 CHECK(put("\x1b[?25l\x1b]50;topaz,8\x07\x1b[38;2;230;230;230m\x1b[48;2;25;60;80m\x1b[2J\x1b[HSTATUS ANCHOR: queries must not change these pixels."));
 for(hidden=0;hidden<2;hidden++){
 CHECK(put(hidden?"\x1b[>9h":"\x1b[>9l"));ULONG before=pixels(w,u);
 for(pos=0;pos<3;pos++)for(query=0;query<6;query++){
 CHECK(put(positions[pos]));int x=u->cu_XCP,y=u->cu_YCP;char expected[64];
 if(query==1||query==3)snprintf(expected,sizeof(expected),query==1?"\x1b[%d;%dR":"\x9b" "%d;%dR",y+1,x+1);
 else if(query==4)snprintf(expected,sizeof(expected),"\x9b" "1;1;%d;%d r",u->cu_YMax+1,u->cu_XMax+1);
 else strcpy(expected,query==2?"\x9b" "0n":"\x1b[0n");
 CHECK(split(requests[query]));CHECK(reply(expected));CHECK(u->cu_XCP==x&&u->cu_YCP==y);CHECK(pixels(w,u)==before);count++;
 }
 printf("STATUS_GROUP kind=%d hidden=%d PASS\n",kind,hidden);fflush(stdout);
 }
 CHECK(put("\x1b[>9l\x1b[2J\x1b[HPASS: 72 exact replies through DOS / native console handler.\r\nRaw reads, WaitForChar and fragmented DOS writes pass.\r\nHidden units answer without painting or moving the cursor.\r\nTwo fresh streams; cooked-to-raw transitions pass."));
 if(kind){printf("STATUS_PASS cases=%d\n",count);fflush(stdout);Delay(600);}CHECK(SetMode(stream,FALSE));Close(stream);stream=0;w=NULL;
 }rc=0;
out:if(stream){SetMode(stream,FALSE);Close(stream);}printf("STATUS_SCRIPT rc=%d\n",rc);return rc;}
