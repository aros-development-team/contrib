/* Queued events, or KEYBOARD mode with external QEMU sendkey input,
 * through RawKeyConvert and the DOS console raw stream. */
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
#define CHECK(c) do{if(!(c)){printf("MODNAV_FAIL line=%d utf=%d app=%d key=%d mod=%d\n",__LINE__,utf,app,key,mod);fflush(stdout);Delay(500);goto out;}}while(0)
int main(int argc,char **argv){int keyboard=argc>1&&!strcmp(argv[1],"KEYBOARD");int rc=20,utf=0,app=0,key=0,mod=0,count=0;struct InfoData info;
 int keys[]={RAWKEY_UP,RAWKEY_DOWN,RAWKEY_RIGHT,RAWKEY_LEFT,RAWKEY_HOME,RAWKEY_END,RAWKEY_INSERT,RAWKEY_DELETE,RAWKEY_PAGEUP,RAWKEY_PAGEDOWN};
 stream=Open((CONST_STRPTR)"RGCON:",MODE_OLDFILE);CHECK(stream);
 struct FileHandle*fh=BADDR(stream);CHECK(DoPkt(fh->fh_Type,ACTION_DISK_INFO,(SIPTR)MKBADDR(&info),0,0,0,0));
 struct IOStdReq*io=(void*)(IPTR)info.id_InUse;CHECK(io&&io->io_Unit);struct ConsoleBase*base=(void*)io->io_Device;ConsoleDevice=io->io_Device;
 struct Window*w=CU(io->io_Unit)->cu_Window;CHECK(w);
 SetWindowTitles(w,(CONST_STRPTR)"ConsoleNG modified navigation / native DOS",(CONST_STRPTR)-1);
 MoveWindow(w,30-w->LeftEdge,30-w->TopEdge);SizeWindow(w,730-w->Width,450-w->Height);Delay(50);ActivateWindow(w);Delay(10);
 CHECK(SetMode(stream,TRUE));CHECK(put("\x1b[?25l\x1b]50;topaz,8\x07\x1b[38;2;230;230;230m\x1b[48;2;25;60;80m\x1b[2J\x1b[HTesting native modified navigation..."));
 for(utf=0;utf<2;utf++)for(app=0;app<2;app++){
 CHECK(put(utf?"\x1b%G":"\x1b%@"));CHECK(put(app?"\x1b[?1h":"\x1b[?1l"));
 for(key=0;key<10;key++)for(mod=0;mod<8;mod++){
 /* i8042 reserves Ctrl+Alt+Delete for cold reboot before console delivery. */
 if(keyboard&&keys[key]==RAWKEY_DELETE&&(mod&6)==6)continue;
 struct InputEvent e={0};e.ie_Class=IECLASS_RAWKEY;e.ie_Code=keys[key];
 e.ie_Qualifier=((mod&1)?IEQUALIFIER_LSHIFT:0)|((mod&2)?IEQUALIFIER_LALT:0)|((mod&4)?IEQUALIFIER_CONTROL:0);
 UBYTE expected[160],mapped[80],got[160];int n=0,len=MapRawKey(&e,mapped,80,NULL);CHECK(len>=0);
 if(utf&&mod){snprintf((char*)expected,sizeof(expected),"\033[%c;%d%c","1111112356"[key],mod+1,"ABCDHF~~~~"[key]);n=6;}
 else {
  if(app&&len==2&&mapped[0]==0x9b&&mapped[1]&&strchr("ABCDHF",mapped[1])){expected[0]=27;expected[1]='O';expected[2]=mapped[1];n=3;}
  else for(int i=0;i<len;i++){if(utf&&mapped[i]==0x9b){expected[n++]=27;expected[n++]='[';}else expected[n++]=mapped[i];}
 }
 if(keyboard){printf("MODNAV_READY case=%d key=%d mod=%d\n",count,key,mod);fflush(stdout);if(!n)Delay(30);}
 else {
 struct cdihMessage*m=AllocMem(sizeof(*m),MEMF_PUBLIC|MEMF_CLEAR);CHECK(m);
 m->msg.mn_Length=sizeof(*m);m->unit=(Object*)io->io_Unit;m->unitGeneration=ICU(io->io_Unit)->inputGeneration;m->ie=e;
 ObtainSemaphore(&base->consoleTaskLock);PutMsg(base->consIHData.inputPort,(struct Message*)m);ReleaseSemaphore(&base->consoleTaskLock);
 }
 for(int i=0;i<n;i++){CHECK(WaitForChar(stream,5000000));CHECK(Read(stream,got+i,1)==1);}
 if(n&&memcmp(got,expected,n)){printf("MODNAV_MISMATCH got=");for(int i=0;i<n;i++)printf("%02x",got[i]);printf(" expected=");for(int i=0;i<n;i++)printf("%02x",expected[i]);puts("");CHECK(0);}
 if(keyboard)Delay(8); /* Let QEMU release modifiers before the next case. */
 CHECK(!WaitForChar(stream,20000));count++;
 }
 printf("MODNAV_GROUP utf=%d app=%d PASS\n",utf,app);fflush(stdout);
 }
 CHECK(put(keyboard?"\x1b[?1l\x1b%G\x1b[2J\x1b[HPASS: 312 QEMU keyboard cases through native DOS.\r\nShift / Alt / Control navigation combinations pass.\r\nUTF-8 and legacy keymaps, normal/application modes.\r\nInput handler, modifier release and exact reads pass.\r\nCtrl+Alt+Delete reserved for reboot; queued test covers it.":"\x1b[?1l\x1b%G\x1b[2J\x1b[HPASS: 320 native queued navigation cases through DOS.\r\nShift / Alt / Control combinations: arrows, Home, End,\r\nInsert, Delete, Page Up and Page Down.\r\nUTF-8 modifiers match xterm; legacy keymap preserved.\r\nNormal and application cursor modes both pass."));
 printf("MODNAV_PASS cases=%d\n",count);fflush(stdout);Delay(600);rc=0;
 out:if(stream){SetMode(stream,FALSE);Close(stream);}printf("MODNAV_SCRIPT rc=%d\n",rc);return rc;
}
