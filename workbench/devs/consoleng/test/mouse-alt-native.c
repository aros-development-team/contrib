/* Real input.device Alt qualifiers through raw DOS mouse reporting.
 * Default: strict nine-case test. left-control: explicit six-case subset.
 * The subset is diagnostic and does not qualify right Alt.
 * left-control trace adds a passive bounded input trace, printed after removal. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/input.h>
#include <devices/inputevent.h>
#include <devices/input.h>
#include <exec/interrupts.h>
#include <aros/asmcall.h>
struct Device *InputBase;
#include <proto/intuition.h>
#include <devices/conunit.h>
#include <dos/dosextens.h>
#include <stdio.h>
#include <string.h>
struct TraceRecord { UWORD code,qualifier; UBYTE eventClass; ULONG seconds,micros; };
struct TraceState { struct TraceRecord records[128]; ULONG count,dropped; };
static struct TraceState traceState;
AROS_UFH2(struct InputEvent *, traceEvents,
 AROS_UFHA(struct InputEvent *, chain, A0),
 AROS_UFHA(struct TraceState *, state, A1))
{
 AROS_USERFUNC_INIT
 for(struct InputEvent *e=chain;e;e=e->ie_NextEvent) {
  if(e->ie_Class!=IECLASS_RAWKEY && e->ie_Class!=IECLASS_RAWMOUSE)continue;
  if(e->ie_Class==IECLASS_RAWMOUSE && e->ie_Code==IECODE_NOBUTTON)continue;
  if(state->count==128){state->dropped++;continue;}
  struct TraceRecord *r=&state->records[state->count++];
  r->code=e->ie_Code;r->qualifier=e->ie_Qualifier;r->eventClass=e->ie_Class;
  r->seconds=e->ie_TimeStamp.tv_secs;r->micros=e->ie_TimeStamp.tv_micro;
 }
 return chain;
 AROS_USERFUNC_EXIT
}
static BPTR stream;
static int put(const char*s){return Write(stream,s,strlen(s))==(LONG)strlen(s);}
static int get(const UBYTE*expected,int n){UBYTE got[64]={0};for(int i=0;i<n;i++)if(!WaitForChar(stream,5000000)||Read(stream,got+i,1)!=1)return 0;if(memcmp(got,expected,n)){printf("ALTMOUSE_BYTES ");for(int i=0;i<n;i++)printf("%02x",got[i]);puts("");return 0;}return 1;}
#define CHECK(x) do{if(!(x)){printf("ALTMOUSE_FAIL line=%d phase=%d\n",__LINE__,phase);fflush(stdout);goto out;}}while(0)
int main(int argc,char **argv){int rc=20,phase=0;int subset=argc>=2&&!strcmp(argv[1],"left-control");int trace=argc==3&&!strcmp(argv[2],"trace"),installed=0;struct Interrupt handler={0};if((argc>1&&!subset)||argc>3||(argc==3&&!trace))return 20;struct InfoData info;struct MsgPort*port=CreateMsgPort();struct IOStdReq*input=NULL;CHECK(port);input=(void*)CreateIORequest(port,sizeof(*input));CHECK(input);CHECK(!OpenDevice((CONST_STRPTR)"input.device",0,(struct IORequest*)input,0));InputBase=input->io_Device;
 if(trace){handler.is_Node.ln_Pri=100;handler.is_Node.ln_Name="ConsoleNG modifier trace";handler.is_Data=&traceState;handler.is_Code=(VOID_FUNC)AROS_ASMSYMNAME(traceEvents);input->io_Command=IND_ADDHANDLER;input->io_Data=&handler;CHECK(!DoIO((struct IORequest*)input));installed=1;}

 stream=Open((CONST_STRPTR)"RGCON:",MODE_OLDFILE);CHECK(stream);struct FileHandle*fh=BADDR(stream);CHECK(DoPkt(fh->fh_Type,ACTION_DISK_INFO,(SIPTR)MKBADDR(&info),0,0,0,0));
 struct IOStdReq*io=(void*)(IPTR)info.id_InUse;CHECK(io&&io->io_Unit);struct ConUnit*u=(void*)io->io_Unit;struct Window*w=u->cu_Window;CHECK(w);ActivateWindow(w);SetWindowTitles(w,(CONST_STRPTR)"ConsoleNG native Alt mouse modifiers",(CONST_STRPTR)-1);
 CHECK(SetMode(stream,TRUE));CHECK(put("\x1b]50;topaz,8\x07\x1b[?25l\x1b[2J\x1b[HNative Alt mouse modifier acceptance\r\n"));
 for(phase=0;phase<9;phase++){
  int encoding=phase/3,modifier=phase%3,button=modifier==2?24:8;
  if(subset&&modifier==1)continue;
  CHECK(put("\x1b[?1000h\x1b[?1005;1006l"));CHECK(put(encoding==0?"\x1b[?1006h":encoding==1?"\x1b[?1005h":""));
  int x=w->LeftEdge+u->cu_XROrigin+4*u->cu_XRSize+u->cu_XRSize/2,y=w->TopEdge+u->cu_YROrigin+3*u->cu_YRSize+u->cu_YRSize/2;
  UWORD mask=IEQUALIFIER_LALT|IEQUALIFIER_RALT|IEQUALIFIER_CONTROL;
  UWORD wanted=modifier==0?IEQUALIFIER_LALT:modifier==1?IEQUALIFIER_RALT:IEQUALIFIER_LALT|IEQUALIFIER_CONTROL;
  for(int wait=0;wait<500&&(PeekQualifier()&mask);wait++)Delay(1);
  CHECK(!(PeekQualifier()&mask));
  printf("ALTMOUSE_HOLD phase=%d modifier=%d\n",phase,modifier);fflush(stdout);
  for(int wait=0;wait<250&&(PeekQualifier()&mask)!=wanted;wait++)Delay(1);
  printf("ALTMOUSE_QUAL phase=%d got=%x wanted=%x\n",phase,PeekQualifier()&mask,wanted);fflush(stdout);
  CHECK((PeekQualifier()&mask)==wanted);
  printf("ALTMOUSE_CLICK phase=%d modifier=%d x=%d y=%d\n",phase,modifier,x,y);fflush(stdout);
  UBYTE expected[64];int n=0;
  if(encoding==0)n=snprintf((char*)expected,sizeof(expected),"\x1b[<%d;5;4M\x1b[<%d;5;4m",button,button);
  else for(int release=0;release<2;release++){expected[n++]=27;expected[n++]='[';expected[n++]='M';expected[n++]=32+(release?(button|3):button);expected[n++]=37;expected[n++]=36;}
  CHECK(get(expected,n));CHECK(put("\x1b[5n"));CHECK(get((UBYTE*)"\x1b[0n",4));printf("ALTMOUSE_PHASE_PASS phase=%d\n",phase);fflush(stdout);Delay(100);
 }
 CHECK(put("\x1b[?1000;1005;1006l\x1b[2J\x1b[H"));
 if(subset){CHECK(put("PASS: left Alt and Control+Alt through DOS.\r\nSix exact click/release cases: SGR, UTF-8 and legacy.\r\nRight Alt excluded: keyboard readiness remains unresolved.\r\n"));puts("ALTMOUSE_SUBSET_PASS cases=6");}
 else {CHECK(put("PASS: native Alt mouse modifiers through DOS.\r\nLeft Alt, right Alt, Control+Alt press and release.\r\nSGR, UTF-8 and legacy reports matched exact bytes.\r\nDSR sentinels confirm no extra application input.\r\n"));puts("ALTMOUSE_PASS cases=9");}fflush(stdout);Delay(150);rc=0;
out:if(installed){input->io_Command=IND_REMHANDLER;input->io_Data=&handler;if(DoIO((struct IORequest*)input)){puts("ALTMOUSE_TRACE_REMOVE_FAIL");fflush(stdout);for(;;)Delay(50);}installed=0;
 for(ULONG i=0;i<traceState.count;i++){struct TraceRecord*t=&traceState.records[i];printf("ALTMOUSE_TRACE class=%u code=%x qualifier=%x time=%lu.%06lu\n",t->eventClass,t->code,t->qualifier,(unsigned long)t->seconds,(unsigned long)t->micros);}
 printf("ALTMOUSE_TRACE_END events=%lu dropped=%lu\n",(unsigned long)traceState.count,(unsigned long)traceState.dropped);}
 if(stream){put("\x1b[?1000;1005;1006l");SetMode(stream,FALSE);Close(stream);}if(InputBase)CloseDevice((struct IORequest*)input);if(input)DeleteIORequest((struct IORequest*)input);if(port)DeleteMsgPort(port);printf("ALTMOUSE_SCRIPT rc=%d\n",rc);return rc;
}
