/* Verify library defaults AND a freshly opened unit after startup/CLI actions. */
#include <proto/exec.h>
#include <devices/conunit.h>
#include <stdio.h>
#include <stdlib.h>
#include "consoleng.h"
int main(int argc,char **argv)
{
    if(argc!=2)return 20;
    ULONG lines=strtoul(argv[1],NULL,10);
    struct MsgPort *port=CreateMsgPort();
    struct IOStdReq *io=port?(struct IOStdReq*)CreateIORequest(port,sizeof(*io)):NULL;
    int rc=20;
    if(!io)goto out;
    for(int i=0;i<2;i++){
        io->io_Data=NULL;io->io_Length=0;
        if(OpenDevice((CONST_STRPTR)"consoleng.device",i?CONU_CHARMAP:CONU_LIBRARY,(struct IORequest *)io,0))goto out;
        struct ConsolengScrollPrefs p;
        io->io_Command=i?CD_GETSCROLLPREFS:CD_GETDEFAULTSCROLLPREFS;io->io_Data=&p;io->io_Length=sizeof(p);
        int ok=!DoIO((struct IORequest*)io)&&io->io_Actual==sizeof(p)&&p.sp_Size==sizeof(p)&&p.sp_Version==1&&p.sp_Lines==lines;
        CloseDevice((struct IORequest*)io);
        if(!ok)goto out;
    }
    rc=0;
out:
    if(io)DeleteIORequest((struct IORequest*)io);
    if(port)DeleteMsgPort(port);
    printf("PREFS_WHEEL_PROBE %lu rc=%d\n",(unsigned long)lines,rc);return rc;
}
