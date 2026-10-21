/* Native resize, alternate-screen restore and logical clipboard acceptance. */
#include <aros/asmcall.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/console.h>
#include <utility/hooks.h>
#include <devices/conunit.h>
#include <stdio.h>
#include <string.h>
#include "consoleng.h"
struct Device *ConsoleDevice;
static volatile unsigned copies;
AROS_UFH3S(ULONG,sink,AROS_UFHA(struct Hook *,h,A0),AROS_UFHA(APTR,o,A2),AROS_UFHA(APTR,m,A1)){
 AROS_USERFUNC_INIT
 copies++;return 0;
 AROS_USERFUNC_EXIT
}
static int put(struct IOStdReq *io,const char *s){io->io_Command=CMD_WRITE;io->io_Data=(void*)s;io->io_Length=strlen(s);return !DoIO((struct IORequest*)io);}
#define CHECK(c) do{if(!(c)){printf("REFLOW_FAIL line=%d\n",__LINE__);goto out;}}while(0)
int main(void){
 int rc=20,opened=0,hooked=0;struct Window *w=NULL;struct Hook hook={0};
 struct MsgPort *p=CreateMsgPort();struct IOStdReq *io=p?(void*)CreateIORequest(p,sizeof(*io)):NULL;
 CHECK(io);
 w=OpenWindowTags(NULL,WA_Left,30,WA_Top,30,WA_Width,450,WA_Height,300,WA_Title,(IPTR)"ConsoleNG reflow and history stress",WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_SizeGadget,TRUE,WA_MinWidth,180,WA_MinHeight,160,WA_Activate,TRUE,TAG_DONE);CHECK(w);
 io->io_Data=w;io->io_Length=sizeof(*w);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",CONU_CHARMAP,(struct IORequest*)io,0));opened=1;
 ConsoleDevice=(void*)io->io_Device;hook.h_Entry=(HOOKFUNC)sink;AddConSnipHook(&hook);hooked=1;
 CHECK(put(io,"\x1b%G\x1b]50;topaz,8\x07\x1b[2J\x1b[H"));
 struct ConUnit *u=(void*)io->io_Unit;char text[106],expected[107];
 for(int i=0;i<105;i++) { text[i]='A'+i%26; }
 text[105]=0;strcpy(expected,text);strcat(expected,"\r");
 puts("REFLOW_INITIAL_READY");fflush(stdout);Delay(50*8);
 for(int phase=0;phase<3;phase++){
  unsigned oldcopies=copies;
  struct ConsolengHistoryPrefs prefs={sizeof(prefs),CONSOLENG_HISTORY_PREFS_VERSION,phase==0?0:phase==1?7:64};
  io->io_Command=CD_SETHISTORYPREFS;io->io_Data=&prefs;io->io_Length=sizeof(prefs);CHECK(!DoIO((struct IORequest*)io));
  io->io_Command=CD_GETHISTORYPREFS;CHECK(!DoIO((struct IORequest*)io));
  CHECK(prefs.hp_Lines==(ULONG)(phase==0?0:phase==1?7:64));
  for(int n=0;n<200;n++){
   char line[180];snprintf(line,sizeof(line),"\r\nHISTORY-%03d %s",n,text);CHECK(put(io,line));
  }
  CHECK(put(io,"\r\n"));CHECK(put(io,text));
  if(phase==0)SizeWindow(w,-200,0);
  if(phase==1)SizeWindow(w,200,0);
  if(phase==2){
   CHECK(put(io,"\x1b[?1049h\x1b[2J\x1b[H"));char alt[81];memset(alt,'#',80);alt[80]=0;CHECK(put(io,alt));
   SizeWindow(w,-160,0);Delay(50*2);CHECK(u->cu_YCP==1&&u->cu_XCP==28);
   CHECK(put(io,"\x1b[?1049l"));
  }
  Delay(50*2);
  int width=u->cu_XMax+1,col=105%width,span=105/width,row=u->cu_YCP;
  if(!col){col=width-1;span--;}
  printf("REFLOW_CURSOR phase=%d width=%d actual=%d,%d expected=%d,%d\n",phase,width,u->cu_XCP,u->cu_YCP,col,row);
  CHECK(u->cu_XCP==col&&row>=span&&row<=u->cu_YMax);
  printf("REFLOW_HISTORY phase=%d budget=%lu span=%d\n",phase,(unsigned long)prefs.hp_Lines,span);
  int x=w->LeftEdge+u->cu_XROrigin,y=w->TopEdge+u->cu_YROrigin;
  printf("REFLOW_SELECT phase=%d x0=%d y0=%d x1=%d y1=%d\n",phase,x+1,y+(row-span)*u->cu_YRSize+u->cu_YRSize/2,x+(105%width?105%width:width)*u->cu_XRSize+1,y+row*u->cu_YRSize+u->cu_YRSize/2);fflush(stdout);
  for(int wait=0;wait<120&&copies==oldcopies;wait++)Delay(50);
  CHECK(copies!=oldcopies);char *s=GetConSnip();CHECK(s);int good=!strcmp(s,expected);
  printf("REFLOW_COPY phase=%d bytes=%lu match=%d\n",phase,(unsigned long)strlen(s),good);FreeVec(s);CHECK(good);
  printf("REFLOW_PHASE_PASS phase=%d\n",phase);fflush(stdout);Delay(50*3);
 }
 puts("REFLOW_PASS");CHECK(put(io,"\r\n\r\nPASS: 600 history paragraphs; reflow and copy intact.\r\n"));fflush(stdout);Delay(50*12);rc=0;
out:
 if(hooked)RemConSnipHook(&hook);
 if(opened)CloseDevice((struct IORequest*)io);
 if(w)CloseWindow(w);
 if(io)DeleteIORequest((struct IORequest*)io);
 if(p)DeleteMsgPort(p);
 printf("REFLOW_SCRIPT rc=%d\n",rc);return rc;
}
