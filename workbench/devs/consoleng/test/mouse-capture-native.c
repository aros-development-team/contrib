/* Press in terminal content, release beyond each window edge through real DOS. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <devices/conunit.h>
#include <dos/dosextens.h>
#include <stdio.h>
#include <string.h>
static BPTR stream;
static int put(const char*s){return Write(stream,s,strlen(s))==(LONG)strlen(s);}
static int get(const UBYTE*want,int n){UBYTE got[64]={0};for(int i=0;i<n;i++)if(!WaitForChar(stream,15000000)||Read(stream,got+i,1)!=1){printf("CAPTURE_TIMEOUT bytes=%d expected=%d\n",i,n);return 0;}if(memcmp(got,want,n)){printf("CAPTURE_BYTES ");for(int i=0;i<n;i++)printf("%02x",got[i]);puts("");return 0;}return 1;}
static int report(int encoding,int button,int release,int x,int y){UBYTE want[40];int n;if(!encoding)n=snprintf((char*)want,sizeof(want),"\x1b[<%d;%d;%d%c",button,x+1,y+1,release?'m':'M');else{want[0]=27;want[1]='[';want[2]='M';want[3]=release?35:32+button;want[4]=x+33;want[5]=y+33;n=6;}return get(want,n);}
#define CHECK(x) do{if(!(x)){printf("CAPTURE_FAIL line=%d phase=%d\n",__LINE__,phase);fflush(stdout);Delay(150);goto out;}}while(0)
int main(int argc,char **argv){int rc=20,phase=0;int phases=argc==2&&!strcmp(argv[1],"all-buttons")?36:12;struct InfoData info;
 stream=Open((CONST_STRPTR)"RGCON:",MODE_OLDFILE);CHECK(stream);struct FileHandle*fh=BADDR(stream);CHECK(DoPkt(fh->fh_Type,ACTION_DISK_INFO,(SIPTR)MKBADDR(&info),0,0,0,0));struct IOStdReq*io=(void*)(IPTR)info.id_InUse;CHECK(io&&io->io_Unit);struct ConUnit*u=(void*)io->io_Unit;struct Window*w=u->cu_Window;CHECK(w);ActivateWindow(w);SetWindowTitles(w,(CONST_STRPTR)"ConsoleNG outside-window mouse release",(CONST_STRPTR)-1);
 MoveWindow(w,30-w->LeftEdge,35-w->TopEdge);SizeWindow(w,720-w->Width,430-w->Height);Delay(25);CHECK(SetMode(stream,TRUE));CHECK(put("\x1b]50;topaz,8\x07\x1b[?25l\x1b[2J\x1b[HOutside-window release acceptance\r\n"));CHECK(u->cu_XMax<95&&u->cu_YMax<95);
 for(phase=0;phase<phases;phase++){
 int edge=phase%4,encoding=(phase%12)/4,button=phase/12;CHECK(put("\x1b[?1000h\x1b[?1005;1006l"));CHECK(put(encoding==0?"\x1b[?1006h":encoding==1?"\x1b[?1005h":""));
 int sx=w->LeftEdge+u->cu_XROrigin+4*u->cu_XRSize+u->cu_XRSize/2,sy=w->TopEdge+u->cu_YROrigin+3*u->cu_YRSize+u->cu_YRSize/2;
 int tx=edge==0?w->LeftEdge-5:edge==1?w->LeftEdge+w->Width+5:sx;
 int ty=edge==2?w->TopEdge-5:edge==3?w->TopEdge+w->Height+5:sy;
 printf("CAPTURE_PRESS phase=%d x=%d y=%d button=%d\n",phase,sx,sy,button);fflush(stdout);CHECK(report(encoding,button,0,4,3));
 printf("CAPTURE_RELEASE phase=%d x=%d y=%d button=%d\n",phase,tx,ty,button);fflush(stdout);CHECK(report(encoding,button,1,edge==0?0:edge==1?u->cu_XMax:4,edge==2?0:edge==3?u->cu_YMax:3));
 CHECK(put("\x1b[5n"));CHECK(get((const UBYTE*)"\x1b[0n",4));printf("CAPTURE_PHASE_PASS phase=%d\n",phase);fflush(stdout);
 }
 CHECK(put("\x1b[?1000;1005;1006l\x1b[2J\x1b[HPASS: mouse release survives leaving the window.\r\nAll four edges; SGR, UTF-8 and legacy encoding.\r\nRelease coordinates clamp to the nearest terminal cell.\r\nExact DOS bytes and DSR sentinels verified.\r\n"));printf("CAPTURE_PASS cases=%d\n",phases);fflush(stdout);Delay(150);rc=0;
out:if(stream){put("\x1b[?1000;1005;1006l");SetMode(stream,FALSE);Close(stream);}printf("CAPTURE_SCRIPT rc=%d\n",rc);return rc;
}
