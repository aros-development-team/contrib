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
#define CHECK(c) do{count++;if(!(c)){printf("SEMANTIC_NAV_FAIL line=%d count=%d\n",__LINE__,count);fflush(stdout);Delay(500);goto out;}}while(0)

static ULONG pixels(struct Window*w){
 struct ConUnit*u=CU(unit);ULONG h=2166136261UL;
 for(int y=u->cu_YROrigin;y<u->cu_YROrigin+u->cu_YRSize*5;y++)
 for(int x=u->cu_XROrigin;x<u->cu_XROrigin+u->cu_XRSize*40;x++)h=(h^(ULONG)ReadPixel(w->RPort,x,y))*16777619UL;
 return h;
}
#define TAP(k) (key(k,0)&&key((k)|IECODE_UP_PREFIX,0)&&bytes(""))

int main(void){
 int rc=20,count=0;struct InfoData info;struct MsgPort*port=NULL;struct Window*w=NULL;
 struct ConsolengBindingPrefs p={sizeof(p),17,6,{{CONSOLENG_BIND_RAW+RAWKEY_F3,0,CONSOLENG_BIND_NAVIGATE},{CONSOLENG_BIND_RAW+RAWKEY_F4,CONSOLENG_BIND_NAVONLY,CONSOLENG_BIND_NAVSELECTSEMANTIC},{CONSOLENG_BIND_RAW+RAWKEY_F5,CONSOLENG_BIND_NAVONLY,CONSOLENG_BIND_NAVSELECTCHAR},{CONSOLENG_BIND_RAW+RAWKEY_F6,CONSOLENG_BIND_NAVONLY,CONSOLENG_BIND_NAVSEMANTICNEXT},{CONSOLENG_BIND_RAW+RAWKEY_F7,CONSOLENG_BIND_NAVONLY,CONSOLENG_BIND_NAVSEEDFORWARD},{CONSOLENG_BIND_RAW+RAWKEY_F8,CONSOLENG_BIND_NAVONLY,CONSOLENG_BIND_NAVSEARCHFORWARD}}};
 stream=Open((CONST_STRPTR)"RGCON:",MODE_OLDFILE);CHECK(stream);
 struct FileHandle*fh=BADDR(stream);CHECK(DoPkt(fh->fh_Type,ACTION_DISK_INFO,(SIPTR)MKBADDR(&info),0,0,0,0));
 struct IOStdReq*io=(void*)(IPTR)info.id_InUse;CHECK(io&&io->io_Unit);unit=(Object*)io->io_Unit;base=(void*)io->io_Device;ConsoleDevice=io->io_Device;
 port=CreateMsgPort();CHECK(port);request=(void*)CreateIORequest(port,sizeof(*request));CHECK(request);request->io_Device=io->io_Device;
 CHECK(exchange(CD_SETBINDINGPREFS,io->io_Unit,&p,sizeof(p),0));
 w=CU(unit)->cu_Window;CHECK(w);SetWindowTitles(w,(CONST_STRPTR)"ConsoleNG custom word separators",(CONST_STRPTR)-1);
 MoveWindow(w,30-w->LeftEdge,30-w->TopEdge);SizeWindow(w,730-w->Width,450-w->Height);ActivateWindow(w);Delay(20);CHECK(SetMode(stream,TRUE));
 CHECK(put("\x1b%G\x1b[?25l\x1b]50;DejaVuSansMono,16\7\x1b[2J\x1b[Hab/cd,ef\x1b[H"));Delay(10);
 struct ConsolengSemanticPrefs policy={sizeof(policy),1,1,{'/'}};
 /* Compare semantic selection with independent explicit character endpoints. */
 for(unsigned mode=0;mode<4;mode++){
  policy.sp_Chars[0]=mode==0?'/':mode==1?',':mode==3?0x03a9:0;policy.sp_Count=mode==2?0:1;
  unsigned last=mode==0||mode==3?1:mode==1?4:(unsigned)CU(unit)->cu_XMax;
  CHECK(put(mode==3?"\x1b[2J\x1b[Hab\xce\xa9""cd,ef\x1b[H":"\x1b[2J\x1b[Hab/cd,ef\x1b[H"));
  CHECK(exchange(CD_SETSEMANTICPREFS,io->io_Unit,&policy,sizeof(policy),0));
  CHECK(put("\x1b[H"));CHECK(TAP(RAWKEY_F3));CHECK(TAP(RAWKEY_F4));Delay(10);ULONG selected=pixels(w);
  CHECK(TAP(RAWKEY_F3));CHECK(TAP(RAWKEY_F3));CHECK(TAP(RAWKEY_F5));
  for(unsigned i=0;i<last;i++){CHECK(TAP(RAWKEY_RIGHT));}Delay(10);
  CHECK(pixels(w)==selected);CHECK(TAP(RAWKEY_F3));
  if(mode!=2){
   CHECK(put("\x1b[H"));CHECK(TAP(RAWKEY_F3));CHECK(TAP(RAWKEY_F6));Delay(10);ULONG moved=pixels(w);
   CHECK(TAP(RAWKEY_F3));CHECK(TAP(RAWKEY_F3));
   for(unsigned i=0;i<last+1;i++){CHECK(TAP(RAWKEY_RIGHT));}Delay(10);
   CHECK(pixels(w)==moved);CHECK(TAP(RAWKEY_F3));
  }
  printf("SEMANTIC_NAV_SELECTION mode=%u last=%u\n",mode,last);fflush(stdout);
 }
 /* Changing live settings does not mutate an active navigation snapshot. */
 CHECK(put("\x1b[2J\x1b[Hab/cd,ef\x1b[H"));
 policy.sp_Count=1;policy.sp_Chars[0]='/';CHECK(exchange(CD_SETSEMANTICPREFS,io->io_Unit,&policy,sizeof(policy),0));
 CHECK(put("\x1b[H"));CHECK(TAP(RAWKEY_F3));CHECK(TAP(RAWKEY_F4));Delay(10);ULONG frozen=pixels(w);
 CHECK(TAP(RAWKEY_F4));policy.sp_Chars[0]=',';CHECK(exchange(CD_SETSEMANTICPREFS,io->io_Unit,&policy,sizeof(policy),0));
 CHECK(TAP(RAWKEY_F4));Delay(10);CHECK(pixels(w)==frozen);CHECK(TAP(RAWKEY_F3));
 CHECK(TAP(RAWKEY_F3));CHECK(TAP(RAWKEY_F4));Delay(10);CHECK(pixels(w)!=frozen);CHECK(TAP(RAWKEY_F3));

 /* Seeded search skips the source occurrence; manual query plus next must agree. */
 policy.sp_Chars[0]='/';CHECK(exchange(CD_SETSEMANTICPREFS,io->io_Unit,&policy,sizeof(policy),0));
 CHECK(put("\x1b[2J\x1b[Hab/cd,ef ab/cd,gh\x1b[H"));CHECK(TAP(RAWKEY_F3));CHECK(TAP(RAWKEY_F7));Delay(30);ULONG seeded=pixels(w);
 CHECK(TAP(RAWKEY_F3));CHECK(TAP(RAWKEY_F3));CHECK(TAP(RAWKEY_F8));CHECK(TAP(RAWKEY_A));CHECK(TAP(RAWKEY_B));Delay(20);CHECK(TAP(RAWKEY_RETURN));Delay(30);CHECK(TAP(RAWKEY_N));Delay(30);
 CHECK(pixels(w)==seeded);CHECK(TAP(RAWKEY_F3));
 CHECK(put("\x1b[2J\x1b[HPASS: custom word separator selection\r\n\r\nSlash, comma, Unicode and empty policies match explicit spans.\r\nSemantic motions match explicit character positions.\r\nActive navigation keeps its original boundaries.\r\nNew navigation picks up changed preferences.\r\nSeeded search matches an explicit word query.\r\nApplication input remains isolated.\r\n"));
 printf("SEMANTIC_NAV_PASS checks=%d\n",count);fflush(stdout);Delay(300);rc=0;
out:
 if(request)DeleteIORequest((struct IORequest*)request);
 if(port)DeleteMsgPort(port);
 if(stream){SetMode(stream,FALSE);Close(stream);}
 printf("SEMANTIC_NAV_DONE rc=%d\n",rc);fflush(stdout);return rc;
}
