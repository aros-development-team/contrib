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
#define CHECK(c) do{count++;if(!(c)){printf("SEARCHMODE_BINDINGS_FAIL line=%d count=%d\n",__LINE__,count);fflush(stdout);Delay(500);goto out;}}while(0)


#define TAP(c) (key((c),0)&&key((c)|IECODE_UP_PREFIX,0))
int main(void){
 int rc=20,count=0;struct InfoData info;struct MsgPort*port=NULL;struct Window*w=NULL;
 struct ConsolengBindingPrefs p={sizeof(p),19,4,{
 {CONSOLENG_BIND_RAW+RAWKEY_F1,0,CONSOLENG_BIND_SEARCH},
 {CONSOLENG_BIND_RAW+RAWKEY_F2,0,CONSOLENG_BIND_NAVIGATE},
 {CONSOLENG_BIND_RAW+RAWKEY_F3,0,CONSOLENG_BIND_NAVSEARCHFORWARD},
 {CONSOLENG_BIND_RAW+RAWKEY_X,CONSOLENG_BIND_SEARCHMODE,CONSOLENG_BIND_CLEARSELECTION}}},got;
 stream=Open((CONST_STRPTR)"RGCON:",MODE_OLDFILE);CHECK(stream);
 struct FileHandle*fh=BADDR(stream);CHECK(DoPkt(fh->fh_Type,ACTION_DISK_INFO,(SIPTR)MKBADDR(&info),0,0,0,0));
 struct IOStdReq*io=(void*)(IPTR)info.id_InUse;CHECK(io&&io->io_Unit);unit=(Object*)io->io_Unit;base=(void*)io->io_Device;ConsoleDevice=io->io_Device;
 port=CreateMsgPort();CHECK(port);request=(void*)CreateIORequest(port,sizeof(*request));CHECK(request);request->io_Device=io->io_Device;
 CHECK(exchange(CD_SETBINDINGPREFS,io->io_Unit,&p,sizeof(p),0));got=p;got.bp_Version=18;
 CHECK(exchange(CD_SETBINDINGPREFS,io->io_Unit,&got,sizeof(got),CONSOLENG_ERR_BADPREFS));
 CHECK(exchange(CD_GETBINDINGPREFS,io->io_Unit,&got,sizeof(got),0));CHECK(!memcmp(&got,&p,sizeof(p)));
 w=CU(unit)->cu_Window;CHECK(w);SetWindowTitles(w,(CONST_STRPTR)"ConsoleNG search-mode shortcuts",(CONST_STRPTR)-1);
 MoveWindow(w,30-w->LeftEdge,30-w->TopEdge);SizeWindow(w,730-w->Width,450-w->Height);ActivateWindow(w);Delay(20);CHECK(SetMode(stream,TRUE));
 CHECK(put("\x1b%G\x1b[?25l\x1b]50;DejaVuSansMono,16\7"));
 for(unsigned nav=0;nav<2;nav++)for(unsigned rule=0;rule<9;rule++){
  CHECK(put("\x1b[2J\x1b[Halpha x beta\r\n"));Delay(5);
  unsigned a=rule%3,b=rule/3;int match=(!a||((a==1)==!!nav))&&b!=2;
  p.bp_Entries[3].kb_Modifiers=(a<<5)|(b<<13);
  CHECK(exchange(CD_SETBINDINGPREFS,io->io_Unit,&p,sizeof(p),0));
  if(nav){CHECK(TAP(RAWKEY_F2));Delay(10);CHECK(TAP(RAWKEY_F3));}else CHECK(TAP(RAWKEY_F1));
  Delay(10);CHECK(w->Title&&strstr((char*)w->Title,nav?"Navigate /: ":"Find: "));
  char title[256];snprintf(title,sizeof(title),"%s",(char*)w->Title);
  CHECK(TAP(RAWKEY_X));Delay(10);CHECK((!strcmp(title,(char*)w->Title))==match);CHECK(bytes(""));
 }
 /* Version 18 retains its former navigation/query exclusion. */
 for(unsigned version=18;version<=19;version++){
  CHECK(put("\x1b[2J\x1b[Halpha x beta\r\n"));Delay(5);p.bp_Version=version;p.bp_Entries[3].kb_Modifiers=CONSOLENG_BIND_NAVONLY;
  CHECK(exchange(CD_SETBINDINGPREFS,io->io_Unit,&p,sizeof(p),0));CHECK(TAP(RAWKEY_F2));Delay(10);CHECK(TAP(RAWKEY_F3));Delay(10);
  char title[256];snprintf(title,sizeof(title),"%s",(char*)w->Title);CHECK(TAP(RAWKEY_X));Delay(10);
  CHECK((!strcmp(title,(char*)w->Title))==(version==19));CHECK(bytes(""));
 }
 CHECK(put("\x1b[2J\x1b[HPASS: Search and Navigation shortcut conditions\r\n\r\n18 query/context combinations agree.\r\nVersion 18 keeps its original query exclusion.\r\nVersion 19 supports Navigation + Search together.\r\nNo query input leaked to the application.\r\n"));
 printf("SEARCHMODE_BINDINGS_PASS count=%d\n",count);fflush(stdout);Delay(300);rc=0;
out:
 if(request)DeleteIORequest((struct IORequest*)request);if(port)DeleteMsgPort(port);
 if(stream){SetMode(stream,FALSE);Close(stream);}printf("SEARCHMODE_BINDINGS_DONE rc=%d\n",rc);fflush(stdout);return rc;
}
