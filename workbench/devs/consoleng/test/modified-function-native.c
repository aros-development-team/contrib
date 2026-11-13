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
#define CHECK(c) do{if(!(c)){printf("MODFUNC_FAIL line=%d utf=%d app=%d key=%d mod=%d\n",__LINE__,utf,app,key,mod);fflush(stdout);Delay(500);goto out;}}while(0)
int main(int argc,char **argv){int keyboard=argc>1&&!strcmp(argv[1],"KEYBOARD");int rc=20,utf=0,app=0,key=0,mod=0,count=0;struct InfoData info;
 int keys[]={RAWKEY_F1,RAWKEY_F2,RAWKEY_F3,RAWKEY_F4,RAWKEY_F5,RAWKEY_F6,RAWKEY_F7,RAWKEY_F8,RAWKEY_F9,RAWKEY_F10,RAWKEY_F11,RAWKEY_F12};
 stream=Open((CONST_STRPTR)"RGCON:",MODE_OLDFILE);CHECK(stream);
 struct FileHandle*fh=BADDR(stream);CHECK(DoPkt(fh->fh_Type,ACTION_DISK_INFO,(SIPTR)MKBADDR(&info),0,0,0,0));
 struct IOStdReq*io=(void*)(IPTR)info.id_InUse;CHECK(io&&io->io_Unit);struct ConsoleBase*base=(void*)io->io_Device;ConsoleDevice=io->io_Device;
 struct Window*w=CU(io->io_Unit)->cu_Window;CHECK(w);
 SetWindowTitles(w,(CONST_STRPTR)"ConsoleNG modified function keys / native DOS",(CONST_STRPTR)-1);
 MoveWindow(w,30-w->LeftEdge,30-w->TopEdge);SizeWindow(w,730-w->Width,450-w->Height);Delay(50);ActivateWindow(w);Delay(10);
 CHECK(SetMode(stream,TRUE));CHECK(put("\x1b[?25l\x1b]50;topaz,8\x07\x1b[38;2;230;230;230m\x1b[48;2;25;60;80m\x1b[2J\x1b[HTesting native modified function keys..."));
 for(utf=0;utf<2;utf++)for(app=0;app<2;app++){
 CHECK(put(utf?"\x1b%G":"\x1b%@"));CHECK(put(app?"\x1b[?1h":"\x1b[?1l"));
 for(key=0;key<12;key++)for(mod=0;mod<8;mod++){
 /* PC i8042 maps F11 to Help and F12 to Right Amiga before delivery. */
 if(keyboard&&key>=10)continue;
 struct InputEvent e={0};e.ie_Class=IECLASS_RAWKEY;e.ie_Code=keys[key];
 e.ie_Qualifier=((mod&1)?IEQUALIFIER_LSHIFT:0)|((mod&2)?IEQUALIFIER_LALT:0)|((mod&4)?IEQUALIFIER_CONTROL:0);
 UBYTE expected[160],mapped[80],got[160];int n=0,len=MapRawKey(&e,mapped,80,NULL);CHECK(len>=0);
 if(utf&&mod){const char*bases[]={"1","1","1","1","15","17","18","19","20","21","23","24"};n=snprintf((char*)expected,sizeof(expected),"\033[%s;%d%c",bases[key],mod+1,"PQRS~~~~~~~~"[key]);}
 else {
  if(app&&len==2&&mapped[0]==0x9b&&mapped[1]&&strchr("ABCDHF",mapped[1])){expected[0]=27;expected[1]='O';expected[2]=mapped[1];n=3;}
  else for(int i=0;i<len;i++){if(utf&&mapped[i]==0x9b){expected[n++]=27;expected[n++]='[';}else expected[n++]=mapped[i];}
 }
 if(keyboard){printf("MODFUNC_READY case=%d key=%d mod=%d\n",count,key,mod);fflush(stdout);if(!n)Delay(30);}
 else {
 struct cdihMessage*m=AllocMem(sizeof(*m),MEMF_PUBLIC|MEMF_CLEAR);CHECK(m);
 m->msg.mn_Length=sizeof(*m);m->unit=(Object*)io->io_Unit;m->unitGeneration=ICU(io->io_Unit)->inputGeneration;m->ie=e;
 ObtainSemaphore(&base->consoleTaskLock);PutMsg(base->consIHData.inputPort,(struct Message*)m);ReleaseSemaphore(&base->consoleTaskLock);
 }
 for(int i=0;i<n;i++){CHECK(WaitForChar(stream,5000000));CHECK(Read(stream,got+i,1)==1);}
 if(n&&memcmp(got,expected,n)){printf("MODFUNC_MISMATCH got=");for(int i=0;i<n;i++)printf("%02x",got[i]);printf(" expected=");for(int i=0;i<n;i++)printf("%02x",expected[i]);puts("");CHECK(0);}
 if(keyboard)Delay(8); /* Let QEMU release modifiers before the next case. */
 CHECK(!WaitForChar(stream,20000));count++;
 }
 printf("MODFUNC_GROUP utf=%d app=%d PASS\n",utf,app);fflush(stdout);
 }
 CHECK(put(keyboard?"\x1b[?1l\x1b%G\x1b[2J\x1b[HPASS: 320 QEMU F1-F10 cases through DOS.\r\nShift / Alt / Control, UTF-8 and classic modes.\r\nExact reads; input.device and Intuition delivery.\r\nPC F11 = Help; F12 = Right Amiga.\r\nAll 12 native key codes pass the queued test.":"\x1b[?1l\x1b%G\x1b[2J\x1b[HPASS: 384 native F1-F12 cases through DOS.\r\nShift / Alt / Control combinations.\r\nUTF-8 and classic keymaps.\r\nNormal and application cursor modes.\r\nExact byte sequences and no surplus input."));
 printf("MODFUNC_PASS cases=%d\n",count);fflush(stdout);Delay(600);rc=0;
 out:if(stream){SetMode(stream,FALSE);Close(stream);}printf("MODFUNC_SCRIPT rc=%d\n",rc);return rc;
}
