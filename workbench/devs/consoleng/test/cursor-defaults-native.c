/* Native defaults lifetime: new units inherit, existing units stay unchanged. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <devices/conunit.h>
#include <stdio.h>
#include <string.h>
#include "consoleng.h"
static int request(struct IOStdReq *io,int cmd,struct ConsolengCursorPrefs *p)
{io->io_Command=cmd;io->io_Data=p;io->io_Length=sizeof(*p);return DoIO((struct IORequest *)io);}
int main(void)
{
    struct MsgPort *port=CreateMsgPort();struct IOStdReq *io[4]={0};int opened[4]={0},rc=20,haveSaved=0;
    struct ConsolengCursorPrefs saved,changed,got,override;
    if(!port)return 20;
    for(int i=0;i<4;i++){io[i]=(struct IOStdReq *)CreateIORequest(port,sizeof(*io[i]));if(!io[i])goto out;}
    if(OpenDevice((CONST_STRPTR)"consoleng.device",CONU_LIBRARY,(struct IORequest *)io[0],0))goto out;
    opened[0]=1;
    if(request(io[0],CD_GETDEFAULTCURSORPREFS,&saved))goto out;
    haveSaved=1;
    if(OpenDevice((CONST_STRPTR)"consoleng.device",CONU_CHARMAP,(struct IORequest *)io[1],0))goto out;
    opened[1]=1;
    changed=saved;changed.cp_Style=5;changed.cp_BlinkIntervalMS=350;changed.cp_IdleTimeoutMS=8000;
    if(request(io[0],CD_SETDEFAULTCURSORPREFS,&changed))goto out;
    if(request(io[1],CD_GETCURSORPREFS,&got)||memcmp(&saved,&got,sizeof(got)))goto out;
    if(OpenDevice((CONST_STRPTR)"consoleng.device",CONU_CHARMAP,(struct IORequest *)io[2],0))goto out;
    opened[2]=1;
    if(request(io[2],CD_GETCURSORPREFS,&got)||memcmp(&changed,&got,sizeof(got)))goto out;
    override=changed;override.cp_Style=2;
    if(request(io[2],CD_SETCURSORPREFS,&override))goto out;
    if(request(io[0],CD_GETDEFAULTCURSORPREFS,&got)||memcmp(&changed,&got,sizeof(got)))goto out;
    got=changed;got.cp_Version=2;
    if(request(io[0],CD_SETDEFAULTCURSORPREFS,&got)!=CONSOLENG_ERR_BADPREFS)goto out;
    if(request(io[0],CD_GETDEFAULTCURSORPREFS,&got)||memcmp(&changed,&got,sizeof(got)))goto out;
    if(request(io[0],CD_SETDEFAULTCURSORPREFS,&saved))goto out;
    if(OpenDevice((CONST_STRPTR)"consoleng.device",CONU_CHARMAP,(struct IORequest *)io[3],0))goto out;
    opened[3]=1;
    if(request(io[3],CD_GETCURSORPREFS,&got)||memcmp(&saved,&got,sizeof(got)))goto out;
    if(request(io[2],CD_GETCURSORPREFS,&got)||memcmp(&override,&got,sizeof(got)))goto out;
    puts("CURSOR_DEFAULTS_PASS: old unit unchanged, new unit inherits, override isolated, defaults restored");rc=0;
out:
    if(haveSaved)request(io[0],CD_SETDEFAULTCURSORPREFS,&saved);
    for(int i=3;i>=0;i--){if(opened[i])CloseDevice((struct IORequest *)io[i]);if(io[i])DeleteIORequest((struct IORequest *)io[i]);}
    DeleteMsgPort(port);printf("CURSOR_DEFAULTS_DONE rc=%d\n",rc);return rc;
}
