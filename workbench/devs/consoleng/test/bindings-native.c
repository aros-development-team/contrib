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
int main(void){int rc=20,count=0,changed=0;struct InfoData info;struct MsgPort*port=NULL;
 struct ConsolengBindingPrefs original,p,got,defaults;struct Window*w=NULL;
 stream=Open((CONST_STRPTR)"RGCON:",MODE_OLDFILE);CHECK(stream);
 struct FileHandle*fh=BADDR(stream);CHECK(DoPkt(fh->fh_Type,ACTION_DISK_INFO,(SIPTR)MKBADDR(&info),0,0,0,0));
 struct IOStdReq*io=(void*)(IPTR)info.id_InUse;CHECK(io&&io->io_Unit);unit=(Object*)io->io_Unit;base=(void*)io->io_Device;ConsoleDevice=io->io_Device;
 port=CreateMsgPort();CHECK(port);request=(void*)CreateIORequest(port,sizeof(*request));CHECK(request);request->io_Device=io->io_Device;
 CHECK(exchange(CD_GETBINDINGPREFS,io->io_Unit,&original,sizeof(original),0));CHECK(original.bp_Count==4);
 CHECK(exchange(CD_GETDEFAULTBINDINGPREFS,(void*)CONU_LIBRARY,&defaults,sizeof(defaults),0));CHECK(!memcmp(&original,&defaults,sizeof(original)));
 p=original;p.bp_Entries[0]=(struct ConsolengKeyBinding){'x',5,CONSOLENG_BIND_COPY};p.bp_Entries[3]=(struct ConsolengKeyBinding){'s',5,CONSOLENG_BIND_SEARCH};
 CHECK(exchange(CD_SETBINDINGPREFS,io->io_Unit,&p,sizeof(p)-1,IOERR_BADLENGTH));
 CHECK(exchange(CD_SETBINDINGPREFS,(void*)CONU_LIBRARY,&p,sizeof(p),IOERR_BADADDRESS));
 got=p;got.bp_Entries[1]=got.bp_Entries[0];CHECK(exchange(CD_SETBINDINGPREFS,io->io_Unit,&got,sizeof(got),CONSOLENG_ERR_BADPREFS));
 CHECK(exchange(CD_GETBINDINGPREFS,io->io_Unit,&got,sizeof(got),0));CHECK(!memcmp(&got,&original,sizeof(got)));
 CHECK(exchange(CD_SETBINDINGPREFS,io->io_Unit,&p,sizeof(p),0));
 CHECK(exchange(CD_GETDEFAULTBINDINGPREFS,(void*)CONU_LIBRARY,&got,sizeof(got),0));CHECK(!memcmp(&got,&defaults,sizeof(got)));
 w=CU(unit)->cu_Window;CHECK(w);SetWindowTitles(w,(CONST_STRPTR)"ConsoleNG configurable shortcuts",(CONST_STRPTR)-1);
 MoveWindow(w,30-w->LeftEdge,30-w->TopEdge);SizeWindow(w,730-w->Width,450-w->Height);ActivateWindow(w);Delay(20);
 CHECK(SetMode(stream,TRUE));CHECK(put("\x1b%G\x1b[?25l\x1b]50;DejaVuSansMono,16\7\x1b[2J\x1b[HNative shortcut policy\r\nTesting custom keys and application isolation...\r\n"));
 for(int mode=0;mode<2;mode++){
  CHECK(put(mode?"\x1b[=31u":"\x1b[=0u"));
  CHECK(key(RAWKEY_X,IEQUALIFIER_CONTROL|IEQUALIFIER_LSHIFT));CHECK(bytes(""));
  CHECK(key(RAWKEY_X,IEQUALIFIER_CONTROL|IEQUALIFIER_LSHIFT|IEQUALIFIER_REPEAT));CHECK(bytes(""));
  CHECK(key(RAWKEY_X|IECODE_UP_PREFIX,0));CHECK(bytes(""));
  CHECK(key(RAWKEY_X,0));CHECK(bytes(mode?"\x1b[120;1;120u":"x"));
  CHECK(key(RAWKEY_X|IECODE_UP_PREFIX,0));CHECK(bytes(mode?"\x1b[120;1:3u":""));
  /* Search uses its remapped chord and captures the release before encoding. */
  CHECK(key(RAWKEY_S,IEQUALIFIER_CONTROL|IEQUALIFIER_LSHIFT));CHECK(strstr((char*)w->Title,"Find:"));
  CHECK(key(RAWKEY_S|IECODE_UP_PREFIX,0));CHECK(bytes(""));
  CHECK(key(RAWKEY_ESCAPE,0));CHECK(key(RAWKEY_ESCAPE|IECODE_UP_PREFIX,0));CHECK(bytes(""));
  CHECK(!strcmp((char*)w->Title,"ConsoleNG configurable shortcuts"));
 }
 /* Cancellation without key-up must not swallow the next fresh search. */
 CHECK(key(RAWKEY_S,IEQUALIFIER_CONTROL|IEQUALIFIER_LSHIFT));CHECK(strstr((char*)w->Title,"Find:"));
 CHECK(put("Output cancels search before key release.\r\n"));CHECK(!strcmp((char*)w->Title,"ConsoleNG configurable shortcuts"));
 CHECK(key(RAWKEY_S,IEQUALIFIER_CONTROL|IEQUALIFIER_LSHIFT|IEQUALIFIER_REPEAT));CHECK(!strcmp((char*)w->Title,"ConsoleNG configurable shortcuts"));CHECK(bytes(""));
 CHECK(key(RAWKEY_S,IEQUALIFIER_CONTROL|IEQUALIFIER_LSHIFT));CHECK(strstr((char*)w->Title,"Find:"));
 CHECK(key(RAWKEY_S|IECODE_UP_PREFIX,0));CHECK(key(RAWKEY_ESCAPE,0));CHECK(key(RAWKEY_ESCAPE|IECODE_UP_PREFIX,0));CHECK(bytes(""));
 /* Named F-keys bind without depending on RawKeyConvert output. */
 struct ConsolengBindingPrefs special=p;special.bp_Version=2;
 special.bp_Entries[0]=(struct ConsolengKeyBinding){CONSOLENG_BIND_RAW+RAWKEY_F1,0,CONSOLENG_BIND_COPY};
 special.bp_Entries[3]=(struct ConsolengKeyBinding){CONSOLENG_BIND_RAW+RAWKEY_F2,0,CONSOLENG_BIND_SEARCH};
 CHECK(exchange(CD_SETBINDINGPREFS,io->io_Unit,&special,sizeof(special),0));
 got=special;got.bp_Version=1;CHECK(exchange(CD_SETBINDINGPREFS,io->io_Unit,&got,sizeof(got),CONSOLENG_ERR_BADPREFS));
 for(int mode=0;mode<2;mode++) {
  CHECK(put(mode?"\x1b[=31u":"\x1b[=0u"));
  CHECK(key(RAWKEY_F1,0));CHECK(key(RAWKEY_F1,IEQUALIFIER_REPEAT));CHECK(key(RAWKEY_F1|IECODE_UP_PREFIX,0));CHECK(bytes(""));
  CHECK(key(RAWKEY_F2,0));CHECK(strstr((char*)w->Title,"Find:"));CHECK(key(RAWKEY_F2|IECODE_UP_PREFIX,0));CHECK(bytes(""));
  CHECK(key(RAWKEY_ESCAPE,0));CHECK(key(RAWKEY_ESCAPE|IECODE_UP_PREFIX,0));CHECK(bytes(""));
 }
 CHECK(exchange(CD_SETBINDINGPREFS,io->io_Unit,&p,sizeof(p),0));
 /* Updating defaults affects only subsequently opened units. */
 CHECK(exchange(CD_SETDEFAULTBINDINGPREFS,(void*)CONU_LIBRARY,&p,sizeof(p),0));changed=1;
 BPTR other=Open((CONST_STRPTR)"RGCON:",MODE_OLDFILE);CHECK(other);
 struct InfoData info2;struct FileHandle*fh2=BADDR(other);
 CHECK(DoPkt(fh2->fh_Type,ACTION_DISK_INFO,(SIPTR)MKBADDR(&info2),0,0,0,0));
 struct IOStdReq*io2=(void*)(IPTR)info2.id_InUse;
 CHECK(exchange(CD_GETBINDINGPREFS,io2->io_Unit,&got,sizeof(got),0));CHECK(!memcmp(&p,&got,sizeof(p)));Close(other);
 CHECK(exchange(CD_SETDEFAULTBINDINGPREFS,(void*)CONU_LIBRARY,&defaults,sizeof(defaults),0));changed=0;
 CHECK(exchange(CD_GETBINDINGPREFS,io->io_Unit,&got,sizeof(got),0));CHECK(!memcmp(&p,&got,sizeof(p)));
 /* A changed policy must still consume the matching key release. */
 CHECK(key(RAWKEY_X,IEQUALIFIER_CONTROL|IEQUALIFIER_LSHIFT));CHECK(bytes(""));
 memset(&p,0,sizeof(p));p.bp_Size=sizeof(p);p.bp_Version=1;
 CHECK(exchange(CD_SETBINDINGPREFS,io->io_Unit,&p,sizeof(p),0));
 CHECK(key(RAWKEY_X|IECODE_UP_PREFIX,0));CHECK(bytes(""));
 CHECK(key(RAWKEY_X,IEQUALIFIER_CONTROL|IEQUALIFIER_LSHIFT));CHECK(bytes("\x1b[120:88;6u"));
 CHECK(key(RAWKEY_X|IECODE_UP_PREFIX,IEQUALIFIER_CONTROL|IEQUALIFIER_LSHIFT));CHECK(bytes("\x1b[120:88;6:3u"));
 /* Local history shortcuts move the native scrollbar, never application input. */
 memset(&p,0,sizeof(p));p.bp_Size=sizeof(p);p.bp_Version=3;p.bp_Count=6;
 for(unsigned i=0;i<6;i++)p.bp_Entries[i]=(struct ConsolengKeyBinding){CONSOLENG_BIND_RAW+RAWKEY_F1+i,0,CONSOLENG_BIND_LINEUP+i};
 CHECK(exchange(CD_SETBINDINGPREFS,io->io_Unit,&p,sizeof(p),0));
 got=p;got.bp_Version=2;CHECK(exchange(CD_SETBINDINGPREFS,io->io_Unit,&got,sizeof(got),CONSOLENG_ERR_BADPREFS));
 for(unsigned i=0;i<100;i++){char line[80];snprintf(line,sizeof(line),"Retained history row %03u\r\n",i);CHECK(put(line));}
 struct PropInfo *prop=NULL;
 for(struct Gadget*g=w->FirstGadget;g;g=g->NextGadget)
  if((g->GadgetType&GTYP_GTYPEMASK)==GTYP_PROPGADGET&&g->SpecialInfo&&(((struct PropInfo*)g->SpecialInfo)->Flags&FREEVERT)){prop=g->SpecialInfo;break;}
 CHECK(prop);CHECK(prop->VertPot==MAXPOT);
 #define TAP(k) (key(k,0)&&key((k)|IECODE_UP_PREFIX,0)&&bytes(""))
 CHECK(TAP(RAWKEY_F1));ULONG oneUp=prop->VertPot;CHECK(oneUp<MAXPOT&&oneUp>0);
 CHECK(TAP(RAWKEY_F2));CHECK(prop->VertPot==MAXPOT);
 CHECK(TAP(RAWKEY_F3));CHECK(prop->VertPot<oneUp);
 CHECK(TAP(RAWKEY_F4));CHECK(prop->VertPot==MAXPOT);
 CHECK(TAP(RAWKEY_F5));CHECK(prop->VertPot==0);CHECK(TAP(RAWKEY_F1));CHECK(prop->VertPot==0);
 CHECK(TAP(RAWKEY_F2));CHECK(prop->VertPot>0&&prop->VertPot<MAXPOT);
 /* Held navigation repeats, but a remapped held action never changes meaning. */
 CHECK(TAP(RAWKEY_F6));CHECK(key(RAWKEY_F1,0));ULONG heldPot=prop->VertPot;
 CHECK(key(RAWKEY_F1,IEQUALIFIER_REPEAT));CHECK(prop->VertPot<heldPot);CHECK(bytes(""));
 heldPot=prop->VertPot;got=p;got.bp_Entries[0].kb_Action=CONSOLENG_BIND_BOTTOM;
 CHECK(exchange(CD_SETBINDINGPREFS,io->io_Unit,&got,sizeof(got),0));
 CHECK(key(RAWKEY_F1,IEQUALIFIER_REPEAT));CHECK(prop->VertPot==heldPot);CHECK(bytes(""));
 CHECK(key(RAWKEY_F1|IECODE_UP_PREFIX,0));CHECK(bytes(""));
 CHECK(key(RAWKEY_F1,0));CHECK(prop->VertPot==MAXPOT);CHECK(key(RAWKEY_F1|IECODE_UP_PREFIX,0));CHECK(bytes(""));
 CHECK(exchange(CD_SETBINDINGPREFS,io->io_Unit,&p,sizeof(p),0));
 CHECK(TAP(RAWKEY_F6));CHECK(prop->VertPot==MAXPOT);CHECK(TAP(RAWKEY_F4));CHECK(prop->VertPot==MAXPOT);
 CHECK(put("\x1b[?1049hAlternate screen must not navigate primary history."));
 CHECK(TAP(RAWKEY_F5));CHECK(TAP(RAWKEY_F3));CHECK(put("\x1b[?1049l"));CHECK(prop->VertPot==MAXPOT);
 puts("HISTORY_BINDINGS_PASS line/page/top/bottom bounds, held repeat and remapping, alternate isolation, no input leakage");fflush(stdout);
 CHECK(put("\x1b[=0u\x1b[2J\x1b[HShortcut bindings: PASS\r\n\r\nCopy remapped to Ctrl+Shift+X\r\nSearch remapped to Ctrl+Shift+S\r\nF1-F6 history navigation and bounds also pass.\r\nHeld keys and releases stay out of application input.\r\nInvalid profiles rejected; defaults and units isolated.\r\n"));
 printf("BINDINGS_NATIVE_PASS count=%d\n",count);fflush(stdout);Delay(600);rc=0;
out:
 if(changed&&request)exchange(CD_SETDEFAULTBINDINGPREFS,(void*)CONU_LIBRARY,&defaults,sizeof(defaults),0);
 if(request)DeleteIORequest((struct IORequest*)request);
 if(port)DeleteMsgPort(port);
 if(stream){SetMode(stream,FALSE);Close(stream);}printf("BINDINGS_SCRIPT rc=%d\n",rc);return rc;
}
