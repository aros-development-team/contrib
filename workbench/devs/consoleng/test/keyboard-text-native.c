/* Opt-in disambiguated key events through native DOS console streams. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/keymap.h>
#include <devices/rawkeycodes.h>
#include <devices/keymap.h>
#include <dos/dosextens.h>
#include <exec/memory.h>
#include <stdio.h>
#include <string.h>
#include "../console_gcc.h"
static BPTR stream;
static struct KeyMap *savedMap;
struct FixtureMap {struct KeyMap map;UBYTE types[64];IPTR values[64];UBYTE text[82];};
struct Device *ConsoleDevice;
static int put(const char*s){return Write(stream,s,strlen(s))==(LONG)strlen(s);}
static int expectbytes(const char*s,int n){UBYTE got[400];int used=0;
 for(int i=0;i<n;i++){if(!WaitForChar(stream,5000000)||Read(stream,got+used,1)!=1)break;used++;}
 if(used==n&&!memcmp(got,s,n)&&!WaitForChar(stream,20000))return 1;
 while(used<400&&WaitForChar(stream,20000)&&Read(stream,got+used,1)==1)used++;
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
 int flagsets[]={16,17,18,20,24,27,31};
 for(int f=0;f<7;f++)for(utf=0;utf<2;utf++)for(app=0;app<2;app++){
 flags=flagsets[f];char set[20];snprintf(set,sizeof(set),"\x1b[=%du",flags);CHECK(put(set));CHECK(query(flags));
 CHECK(put(utf?"\x1b%G":"\x1b%@"));CHECK(put(app?"\x1b[?1h":"\x1b[?1l"));
 for(key=0;key<10;key++)for(mod=0;mod<8;mod++){
 struct InputEvent e={0};e.ie_Class=IECLASS_RAWKEY;e.ie_Code=keys[key];e.ie_Qualifier=((mod&1)?IEQUALIFIER_LSHIFT:0)|((mod&2)?IEQUALIFIER_LALT:0)|((mod&4)?IEQUALIFIER_CONTROL:0);

 for(int type=0;type<3;type++){
 char expected[400]="",payload[20],mods[8]="",event[8]="",text[330]="";int expectedLength=0;
 int all=flags&8,disambig=flags&1,events=flags&2,fallback=0,suppressed=0;
 e.ie_Code=keys[key]|(type==2?IECODE_UP_PREFIX:0);
 e.ie_Qualifier=((mod&1)?IEQUALIFIER_LSHIFT:0)|((mod&2)?IEQUALIFIER_LALT:0)|((mod&4)?IEQUALIFIER_CONTROL:0)|(type==1?IEQUALIFIER_REPEAT:0);
 if(!(flags&11))fallback=1;
 else if(type==2&&!events)suppressed=1;
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
  /* No active encoding flag: preserve the existing UTF8 modified-function
   * and application-cursor routes before comparing native fallback bytes. */
  if(utf&&mod&&type!=2&&(key==8||key==9))
   expectedLength=snprintf(expected,sizeof(expected),"\x1b[1;%d%c",mod+1,key==8?'R':'A');
  else if(app&&((used==2&&(UBYTE)expected[0]==0x9b)||(used==3&&expected[0]==27&&expected[1]=='['))&&strchr("ABCDHF",expected[used-1])&&expected[used-1]){
   char final=expected[used-1];expected[0]=27;expected[1]='O';expected[2]=final;expectedLength=3;
  }
 }else if(!suppressed){
  int hasEvent=events&&type;
  if(values[key])snprintf(payload,sizeof(payload),"%d",values[key]);else strcpy(payload,(mod||hasEvent)?"1":"");
  if((flags&4)&&(mod&1)&&(key==4||key==5))snprintf(payload+strlen(payload),sizeof(payload)-strlen(payload),":%d",key==4?65:33);
  if((flags&16)&&type!=2&&(key==4||key==5||key==6)){
   struct InputEvent printable=e;UBYTE bytes[80];printable.ie_Qualifier&=~IEQUALIFIER_REPEAT;
   int n=MapRawKey(&printable,bytes,sizeof(bytes),NULL);
   if(n>0&&n<=80&&!(n==1&&(bytes[0]<32||(bytes[0]>=127&&bytes[0]<160)))){
    int at=0;for(int i=0;i<n;i++)at+=snprintf(text+at,sizeof(text)-at,"%c%u",i?':':';',bytes[i]);
   }
  }
  if(mod||hasEvent||text[0])snprintf(mods,sizeof(mods),";%d",mod+1);
  if(hasEvent)snprintf(event,sizeof(event),":%d",type+1);
  expectedLength=snprintf(expected,sizeof(expected),"\x1b[%s%s%s%s%c",payload,mods,event,text,key==8?'~':key==9?'A':'u');
 }
 struct cdihMessage*m=AllocMem(sizeof(*m),MEMF_PUBLIC|MEMF_CLEAR);CHECK(m);m->msg.mn_Length=sizeof(*m);m->unit=(Object*)io->io_Unit;m->unitGeneration=ICU(io->io_Unit)->inputGeneration;m->ie=e;
 ObtainSemaphore(&base->consoleTaskLock);PutMsg(base->consIHData.inputPort,(struct Message*)m);ReleaseSemaphore(&base->consoleTaskLock);
 if(!expectbytes(expected,expectedLength)){printf("KEYEVENT_MISMATCH flags=%d type=%d\n",flags,type);CHECK(0);}count++;
 }

 }
 printf("KEYEVENTS_GROUP flags=%d utf=%d app=%d count=%d PASS\n",flags,utf,app,count);fflush(stdout);
 }
 /* This fixture runs only in its disposable VM. Keep its public keymap
  * storage valid until VM teardown, since other tasks may borrow defaults. */
 savedMap=AskKeyMapDefault();CHECK(savedMap);
 struct FixtureMap *fm=AllocMem(sizeof(*fm),MEMF_PUBLIC|MEMF_CLEAR);CHECK(fm);
 fm->map=*savedMap;CopyMem(savedMap->km_LoKeyMapTypes,fm->types,sizeof(fm->types));CopyMem(savedMap->km_LoKeyMap,fm->values,sizeof(fm->values));
 fm->types[RAWKEY_E]=KCF_STRING;fm->text[0]=80;fm->text[1]=2;memset(fm->text+2,233,80);fm->values[RAWKEY_E]=(IPTR)fm->text;fm->map.km_LoKeyMapTypes=fm->types;fm->map.km_LoKeyMap=fm->values;
 SetKeyMapDefault(&fm->map);
 for(int longcase=0;longcase<3;longcase++){
 CHECK(put(longcase==0?"\x1b%G\x1b[=24u":longcase==1?"\x1b%@\x1b[=16u":"\x1b%G\x1b[=0u"));
 struct cdihMessage *longmsg=AllocMem(sizeof(*longmsg),MEMF_PUBLIC|MEMF_CLEAR);CHECK(longmsg);
 longmsg->msg.mn_Length=sizeof(*longmsg);longmsg->unit=(Object*)io->io_Unit;longmsg->unitGeneration=ICU(io->io_Unit)->inputGeneration;longmsg->ie.ie_Class=IECLASS_RAWKEY;longmsg->ie.ie_Code=RAWKEY_E;
 ObtainSemaphore(&base->consoleTaskLock);PutMsg(base->consIHData.inputPort,(struct Message*)longmsg);ReleaseSemaphore(&base->consoleTaskLock);
 char longtext[352]="\x1b[0;1";int length=0;
 if(longcase==0){for(int i=0;i<80;i++)strcat(longtext,i?":233":";233");strcat(longtext,"u");length=strlen(longtext);}
 else if(longcase==1){memset(longtext,233,80);length=80;}
 else {for(int i=0;i<80;i++){longtext[i*2]=0xc3;longtext[i*2+1]=0xa9;}length=160;}
 CHECK(expectbytes(longtext,length));count++;
 printf("KEYTEXT_LONG_PASS case=%d bytes=%d\n",longcase,length);fflush(stdout);
 }
 SetKeyMapDefault(savedMap);savedMap=NULL;
 CHECK(put("\x1b" "c"));CHECK(query(0));count++;
 CHECK(put("\x1b%G\x1b[?25l\x1b[2J\x1b[HPASS: keyboard press/repeat/release through native DOS.\r\n\r\nEscape / Tab / Return / Backspace / text / keypad / F3 / Up\r\nShift / Alt / Control; press/repeat/release\r\nClassic and UTF8; normal and application cursor modes\r\nPrimary/alternate stacks; split requests; reset\r\n\r\nAll five flags + exact 326-byte associated-text frame.\r\n"));printf("KEYEVENTS_NATIVE_PASS count=%d\n",count);fflush(stdout);Delay(600);rc=0;
out:
 if(savedMap)SetKeyMapDefault(savedMap);
 if(stream){SetMode(stream,FALSE);Close(stream);}printf("KEYEVENTS_SCRIPT rc=%d\n",rc);return rc;
}
