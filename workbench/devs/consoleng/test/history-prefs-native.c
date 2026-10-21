/* Exercise public history requests against real units; restore shared defaults. */
#include <proto/exec.h>
#include <devices/conunit.h>
#include <exec/errors.h>
#include <stdio.h>
#include <string.h>
#include "consoleng.h"
static int request(struct IOStdReq *io,UWORD cmd,struct ConsolengHistoryPrefs *p)
{
    io->io_Command=cmd;io->io_Data=p;io->io_Length=sizeof(*p);
    return DoIO((struct IORequest *)io);
}
static int value(struct IOStdReq *io,UWORD cmd,ULONG expected)
{
    struct ConsolengHistoryPrefs p={0};
    return !request(io,cmd,&p)&&io->io_Actual==sizeof(p)&&p.hp_Size==sizeof(p)&&p.hp_Version==1&&p.hp_Lines==expected;
}
static int text(struct IOStdReq *io,const char *s)
{
    io->io_Command=CMD_WRITE;io->io_Data=(APTR)s;io->io_Length=strlen(s);
    return !DoIO((struct IORequest*)io);
}
#define CHECK(x) do { if(!(x)){printf("HISTORY_FAIL line=%d\n",__LINE__);goto out;} }while(0)
int main(void)
{
    struct MsgPort *port=CreateMsgPort();struct IOStdReq *io[4]={0};int opened[4]={0};
    int rc=20,haveOriginal=0;struct ConsolengHistoryPrefs original={0},p={sizeof(p),1,4096};
    if(!port)goto out;
    for(int i=0;i<4;i++){io[i]=(struct IOStdReq*)CreateIORequest(port,sizeof(*io[i]));CHECK(io[i]);}
    CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",CONU_LIBRARY,(struct IORequest*)io[0],0));opened[0]=1;
    CHECK(!request(io[0],CD_GETDEFAULTHISTORYPREFS,&original));haveOriginal=1;
    CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",CONU_CHARMAP,(struct IORequest*)io[1],0));opened[1]=1;
    CHECK(value(io[1],CD_GETHISTORYPREFS,original.hp_Lines));
    CHECK(!request(io[0],CD_SETDEFAULTHISTORYPREFS,&p));
    CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",CONU_CHARMAP,(struct IORequest*)io[2],0));opened[2]=1;
    CHECK(value(io[2],CD_GETHISTORYPREFS,4096));CHECK(value(io[1],CD_GETHISTORYPREFS,original.hp_Lines));
    p.hp_Lines=7;CHECK(!request(io[1],CD_SETHISTORYPREFS,&p));
    CHECK(text(io[1],"\x1b[?1049h"));p.hp_Lines=19;CHECK(!request(io[1],CD_SETHISTORYPREFS,&p));
    CHECK(value(io[1],CD_GETHISTORYPREFS,19));CHECK(text(io[1],"\x1b[?1049l"));
    CHECK(value(io[1],CD_GETHISTORYPREFS,19));CHECK(value(io[2],CD_GETHISTORYPREFS,4096));
    p.hp_Lines=CONSOLENG_HISTORY_MAX_LINES+1;CHECK(request(io[1],CD_SETHISTORYPREFS,&p)==CONSOLENG_ERR_BADPREFS);
    CHECK(io[1]->io_Actual==0&&value(io[1],CD_GETHISTORYPREFS,19));
    CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",CONU_STANDARD,(struct IORequest*)io[3],0));opened[3]=1;
    CHECK(request(io[3],CD_GETHISTORYPREFS,&p)==IOERR_NOCMD&&io[3]->io_Actual==0);
    CHECK(!request(io[0],CD_SETDEFAULTHISTORYPREFS,&original));
    CHECK(value(io[0],CD_GETDEFAULTHISTORYPREFS,original.hp_Lines));
    rc=0;
out:
    if(haveOriginal && request(io[0],CD_SETDEFAULTHISTORYPREFS,&original))rc=20;
    for(int i=3;i>=0;i--){if(opened[i])CloseDevice((struct IORequest*)io[i]);if(io[i])DeleteIORequest((struct IORequest*)io[i]);}
    if(port)DeleteMsgPort(port);
    printf("HISTORY_NATIVE_%s: defaults, old/new units, alternate primary budget, rejected range, unsupported unit, restoration\n",rc?"FAIL":"PASS");return rc;
}
