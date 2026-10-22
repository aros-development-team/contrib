/* Native focus negotiation through DOS Read/Write on an isolated RGCON mount. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <dos/dosextens.h>
#include <devices/conunit.h>
#include <stdio.h>
#include <string.h>
static BPTR stream;
static int put(const char *s){return Write(stream,s,strlen(s))==(LONG)strlen(s);}
static int focus(char final){
 unsigned char b[3];
 for(int n=0;n<3;n++)if(!WaitForChar(stream,10000000)||Read(stream,b+n,1)!=1)return 0;
 printf("FOCUS_BYTES %02x %02x %02x\n",b[0],b[1],b[2]);
 return b[0]==27&&b[1]=='['&&b[2]==final;
}
#define CHECK(c) do{if(!(c)){printf("FOCUS_FAIL line=%d\n",__LINE__);goto out;}}while(0)
int main(void){
 int rc=20;struct InfoData info;
 stream=Open((CONST_STRPTR)"RGCON:",MODE_OLDFILE);CHECK(stream);
 struct FileHandle *fh=BADDR(stream);CHECK(DoPkt(fh->fh_Type,ACTION_DISK_INFO,(SIPTR)MKBADDR(&info),0,0,0,0));
 struct IOStdReq *io=(void*)(IPTR)info.id_InUse;CHECK(io&&io->io_Unit);
 struct ConUnit *u=(void*)io->io_Unit;struct Window *w=u->cu_Window;CHECK(w);
 CHECK(SetMode(stream,TRUE));
 CHECK(put("\x1b%G\x1b]50;topaz,8\x07\x1b[2J\x1b[HNative focus-report acceptance\r\n"));
 SetWindowTitles(w,(CONST_STRPTR)"ConsoleNG native focus reporting",(CONST_STRPTR)-1);
 Delay(50);CHECK(put("\x1b[?1004h"));
 for(int n=0;n<3;n++){
  printf("FOCUS_OUT round=%d\n",n);fflush(stdout);CHECK(focus('O'));
  printf("FOCUS_IN round=%d x=%d y=%d\n",n,w->LeftEdge+120,w->TopEdge+12);fflush(stdout);CHECK(focus('I'));
 }
 CHECK(put("\x1b[?1004l"));puts("FOCUS_DISABLED_OUT");fflush(stdout);
 CHECK(!WaitForChar(stream,4000000));
 printf("FOCUS_DISABLED_IN x=%d y=%d\n",w->LeftEdge+120,w->TopEdge+12);fflush(stdout);
 CHECK(!WaitForChar(stream,4000000));
 CHECK(put("PASS: three focus-out/in cycles; disabled mode is silent.\r\n"));
 puts("FOCUS_PASS");fflush(stdout);Delay(600);rc=0;
out:
 if(stream){put("\x1b[?1004l");SetMode(stream,FALSE);Close(stream);}
 printf("FOCUS_SCRIPT rc=%d\n",rc);return rc;
}
