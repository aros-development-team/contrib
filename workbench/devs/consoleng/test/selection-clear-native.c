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
#define CHECK(c) do{count++;if(!(c)){printf("BINDINGS_FAIL line=%d count=%d\n",__LINE__,count);fflush(stdout);Delay(500);goto out;}}while(0)

static ULONG pixels(struct Window*w){
 struct ConUnit*u=CU(unit);ULONG h=2166136261UL;
 for(int y=u->cu_YROrigin;y<u->cu_YROrigin+u->cu_YRSize*5;y++)
 for(int x=u->cu_XROrigin;x<u->cu_XROrigin+u->cu_XRSize*40;x++)h=(h^(ULONG)ReadPixel(w->RPort,x,y))*16777619UL;
 return h;
}
#define TAP(k) (key(k,0)&&key((k)|IECODE_UP_PREFIX,0)&&bytes(""))
int main(void){
 int rc=20,count=0;struct InfoData info;struct MsgPort*port=NULL;struct Window*w=NULL;
 struct ConsolengBindingPrefs p={sizeof(p),10,3,{{CONSOLENG_BIND_RAW+RAWKEY_F1,0,CONSOLENG_BIND_SELECTALL},{CONSOLENG_BIND_RAW+RAWKEY_F2,0,CONSOLENG_BIND_CLEARSELECTION},{CONSOLENG_BIND_RAW+RAWKEY_F3,0,CONSOLENG_BIND_NAVIGATE}}},got;
 stream=Open((CONST_STRPTR)"RGCON:",MODE_OLDFILE);CHECK(stream);
 struct FileHandle*fh=BADDR(stream);CHECK(DoPkt(fh->fh_Type,ACTION_DISK_INFO,(SIPTR)MKBADDR(&info),0,0,0,0));
 struct IOStdReq*io=(void*)(IPTR)info.id_InUse;CHECK(io&&io->io_Unit);unit=(Object*)io->io_Unit;base=(void*)io->io_Device;ConsoleDevice=io->io_Device;
 port=CreateMsgPort();CHECK(port);request=(void*)CreateIORequest(port,sizeof(*request));CHECK(request);request->io_Device=io->io_Device;
 CHECK(exchange(CD_SETBINDINGPREFS,io->io_Unit,&p,sizeof(p),0));
 got=p;got.bp_Version=9;CHECK(exchange(CD_SETBINDINGPREFS,io->io_Unit,&got,sizeof(got),CONSOLENG_ERR_BADPREFS));
 CHECK(exchange(CD_GETBINDINGPREFS,io->io_Unit,&got,sizeof(got),0));CHECK(!memcmp(&got,&p,sizeof(p)));
 w=CU(unit)->cu_Window;CHECK(w);SetWindowTitles(w,(CONST_STRPTR)"ConsoleNG clear selection",(CONST_STRPTR)-1);
 MoveWindow(w,30-w->LeftEdge,30-w->TopEdge);SizeWindow(w,730-w->Width,450-w->Height);ActivateWindow(w);Delay(20);CHECK(SetMode(stream,TRUE));
 CHECK(put("\x1b%G\x1b[?25l\x1b]50;DejaVuSansMono,16\7\x1b[2J\x1b[HClear selection without losing your place\r\n\r\nSelected text stays intact.\r\nNavigation and application cursor stay put.\r\n"));Delay(10);
 ULONG baseline=pixels(w);WORD x=CU(unit)->cu_XCP,y=CU(unit)->cu_YCP;
 CHECK(TAP(RAWKEY_F1));Delay(10);CHECK(pixels(w)!=baseline);
 puts("CLEAR_SELECTION_SELECTED");fflush(stdout);Delay(150);
 CHECK(key(RAWKEY_F2,0));CHECK(key(RAWKEY_F2,IEQUALIFIER_REPEAT));CHECK(bytes(""));
 /* Remapping a held key must not leak its release to an all-key application. */
 CHECK(put("\x1b[=31u"));got=p;got.bp_Count=0;memset(got.bp_Entries,0,sizeof(got.bp_Entries));
 CHECK(exchange(CD_SETBINDINGPREFS,io->io_Unit,&got,sizeof(got),0));
 CHECK(key(RAWKEY_F2|IECODE_UP_PREFIX,0));CHECK(bytes(""));CHECK(pixels(w)==baseline);
 CHECK(CU(unit)->cu_XCP==x&&CU(unit)->cu_YCP==y);
 CHECK(exchange(CD_SETBINDINGPREFS,io->io_Unit,&p,sizeof(p),0));CHECK(TAP(RAWKEY_F2));CHECK(pixels(w)==baseline);
 CHECK(put("\x1b[=0u"));CHECK(TAP(RAWKEY_F3));Delay(10);CHECK(strcmp((char*)w->Title,"ConsoleNG clear selection"));
 ULONG nav=pixels(w);CHECK(TAP(RAWKEY_V));Delay(10);CHECK(TAP(RAWKEY_F2));Delay(10);CHECK(pixels(w)==nav);
 CHECK(strcmp((char*)w->Title,"ConsoleNG clear selection"));CHECK(CU(unit)->cu_XCP==x&&CU(unit)->cu_YCP==y);
 CHECK(TAP(RAWKEY_F3));CHECK(!strcmp((char*)w->Title,"ConsoleNG clear selection"));CHECK(pixels(w)==baseline);
 CHECK(put("\r\nPASS: clear, repeat, release and navigation.\r\n"));
 printf("CLEAR_SELECTION_PASS count=%d\n",count);fflush(stdout);Delay(1500);rc=0;
out:
 if(request)DeleteIORequest((struct IORequest*)request);if(port)DeleteMsgPort(port);
 if(stream){SetMode(stream,FALSE);Close(stream);}printf("CLEAR_SELECTION_DONE rc=%d\n",rc);fflush(stdout);return rc;
}
