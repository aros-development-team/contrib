/* Native mouse-selection -> RAmiga-C -> GetConSnip integration fixture. */
#include <aros/asmcall.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/console.h>
#include <utility/hooks.h>
#include <devices/conunit.h>
#include <stdio.h>
#include <string.h>
struct Device *ConsoleDevice;
AROS_UFH3S(ULONG, sink, AROS_UFHA(struct Hook *,h,A0), AROS_UFHA(APTR,o,A2), AROS_UFHA(APTR,m,A1)){
 AROS_USERFUNC_INIT
 return 0;
 AROS_USERFUNC_EXIT
}
static int put(struct IOStdReq *io,const char *s){io->io_Command=CMD_WRITE;io->io_Data=(void*)s;io->io_Length=strlen(s);return !DoIO((struct IORequest*)io);}
int main(int argc,char **argv){
 int rc=20,opened=0,hooked=0;struct Window *w=NULL;struct Hook hook={0};
 struct MsgPort *p=CreateMsgPort();struct IOStdReq *io=p?(void*)CreateIORequest(p,sizeof(*io)):NULL;
 if(!io)goto out;
 w=OpenWindowTags(NULL,WA_Left,30,WA_Top,30,WA_Width,450,WA_Height,300,WA_Title,(IPTR)"ConsoleNG soft-wrap clipboard",WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_SizeGadget,TRUE,WA_Activate,TRUE,TAG_DONE);if(!w)goto out;
 io->io_Data=w;io->io_Length=sizeof(*w);if(OpenDevice((CONST_STRPTR)"consoleng.device",CONU_CHARMAP,(struct IORequest*)io,0))goto out;opened=1;
 ConsoleDevice=(void*)io->io_Device;hook.h_Entry=(HOOKFUNC)sink;AddConSnipHook(&hook);hooked=1;
 if(!put(io,"\x1b%G\x1b]50;topaz,8\x07\x1b[2J\x1b[H"))goto out;
 struct ConUnit *u=(void*)io->io_Unit;int width=u->cu_XMax+1;
 if(width<10||width>200)goto out;
 char text[240],expected[240];
 memset(text,'A',width);text[width]=0;
 for(int control=0;control<2;control++){
  if(!put(io,"\x1b[2J\x1b[H")||!put(io,text)||!put(io,control?"\x1b" "EZ":"\x1b" "DZ"))goto out;
  if(u->cu_YCP!=1||u->cu_XCP!=(control?1:width-1)){puts("HARD_BREAK_NATIVE_FAIL");goto out;}
 }
 puts("HARD_BREAK_NATIVE_PASS");
 if(!put(io,"\x1b[2J\x1b[H"))goto out;
 if(argc>1&&!strcmp(argv[1],"WIDE")){
  memset(text,'A',width-1);strcpy(text+width-1,"\xe2\x98\x95" "D\r\nHARD");
  memset(expected,'A',width-1);strcpy(expected+width-1,"\xe2\x98\x95" "D\rHARD\r");
 }else{
  memset(text,'A',width);strcpy(text+width,"BBBBB\r\nHARD");
  memset(expected,'A',width);strcpy(expected+width,"BBBBB\rHARD\r");
 }
 if(!put(io,text))goto out;
 int x=w->LeftEdge+u->cu_XROrigin,y=w->TopEdge+u->cu_YROrigin;
 printf("SOFTWRAP_SELECT x0=%d y0=%d x1=%d y1=%d width=%d\n",x+1,y+u->cu_YRSize/2,x+4*u->cu_XRSize+1,y+2*u->cu_YRSize+u->cu_YRSize/2,width);fflush(stdout);
 for(int i=0;i<90;i++){
  char *s=GetConSnip();if(s){printf("SOFTWRAP_COPY bytes=%lu expected=%lu\n",(unsigned long)strlen(s),(unsigned long)strlen(expected));if(!strcmp(s,expected))rc=0;else for(unsigned j=0;j<strlen(s);j++)printf("%02x",(unsigned char)s[j]);FreeVec(s);break;}Delay(50);
 }
 if(!rc){puts("SOFTWRAP_COPY_PASS");put(io,"\r\n\r\nPASS: soft rows joined, hard newline retained.\r\n");fflush(stdout);Delay(50*15);}
out:
 if(hooked)RemConSnipHook(&hook);
 if(opened)CloseDevice((struct IORequest*)io);
 if(w)CloseWindow(w);
 if(io)DeleteIORequest((struct IORequest*)io);
 if(p)DeleteMsgPort(p);
 printf("SOFTWRAP_SCRIPT rc=%d\n",rc);return rc;
}
