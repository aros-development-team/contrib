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
#define CHECK(c) do{count++;if(!(c)){printf("SELECTION_BINDINGS_FAIL line=%d count=%d\n",__LINE__,count);fflush(stdout);Delay(500);goto out;}}while(0)

static ULONG pixels(struct Window*w){
 struct ConUnit*u=CU(unit);ULONG h=2166136261UL;
 for(int y=u->cu_YROrigin;y<u->cu_YROrigin+u->cu_YRSize*(u->cu_YMax+1);y++)
 for(int x=u->cu_XROrigin;x<u->cu_XROrigin+u->cu_XRSize*40;x++)h=(h^(ULONG)ReadPixel(w->RPort,x,y))*16777619UL;
 return h;
}
#define TAP(k) (key(k,0)&&key((k)|IECODE_UP_PREFIX,0)&&bytes(""))
static int captured(void){char got[128]={0};BPTR f=Open((CONST_STRPTR)"RAM:ConsoleNG-URL-Test",MODE_OLDFILE);if(!f)return 0;LONG n=Read(f,got,sizeof(got)-1);Close(f);return n==20&&!memcmp(got,"https://example.org/",20);}
int main(void){
 int rc=20,count=0;struct InfoData info;struct MsgPort*port=NULL;struct Window*w=NULL;
 struct ConsolengBindingPrefs p={sizeof(p),13,10,{{CONSOLENG_BIND_RAW+RAWKEY_F1,4,CONSOLENG_BIND_NAVOPENLINK},{CONSOLENG_BIND_RAW+RAWKEY_F2,2,CONSOLENG_BIND_NAVCENTER},{CONSOLENG_BIND_RAW+RAWKEY_F3,0,CONSOLENG_BIND_NAVIGATE},{CONSOLENG_BIND_RAW+RAWKEY_X,CONSOLENG_BIND_NAVONLY,CONSOLENG_BIND_NAVCENTER},{CONSOLENG_BIND_RAW+RAWKEY_F4,CONSOLENG_BIND_NAVONLY,CONSOLENG_BIND_NAVCENTER},{CONSOLENG_BIND_RAW+RAWKEY_F4,CONSOLENG_BIND_NOTNAV,CONSOLENG_BIND_NAVOPENLINK},{CONSOLENG_BIND_RAW+RAWKEY_F5,32,22},{CONSOLENG_BIND_RAW+RAWKEY_F6,32,23},{CONSOLENG_BIND_RAW+RAWKEY_F7,32,24},{CONSOLENG_BIND_RAW+RAWKEY_F8,32,25}}},got;
 stream=Open((CONST_STRPTR)"RGCON:",MODE_OLDFILE);CHECK(stream);
 struct FileHandle*fh=BADDR(stream);CHECK(DoPkt(fh->fh_Type,ACTION_DISK_INFO,(SIPTR)MKBADDR(&info),0,0,0,0));
 struct IOStdReq*io=(void*)(IPTR)info.id_InUse;CHECK(io&&io->io_Unit);unit=(Object*)io->io_Unit;base=(void*)io->io_Device;ConsoleDevice=io->io_Device;
 port=CreateMsgPort();CHECK(port);request=(void*)CreateIORequest(port,sizeof(*request));CHECK(request);request->io_Device=io->io_Device;
 CHECK(exchange(CD_SETBINDINGPREFS,io->io_Unit,&p,sizeof(p),0));
 got=p;got.bp_Version=12;CHECK(exchange(CD_SETBINDINGPREFS,io->io_Unit,&got,sizeof(got),CONSOLENG_ERR_BADPREFS));
 CHECK(exchange(CD_GETBINDINGPREFS,io->io_Unit,&got,sizeof(got),0));CHECK(!memcmp(&got,&p,sizeof(p)));
 w=CU(unit)->cu_Window;CHECK(w);SetWindowTitles(w,(CONST_STRPTR)"ConsoleNG navigation shortcuts",(CONST_STRPTR)-1);
 MoveWindow(w,30-w->LeftEdge,30-w->TopEdge);SizeWindow(w,730-w->Width,450-w->Height);ActivateWindow(w);Delay(20);CHECK(SetMode(stream,TRUE));
 CHECK(put("\x1b%G\x1b[?25l\x1b]50;DejaVuSansMono,16\7\x1b[2J\x1b[H"));
 for(int i=0;i<90;i++){char line[96];snprintf(line,sizeof(line),"Row %02d  https://example.org/  configurable navigation\r\n",i);CHECK(put(line));}
 CHECK(put("\x1b[5;12H\x1b[=31u"));Delay(10);WORD x=CU(unit)->cu_XCP,y=CU(unit)->cu_YCP;
 /* In application mode all press/repeat/release bytes remain local. */
 CHECK(key(RAWKEY_F1,IEQUALIFIER_CONTROL));CHECK(key(RAWKEY_F1,IEQUALIFIER_CONTROL|IEQUALIFIER_REPEAT));CHECK(key(RAWKEY_F1|IECODE_UP_PREFIX,IEQUALIFIER_CONTROL));CHECK(bytes(""));
 CHECK(key(RAWKEY_F2,IEQUALIFIER_LALT));CHECK(key(RAWKEY_F2|IECODE_UP_PREFIX,IEQUALIFIER_LALT));CHECK(bytes(""));
 CHECK(TAP(RAWKEY_F4));CHECK(put("\x1b[=0u"));
 CHECK(key(RAWKEY_X,0));CHECK(key(RAWKEY_X|IECODE_UP_PREFIX,0));CHECK(bytes("x"));
 CHECK(TAP(RAWKEY_F3));Delay(10);
 /* Built-in z and configured Alt+F2 must produce the same viewport. */
 CHECK(TAP(RAWKEY_Z));Delay(10);ULONG centered=pixels(w);
 CHECK(TAP(RAWKEY_F3));CHECK(TAP(RAWKEY_F3));Delay(10);
 CHECK(key(RAWKEY_F2,IEQUALIFIER_LALT));CHECK(key(RAWKEY_F2|IECODE_UP_PREFIX,IEQUALIFIER_LALT));Delay(10);CHECK(bytes(""));CHECK(pixels(w)==centered);
 CHECK(TAP(RAWKEY_X));Delay(10);CHECK(pixels(w)==centered);
 CHECK(TAP(RAWKEY_F4));Delay(10);CHECK(pixels(w)==centered);
 /* Each configured action must paint exactly the built-in selection. */
 const UWORD quals[]={0,IEQUALIFIER_LSHIFT,IEQUALIFIER_CONTROL,IEQUALIFIER_LALT};
 for(unsigned i=0;i<4;i++) {
  ULONG plain=pixels(w);
  CHECK(key(RAWKEY_V,quals[i]));CHECK(key(RAWKEY_V|IECODE_UP_PREFIX,quals[i]));Delay(10);
  for(unsigned j=0;j<4;j++)CHECK(TAP(RAWKEY_RIGHT));Delay(10);
  ULONG selected=pixels(w);CHECK(selected!=plain);
  CHECK(key(RAWKEY_V,quals[i]));CHECK(key(RAWKEY_V|IECODE_UP_PREFIX,quals[i]));
  for(unsigned j=0;j<4;j++)CHECK(TAP(RAWKEY_LEFT));Delay(10);CHECK(pixels(w)==plain);
  CHECK(key(RAWKEY_F5+i,0));Delay(10);
  for(unsigned j=0;j<4;j++)CHECK(TAP(RAWKEY_RIGHT));Delay(10);CHECK(pixels(w)==selected);
  CHECK(key(RAWKEY_F5+i,IEQUALIFIER_REPEAT));Delay(5);CHECK(pixels(w)==selected);
  CHECK(key((RAWKEY_F5+i)|IECODE_UP_PREFIX,0));CHECK(bytes(""));
  printf("SELECTION_BINDINGS_SHOT mode=%u\n",i);fflush(stdout);Delay(100);
  CHECK(TAP(RAWKEY_F5+i));
  for(unsigned j=0;j<4;j++)CHECK(TAP(RAWKEY_LEFT));Delay(10);CHECK(pixels(w)==plain);
 }
 DeleteFile((CONST_STRPTR)"RAM:ConsoleNG-URL-Test");
 CHECK(key(RAWKEY_F1,IEQUALIFIER_CONTROL));for(int n=0;n<200&&!captured();n++)Delay(1);CHECK(captured());
 DeleteFile((CONST_STRPTR)"RAM:ConsoleNG-URL-Test");
 CHECK(key(RAWKEY_F1,IEQUALIFIER_CONTROL|IEQUALIFIER_REPEAT));CHECK(key(RAWKEY_F1|IECODE_UP_PREFIX,IEQUALIFIER_CONTROL));Delay(10);CHECK(!captured());CHECK(bytes(""));
 CHECK(CU(unit)->cu_XCP==x&&CU(unit)->cu_YCP==y);
 puts("SELECTION_BINDINGS_CENTERED");fflush(stdout);Delay(200);
 CHECK(TAP(RAWKEY_F3));CHECK(!strcmp((char*)w->Title,"ConsoleNG navigation shortcuts"));
 CHECK(put("\x1b[2J\x1b[HPASS: configurable native selection shortcuts\r\n\r\nCtrl+F1 opens the link at the navigation cursor.\r\nAlt+F2 centers the viewport exactly like z.\r\n\r\nNavigation-only X reaches the app outside navigation.\r\nF5/F6/F7/F8 match character/line/rectangle/word selection.\r\n"));
 printf("SELECTION_BINDINGS_PASS count=%d\n",count);fflush(stdout);Delay(400);rc=0;
out:
 if(request)DeleteIORequest((struct IORequest*)request);if(port)DeleteMsgPort(port);
 if(stream){SetMode(stream,FALSE);Close(stream);}printf("SELECTION_BINDINGS_DONE rc=%d\n",rc);fflush(stdout);return rc;
}
