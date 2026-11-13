/* Real console task queue -> native DOS handler input, without global input. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/cybergraphics.h>
#include <dos/dosextens.h>
#include <devices/rawkeycodes.h>
#include <stdio.h>
#include <string.h>
#include "../console_gcc.h"
struct Device *ConsoleDevice;
struct Library *CyberGfxBase;
static struct ConsoleBase *base;static Object *unit;static BPTR stream;
static int put(const char*s){return Write(stream,s,strlen(s))==(LONG)strlen(s);}
static int expect(const char*s){for(;*s;s++){char b;if(!WaitForChar(stream,2000000)||Read(stream,&b,1)!=1||b!=*s)return 0;}return !WaitForChar(stream,20000);}
static int wheel(int down,int shift){struct cdihMessage*m=AllocMem(sizeof(*m),MEMF_PUBLIC|MEMF_CLEAR);if(!m)return 0;m->msg.mn_Length=sizeof(*m);m->unit=unit;m->unitGeneration=ICU(unit)->inputGeneration;m->ie.ie_Class=IECLASS_RAWKEY;m->ie.ie_Code=RAWKEY_NM_WHEEL_UP+down;m->ie.ie_Qualifier=shift?IEQUALIFIER_LSHIFT:0;m->mouseX=CU(unit)->cu_XROrigin+2;m->mouseY=CU(unit)->cu_YROrigin+2;if(CU(unit)->cu_Window->Flags&WFLG_GIMMEZEROZERO){m->mouseX+=CU(unit)->cu_Window->BorderLeft;m->mouseY+=CU(unit)->cu_Window->BorderTop;}ObtainSemaphore(&base->consoleTaskLock);PutMsg(base->consIHData.inputPort,(struct Message*)m);ReleaseSemaphore(&base->consoleTaskLock);Delay(3);return 1;}
static ULONG pixels(void){struct ConUnit*u=CU(unit);ULONG hash=2166136261UL;for(int y=0;y<3*u->cu_YRSize;y++)for(int x=0;x<20*u->cu_XRSize;x++)hash=(hash^ReadRGBPixel(u->cu_Window->RPort,u->cu_XROrigin+x,u->cu_YROrigin+y))*16777619UL;return hash;}
#define CHECK(c) do{if(!(c)){printf("ALTWHEEL_FAIL check=%s line=%d alt=%d enabled=%d shift=%d down=%d app=%d\n",#c,__LINE__,alt,enabled,shift,down,app);fflush(stdout);Delay(500);goto out;}}while(0)
int main(void){int rc=20,alt=0,enabled=0,shift=0,down=0,app=0,cases=0;struct InfoData info;
CyberGfxBase=OpenLibrary((CONST_STRPTR)"cybergraphics.library",0);CHECK(CyberGfxBase);
stream=Open((CONST_STRPTR)"RGCON:",MODE_OLDFILE);CHECK(stream);struct FileHandle*fh=BADDR(stream);CHECK(DoPkt(fh->fh_Type,ACTION_DISK_INFO,(SIPTR)MKBADDR(&info),0,0,0,0));struct IOStdReq*io=(void*)(IPTR)info.id_InUse;CHECK(io&&io->io_Unit);unit=(Object*)io->io_Unit;base=(void*)io->io_Device;ConsoleDevice=io->io_Device;CHECK(SetMode(stream,TRUE));CHECK(put("\x1b%G\x1b[?25l\x1b[?1007$p"));CHECK(expect("\x1b[?1007;2$y"));
CHECK(put("\x1b[?1007h"));for(int row=0;row<80;row++){char line[80];snprintf(line,sizeof(line),"History row %03d - scroll verification\r\n",row);CHECK(put(line));}Delay(5);ULONG original=pixels();printf("ALTWHEEL_SHOT before hash=%lx\n",(unsigned long)original);fflush(stdout);Delay(150);CHECK(wheel(0,0));CHECK(expect(""));printf("ALTWHEEL_SHOT up hash=%lx\n",(unsigned long)pixels());fflush(stdout);Delay(150);CHECK(pixels()!=original);CHECK(wheel(1,0));CHECK(expect(""));printf("ALTWHEEL_SHOT down hash=%lx\n",(unsigned long)pixels());fflush(stdout);Delay(150);CHECK(pixels()==original);puts("ALTWHEEL_HISTORY_PASS");
for(alt=0;alt<2;alt++)for(enabled=0;enabled<2;enabled++)for(shift=0;shift<2;shift++)for(down=0;down<4;down++)for(app=0;app<2;app++){
 CHECK(put(alt?"\x1b[?1049h":"\x1b[?1049l"));CHECK(put(enabled?"\x1b[?1007h":"\x1b[?1007l"));CHECK(put(app?"\x1b[?1h":"\x1b[?1l"));CHECK(put("\x1b[?1007$p"));CHECK(expect(enabled?"\x1b[?1007;1$y":"\x1b[?1007;2$y"));CHECK(wheel(down,shift));CHECK(expect(alt&&enabled&&!shift?(down==0?"\x1bOA":down==1?"\x1bOB":down==2?"\x1bOD":"\x1bOC"):""));cases++;
}
for(alt=0;alt<2;alt++)for(enabled=0;enabled<2;enabled++)for(shift=0;shift<2;shift++)for(down=0;down<4;down++)for(int protocol=0;protocol<2;protocol++)for(int mode=0;mode<3;mode++){
 char seq[64],reply[40];int modes[]={1000,1002,1003};CHECK(put(alt?"\x1b[?1049h":"\x1b[?1049l"));CHECK(put(enabled?"\x1b[?1007h":"\x1b[?1007l"));snprintf(seq,sizeof(seq),"\x1b[?%dh\x1b[?1006%c",modes[mode],protocol?'h':'l');CHECK(put(seq));CHECK(wheel(down,shift));
 if(protocol)snprintf(reply,sizeof(reply),"\x1b[<%d;1;1M",64+down);else{const char legacy[]={27,'[','M',96+down,33,33,0};memcpy(reply,legacy,sizeof(legacy));}
 CHECK(expect(shift?"":reply));CHECK(put("\x1b[?1000;1002;1003;1006l"));cases++;
}
CHECK(put("\x1b[?1007h\x1b" "c\x1b[?1007$p"));CHECK(expect("\x1b[?1007;2$y"));
CHECK(put("\x1b[?1049l\x1b]50;DejaVuSansMono,16\7\x1b[2J\x1b[HPASS: 256 four-direction wheel combinations + history pixels.\r\n\r\nMode 1007 set/reset and exact replies\r\nAlternate screen: wheel -> application arrows\r\nPrimary screen and Shift: native scrolling\r\nNormal/application cursor modes both covered\r\nRIS reset restores native scroll policy\r\n\r\nMouse protocols take priority; Shift overrides\r\nReal ConsoleNG input task -> DOS Read"));printf("ALTWHEEL_PASS cases=%d\n",cases);fflush(stdout);Delay(600);rc=0;
out:if(stream){put("\x1b[?1049;1007l");SetMode(stream,FALSE);Close(stream);}if(CyberGfxBase)CloseLibrary(CyberGfxBase);printf("ALTWHEEL_SCRIPT rc=%d\n",rc);return rc;}
