/* Visual-bell API validation, defaults inheritance and legacy-profile isolation. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <devices/conunit.h>
#include <stdio.h>
#include <string.h>
#include "consoleng.h"
static struct IOStdReq *control,*first,*second;
static struct Window *w1,*w2;
static struct Screen *screen;
static int request(struct IOStdReq *io,UWORD cmd,void *p,ULONG size){io->io_Command=cmd;io->io_Data=p;io->io_Length=size;return !DoIO((struct IORequest*)io)&&io->io_Actual==size;}
static int bell(struct IOStdReq *io){return request(io,CMD_WRITE,(void*)"\7",1);}
static int waitbeep(int on){for(int t=0;t<100;t++){if(!!(screen->Flags&BEEPING)==on)return 1;Delay(1);}return 0;}
static int quiet(void){for(int t=0;t<30;t++){if(screen->Flags&BEEPING)return 0;Delay(1);}return 1;}
#define CHECK(c) do{if(!(c)){printf("BELLPREF_FAIL line=%d check=%s\n",__LINE__,#c);fflush(stdout);goto out;}}while(0)
int main(int argc,char **argv){int rc=20,copen=0,fopen=0,sopen=0,saved=0;struct ConsolengSessionProfile original,current,bad;
struct MsgPort *port=CreateMsgPort();control=port?(void*)CreateIORequest(port,sizeof(*control)):NULL;first=port?(void*)CreateIORequest(port,sizeof(*first)):NULL;second=port?(void*)CreateIORequest(port,sizeof(*second)):NULL;CHECK(control&&first&&second);
CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",CONU_LIBRARY,(struct IORequest*)control,0));copen=1;
CHECK(request(control,CD_GETSESSIONPROFILE,&original,sizeof(original)));saved=1;
if(argc>1){CHECK(original.rp_Bell.bp_Enabled==0);puts("BELLPREF_GUI_DEFAULT_OFF_PASS");rc=0;goto out;}
CHECK(original.rp_Bell.bp_Enabled==1);
w1=OpenWindowTags(NULL,WA_Left,30,WA_Top,30,WA_Width,600,WA_Height,300,WA_Title,(IPTR)"ConsoleNG bell policy",WA_DragBar,TRUE,WA_DepthGadget,TRUE,TAG_DONE);CHECK(w1);screen=w1->WScreen;
first->io_Data=w1;first->io_Length=sizeof(*w1);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",CONU_CHARMAP,(struct IORequest*)first,0));fopen=1;
struct ConsolengBellPrefs p={sizeof(p),1,0};CHECK(request(control,CD_SETDEFAULTBELLPREFS,&p,sizeof(p)));
CHECK(request(first,CD_GETBELLPREFS,&p,sizeof(p))&&p.bp_Enabled==1);
struct ConsolengRenderProfile legacy;CHECK(request(control,CD_GETRENDERPROFILE,&legacy,sizeof(legacy)));CHECK(request(control,CD_SETRENDERPROFILE,&legacy,sizeof(legacy)));CHECK(request(control,CD_GETDEFAULTBELLPREFS,&p,sizeof(p))&&p.bp_Enabled==0);
w2=OpenWindowTags(NULL,WA_Left,60,WA_Top,60,WA_Width,600,WA_Height,300,WA_Title,(IPTR)"ConsoleNG muted session",WA_DragBar,TRUE,WA_DepthGadget,TRUE,TAG_DONE);CHECK(w2);
second->io_Data=w2;second->io_Length=sizeof(*w2);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",CONU_CHARMAP,(struct IORequest*)second,0));sopen=1;
CHECK(request(second,CD_GETBELLPREFS,&p,sizeof(p))&&p.bp_Enabled==0);CHECK(waitbeep(0));CHECK(bell(second));CHECK(quiet());
p.bp_Enabled=1;CHECK(request(second,CD_SETBELLPREFS,&p,sizeof(p)));CHECK(bell(second));CHECK(waitbeep(1));CHECK(waitbeep(0));
for(int field=0;field<3;field++){struct ConsolengBellPrefs invalid=p;((ULONG*)&invalid)[field]=0xffffffffUL;CHECK(!request(second,CD_SETBELLPREFS,&invalid,sizeof(invalid))&&second->io_Actual==0);CHECK(request(second,CD_GETBELLPREFS,&invalid,sizeof(invalid))&&invalid.bp_Enabled==1);}
CHECK(!request(control,CD_SETBELLPREFS,&p,sizeof(p))&&control->io_Actual==0);
CHECK(!request(first,CD_SETDEFAULTBELLPREFS,&p,sizeof(p))&&first->io_Actual==0);
CHECK(!request(first,CD_SETBELLPREFS,&p,sizeof(p)-1)&&first->io_Actual==0);
CHECK(request(control,CD_GETSESSIONPROFILE,&current,sizeof(current)));
for(int field=0;field<3;field++){bad=current;((ULONG*)&bad.rp_Bell)[field]=0xffffffffUL;bad.rp_Theme.tp_Cursor.cp_Style=6;CHECK(!request(control,CD_SETSESSIONPROFILE,&bad,sizeof(bad))&&control->io_Actual==0);CHECK(request(control,CD_GETSESSIONPROFILE,&bad,sizeof(bad))&&!memcmp(&bad,&current,sizeof(bad)));}
CHECK(request(control,CD_SETSESSIONPROFILE,&original,sizeof(original)));CHECK(request(control,CD_GETSESSIONPROFILE,&current,sizeof(current))&&!memcmp(&current,&original,sizeof(current)));
puts("BELLPREF_PASS inheritance, local mute, validation, legacy ABI and atomic profile");rc=0;
out:if(saved)request(control,CD_SETSESSIONPROFILE,&original,sizeof(original));if(sopen)CloseDevice((struct IORequest*)second);if(fopen)CloseDevice((struct IORequest*)first);if(w2)CloseWindow(w2);if(w1)CloseWindow(w1);if(copen)CloseDevice((struct IORequest*)control);if(second)DeleteIORequest((struct IORequest*)second);if(first)DeleteIORequest((struct IORequest*)first);if(control)DeleteIORequest((struct IORequest*)control);if(port)DeleteMsgPort(port);printf("BELLPREF_RUN rc=%d\n",rc);return rc;}
