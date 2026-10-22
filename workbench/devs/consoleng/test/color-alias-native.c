/* Distinct logical colors may initially share a physical screen pen. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <devices/conunit.h>
#include <stdio.h>
#include <string.h>
#include "consoleng.h"
static int req(struct IOStdReq *io,UWORD c,void *p,ULONG n){io->io_Command=c;io->io_Data=p;io->io_Length=n;return !DoIO((struct IORequest*)io)&&io->io_Actual==n;}
#define CHECK(x) do{if(!(x)){printf("COLOR_ALIAS_FAIL line=%d\n",__LINE__);goto out;}}while(0)
int main(void){
 int rc=20,opened=0;struct MsgPort *port=CreateMsgPort();struct IOStdReq *io=NULL;
 struct Window *w=NULL;CHECK(port);io=(void*)CreateIORequest(port,sizeof(*io));CHECK(io);
 w=OpenWindowTags(NULL,WA_Left,30,WA_Top,30,WA_Width,730,WA_Height,450,WA_Title,(IPTR)"ConsoleNG logical palette identity",WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_SizeGadget,TRUE,TAG_DONE);CHECK(w);
 io->io_Data=w;io->io_Length=sizeof(*w);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",CONU_CHARMAP,(struct IORequest*)io,0));opened=1;
 struct ConsolengColorPrefs p={sizeof(p),1,(1UL<<0)|(1UL<<1)|(1UL<<3),{0}};p.cp_RGB[0]=0xe5e5e5;p.cp_RGB[1]=p.cp_RGB[3]=0x203e4c;
 CHECK(req(io,CD_SETCOLORPREFS,&p,sizeof(p)));
 const char *s="\x1b[2J\x1b[H\x1b[49m    DEFAULT BACKGROUND\r\n\x1b[48;5;1m    PALETTE BACKGROUND\x1b[49m\r\n";
 CHECK(req(io,CMD_WRITE,(void*)s,strlen(s)));
 struct ConUnit *cu=(void*)io->io_Unit;
 LONG x=cu->cu_XROrigin+cu->cu_XRSize,y=cu->cu_YROrigin+cu->cu_YRSize/2;
 ULONG before0=ReadPixel(w->RPort,x,y),before1=ReadPixel(w->RPort,x,y+cu->cu_YRSize);
 CHECK(before0==before1);
 p.cp_RGB[3]=0xa63e7c;CHECK(req(io,CD_SETCOLORPREFS,&p,sizeof(p)));
 ULONG after0=ReadPixel(w->RPort,x,y),after1=ReadPixel(w->RPort,x,y+cu->cu_YRSize);
 printf("COLOR_ALIAS_PIXELS before=%lx/%lx after=%lx/%lx\n",(unsigned long)before0,(unsigned long)before1,(unsigned long)after0,(unsigned long)after1);
 puts("COLOR_ALIAS_READY");fflush(stdout);Delay(50*15);
 CHECK(after0==before0&&after1!=before1);
 puts("COLOR_ALIAS_PASS");rc=0;
out:
 if(opened)CloseDevice((struct IORequest*)io);
 if(w)CloseWindow(w);
 if(io)DeleteIORequest((struct IORequest*)io);
 if(port)DeleteMsgPort(port);
 printf("COLOR_ALIAS_SCRIPT rc=%d\n",rc);return rc;
}
