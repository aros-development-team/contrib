/* Verify a coherent library snapshot and a newly opened native console. */
#include <proto/exec.h>
#include <devices/conunit.h>
#include <stdio.h>
#include <stdlib.h>
#include "consoleng.h"
int main(int argc,char **argv)
{
    if(argc!=5)return 20;
    ULONG style=strtoul(argv[1],NULL,10),interval=strtoul(argv[2],NULL,10),timeout=strtoul(argv[3],NULL,10),lines=strtoul(argv[4],NULL,10);
    struct MsgPort *port=CreateMsgPort();struct IOStdReq *io=port?(void*)CreateIORequest(port,sizeof(*io)):NULL;
    int rc=20;struct ConsolengDefaultPrefs p;
    if(!io)goto out;
    for(int i=0;i<2;i++){
        io->io_Data=NULL;io->io_Length=0;
        if(OpenDevice((CONST_STRPTR)"consoleng.device",i?CONU_CHARMAP:CONU_LIBRARY,(struct IORequest*)io,0))goto out;
        int ok;
        if(!i){io->io_Command=CD_GETDEFAULTPREFS;io->io_Data=&p;io->io_Length=sizeof(p);ok=!DoIO((struct IORequest*)io)&&io->io_Actual==sizeof(p)&&p.dp_Size==sizeof(p)&&p.dp_Version==1;}
        else {
            io->io_Command=CD_GETCURSORPREFS;io->io_Data=&p.dp_Cursor;io->io_Length=sizeof(p.dp_Cursor);ok=!DoIO((struct IORequest*)io)&&io->io_Actual==sizeof(p.dp_Cursor);
            io->io_Command=CD_GETHISTORYPREFS;io->io_Data=&p.dp_History;io->io_Length=sizeof(p.dp_History);ok=ok&&!DoIO((struct IORequest*)io)&&io->io_Actual==sizeof(p.dp_History);
        }
        CloseDevice((struct IORequest*)io);
        if(!ok||p.dp_Cursor.cp_Style!=style||p.dp_Cursor.cp_BlinkIntervalMS!=interval||p.dp_Cursor.cp_IdleTimeoutMS!=timeout||p.dp_History.hp_Lines!=lines)goto out;
    }
    rc=0;
out:
    if(io)DeleteIORequest((struct IORequest*)io);
    if(port)DeleteMsgPort(port);
    printf("DEFAULT_SETTINGS_PROBE %lu/%lu/%lu history=%lu rc=%d\n",(unsigned long)style,(unsigned long)interval,(unsigned long)timeout,(unsigned long)lines,rc);return rc;
}
