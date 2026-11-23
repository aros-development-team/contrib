/* Native resize, alternate-screen restore and logical clipboard acceptance. */
#include <aros/asmcall.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/console.h>
#include <utility/hooks.h>
#include <devices/conunit.h>
#include <dos/dosextens.h>
#include <stdio.h>
#include <string.h>
struct Device *ConsoleDevice;
static volatile unsigned copies;
AROS_UFH3S(ULONG,sink,AROS_UFHA(struct Hook *,h,A0),AROS_UFHA(APTR,o,A2),AROS_UFHA(APTR,m,A1)){
 AROS_USERFUNC_INIT
 copies++;return 0;
 AROS_USERFUNC_EXIT
}
static BPTR stream;
static int put(struct IOStdReq *io,const char *s){(void)io;return Write(stream,s,strlen(s))==(LONG)strlen(s);}

#define CHECK(c) do{if(!(c)){printf("REFLOW_FAIL line=%d\n",__LINE__);goto out;}}while(0)
int main(void){
 int rc=20,hooked=0;struct Window *w=NULL;struct Hook hook={0};
 struct IOStdReq *io=NULL;struct InfoData storage,*info=&storage;
 CHECK(info);stream=Open((CONST_STRPTR)"RGCON:",MODE_OLDFILE);CHECK(stream);
 struct FileHandle *fh=BADDR(stream);
 CHECK(DoPkt(fh->fh_Type,ACTION_DISK_INFO,(SIPTR)MKBADDR(info),0,0,0,0));
 io=(struct IOStdReq*)(IPTR)info->id_InUse;CHECK(io&&io->io_Unit);
 struct ConUnit *unit=(void*)io->io_Unit;w=unit->cu_Window;CHECK(w);
 SetWindowTitles(w,(CONST_STRPTR)"ConsoleNG DOS handler reflow",(CONST_STRPTR)-1);
 MoveWindow(w,30-w->LeftEdge,30-w->TopEdge);SizeWindow(w,450-w->Width,300-w->Height);Delay(100);
 ConsoleDevice=(void*)io->io_Device;hook.h_Entry=(HOOKFUNC)sink;AddConSnipHook(&hook);hooked=1;
 CHECK(SetMode(stream,TRUE));
 CHECK(put(io,"\x1b%G\x1b]50;topaz,8\x07\x1b[2J\x1b[H"));
 struct ConUnit *u=(void*)io->io_Unit;char text[106],expected[107];
 for(int i=0;i<105;i++) { text[i]='A'+i%26; }
 text[105]=0;strcpy(expected,text);strcat(expected,"\r");CHECK(put(io,text));
 puts("REFLOW_INITIAL_READY");fflush(stdout);Delay(50*8);
 for(int phase=0;phase<3;phase++){
  unsigned oldcopies=copies;
  if(phase==0)SizeWindow(w,-200,0);
  if(phase==1)SizeWindow(w,200,0);
  if(phase==2){
   CHECK(put(io,"\x1b[?1049h\x1b[2J\x1b[H"));char alt[81];memset(alt,'#',80);alt[80]=0;CHECK(put(io,alt));
   SizeWindow(w,-160,0);Delay(50*2);CHECK(u->cu_YCP==1&&u->cu_XCP==28);
   CHECK(put(io,"\x1b[?1049l"));
  }
  Delay(50*2);
  int width=u->cu_XMax+1,col=105%width,row=105/width;
  if(!col){col=width-1;row--;}
  printf("REFLOW_CURSOR phase=%d width=%d actual=%d,%d expected=%d,%d\n",phase,width,u->cu_XCP,u->cu_YCP,col,row);
  CHECK(u->cu_XCP==col&&u->cu_YCP==row);
  int x=w->LeftEdge+u->cu_XROrigin,y=w->TopEdge+u->cu_YROrigin;
  printf("REFLOW_SELECT phase=%d x0=%d y0=%d x1=%d y1=%d\n",phase,x+1,y+u->cu_YRSize/2,x+(105%width?105%width:width)*u->cu_XRSize+1,y+row*u->cu_YRSize+u->cu_YRSize/2);fflush(stdout);
  for(int wait=0;wait<120&&copies==oldcopies;wait++)Delay(50);
  CHECK(copies!=oldcopies);char *s=GetConSnip();CHECK(s);int good=!strcmp(s,expected);
  printf("REFLOW_COPY phase=%d bytes=%lu match=%d\n",phase,(unsigned long)strlen(s),good);FreeVec(s);CHECK(good);
  printf("REFLOW_PHASE_PASS phase=%d\n",phase);fflush(stdout);Delay(50*3);
 }
 /* Address beyond retained output, then shrink: a blank cursor row must
    remain addressable and not suppress reflow of earlier text. */
 CHECK(put(io,"\x1b[21;3H"));SizeWindow(w,-40,0);Delay(50*2);
 printf("REFLOW_BLANK_CURSOR actual=%d,%d expected=2,20\n",u->cu_XCP,u->cu_YCP);
 CHECK(u->cu_XCP==2&&u->cu_YCP==20);
 CHECK(put(io,"\x1b[H"));
 CHECK(SetMode(stream,FALSE));
 puts("HANDLER_COOKED_READY");fflush(stdout);
 CHECK(WaitForChar(stream,30000000));char cooked[128]={0};LONG got=Read(stream,cooked,sizeof(cooked)-1);
 printf("HANDLER_COOKED bytes=%ld match=%d\n",(long)got,got==4&&!memcmp(cooked,"abc\n",4));
 CHECK(got==4&&!memcmp(cooked,"abc\n",4));
 CHECK(SetMode(stream,TRUE));puts("HANDLER_RAW_READY");fflush(stdout);
 unsigned char raw[4];for(int i=0;i<4;i++){CHECK(WaitForChar(stream,10000000));CHECK(Read(stream,raw+i,1)==1);}
 printf("HANDLER_RAW bytes=%02x,%02x,%02x,%02x\n",raw[0],raw[1],raw[2],raw[3]);
 CHECK(!memcmp(raw,"\x1b[Ax",4));CHECK(SetMode(stream,FALSE));
 puts("REFLOW_PASS");CHECK(put(io,"\r\nPASS: DOS Write/Read, cooked editing, raw keys, resize.\r\n"));fflush(stdout);Delay(50*12);rc=0;
out:
 if(hooked)RemConSnipHook(&hook);
 if(stream)Close(stream);

 printf("REFLOW_SCRIPT rc=%d\n",rc);return rc;
}
