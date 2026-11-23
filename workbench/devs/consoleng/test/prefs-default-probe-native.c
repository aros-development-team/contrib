/* Verify library defaults AND a freshly opened unit after startup/CLI actions. */
#include <proto/exec.h>
#include <devices/conunit.h>
#include <stdio.h>
#include <stdlib.h>
#include "consoleng.h"
int main(int argc,char **argv)
{
    if(argc!=4)return 20;
    ULONG style=strtoul(argv[1],NULL,10),interval=strtoul(argv[2],NULL,10),timeout=strtoul(argv[3],NULL,10);
    struct MsgPort *port=CreateMsgPort();
    struct IOStdReq *io=port?(struct IOStdReq*)CreateIORequest(port,sizeof(*io)):NULL;
    int rc=20;
    if(!io)goto out;
    for(int i=0;i<2;i++){
        io->io_Data=NULL;io->io_Length=0;
        if(OpenDevice((CONST_STRPTR)"consoleng.device",i?CONU_CHARMAP:CONU_LIBRARY,(struct IORequest *)io,0))goto out;
        struct ConsolengCursorPrefs p;
        io->io_Command=i?CD_GETCURSORPREFS:CD_GETDEFAULTCURSORPREFS;io->io_Data=&p;io->io_Length=sizeof(p);
        int ok=!DoIO((struct IORequest*)io)&&p.cp_Size==sizeof(p)&&p.cp_Version==1&&p.cp_Style==style&&p.cp_BlinkIntervalMS==interval&&p.cp_IdleTimeoutMS==timeout;
        CloseDevice((struct IORequest*)io);
        if(!ok)goto out;
    }
    rc=0;
out:
    if(io)DeleteIORequest((struct IORequest*)io);
    if(port)DeleteMsgPort(port);
    printf("PREFS_DEFAULT_PROBE %lu/%lu/%lu rc=%d\n",(unsigned long)style,(unsigned long)interval,(unsigned long)timeout,rc);return rc;
}
