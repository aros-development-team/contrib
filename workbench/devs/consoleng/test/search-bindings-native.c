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
#define CHECK(c) do{count++;if(!(c)){printf("SEARCH_BINDINGS_FAIL line=%d count=%d\n",__LINE__,count);fflush(stdout);Delay(500);goto out;}}while(0)

static ULONG pixels(struct Window*w){
 struct ConUnit*u=CU(unit);ULONG h=2166136261UL;
 for(int y=u->cu_YROrigin;y<u->cu_YROrigin+u->cu_YRSize*(u->cu_YMax+1);y++)
 for(int x=u->cu_XROrigin;x<u->cu_XROrigin+u->cu_XRSize*40;x++)h=(h^(ULONG)ReadPixel(w->RPort,x,y))*16777619UL;
 return h;
}
#define TAP(k) (key(k,0)&&key((k)|IECODE_UP_PREFIX,0)&&bytes(""))
int main(void){
 int rc=20,count=0;struct InfoData info;struct MsgPort*port=NULL;struct Window*w=NULL;
 struct ConsolengBindingPrefs p={sizeof(p),17,2,{{CONSOLENG_BIND_RAW+RAWKEY_F3,0,CONSOLENG_BIND_NAVIGATE},{CONSOLENG_BIND_RAW+RAWKEY_F4,4,CONSOLENG_BIND_NAVSEARCHFORWARD}}},got;
 stream=Open((CONST_STRPTR)"RGCON:",MODE_OLDFILE);CHECK(stream);
 struct FileHandle*fh=BADDR(stream);CHECK(DoPkt(fh->fh_Type,ACTION_DISK_INFO,(SIPTR)MKBADDR(&info),0,0,0,0));
 struct IOStdReq*io=(void*)(IPTR)info.id_InUse;CHECK(io&&io->io_Unit);unit=(Object*)io->io_Unit;base=(void*)io->io_Device;ConsoleDevice=io->io_Device;
 port=CreateMsgPort();CHECK(port);request=(void*)CreateIORequest(port,sizeof(*request));CHECK(request);request->io_Device=io->io_Device;
 CHECK(exchange(CD_SETBINDINGPREFS,io->io_Unit,&p,sizeof(p),0));
 got=p;got.bp_Version=16;CHECK(exchange(CD_SETBINDINGPREFS,io->io_Unit,&got,sizeof(got),CONSOLENG_ERR_BADPREFS));
 CHECK(exchange(CD_GETBINDINGPREFS,io->io_Unit,&got,sizeof(got),0));CHECK(!memcmp(&got,&p,sizeof(p)));
 w=CU(unit)->cu_Window;CHECK(w);SetWindowTitles(w,(CONST_STRPTR)"ConsoleNG navigation shortcuts",(CONST_STRPTR)-1);
 MoveWindow(w,30-w->LeftEdge,30-w->TopEdge);SizeWindow(w,730-w->Width,450-w->Height);ActivateWindow(w);Delay(20);CHECK(SetMode(stream,TRUE));
 CHECK(put("\x1b%G\x1b[?25l\x1b]50;DejaVuSansMono,16\7\x1b[2J\x1b[H"));
 for(int i=0;i<90;i++){char line[96];snprintf(line,sizeof(line),"Row %02d  https://example.org/  configurable navigation\r\n",i);CHECK(put(line));}
 CHECK(put("\x1b[5;12H\x1b[=31u"));Delay(10);WORD x=CU(unit)->cu_XCP,y=CU(unit)->cu_YCP;
 /* Configured movements consume their chord outside navigation. */
 CHECK(key(RAWKEY_F4,IEQUALIFIER_CONTROL));CHECK(key(RAWKEY_F4,IEQUALIFIER_CONTROL|IEQUALIFIER_REPEAT));CHECK(key(RAWKEY_F4|IECODE_UP_PREFIX,IEQUALIFIER_CONTROL));CHECK(bytes(""));CHECK(put("\x1b[=0u"));
 const UWORD codes[]={0x3a,0x3a,0x08,0x03,0x36,0x36};
 const UWORD quals[]={0,IEQUALIFIER_LSHIFT,IEQUALIFIER_LSHIFT,IEQUALIFIER_LSHIFT,0,IEQUALIFIER_LSHIFT};
 for(unsigned i=0;i<6;i++)for(unsigned selected=0;selected<2;selected++) {
  p.bp_Entries[1].kb_Action=CONSOLENG_BIND_NAVSEARCHFORWARD+i;
  CHECK(exchange(CD_SETBINDINGPREFS,io->io_Unit,&p,sizeof(p),0));
  ULONG expected=0;
  for(unsigned configured=0;configured<2;configured++){
   CHECK(TAP(RAWKEY_F3));Delay(10);CHECK(TAP(0x0a));Delay(10);
   if(selected){CHECK(TAP(RAWKEY_V));Delay(10);}
   if(i>=4){CHECK(TAP(0x3a));CHECK(TAP(0x18));Delay(20);CHECK(TAP(RAWKEY_RETURN));Delay(20);}
   UWORD code=configured?RAWKEY_F4:codes[i],qual=configured?IEQUALIFIER_CONTROL:quals[i];
   CHECK(key(code,qual));CHECK(key(code|IECODE_UP_PREFIX,qual));
   if(i<2){CHECK(TAP(0x18));Delay(20);CHECK(TAP(RAWKEY_RETURN));}
   Delay(30);CHECK(bytes(""));
   if(!configured)expected=pixels(w);else CHECK(pixels(w)==expected);
   if(configured && selected){printf("SEARCH_BINDINGS_SHOT action=%u\n",i+59);fflush(stdout);Delay(100);}
   CHECK(TAP(RAWKEY_F3));CHECK(CU(unit)->cu_XCP==x&&CU(unit)->cu_YCP==y);
  }
 }
 CHECK(put("\x1b[2J\x1b[HPASS: 6 configurable navigation search actions\r\n\r\nBuilt-in and configured keys render identical results.\r\nQueries, word searches and match repeats agree.\r\nApplication cursor and input remain intact.\r\n"));
 printf("SEARCH_BINDINGS_PASS count=%d\n",count);fflush(stdout);Delay(300);rc=0;
out:
 if(request)DeleteIORequest((struct IORequest*)request);if(port)DeleteMsgPort(port);
 if(stream){SetMode(stream,FALSE);Close(stream);}printf("SEARCH_BINDINGS_DONE rc=%d\n",rc);fflush(stdout);return rc;
}
