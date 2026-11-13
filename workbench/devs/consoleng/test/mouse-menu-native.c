/* Native MenuStrip and explicit RMBTRAP ownership through real DOS input. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <devices/conunit.h>
#include <dos/dosextens.h>
#include <stdio.h>
#include <graphics/rastport.h>
#include <string.h>
static BPTR stream;
static int put(const char*s){return Write(stream,s,strlen(s))==(LONG)strlen(s);}
static int get(const UBYTE*want,int n){UBYTE got[64]={0};for(int i=0;i<n;i++)if(!WaitForChar(stream,15000000)||Read(stream,got+i,1)!=1){printf("CAPTURE_TIMEOUT bytes=%d expected=%d\n",i,n);return 0;}if(memcmp(got,want,n)){printf("CAPTURE_BYTES ");for(int i=0;i<n;i++)printf("%02x",got[i]);puts("");return 0;}return 1;}
static int report(int encoding,int button,int release,int x,int y){UBYTE want[40];int n;if(!encoding)n=snprintf((char*)want,sizeof(want),"\x1b[<%d;%d;%d%c",button,x+1,y+1,release?'m':'M');else{want[0]=27;want[1]='[';want[2]='M';want[3]=release?35:32+button;want[4]=x+33;want[5]=y+33;n=6;}return get(want,n);}
#define CHECK(x) do{if(!(x)){printf("MENU_OWNER_FAIL line=%d phase=%d\n",__LINE__,phase);fflush(stdout);Delay(150);goto out;}}while(0)

static void trap(struct Window *w,BOOL enabled){ULONG lock=LockIBase(0);if(enabled)w->Flags|=WFLG_RMBTRAP;else w->Flags&=~WFLG_RMBTRAP;UnlockIBase(lock);}
int main(void){int rc=20,phase=0,installed=0;ULONG originalTrap=0;struct InfoData info;struct Window*w=NULL;
 struct IntuiText label={1,0,JAM1,0,0,NULL,(UBYTE*)"Menu item",NULL};
 struct MenuItem item={NULL,0,0,120,14,ITEMTEXT|ITEMENABLED|HIGHCOMP,0,&label,NULL,0,NULL,0};
 struct Menu menu={NULL,0,0,100,14,MENUENABLED,(BYTE*)"Native menu",&item,0,0,0,0};
 stream=Open((CONST_STRPTR)"RGCON:",MODE_OLDFILE);CHECK(stream);struct FileHandle*fh=BADDR(stream);CHECK(DoPkt(fh->fh_Type,ACTION_DISK_INFO,(SIPTR)MKBADDR(&info),0,0,0,0));struct IOStdReq*io=(void*)(IPTR)info.id_InUse;CHECK(io&&io->io_Unit);struct ConUnit*u=(void*)io->io_Unit;w=u->cu_Window;CHECK(w&&!w->MenuStrip);originalTrap=w->Flags&WFLG_RMBTRAP;
 ActivateWindow(w);SetWindowTitles(w,(CONST_STRPTR)"ConsoleNG native menu ownership",(CONST_STRPTR)-1);MoveWindow(w,30-w->LeftEdge,35-w->TopEdge);SizeWindow(w,720-w->Width,430-w->Height);Delay(25);CHECK(SetMode(stream,TRUE));CHECK(put("\x1b]50;topaz,8\7\x1b[?25l\x1b[2J\x1b[HNative menus and terminal mouse ownership\r\n"));
 for(phase=0;phase<12;phase++){
  int mode=phase/3,encoding=phase%3,native=mode==1;
  if(installed){ClearMenuStrip(w);installed=0;}
  trap(w,mode==2);
  if(mode==1||mode==2){CHECK(SetMenuStrip(w,&menu));installed=1;}
  CHECK(put("\x1b[?1000h\x1b[?1005;1006l"));CHECK(put(encoding==0?"\x1b[?1006h":encoding==1?"\x1b[?1005h":""));
  int x=w->LeftEdge+u->cu_XROrigin+4*u->cu_XRSize+u->cu_XRSize/2,y=w->TopEdge+u->cu_YROrigin+3*u->cu_YRSize+u->cu_YRSize/2;
  printf("MENU_PRESS phase=%d x=%d y=%d native=%d\n",phase,x,y,native);fflush(stdout);
  if(native){Delay(100);printf("MENU_STATE flags=%lx strip=%d\n",(unsigned long)w->Flags,w->MenuStrip==&menu);fflush(stdout);if(WaitForChar(stream,200000)){UBYTE byte;printf("MENU_UNEXPECTED");while(WaitForChar(stream,10000)&&Read(stream,&byte,1)==1)printf(" %02x",byte);puts("");fflush(stdout);CHECK(0);}CHECK(w->MenuStrip==&menu);CHECK(!(w->Flags&WFLG_RMBTRAP));}
  else CHECK(report(encoding,2,0,4,3));
  printf("MENU_RELEASE phase=%d x=%d y=%d native=%d\n",phase,x,y,native);fflush(stdout);
  if(native){Delay(100);CHECK(!WaitForChar(stream,200000));}else CHECK(report(encoding,2,1,4,3));
  CHECK(put("\x1b[5n"));CHECK(get((const UBYTE*)"\x1b[0n",4));
  CHECK(!!(w->Flags&WFLG_RMBTRAP)==(mode==2));
  printf("MENU_PHASE_PASS phase=%d\n",phase);fflush(stdout);
 }
 if(installed){ClearMenuStrip(w);installed=0;}trap(w,!!originalTrap);
 CHECK(put("\x1b[?1000;1005;1006l\x1b[2J\x1b[HPASS: native menu and terminal ownership\r\n\r\nNative menus retain right-button events.\r\nExplicit RMBTRAP routes right clicks to the application.\r\nRemoving the menu restores ordinary terminal reporting.\r\nSGR, UTF-8 and legacy bytes checked through DOS.\r\n"));puts("MENU_OWNER_PASS cases=12");fflush(stdout);Delay(150);rc=0;
out:
 if(installed)ClearMenuStrip(w);
 if(w)trap(w,!!originalTrap);
 if(stream){put("\x1b[?1000;1005;1006l");SetMode(stream,FALSE);Close(stream);}
 printf("MENU_OWNER_SCRIPT rc=%d\n",rc);return rc;
}
