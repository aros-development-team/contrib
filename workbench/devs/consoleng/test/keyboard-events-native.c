/* Opt-in disambiguated key events through native DOS console streams. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/keymap.h>
#include <devices/rawkeycodes.h>
#include <dos/dosextens.h>
#include <exec/memory.h>
#include <stdio.h>
#include <string.h>
#include "../console_gcc.h"
static BPTR stream;
struct Device *ConsoleDevice;
static int put(const char*s){return Write(stream,s,strlen(s))==(LONG)strlen(s);}
static int expectbytes(const char*s,int n){UBYTE got[100];int used=0;
 for(int i=0;i<n;i++){if(!WaitForChar(stream,5000000)||Read(stream,got+used,1)!=1)break;used++;}
 if(used==n&&!memcmp(got,s,n)&&!WaitForChar(stream,20000))return 1;
 while(used<100&&WaitForChar(stream,20000)&&Read(stream,got+used,1)==1)used++;
 printf("KEYEVENT_BYTES expected=");for(int i=0;i<n;i++)printf("%02x",(UBYTE)s[i]);printf(" got=");for(int i=0;i<used;i++)printf("%02x",got[i]);puts("");fflush(stdout);return 0;
}
static int expect(const char*s){return expectbytes(s,strlen(s));}
static int query(int flags){char expected[20];const char*s="\x1b[?u";for(int i=0;s[i];i++)if(Write(stream,s+i,1)!=1)return 0;snprintf(expected,sizeof(expected),"\x1b[?%du",flags);return expect(expected);}
#define CHECK(c) do{if(!(c)){printf("KEYEVENTS_FAIL line=%d utf=%d app=%d key=%d mod=%d count=%d\n",__LINE__,utf,app,key,mod,count);fflush(stdout);goto out;}}while(0)
int main(void){int rc=20,utf=0,app=0,key=0,mod=0,count=0;int flags=0;struct InfoData info;
 int keys[]={RAWKEY_ESCAPE,RAWKEY_TAB,RAWKEY_RETURN,RAWKEY_BACKSPACE,RAWKEY_A,RAWKEY_1,RAWKEY_KP_0,RAWKEY_KP_ENTER,RAWKEY_F3,RAWKEY_UP};
 int values[]={27,9,13,127,97,49,57399,57414,13,0};
 stream=Open((CONST_STRPTR)"RGCON:",MODE_OLDFILE);CHECK(stream);
 struct FileHandle*fh=BADDR(stream);CHECK(DoPkt(fh->fh_Type,ACTION_DISK_INFO,(SIPTR)MKBADDR(&info),0,0,0,0));
 struct IOStdReq*io=(void*)(IPTR)info.id_InUse;CHECK(io&&io->io_Unit);struct ConsoleBase*base=(void*)io->io_Device;ConsoleDevice=io->io_Device;
 struct Window*w=CU(io->io_Unit)->cu_Window;CHECK(w);
 SetWindowTitles(w,(CONST_STRPTR)"ConsoleNG keyboard protocol / native DOS",(CONST_STRPTR)-1);
 MoveWindow(w,30-w->LeftEdge,30-w->TopEdge);SizeWindow(w,730-w->Width,450-w->Height);Delay(50);ActivateWindow(w);Delay(10);
 CHECK(SetMode(stream,TRUE));CHECK(put("\x1b[?25l\x1b]50;topaz,8\x07\x1b[38;2;230;230;230m\x1b[48;2;25;60;80m\x1b[2J\x1b[HTesting opt-in keyboard disambiguation..."));
 CHECK(query(0));
 /* Each split of push and pop plus unsupported flags and both screens. */
 for(int i=0;i<=5;i++){
  CHECK(Write(stream,"\x1b[>1u",i)==i);CHECK(put("\x1b[>1u"+i));CHECK(query(1));CHECK(put("\x1b[<u"));CHECK(query(0));count++;
 }
 CHECK(put("\x1b[>33u"));CHECK(query(1));CHECK(put("\x1b[?1049h"));CHECK(query(0));CHECK(put("\x1b[>1u\x1b[>0u"));CHECK(query(0));CHECK(put("\x1b[?1049l"));CHECK(query(1));CHECK(put("\x1b[?1049h\x1b[<u"));CHECK(query(1));CHECK(put("\x1b[?1049l"));CHECK(query(1));
 CHECK(put("\x1b[=1;3u"));CHECK(query(0));CHECK(put("\x1b[=1;2u"));CHECK(query(1));CHECK(put("\x1b[=1:2u\x1b[=65536u"));CHECK(query(1));count+=9;
 int flagsets[]={2,3,5,7,8,10,11,12,14,15};
 for(int f=0;f<10;f++)for(utf=0;utf<2;utf++)for(app=0;app<2;app++){
 flags=flagsets[f];char set[20];snprintf(set,sizeof(set),"\x1b[=%du",flags);CHECK(put(set));CHECK(query(flags));
 CHECK(put(utf?"\x1b%G":"\x1b%@"));CHECK(put(app?"\x1b[?1h":"\x1b[?1l"));
 for(key=0;key<10;key++)for(mod=0;mod<8;mod++){
 struct InputEvent e={0};e.ie_Class=IECLASS_RAWKEY;e.ie_Code=keys[key];e.ie_Qualifier=((mod&1)?IEQUALIFIER_LSHIFT:0)|((mod&2)?IEQUALIFIER_LALT:0)|((mod&4)?IEQUALIFIER_CONTROL:0);

 for(int type=0;type<3;type++){
 char expected[80]="",payload[20],mods[8]="",event[8]="";int expectedLength=0;
 int all=flags&8,disambig=flags&1,events=flags&2,fallback=0,suppressed=0;
 e.ie_Code=keys[key]|(type==2?IECODE_UP_PREFIX:0);
 e.ie_Qualifier=((mod&1)?IEQUALIFIER_LSHIFT:0)|((mod&2)?IEQUALIFIER_LALT:0)|((mod&4)?IEQUALIFIER_CONTROL:0)|(type==1?IEQUALIFIER_REPEAT:0);
 if(type==2&&!events)suppressed=1;
 else if(type==2&&key>=1&&key<=3&&!all)suppressed=1;
 else if(type==2&&key==7&&!all)suppressed=1;
 else if(type!=2&&!all){
  if(key==0&&!disambig)fallback=1;
  if(key>=1&&key<=3&&(!disambig||!mod))fallback=1;
  if((key==4||key==5)&&(!disambig||mod<2))fallback=1;
  if((key==6||key==7)&&!disambig)fallback=1;
 }
 if(fallback){
  UBYTE mapped[40];int n=MapRawKey(&e,mapped,40,NULL);CHECK(n>=0&&n<80);int used=0;
  for(int i=0;i<n;i++){if(utf&&mapped[i]==0x9b){expected[used++]=27;expected[used++]='[';}else if(utf&&mapped[i]>=128){expected[used++]=0xc0|(mapped[i]>>6);expected[used++]=0x80|(mapped[i]&63);}else expected[used++]=mapped[i];}expected[used]=0;expectedLength=used;
 }else if(!suppressed){
  int hasEvent=events&&type;
  if(values[key])snprintf(payload,sizeof(payload),"%d",values[key]);else strcpy(payload,(mod||hasEvent)?"1":"");
  if((flags&4)&&(mod&1)&&(key==4||key==5))snprintf(payload+strlen(payload),sizeof(payload)-strlen(payload),":%d",key==4?65:33);
  if(mod||hasEvent)snprintf(mods,sizeof(mods),";%d",mod+1);
  if(hasEvent)snprintf(event,sizeof(event),":%d",type+1);
  expectedLength=snprintf(expected,sizeof(expected),"\x1b[%s%s%s%c",payload,mods,event,key==8?'~':key==9?'A':'u');
 }
 struct cdihMessage*m=AllocMem(sizeof(*m),MEMF_PUBLIC|MEMF_CLEAR);CHECK(m);m->msg.mn_Length=sizeof(*m);m->unit=(Object*)io->io_Unit;m->unitGeneration=ICU(io->io_Unit)->inputGeneration;m->ie=e;
 ObtainSemaphore(&base->consoleTaskLock);PutMsg(base->consIHData.inputPort,(struct Message*)m);ReleaseSemaphore(&base->consoleTaskLock);
 if(!expectbytes(expected,expectedLength)){printf("KEYEVENT_MISMATCH flags=%d type=%d\n",flags,type);CHECK(0);}count++;
 }

 }
 printf("KEYEVENTS_GROUP flags=%d utf=%d app=%d count=%d PASS\n",flags,utf,app,count);fflush(stdout);
 }
 CHECK(put("\x1b" "c"));CHECK(query(0));count++;
 CHECK(put("\x1b%G\x1b[?25l\x1b[2J\x1b[HPASS: keyboard press/repeat/release through native DOS.\r\n\r\nEscape / Tab / Return / Backspace / text / keypad / F3 / Up\r\nShift / Alt / Control; press/repeat/release\r\nClassic and UTF8; normal and application cursor modes\r\nPrimary/alternate stacks; split requests; reset\r\n\r\nDisambiguation + event types + alternate codes + all-key mode.\r\n"));printf("KEYEVENTS_NATIVE_PASS count=%d\n",count);fflush(stdout);Delay(600);rc=0;
out:
 if(stream){SetMode(stream,FALSE);Close(stream);}printf("KEYEVENTS_SCRIPT rc=%d\n",rc);return rc;
}
