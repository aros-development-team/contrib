#include <proto/exec.h>
#include <proto/dos.h>
#include <devices/conunit.h>
#include <stdio.h>
#include <string.h>
#include "consoleng.h"
#include "../prefs/settings.h"
static int req(struct IOStdReq*q,UWORD cmd,void*p,ULONG n){q->io_Command=cmd;q->io_Data=p;q->io_Length=n;return !DoIO((struct IORequest*)q)&&q->io_Actual==n;}
#define CHECK(c) do{if(!(c)){printf("EMOJISAVE_FAIL line=%d\n",__LINE__);goto out;}}while(0)
int main(void){int rc=20,opened=0,saved=0;struct MsgPort*port=CreateMsgPort();struct IOStdReq*io=port?(void*)CreateIORequest(port,sizeof(*io)):NULL;struct ConsolengRenderProfile original,got;
struct ConsoleSettings p={{20,1,0,500,0},{12,1,1000},{140,1,16,"DejaVuSansMono.font"},{84,1,3,{0xe5e5e5,0x193c50}},0},q;
CHECK(io);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",CONU_LIBRARY,(struct IORequest*)io,0));opened=1;CHECK(req(io,CD_GETRENDERPROFILE,&original,sizeof(original)));saved=1;
CHECK(settingsSave("RAM:emoji-input.prefs",&p));CHECK(SystemTags((CONST_STRPTR)"EMOP:ConsoleNG FROM RAM:emoji-input.prefs SAVE",TAG_DONE)==0);
CHECK(req(io,CD_GETRENDERPROFILE,&got,sizeof(got))&&got.rp_Emoji.ep_Enabled==0);
q=p;CHECK(settingsLoad("ENV:SYS/consoleng.prefs",&q)&&!memcmp(&p,&q,sizeof(p)));q=p;CHECK(settingsLoad("ENVARC:SYS/consoleng.prefs",&q)&&!memcmp(&p,&q,sizeof(p)));
p.emoji=1;CHECK(settingsSave("RAM:emoji-input.prefs",&p));CHECK(SystemTags((CONST_STRPTR)"EMOP:ConsoleNG FROM RAM:emoji-input.prefs USE",TAG_DONE)==0);
CHECK(req(io,CD_GETRENDERPROFILE,&got,sizeof(got))&&got.rp_Emoji.ep_Enabled==1);
q=p;CHECK(settingsLoad("ENV:SYS/consoleng.prefs",&q)&&q.emoji==1);CHECK(settingsLoad("ENVARC:SYS/consoleng.prefs",&q)&&q.emoji==0);
CHECK(DeleteFile((CONST_STRPTR)"ENV:SYS/consoleng.prefs"));CHECK(SystemTags((CONST_STRPTR)"EMOP:ConsoleNG RESTORE",TAG_DONE)==0);CHECK(req(io,CD_GETRENDERPROFILE,&got,sizeof(got))&&got.rp_Emoji.ep_Enabled==0);
/* Keep a legible enabled profile for the subsequent GUI screenshot. */
CHECK(SystemTags((CONST_STRPTR)"EMOP:ConsoleNG FROM RAM:emoji-input.prefs USE",TAG_DONE)==0);
printf("EMOJISAVE_PASS SAVE_USE_RESTORE\n");rc=0;
out:if(opened){if(saved)req(io,CD_SETRENDERPROFILE,&original,sizeof(original));CloseDevice((struct IORequest*)io);}if(io)DeleteIORequest((struct IORequest*)io);if(port)DeleteMsgPort(port);printf("EMOJISAVE_SCRIPT rc=%d\n",rc);return rc;}
