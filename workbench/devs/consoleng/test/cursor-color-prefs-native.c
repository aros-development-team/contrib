/* Additive cursor-color defaults, legacy ABI isolation and atomic profiles. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <devices/conunit.h>
#include <stdio.h>
#include <string.h>
#include "consoleng.h"
static struct IOStdReq *control,*first,*second;
static struct Window *w1,*w2;
static int request(struct IOStdReq *io,UWORD cmd,void *p,ULONG size){io->io_Command=cmd;io->io_Data=p;io->io_Length=size;return !DoIO((struct IORequest*)io)&&io->io_Actual==size;}
static int put(struct IOStdReq *io,const char *s){return request(io,CMD_WRITE,(void*)s,strlen(s));}
#define CHECK(c) do{if(!(c)){printf("CURPREF_FAIL line=%d check=%s\n",__LINE__,#c);fflush(stdout);goto out;}}while(0)
int main(int argc,char **argv){
 int rc=20,copen=0,fopen=0,sopen=0,saved=0;
 struct ConsolengAppearanceProfile original,current,bad;
 struct MsgPort *port=CreateMsgPort();
 control=port?(void*)CreateIORequest(port,sizeof(*control)):NULL;
 first=port?(void*)CreateIORequest(port,sizeof(*first)):NULL;
 second=port?(void*)CreateIORequest(port,sizeof(*second)):NULL;CHECK(control&&first&&second);
 CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",CONU_LIBRARY,(struct IORequest*)control,0));copen=1;
 CHECK(request(control,CD_GETAPPEARANCEPROFILE,&original,sizeof(original)));saved=1;
 if(argc>1){
  CHECK(original.rp_CursorColor.ccp_Enabled==!!strcmp(argv[1],"OFF"));
  CHECK(original.rp_CursorColor.ccp_RGB==0x12a4e8);
  printf("CURPREF_CHECK_%s_PASS\n",argv[1]);rc=0;goto out;
 }
 w1=OpenWindowTags(NULL,WA_Left,30,WA_Top,30,WA_Width,600,WA_Height,250,WA_Title,(IPTR)"ConsoleNG cursor preference validation",WA_DragBar,TRUE,WA_DepthGadget,TRUE,TAG_DONE);CHECK(w1);
 first->io_Data=w1;first->io_Length=sizeof(*w1);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",CONU_CHARMAP,(struct IORequest*)first,0));fopen=1;
 struct ConsolengCursorColorPrefs before,p={sizeof(p),1,1,0x12a4e8};
 CHECK(request(first,CD_GETCURSORCOLORPREFS,&before,sizeof(before)));
 CHECK(request(control,CD_SETDEFAULTCURSORCOLORPREFS,&p,sizeof(p)));
 struct ConsolengCursorColorPrefs got;CHECK(request(first,CD_GETCURSORCOLORPREFS,&got,sizeof(got))&&!memcmp(&got,&before,sizeof(got)));
 struct ConsolengTerminalProfile legacy;CHECK(sizeof(legacy)==308);
 CHECK(request(control,CD_GETTERMINALPROFILE,&legacy,sizeof(legacy)));CHECK(request(control,CD_SETTERMINALPROFILE,&legacy,sizeof(legacy)));
 CHECK(request(control,CD_GETDEFAULTCURSORCOLORPREFS,&got,sizeof(got))&&!memcmp(&got,&p,sizeof(got)));
 w2=OpenWindowTags(NULL,WA_Left,60,WA_Top,60,WA_Width,600,WA_Height,250,WA_Title,(IPTR)"New console inherits cursor color",WA_DragBar,TRUE,WA_DepthGadget,TRUE,TAG_DONE);CHECK(w2);
 second->io_Data=w2;second->io_Length=sizeof(*w2);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",CONU_CHARMAP,(struct IORequest*)second,0));sopen=1;
 CHECK(request(second,CD_GETCURSORCOLORPREFS,&got,sizeof(got))&&!memcmp(&got,&p,sizeof(got)));
 CHECK(put(second,"\x1b]12;#ff0000\7"));CHECK(request(second,CD_GETCURSORCOLORPREFS,&got,sizeof(got))&&got.ccp_RGB==0xff0000);
 CHECK(put(second,"\x1b]112\7"));CHECK(request(second,CD_GETCURSORCOLORPREFS,&got,sizeof(got))&&!memcmp(&got,&p,sizeof(got)));
 p.ccp_Enabled=0;p.ccp_RGB=0xabcdef;CHECK(request(second,CD_SETCURSORCOLORPREFS,&p,sizeof(p)));
 CHECK(put(second,"\x1b]12;#ff0000\7\x1b]112\7"));CHECK(request(second,CD_GETCURSORCOLORPREFS,&got,sizeof(got))&&!memcmp(&got,&p,sizeof(got)));
 for(int field=0;field<4;field++){
  struct ConsolengCursorColorPrefs invalid=p;((ULONG*)&invalid)[field]=0xffffffffUL;
  CHECK(!request(second,CD_SETCURSORCOLORPREFS,&invalid,sizeof(invalid))&&second->io_Actual==0);
  CHECK(request(second,CD_GETCURSORCOLORPREFS,&got,sizeof(got))&&!memcmp(&got,&p,sizeof(got)));
 }
 CHECK(!request(control,CD_SETCURSORCOLORPREFS,&p,sizeof(p))&&control->io_Actual==0);
 CHECK(!request(first,CD_SETDEFAULTCURSORCOLORPREFS,&p,sizeof(p))&&first->io_Actual==0);
 CHECK(!request(first,CD_SETCURSORCOLORPREFS,&p,sizeof(p)-1)&&first->io_Actual==0);
 CHECK(!request(first,CD_GETCURSORCOLORPREFS,NULL,sizeof(p))&&first->io_Actual==0);
 CHECK(request(control,CD_GETAPPEARANCEPROFILE,&current,sizeof(current)));
 for(int field=0;field<4;field++){
  bad=current;((ULONG*)&bad.rp_CursorColor)[field]=0xffffffffUL;bad.rp_Theme.tp_Cursor.cp_Style=6;
  CHECK(!request(control,CD_SETAPPEARANCEPROFILE,&bad,sizeof(bad))&&control->io_Actual==0);
  CHECK(request(control,CD_GETAPPEARANCEPROFILE,&bad,sizeof(bad))&&!memcmp(&bad,&current,sizeof(bad)));
 }
 struct {struct ConsolengAppearanceProfile p;ULONG guard;} bounded={{0},0x12345678};
 CHECK(request(control,CD_GETAPPEARANCEPROFILE,&bounded,sizeof(bounded.p))&&bounded.guard==0x12345678);
 CHECK(request(control,CD_SETAPPEARANCEPROFILE,&original,sizeof(original)));
 puts("CURPREF_PASS defaults inheritance, existing-unit isolation, OSC reset, disabled RGB retention, validation, legacy ABI and atomic profile");rc=0;
out:
 if(saved)request(control,CD_SETAPPEARANCEPROFILE,&original,sizeof(original));
 if(sopen)CloseDevice((struct IORequest*)second);
 if(fopen)CloseDevice((struct IORequest*)first);
 if(w2)CloseWindow(w2);
 if(w1)CloseWindow(w1);
 if(copen)CloseDevice((struct IORequest*)control);
 if(second)DeleteIORequest((struct IORequest*)second);
 if(first)DeleteIORequest((struct IORequest*)first);
 if(control)DeleteIORequest((struct IORequest*)control);
 if(port)DeleteMsgPort(port);
 printf("CURPREF_RUN rc=%d\n",rc);return rc;
}
