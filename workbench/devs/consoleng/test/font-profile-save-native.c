/* Exercise the real Preferences executable against private ENV/ENVARC assigns. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <devices/conunit.h>
#include <stdio.h>
#include <string.h>
#include "consoleng.h"
#include "../prefs/settings.h"
static int req(struct IOStdReq *io,UWORD cmd,void *p,ULONG n){
 io->io_Command=cmd;io->io_Data=p;io->io_Length=n;
 return !DoIO((struct IORequest*)io)&&io->io_Actual==n;
}
#define CHECK(x) do{if(!(x)){printf("FONT_SAVE_FAIL line=%d\n",__LINE__);goto out;}}while(0)
static LONG run(const char *cmd){LONG rc=SystemTags((CONST_STRPTR)cmd,TAG_DONE);printf("FONT_SAVE_COMMAND rc=%ld %s\n",(long)rc,cmd);return rc;}
int main(void){
 int rc=20,opened=0,saved=0;struct MsgPort *port=CreateMsgPort();struct IOStdReq *io=NULL;
 struct ConsolengDefaultProfile original,got;
 struct ConsoleSettings p={{20,1,5,350,8000},{12,1,4096},{140,1,16,"DejaVuSansMono.font"},{84,1,0,{0}}},q;
 CHECK(port);io=(void*)CreateIORequest(port,sizeof(*io));CHECK(io);
 CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",CONU_LIBRARY,(struct IORequest*)io,0));opened=1;
 CHECK(req(io,CD_GETDEFAULTPROFILE,&original,sizeof(original)));saved=1;
 CHECK(settingsSave("RAM:font-input.prefs",&p));
 CHECK(run("CONFONT:ConsoleNG FROM RAM:font-input.prefs SAVE")==0);
 q=p;CHECK(settingsLoad("ENVARC:SYS/consoleng.prefs",&q)&&!memcmp(&p,&q,sizeof(p)));
 q=p;CHECK(settingsLoad("ENV:SYS/consoleng.prefs",&q)&&!memcmp(&p,&q,sizeof(p)));
 CHECK(req(io,CD_GETDEFAULTPROFILE,&got,sizeof(got))&&got.dp_Font.fp_Pixels==16&&got.dp_Cursor.cp_Style==5&&got.dp_History.hp_Lines==4096);
 CHECK(req(io,CD_SETDEFAULTPROFILE,&original,sizeof(original)));
 CHECK(run("CONFONT:ConsoleNG RESTORE")==0);
 CHECK(req(io,CD_GETDEFAULTPROFILE,&got,sizeof(got))&&got.dp_Font.fp_Pixels==16&&got.dp_Cursor.cp_Style==5);
 struct ConsoleSettings bad=p;strcpy(bad.font.fp_Name,"DefinitelyMissingConsoleNGFont.font");
 CHECK(settingsSave("RAM:font-invalid.prefs",&bad));
 CHECK(run("CONFONT:ConsoleNG FROM RAM:font-invalid.prefs SAVE")==20);
 q=p;CHECK(settingsLoad("ENVARC:SYS/consoleng.prefs",&q)&&!memcmp(&p,&q,sizeof(p)));
 q=p;CHECK(settingsLoad("ENV:SYS/consoleng.prefs",&q)&&!memcmp(&p,&q,sizeof(p)));
 CHECK(req(io,CD_GETDEFAULTPROFILE,&got,sizeof(got))&&got.dp_Font.fp_Pixels==16);
 /* Old profiles retain the current font; USE changes ENV only. */
 FILE *f=fopen("RAM:font-legacy.prefs","w");CHECK(f);
 fputs("ConsoleNG 1\ncursor=6 500 0\nhistory=128\n",f);CHECK(!fclose(f));
 CHECK(run("CONFONT:ConsoleNG FROM RAM:font-legacy.prefs USE")==0);
 CHECK(req(io,CD_GETDEFAULTPROFILE,&got,sizeof(got))&&got.dp_Font.fp_Pixels==16&&got.dp_Cursor.cp_Style==6&&got.dp_History.hp_Lines==128);
 q=p;CHECK(settingsLoad("ENVARC:SYS/consoleng.prefs",&q)&&!memcmp(&p,&q,sizeof(p)));
 CHECK(DeleteFile((CONST_STRPTR)"ENV:SYS/consoleng.prefs"));
 CHECK(req(io,CD_SETDEFAULTPROFILE,&original,sizeof(original)));
 CHECK(run("CONFONT:ConsoleNG RESTORE")==0);
 CHECK(req(io,CD_GETDEFAULTPROFILE,&got,sizeof(got))&&got.dp_Font.fp_Pixels==16&&got.dp_Cursor.cp_Style==5&&got.dp_History.hp_Lines==4096);
 puts("FONT_SAVE_PASS");rc=0;
out:
 if(saved&&!req(io,CD_SETDEFAULTPROFILE,&original,sizeof(original)))rc=20;
 if(opened)CloseDevice((struct IORequest*)io);
 if(io)DeleteIORequest((struct IORequest*)io);
 if(port)DeleteMsgPort(port);
 printf("FONT_SAVE_SCRIPT rc=%d\n",rc);return rc;
}
