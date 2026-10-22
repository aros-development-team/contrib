/* Real DOS input acceptance for DEC1005 and encoding negotiation. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <devices/conunit.h>
#include <dos/dosextens.h>
#include <stdio.h>
#include <string.h>
static BPTR stream;
static int put(const char *s){return Write(stream,s,strlen(s))==(LONG)strlen(s);}
static int get(const UBYTE *expected,int length){UBYTE got[64]={0};if(length<1||length>(int)sizeof(got))return 0;for(int i=0;i<length;i++)if(!WaitForChar(stream,15000000)||Read(stream,got+i,1)!=1){puts("UTF8MOUSE_TIMEOUT");return 0;}if(memcmp(got,expected,length)){printf("UTF8MOUSE_BYTES length=%d got=",length);for(int i=0;i<length;i++)printf("%02x",got[i]);puts("");return 0;}return 1;}
static int mode(int which,int enabled){char query[32],expected[32];snprintf(query,sizeof(query),"\x1b[?%d$p",which);snprintf(expected,sizeof(expected),"\x1b[?%d;%d$y",which,enabled?1:2);return put(query)&&get((UBYTE*)expected,strlen(expected));}
#define CHECK(c) do{if(!(c)){printf("UTF8MOUSE_FAIL line=%d phase=%d\n",__LINE__,phase);fflush(stdout);Delay(500);goto out;}}while(0)
int main(void){int rc=20,phase=0;struct InfoData info;
 stream=Open((CONST_STRPTR)"RGCON:",MODE_OLDFILE);CHECK(stream);
 struct FileHandle*fh=BADDR(stream);CHECK(DoPkt(fh->fh_Type,ACTION_DISK_INFO,(SIPTR)MKBADDR(&info),0,0,0,0));
 struct IOStdReq*io=(void*)(IPTR)info.id_InUse;CHECK(io&&io->io_Unit);struct ConUnit*u=(void*)io->io_Unit;struct Window*w=u->cu_Window;CHECK(w);
 MoveWindow(w,-w->LeftEdge,25-w->TopEdge);Delay(10);SizeWindow(w,800-w->Width,420-w->Height);Delay(40);
 CHECK(SetMode(stream,TRUE));CHECK(put("\x1b]50;topaz,8\x07\x1b[?25l\x1b[2J\x1b[HUTF-8 mouse: native DOS handler acceptance\r\n"));CHECK(u->cu_XMax>=95);
 SetWindowTitles(w,(CONST_STRPTR)"ConsoleNG UTF-8 mouse encoding",(CONST_STRPTR)-1);
 const char*commands[]={"\x1b[?1000;1005h","","\x1b[?1006h","\x1b[?1006;1005h","\x1b[?1005;1006h","\x1b[?1006l","\x1b[?1005h\x1b[?1006l","\x1b[?1005l"};
 for(phase=0;phase<8;phase++){
  int utf8=phase==0||phase==1||phase==3||phase==6,sgr=phase==2||phase==4,col=phase?95:94;
  CHECK(put(commands[phase]));CHECK(mode(1005,utf8));CHECK(mode(1006,sgr));
  int x=w->LeftEdge+u->cu_XROrigin+col*u->cu_XRSize+u->cu_XRSize/2;
  int y=w->TopEdge+u->cu_YROrigin+3*u->cu_YRSize+u->cu_YRSize/2;
  printf("UTF8MOUSE_CLICK phase=%d x=%d y=%d\n",phase,x,y);fflush(stdout);
  UBYTE expected[64];int n=0;
  if(sgr)n=snprintf((char*)expected,sizeof(expected),"\x1b[<0;%d;4M\x1b[<0;%d;4m",col+1,col+1);
  else for(int release=0;release<2;release++){
   expected[n++]=27;expected[n++]='[';expected[n++]='M';expected[n++]=release?35:32;
   if(utf8&&col==95){expected[n++]=0xc2;expected[n++]=0x80;}else expected[n++]=col+33;
   expected[n++]=36;
  }
  CHECK(get(expected,n));CHECK(put("\x1b[5n"));CHECK(get((const UBYTE*)"\x1b[0n",4));
  char line[96];snprintf(line,sizeof(line),"PASS phase %d: %s column %d, click/release and mode queries\r\n",phase,utf8?"UTF-8":sgr?"SGR":"legacy",col+1);CHECK(put(line));
  printf("UTF8MOUSE_PHASE_PASS phase=%d\n",phase);fflush(stdout);
 }
 CHECK(put("\x1b[?1005h\x1b" "c"));CHECK(mode(1005,0));CHECK(mode(1006,0));
 CHECK(put("\x1b[?25l\x1b[HPASS: eight real DOS mouse click/release cases.\r\nUTF-8 coordinate boundary 94/95, SGR/legacy switching,\r\ncombined mode ordering, independent reset and RIS verified.\r\nAll mode queries and DSR sentinels matched exactly.\r\n"));
 puts("UTF8MOUSE_PASS cases=8");fflush(stdout);Delay(600);rc=0;
out:if(stream){put("\x1b[?1000;1002;1003;1005;1006l\x1b[?25h");SetMode(stream,FALSE);Close(stream);}printf("UTF8MOUSE_SCRIPT rc=%d\n",rc);return rc;
}
