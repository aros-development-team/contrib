/* Native pointer -> retained URL -> independently loaded OpenURL command. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <devices/conunit.h>
#include <dos/dosextens.h>
#include <stdio.h>
#include <string.h>
static BPTR stream;
static int put(const char*s){return Write(stream,s,strlen(s))==(LONG)strlen(s);}
static int captured(const char*expected){char got[1024]={0};BPTR in=Open((CONST_STRPTR)"RAM:ConsoleNG-URL-Test",MODE_OLDFILE);if(!in)return 0;LONG n=Read(in,got,sizeof(got)-1);Close(in);return n==(LONG)strlen(expected)&&!memcmp(got,expected,n);}
#define CHECK(c) do{if(!(c)){printf("URL_FAIL line=%d phase=%d\n",__LINE__,phase);fflush(stdout);Delay(500);goto out;}}while(0)
int main(void){int rc=20,phase=0;struct InfoData info;
 stream=Open((CONST_STRPTR)"RGCON:",MODE_OLDFILE);CHECK(stream);struct FileHandle*fh=BADDR(stream);CHECK(DoPkt(fh->fh_Type,ACTION_DISK_INFO,(SIPTR)MKBADDR(&info),0,0,0,0));
 struct IOStdReq*io=(void*)(IPTR)info.id_InUse;CHECK(io&&io->io_Unit);struct ConUnit*u=(void*)io->io_Unit;struct Window*w=u->cu_Window;CHECK(w);CHECK(SetMode(stream,TRUE));
 SetWindowTitles(w,(CONST_STRPTR)"ConsoleNG native OpenURL links",(CONST_STRPTR)-1);
 CHECK(put("\x1b%G\x1b]50;topaz,8\x07\x1b[?25l"));
 char wrapped[256]="https://example.org/";memset(wrapped+strlen(wrapped),'a',100);strcat(wrapped,"?x=1&y=two*three");
 const char*urls[]={"https://example.org/path?x=1&y=two*three",wrapped,"mailto:demo@example.org?subject=AmiWest","https://example.org/\xf0\x9f\x98\x80/x"};
 for(phase=0;phase<4;phase++){
  DeleteFile((CONST_STRPTR)"RAM:ConsoleNG-URL-Test");
  CHECK(put("\x1b[2J\x1b[HCtrl+Shift+click opens URLs using native OpenURL.\r\n"));
  CHECK(put(phase==2?"(":""));CHECK(put(urls[phase]));CHECK(put(phase==2?").\r\n":"\r\n"));
  CHECK(put("SGR underline: \x1b[4m\xce\xa9 \xe4\xb8\xad \xe2\xa0\x81 \xf0\x9f\x98\x80\x1b[24m plain\r\n"));
  int column=phase==1?2:phase==3?20:12,row=phase==1?2:1;
  int x=w->LeftEdge+u->cu_XROrigin+column*u->cu_XRSize+u->cu_XRSize/2;
  int y=w->TopEdge+u->cu_YROrigin+row*u->cu_YRSize+u->cu_YRSize/2;
  printf("URL_CLICK phase=%d x=%d y=%d\n",phase,x,y);fflush(stdout);
  int found=0;for(int n=0;n<800;n++){if(captured(urls[phase])){found=1;break;}Delay(1);}CHECK(found);
  printf("URL_PHASE_PASS phase=%d exact_uri=PASS\n",phase);fflush(stdout);Delay(200);
 }
 CHECK(put("\r\nPASS: native OpenURL command received exact URI bytes.\r\nWeb, wrapped, mailto and wide-cell URLs passed.\r\nArgument * escaping preserved; no shell was involved.\r\n"));
 puts("URL_PASS cases=4");fflush(stdout);Delay(600);rc=0;
out:if(stream){put("\x1b[?25h");SetMode(stream,FALSE);Close(stream);}printf("URL_SCRIPT rc=%d\n",rc);return rc;}
