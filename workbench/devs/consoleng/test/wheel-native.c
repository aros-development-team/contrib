/* Native shortcut policy API and input routing through DOS streams. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
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

#define TAP(k) (key(k,0)&&key((k)|IECODE_UP_PREFIX,0)&&bytes(""))
int main(void){
 int rc=20,count=0,changed=0;struct InfoData info;struct MsgPort*port=NULL;struct Window*w=NULL;struct IOStdReq*fresh=NULL;
 struct ConsolengScrollPrefs original,p,got;
 stream=Open((CONST_STRPTR)"RGCON:",MODE_OLDFILE);CHECK(stream);
 struct FileHandle*fh=BADDR(stream);CHECK(DoPkt(fh->fh_Type,ACTION_DISK_INFO,(SIPTR)MKBADDR(&info),0,0,0,0));
 struct IOStdReq*io=(void*)(IPTR)info.id_InUse;CHECK(io&&io->io_Unit);unit=(Object*)io->io_Unit;base=(void*)io->io_Device;ConsoleDevice=io->io_Device;
 port=CreateMsgPort();CHECK(port);request=(void*)CreateIORequest(port,sizeof(*request));CHECK(request);request->io_Device=io->io_Device;
 CHECK(exchange(CD_GETDEFAULTSCROLLPREFS,(void*)CONU_LIBRARY,&original,sizeof(original),0));CHECK(original.sp_Lines==1);
 CHECK(exchange(CD_GETSCROLLPREFS,io->io_Unit,&got,sizeof(got),0));CHECK(!memcmp(&original,&got,sizeof(got)));
 p=(struct ConsolengScrollPrefs){sizeof(p),1,7};CHECK(exchange(CD_SETDEFAULTSCROLLPREFS,(void*)CONU_LIBRARY,&p,sizeof(p),0));changed=1;
 CHECK(exchange(CD_GETSCROLLPREFS,io->io_Unit,&got,sizeof(got),0));CHECK(got.sp_Lines==1);
 fresh=(void*)CreateIORequest(port,sizeof(*fresh));CHECK(fresh);
 CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",CONU_CHARMAP,(struct IORequest*)fresh,0));
 CHECK(exchange(CD_GETSCROLLPREFS,fresh->io_Unit,&got,sizeof(got),0));CHECK(got.sp_Lines==7);CloseDevice((struct IORequest*)fresh);DeleteIORequest((struct IORequest*)fresh);fresh=NULL;
 CHECK(exchange(CD_SETDEFAULTSCROLLPREFS,(void*)CONU_LIBRARY,&original,sizeof(original),0));changed=0;
 got=p;got.sp_Lines=101;CHECK(exchange(CD_SETSCROLLPREFS,io->io_Unit,&got,sizeof(got),CONSOLENG_ERR_BADPREFS));
 CHECK(exchange(CD_SETSCROLLPREFS,io->io_Unit,&p,sizeof(p)-1,IOERR_BADLENGTH));
 struct ConsolengBindingPrefs keys={sizeof(keys),3,3,{{CONSOLENG_BIND_RAW+RAWKEY_F1,0,CONSOLENG_BIND_LINEUP},{CONSOLENG_BIND_RAW+RAWKEY_F2,0,CONSOLENG_BIND_BOTTOM},{CONSOLENG_BIND_RAW+RAWKEY_F3,0,CONSOLENG_BIND_LINEDOWN}}};
 CHECK(exchange(CD_SETBINDINGPREFS,io->io_Unit,&keys,sizeof(keys),0));
 w=CU(unit)->cu_Window;CHECK(w);SetWindowTitles(w,(CONST_STRPTR)"ConsoleNG configurable wheel distance",(CONST_STRPTR)-1);
 MoveWindow(w,30-w->LeftEdge,30-w->TopEdge);SizeWindow(w,730-w->Width,450-w->Height);ActivateWindow(w);Delay(20);CHECK(SetMode(stream,TRUE));
 CHECK(put("\x1b%G\x1b[?25l\x1b]50;DejaVuSansMono,16\7"));
 for(unsigned i=0;i<150;i++){char line[80];snprintf(line,sizeof(line),"Retained history row %03u: wheel speed test\r\n",i);CHECK(put(line));}
 struct PropInfo*prop=NULL;for(struct Gadget*g=w->FirstGadget;g;g=g->NextGadget)if((g->GadgetType&GTYP_GTYPEMASK)==GTYP_PROPGADGET&&g->SpecialInfo&&(((struct PropInfo*)g->SpecialInfo)->Flags&FREEVERT)){prop=g->SpecialInfo;break;}CHECK(prop);
 WORD x=CU(unit)->cu_XCP,y=CU(unit)->cu_YCP;unsigned speeds[]={0,1,3,10,100};
 for(unsigned n=0;n<5;n++){
  p.sp_Lines=speeds[n];CHECK(exchange(CD_SETSCROLLPREFS,io->io_Unit,&p,sizeof(p),0));
  CHECK(TAP(RAWKEY_F2));for(unsigned i=0;i<p.sp_Lines;i++)CHECK(TAP(RAWKEY_F1));ULONG expect=prop->VertPot;
  CHECK(TAP(RAWKEY_F2));CHECK(key(RAWKEY_NM_WHEEL_UP,0));CHECK(bytes(""));CHECK(prop->VertPot==expect);
  CHECK(key(RAWKEY_NM_WHEEL_DOWN,0));CHECK(bytes(""));CHECK(prop->VertPot==MAXPOT);
  CHECK(CU(unit)->cu_XCP==x&&CU(unit)->cu_YCP==y);
 }
 p.sp_Lines=3;CHECK(exchange(CD_SETSCROLLPREFS,io->io_Unit,&p,sizeof(p),0));
 CHECK(put("\x1b[?1049h\x1b[?1007h"));CHECK(key(RAWKEY_NM_WHEEL_UP,0));CHECK(bytes("\x1bOA\x1bOA\x1bOA"));
 CHECK(key(RAWKEY_NM_WHEEL_DOWN,0));CHECK(bytes("\x1bOB\x1bOB\x1bOB"));
 p.sp_Lines=0;CHECK(exchange(CD_SETSCROLLPREFS,io->io_Unit,&p,sizeof(p),0));CHECK(key(RAWKEY_NM_WHEEL_UP,0));CHECK(bytes(""));
 CHECK(put("\x1b[?1007l\x1b[?1049l"));CHECK(prop->VertPot==MAXPOT);
 CHECK(put("\x1b[2J\x1b[HWheel distance: PASS\r\n\r\n0, 1, 3, 10 and 100 lines match keyboard scrolling.\r\nCursor and history preserved; defaults isolated.\r\nAlternate-screen arrows respect the setting.\r\nPreferences Save/reload is tested separately.\r\n"));
 printf("WHEEL_NATIVE_PASS count=%d\n",count);fflush(stdout);Delay(1500);rc=0;
out:
 if(changed&&request)exchange(CD_SETDEFAULTSCROLLPREFS,(void*)CONU_LIBRARY,&original,sizeof(original),0);
 if(fresh){if(fresh->io_Device)CloseDevice((struct IORequest*)fresh);DeleteIORequest((struct IORequest*)fresh);}
 if(request)DeleteIORequest((struct IORequest*)request);if(port)DeleteMsgPort(port);
 if(stream){SetMode(stream,FALSE);Close(stream);}printf("WHEEL_NATIVE_DONE rc=%d\n",rc);fflush(stdout);return rc;
}
