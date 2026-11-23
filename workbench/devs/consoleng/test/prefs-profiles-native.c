/* Historical GadTools cursor-only profile fixture (a4deccf).
 * Expanded Zune profiles use settings-regression.py and default-settings-native.c;
 * this fixture is retained for replaying the original registered evidence. */
#include <proto/exec.h>
#include <devices/conunit.h>
#include <stdio.h>
#include "consoleng.h"
int main(void)
{
    unsigned long version,style,interval,timeout;char extra;
    FILE *f=fopen("ENVARC:SYS/ConsoleNG-profile.prefs","r");
    if(!f)return 20;
    int n=fscanf(f,"ConsoleNG-Cursor %lu %lu %lu %lu %c",&version,&style,&interval,&timeout,&extra);
    fclose(f);
    if(n!=4||version!=1||style!=5||interval!=350||timeout!=8000)return 20;
    struct MsgPort *port=CreateMsgPort();
    struct IOStdReq *io=port?(struct IOStdReq *)CreateIORequest(port,sizeof(*io)):NULL;
    int rc=20;
    if(io){
        if(!OpenDevice((CONST_STRPTR)"consoleng.device",CONU_LIBRARY,(struct IORequest *)io,0)){
            struct ConsolengCursorPrefs p;
            io->io_Command=CD_GETDEFAULTCURSORPREFS;io->io_Data=&p;io->io_Length=sizeof(p);
            if(!DoIO((struct IORequest *)io)&&p.cp_Style==0&&p.cp_BlinkIntervalMS==500&&p.cp_IdleTimeoutMS==0)rc=0;
            CloseDevice((struct IORequest *)io);
        }
        DeleteIORequest((struct IORequest *)io);
    }
    if(port)DeleteMsgPort(port);
    printf("PROFILES_NATIVE_%s: written profile 5/350/8000, defaults 0/500/0\n",rc?"FAIL":"PASS");return rc;
}
