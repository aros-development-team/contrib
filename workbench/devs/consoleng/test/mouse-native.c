/* DOS handler acceptance for native mouse reporting; owns only RGCON mount. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <devices/conunit.h>
#include <dos/dosextens.h>
#include <stdio.h>
#include <string.h>
static BPTR stream;
static int put(const char *s){return Write(stream,s,strlen(s))==(LONG)strlen(s);}
static int get(unsigned char *out,int n){for(int i=0;i<n;i++){if(!WaitForChar(stream,10000000)||Read(stream,out+i,1)!=1)return 0;}return 1;}
#define CHECK(c) do{if(!(c)){printf("MOUSE_FAIL line=%d\n",__LINE__);goto out;}}while(0)
int main(void){
 int rc=20;struct InfoData info;
 stream=Open((CONST_STRPTR)"RGCON:",MODE_OLDFILE);CHECK(stream);
 struct FileHandle *fh=BADDR(stream);CHECK(DoPkt(fh->fh_Type,ACTION_DISK_INFO,(SIPTR)MKBADDR(&info),0,0,0,0));
 struct IOStdReq *io=(void*)(IPTR)info.id_InUse;CHECK(io&&io->io_Unit);
 struct ConUnit *u=(void*)io->io_Unit;struct Window *w=u->cu_Window;CHECK(w);
 CHECK(SetMode(stream,TRUE));CHECK(put("\x1b%G\x1b]50;topaz,8\x07\x1b[2J\x1b[HNative DOS mouse report test\r\n"));
 SetWindowTitles(w,(CONST_STRPTR)"ConsoleNG native mouse protocols",(CONST_STRPTR)-1);
 for(int phase=0;phase<2;phase++){
  CHECK(put(phase==0?"\x1b[?1000;1006h":"\x1b[?1006l"));
  int x=w->LeftEdge+u->cu_XROrigin+4*u->cu_XRSize+u->cu_XRSize/2;
  int y=w->TopEdge+u->cu_YROrigin+3*u->cu_YRSize+u->cu_YRSize/2;
  printf("MOUSE_CLICK phase=%d x=%d y=%d\n",phase,x,y);fflush(stdout);
  unsigned char report[32]={0};
  if(phase==0){
   const char *expected="\x1b[<0;5;4M\x1b[<0;5;4m";
   CHECK(get(report,strlen(expected)));CHECK(!memcmp(report,expected,strlen(expected)));
  }else{
   const unsigned char expected[]={27,'[','M',32,37,36,27,'[','M',35,37,36};
   CHECK(get(report,sizeof(expected)));CHECK(!memcmp(report,expected,sizeof(expected)));
  }
  printf("MOUSE_PHASE_PASS phase=%d\n",phase);fflush(stdout);
 }
 CHECK(put("\x1b[?1000l\r\nPASS: SGR and legacy click/release through DOS Read.\r\n"));
 puts("MOUSE_PASS");fflush(stdout);Delay(600);rc=0;
out:
 if(stream){put("\x1b[?1000;1002;1003;1006l");SetMode(stream,FALSE);Close(stream);}
 printf("MOUSE_SCRIPT rc=%d\n",rc);return rc;
}
