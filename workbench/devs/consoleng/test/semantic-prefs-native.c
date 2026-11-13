/* Native semantic preference contract; navigation/UI acceptance is separate. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <exec/errors.h>
#include <devices/conunit.h>
#include <stdio.h>
#include <string.h>
#include "consoleng.h"
static int exchange(struct IOStdReq *io,UWORD cmd,void *p,ULONG size){io->io_Command=cmd;io->io_Data=p;io->io_Length=size;return !DoIO((struct IORequest*)io)&&io->io_Actual==size;}
#define CHECK(x) do{count++;if(!(x)){printf("SEMANTIC_PREFS_FAIL line=%d kind=%d count=%d\n",__LINE__,kind,count);fflush(stdout);goto out;}}while(0)
int main(void){int rc=20,kind=0,count=0,controlOpen=0,unitOpen=0,futureOpen=0,saved=0;struct Window *w=NULL,*futureWindow=NULL;struct MsgPort *port=CreateMsgPort();struct IOStdReq *control=NULL,*io=NULL,*future=NULL;struct ConsolengSemanticPrefs original,p,got,bad;
 CHECK(port);control=(void*)CreateIORequest(port,sizeof(*control));io=(void*)CreateIORequest(port,sizeof(*io));future=(void*)CreateIORequest(port,sizeof(*future));CHECK(control&&io&&future);
 CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",CONU_LIBRARY,(struct IORequest*)control,0));controlOpen=1;

 {
  struct ConsolengSelectionProfile baseline,candidate,result;
  CHECK(exchange(control,CD_GETSELECTIONPROFILE,&baseline,sizeof(baseline)));
  candidate=baseline;candidate.sp_Semantic=(struct ConsolengSemanticPrefs){sizeof(candidate.sp_Semantic),1,1,{'/'}};
  candidate.sp_Input.ip_Scroll.sp_Lines=7;
  CHECK(exchange(control,CD_SETSELECTIONPROFILE,&candidate,sizeof(candidate)));
  CHECK(exchange(control,CD_GETSELECTIONPROFILE,&result,sizeof(result)));
  CHECK(!memcmp(&candidate,&result,sizeof(result)));
  for(int invalid=0;invalid<4;invalid++){
   struct ConsolengSelectionProfile broken=candidate;
   if(invalid==0)broken.sp_Version++;
   if(invalid==1)broken.sp_Semantic.sp_Chars[0]=0xd800;
   if(invalid==2)broken.sp_Input.ip_Scroll.sp_Lines=0xffffffff;
   if(invalid==3)broken.sp_Input.ip_Interaction.ip_Appearance.rp_CursorColor.ccp_RGB=0xffffffff;
   CHECK(!exchange(control,CD_SETSELECTIONPROFILE,&broken,sizeof(broken)));
   CHECK(control->io_Error==CONSOLENG_ERR_BADPREFS&&!control->io_Actual);
   CHECK(exchange(control,CD_GETSELECTIONPROFILE,&result,sizeof(result)));
   CHECK(!memcmp(&candidate,&result,sizeof(result)));
  }
  CHECK(exchange(control,CD_SETINPUTPROFILE,&baseline.sp_Input,sizeof(baseline.sp_Input)));
  CHECK(exchange(control,CD_GETSELECTIONPROFILE,&result,sizeof(result)));
  CHECK(!memcmp(&result.sp_Semantic,&candidate.sp_Semantic,sizeof(candidate.sp_Semantic)));
  CHECK(exchange(control,CD_SETSELECTIONPROFILE,&baseline,sizeof(baseline)));
 }
 CHECK(exchange(control,CD_GETDEFAULTSEMANTICPREFS,&original,sizeof(original)));saved=1;CHECK(original.sp_Count==17);
 ULONG units[]={CONU_STANDARD,CONU_CHARMAP,CONU_SNIPMAP};
 for(kind=0;kind<3;kind++){
  CHECK(exchange(control,CD_SETDEFAULTSEMANTICPREFS,&original,sizeof(original)));
  w=OpenWindowTags(NULL,WA_Left,30,WA_Top,35,WA_Width,720,WA_Height,430,WA_Title,(IPTR)"ConsoleNG semantic policy API",WA_Activate,TRUE,WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_SizeGadget,TRUE,WA_SizeBRight,TRUE,TAG_DONE);CHECK(w);
  io->io_Data=w;io->io_Length=sizeof(*w);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",units[kind],(struct IORequest*)io,0));unitOpen=1;
  CHECK(exchange(io,CD_GETSEMANTICPREFS,&got,sizeof(got)));CHECK(!memcmp(&got,&original,sizeof(got)));
  p=(struct ConsolengSemanticPrefs){sizeof(p),1,2,{'/',0x03a9}};CHECK(exchange(io,CD_SETSEMANTICPREFS,&p,sizeof(p)));
  CHECK(exchange(io,CD_GETSEMANTICPREFS,&got,sizeof(got)));CHECK(!memcmp(&p,&got,sizeof(p)));
  for(int invalid=0;invalid<7;invalid++){
   bad=p;
 if(invalid==0)bad.sp_Size--;
 if(invalid==1)bad.sp_Version++;
 if(invalid==2)bad.sp_Count=65;
 if(invalid==3)bad.sp_Chars[0]=0xd800;
 if(invalid==4)bad.sp_Chars[0]=0x110000;
 if(invalid==5)bad.sp_Chars[1]=bad.sp_Chars[0];
 if(invalid==6)bad.sp_Chars[63]=1;
   CHECK(!exchange(io,CD_SETSEMANTICPREFS,&bad,sizeof(bad)));CHECK(io->io_Error==CONSOLENG_ERR_BADPREFS&&!io->io_Actual);
   CHECK(exchange(io,CD_GETSEMANTICPREFS,&got,sizeof(got)));CHECK(!memcmp(&p,&got,sizeof(p)));
  }
  CHECK(!exchange(io,CD_SETSEMANTICPREFS,&p,sizeof(p)-1));CHECK(io->io_Error==IOERR_BADLENGTH&&!io->io_Actual);
  CHECK(!exchange(control,CD_GETSEMANTICPREFS,&got,sizeof(got)));CHECK(control->io_Error==IOERR_BADADDRESS&&!control->io_Actual);
  CHECK(!exchange(io,CD_GETDEFAULTSEMANTICPREFS,&got,sizeof(got)));CHECK(io->io_Error==IOERR_BADADDRESS&&!io->io_Actual);
  bad=(struct ConsolengSemanticPrefs){sizeof(bad),1,0,{0}};CHECK(exchange(control,CD_SETDEFAULTSEMANTICPREFS,&bad,sizeof(bad)));
  CHECK(exchange(io,CD_GETSEMANTICPREFS,&got,sizeof(got)));CHECK(!memcmp(&p,&got,sizeof(p)));
  futureWindow=OpenWindowTags(NULL,WA_Left,60,WA_Top,70,WA_Width,400,WA_Height,200,WA_Title,(IPTR)"New console policy",TAG_DONE);CHECK(futureWindow);
  future->io_Data=futureWindow;future->io_Length=sizeof(*futureWindow);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",units[kind],(struct IORequest*)future,0));futureOpen=1;
  CHECK(exchange(future,CD_GETSEMANTICPREFS,&got,sizeof(got)));CHECK(!memcmp(&bad,&got,sizeof(bad)));
  bad.sp_Count=64;for(int i=0;i<64;i++)bad.sp_Chars[i]=0x10000+i;CHECK(exchange(future,CD_SETSEMANTICPREFS,&bad,sizeof(bad)));CHECK(exchange(future,CD_GETSEMANTICPREFS,&got,sizeof(got)));CHECK(!memcmp(&bad,&got,sizeof(bad)));
  CloseDevice((struct IORequest*)future);futureOpen=0;CloseWindow(futureWindow);futureWindow=NULL;
  const char *text="\x1b]50;topaz,8\x07\x1b[2J\x1b[HPASS: semantic preference device contract.\r\nUnicode, empty and maximum sets; invalid updates rejected.\r\nPer-console isolation and future-console defaults verified.\r\nNavigation rendering and Preferences UI still require acceptance.\r\n";CHECK(exchange(io,CMD_WRITE,(void*)text,strlen(text)));printf("SEMANTIC_PREFS_UNIT_PASS kind=%d count=%d\n",kind,count);fflush(stdout);Delay(150);
  CloseDevice((struct IORequest*)io);unitOpen=0;CloseWindow(w);w=NULL;
 }
 CHECK(exchange(control,CD_SETDEFAULTSEMANTICPREFS,&original,sizeof(original)));puts("SEMANTIC_PREFS_PASS");rc=0;
out:
 if(saved&&!exchange(control,CD_SETDEFAULTSEMANTICPREFS,&original,sizeof(original)))rc=20;
 if(futureOpen)CloseDevice((struct IORequest*)future);
 if(unitOpen)CloseDevice((struct IORequest*)io);
 if(controlOpen)CloseDevice((struct IORequest*)control);
 if(futureWindow)CloseWindow(futureWindow);
 if(w)CloseWindow(w);
 if(future)DeleteIORequest((struct IORequest*)future);
 if(io)DeleteIORequest((struct IORequest*)io);
 if(control)DeleteIORequest((struct IORequest*)control);
 if(port)DeleteMsgPort(port);
 printf("SEMANTIC_PREFS_SCRIPT rc=%d checks=%d\n",rc,count);return rc;
}
