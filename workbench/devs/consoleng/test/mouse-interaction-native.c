/* DOS handler acceptance for native mouse reporting; owns only RGCON mount. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <devices/conunit.h>
#include <dos/dosextens.h>
#include <stdio.h>
#include <aros/asmcall.h>
#include <proto/console.h>
#include <utility/hooks.h>
#include <string.h>
static BPTR stream;
static int put(const char *s){return Write(stream,s,strlen(s))==(LONG)strlen(s);}
static int get(unsigned char *out,int n){for(int i=0;i<n;i++){if(!WaitForChar(stream,10000000)||Read(stream,out+i,1)!=1)return 0;}return 1;}
struct Device *ConsoleDevice;
static volatile unsigned copies;
AROS_UFH3S(ULONG,sink,AROS_UFHA(struct Hook *,h,A0),AROS_UFHA(APTR,o,A2),AROS_UFHA(APTR,m,A1)){
 AROS_USERFUNC_INIT
 copies++;return 0;
 AROS_USERFUNC_EXIT
}
static int report(int *button,int *x,int *y,char *end){
 unsigned char b[40]={0};int n=0;
 do{if(n==39||!get(b+n,1))return 0;n++;}while(b[n-1]!='M'&&b[n-1]!='m');
 return sscanf((char*)b,"\x1b[<%d;%d;%d%c",button,x,y,end)==4;
}
static void drain(void){unsigned char b;while(WaitForChar(stream,10000))if(Read(stream,&b,1)!=1)break;}
#define CHECK(c) do{if(!(c)){printf("MOUSE_FAIL line=%d\n",__LINE__);goto out;}}while(0)
int main(void){
 int rc=20,hooked=0;struct InfoData info;struct Hook hook={0};
 stream=Open((CONST_STRPTR)"RGCON:",MODE_OLDFILE);CHECK(stream);
 struct FileHandle *fh=BADDR(stream);CHECK(DoPkt(fh->fh_Type,ACTION_DISK_INFO,(SIPTR)MKBADDR(&info),0,0,0,0));
 struct IOStdReq *io=(void*)(IPTR)info.id_InUse;CHECK(io&&io->io_Unit);
 struct ConUnit *u=(void*)io->io_Unit;struct Window *w=u->cu_Window;CHECK(w);
 CHECK(SetMode(stream,TRUE));CHECK(put("\x1b%G\x1b]50;topaz,8\x07\x1b[2J\x1b[HNative DOS mouse report test\r\n"));
 SetWindowTitles(w,(CONST_STRPTR)"ConsoleNG native mouse protocols",(CONST_STRPTR)-1);
 ConsoleDevice=(void*)io->io_Device;hook.h_Entry=(HOOKFUNC)sink;AddConSnipHook(&hook);hooked=1;
 int ox=w->LeftEdge+u->cu_XROrigin,oy=w->TopEdge+u->cu_YROrigin,sx=u->cu_XRSize,sy=u->cu_YRSize;
 int button,x,y,moved=0;char end;
 CHECK(put("\x1b[?1002;1006h"));
 printf("MOUSE_DRAG x0=%d y0=%d x1=%d y1=%d\n",ox+4*sx+sx/2,oy+3*sy+sy/2,ox+8*sx+sx/2,oy+5*sy+sy/2);fflush(stdout);
 CHECK(report(&button,&x,&y,&end));CHECK(button==0&&x==5&&y==4&&end=='M');
 for(int n=0;n<100;n++){
  CHECK(report(&button,&x,&y,&end));
  if(end=='m'){CHECK(button==0&&x==9&&y==6);break;}
  CHECK(button==32);moved++;
 }
 CHECK(end=='m'&&moved);printf("MOUSE_DRAG_PASS motion=%d\n",moved);
 CHECK(put("\x1b[?1003h"));
 printf("MOUSE_MOVE x=%d y=%d\n",ox+10*sx+sx/2,oy+5*sy+sy/2);fflush(stdout);
 for(int n=0;n<100;n++){CHECK(report(&button,&x,&y,&end));CHECK(button==35&&end=='M');if(x==11&&y==6)break;}
 CHECK(x==11&&y==6);CHECK(put("\x1b[?1000h"));Delay(25);drain();puts("MOUSE_MOTION_PASS");
 puts("MOUSE_WHEEL_READY");fflush(stdout);CHECK(report(&button,&x,&y,&end));CHECK(button==64&&x==11&&y==6&&end=='M');puts("MOUSE_WHEEL_PASS");
 printf("MOUSE_CTRL x=%d y=%d\n",ox+10*sx+sx/2,oy+5*sy+sy/2);fflush(stdout);
 CHECK(report(&button,&x,&y,&end));CHECK(button==16&&x==11&&y==6&&end=='M');
 CHECK(report(&button,&x,&y,&end));CHECK(button==16&&x==11&&y==6&&end=='m');puts("MOUSE_CTRL_PASS");fflush(stdout);Delay(300);
 CHECK(put("\x1b[2J\x1b[HSHIFT SELECT"));
 printf("MOUSE_SHIFT x0=%d y0=%d x1=%d y1=%d\n",ox+1,oy+sy/2,ox+12*sx+1,oy+sy/2);fflush(stdout);
 LONG ready=WaitForChar(stream,4000000);
 if(ready){
  printf("MOUSE_SHIFT_UNEXPECTED");
  for(int i=0;i<64&&WaitForChar(stream,0);i++){unsigned char byte;if(Read(stream,&byte,1)!=1)break;printf(" %02x",byte);}
  puts("");fflush(stdout);Delay(250);
 }
 CHECK(!ready);puts("MOUSE_SHIFT_COPY_READY");fflush(stdout);
 for(int n=0;n<15&&!copies;n++)Delay(50);
 CHECK(copies);char *clip=GetConSnip();CHECK(clip);int matches=!strcmp(clip,"SHIFT SELECT\r");
 printf("MOUSE_SHIFT_COPY bytes=%lu match=%d\n",(unsigned long)strlen(clip),matches);FreeVec(clip);CHECK(matches);
 CHECK(put("\x1b[?1000;1002;1003;1006l\r\nPASS: drag, motion, wheel, Ctrl and Shift selection.\r\n"));
 puts("MOUSE_PASS");fflush(stdout);Delay(600);rc=0;
out:
 if(hooked)RemConSnipHook(&hook);
 if(stream){put("\x1b[?1000;1002;1003;1006l");SetMode(stream,FALSE);Close(stream);}
 printf("MOUSE_SCRIPT rc=%d\n",rc);return rc;
}
