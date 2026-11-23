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
static int expect(const char*s){char c;for(int i=0;s[i];i++){if(!WaitForChar(stream,5000000)||Read(stream,&c,1)!=1||(UBYTE)c!=(UBYTE)s[i])return 0;}return !WaitForChar(stream,20000);}
static int query(int flags){char expected[20];const char*s="\x1b[?u";for(int i=0;s[i];i++)if(Write(stream,s+i,1)!=1)return 0;snprintf(expected,sizeof(expected),"\x1b[?%du",flags);return expect(expected);}
#define CHECK(c) do{if(!(c)){printf("KEYBOARD_FAIL line=%d utf=%d app=%d key=%d mod=%d count=%d\n",__LINE__,utf,app,key,mod,count);fflush(stdout);goto out;}}while(0)
int main(void){int rc=20,utf=0,app=0,key=0,mod=0,count=0;struct InfoData info;
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
 for(utf=0;utf<2;utf++)for(app=0;app<2;app++){
 CHECK(put(utf?"\x1b%G":"\x1b%@"));CHECK(put(app?"\x1b[?1h":"\x1b[?1l"));
 for(key=0;key<10;key++)for(mod=0;mod<8;mod++){
 struct InputEvent e={0};e.ie_Class=IECLASS_RAWKEY;e.ie_Code=keys[key];e.ie_Qualifier=((mod&1)?IEQUALIFIER_LSHIFT:0)|((mod&2)?IEQUALIFIER_LALT:0)|((mod&4)?IEQUALIFIER_CONTROL:0);
 char expected[80],payload[20];
 if((key>=1&&key<=3&&!mod)||((key==4||key==5)&&mod<2)){
  UBYTE mapped[40];int n=MapRawKey(&e,mapped,40,NULL);CHECK(n>=0&&n<80);memcpy(expected,mapped,n);expected[n]=0;
 }else{
  if(values[key])snprintf(payload,sizeof(payload),"%d",values[key]);else strcpy(payload,mod?"1":"");
  if(mod)snprintf(expected,sizeof(expected),"\x1b[%s;%d%c",payload,mod+1,key==8?'~':key==9?'A':'u');else snprintf(expected,sizeof(expected),"\x1b[%s%c",payload,key==8?'~':key==9?'A':'u');
 }
 for(int release=0;release<2;release++){
  struct cdihMessage*m=AllocMem(sizeof(*m),MEMF_PUBLIC|MEMF_CLEAR);CHECK(m);m->msg.mn_Length=sizeof(*m);m->unit=(Object*)io->io_Unit;m->unitGeneration=ICU(io->io_Unit)->inputGeneration;m->ie=e;if(release)m->ie.ie_Code|=IECODE_UP_PREFIX;
  ObtainSemaphore(&base->consoleTaskLock);PutMsg(base->consIHData.inputPort,(struct Message*)m);ReleaseSemaphore(&base->consoleTaskLock);
  CHECK(expect(release?"":expected));count++;
 }
 }
 printf("KEYBOARD_GROUP utf=%d app=%d count=%d PASS\n",utf,app,count);fflush(stdout);
 }
 CHECK(put("\x1b" "c"));CHECK(query(0));count++;
 CHECK(put("\x1b%G\x1b[?25l\x1b[2J\x1b[HPASS: negotiated keyboard disambiguation through DOS.\r\n\r\nEscape / Tab / Return / Backspace / text / keypad / F3 / Up\r\nShift / Alt / Control; key presses and releases\r\nClassic and UTF8; normal and application cursor modes\r\nPrimary/alternate stacks; split requests; reset\r\n\r\nOnly implemented protocol flags are advertised.\r\n"));printf("KEYBOARD_NATIVE_PASS count=%d\n",count);fflush(stdout);Delay(600);rc=0;
out:
 if(stream){SetMode(stream,FALSE);Close(stream);}printf("KEYBOARD_SCRIPT rc=%d\n",rc);return rc;
}
