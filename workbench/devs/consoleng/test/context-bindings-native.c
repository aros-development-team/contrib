/* Native shortcut policy API and input routing through DOS streams. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <devices/rawkeycodes.h>
#include <dos/dosextens.h>
#include <exec/errors.h>
#include <stdio.h>
#include <string.h>
#include "../console_gcc.h"
struct Device *ConsoleDevice;
static BPTR stream;
static struct ConsoleBase *base;
static Object *unit;
static struct IOStdReq *request;
static int put(const char*s){return Write(stream,s,strlen(s))==(LONG)strlen(s);}
static int exchange(UWORD cmd,struct Unit*u,void*p,ULONG n,int error){
 request->io_Unit=u;request->io_Command=cmd;request->io_Data=p;request->io_Length=n;
 LONG result=DoIO((struct IORequest*)request);
 return result==error&&request->io_Actual==(error?0:n);
}
static int key(UWORD code,UWORD qual){
 struct cdihMessage*m=AllocMem(sizeof(*m),MEMF_PUBLIC|MEMF_CLEAR);if(!m)return 0;
 m->msg.mn_Length=sizeof(*m);m->unit=unit;m->unitGeneration=ICU(unit)->inputGeneration;
 m->ie.ie_Class=IECLASS_RAWKEY;m->ie.ie_Code=code;m->ie.ie_Qualifier=qual;
 ObtainSemaphore(&base->consoleTaskLock);PutMsg(base->consIHData.inputPort,(struct Message*)m);ReleaseSemaphore(&base->consoleTaskLock);
 Delay(2);return 1;
}
static int bytes(const char*s){char got[128];unsigned n=strlen(s),used=0;
 while(used<n&&WaitForChar(stream,2000000)&&Read(stream,got+used,1)==1)used++;
 if(used==n&&!memcmp(got,s,n)&&!WaitForChar(stream,20000))return 1;
 printf("BINDINGS_BYTES want=%s used=%u\n",s,used);return 0;
}
#define CHECK(c) do{count++;if(!(c)){printf("CONTEXT_BINDINGS_FAIL line=%d count=%d\n",__LINE__,count);fflush(stdout);Delay(500);goto out;}}while(0)

int main(void){
 int rc=20,count=0;struct InfoData info;struct MsgPort*port=NULL;struct Window*w=NULL;
 struct ConsolengBindingPrefs p={sizeof(p),18,1,{{CONSOLENG_BIND_RAW+RAWKEY_X,CONSOLENG_BIND_ALTSCREEN,CONSOLENG_BIND_CLEARSELECTION}}},got;
 stream=Open((CONST_STRPTR)"RGCON:",MODE_OLDFILE);CHECK(stream);
 struct FileHandle*fh=BADDR(stream);CHECK(DoPkt(fh->fh_Type,ACTION_DISK_INFO,(SIPTR)MKBADDR(&info),0,0,0,0));
 struct IOStdReq*io=(void*)(IPTR)info.id_InUse;CHECK(io&&io->io_Unit);unit=(Object*)io->io_Unit;base=(void*)io->io_Device;ConsoleDevice=io->io_Device;
 port=CreateMsgPort();CHECK(port);request=(void*)CreateIORequest(port,sizeof(*request));CHECK(request);request->io_Device=io->io_Device;
 CHECK(exchange(CD_SETBINDINGPREFS,io->io_Unit,&p,sizeof(p),0));
 got=p;got.bp_Version=17;CHECK(exchange(CD_SETBINDINGPREFS,io->io_Unit,&got,sizeof(got),CONSOLENG_ERR_BADPREFS));
 CHECK(exchange(CD_GETBINDINGPREFS,io->io_Unit,&got,sizeof(got),0));CHECK(!memcmp(&got,&p,sizeof(p)));
 w=CU(unit)->cu_Window;CHECK(w);SetWindowTitles(w,(CONST_STRPTR)"ConsoleNG terminal-state shortcuts",(CONST_STRPTR)-1);
 MoveWindow(w,30-w->LeftEdge,30-w->TopEdge);SizeWindow(w,730-w->Width,450-w->Height);ActivateWindow(w);Delay(20);CHECK(SetMode(stream,TRUE));
 CHECK(put("\x1b%G\x1b[?25l\x1b]50;DejaVuSansMono,16\7"));
 for(unsigned state=0;state<8;state++){
  CHECK(put(state&1?"\x1b[?1049h":"\x1b[?1049l"));
  CHECK(put(state&2?"\x1b[?1h":"\x1b[?1l"));CHECK(put(state&4?"\x1b=":"\x1b>"));Delay(3);
  CHECK(!!ICU(unit)->keyboardAlternate==!!(state&1));
  CHECK(!!(ICU(unit)->conFlags&CF_APPLICATION_CURSOR)==!!(state&2));CHECK(!!(ICU(unit)->conFlags&CF_APPLICATION_KEYPAD)==!!(state&4));
  for(unsigned rule=0;rule<27;rule++){
   unsigned n=rule;int match=1;p.bp_Entries[0].kb_Modifiers=0;
   for(unsigned d=0;d<3;d++){unsigned condition=n%3;n/=3;p.bp_Entries[0].kb_Modifiers|=condition<<(7+2*d);if(condition&&((condition==1)!=!!(state&(1<<d))))match=0;}
   CHECK(exchange(CD_SETBINDINGPREFS,io->io_Unit,&p,sizeof(p),0));
   CHECK(key(RAWKEY_X,0));CHECK(key(RAWKEY_X|IECODE_UP_PREFIX,0));CHECK(bytes(match?"":"x"));
  }
  printf("CONTEXT_BINDINGS_STATE state=%u\n",state);fflush(stdout);
 }
 /* A mode change cannot leak repeat/release from an already consumed key. */
 p.bp_Entries[0].kb_Modifiers=CONSOLENG_BIND_APPCURSOR;
 CHECK(exchange(CD_SETBINDINGPREFS,io->io_Unit,&p,sizeof(p),0));CHECK(put("\x1b[?1h"));CHECK(key(RAWKEY_X,0));CHECK(bytes(""));
 CHECK(put("\x1b[?1l"));CHECK(key(RAWKEY_X,IEQUALIFIER_REPEAT));CHECK(key(RAWKEY_X|IECODE_UP_PREFIX,0));CHECK(bytes(""));
 CHECK(key(RAWKEY_X,0));CHECK(key(RAWKEY_X|IECODE_UP_PREFIX,0));CHECK(bytes("x"));
 CHECK(put("\x1b[?1049l\x1b>\x1b[2J\x1b[HPASS: terminal-state shortcut conditions\r\n\r\n216 condition/state combinations agree.\r\nPrimary/alternate screen, cursor keys and keypad.\r\nConsumed held keys stay consumed across mode changes.\r\n"));
 printf("CONTEXT_BINDINGS_PASS count=%d\n",count);fflush(stdout);Delay(300);rc=0;
out:
 if(request)DeleteIORequest((struct IORequest*)request);if(port)DeleteMsgPort(port);
 if(stream){SetMode(stream,FALSE);Close(stream);}printf("CONTEXT_BINDINGS_DONE rc=%d\n",rc);fflush(stdout);return rc;
}
