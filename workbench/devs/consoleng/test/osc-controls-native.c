/* Native binary CMD_WRITE OSC controls: title, font and bounded collector. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <devices/conunit.h>
#include <stdio.h>
#include <string.h>
static struct IOStdReq *io;
static int send(const void *s, ULONG n, int split)
{
    const UBYTE *p=s;
    while(n) {
        ULONG size=split?1:n;
        io->io_Command=CMD_WRITE;io->io_Data=(APTR)p;io->io_Length=size;
        if(DoIO((struct IORequest*)io)||io->io_Actual!=size)return 0;
        p+=size;n-=size;
    }
    return 1;
}
#define TEXT(s) send(s,sizeof(s)-1,0)
#define CHECK(c) do { if(!(c)){printf("OSC_CONTROL_FAIL line=%d kind=%d ctrl=%d hidden=%d split=%d st=%d\n",__LINE__,kind,ctrl,hidden,split,st);fflush(stdout);Delay(500);goto out;} } while(0)
int main(void)
{
    int rc=20,opened=0,kind=0,ctrl=0,hidden=0,split=0,st=0,cases=0;
    struct Window *w=NULL;struct MsgPort *p=CreateMsgPort();
    io=p?(void*)CreateIORequest(p,sizeof(*io)):NULL;CHECK(io);
    for(kind=0;kind<2;kind++) {
        w=OpenWindowTags(NULL,WA_Left,30,WA_Top,30,WA_Width,730,WA_Height,470,
            WA_Title,(IPTR)"ConsoleNG OSC controls",WA_DragBar,TRUE,
            WA_DepthGadget,TRUE,WA_SizeGadget,TRUE,WA_SizeBRight,TRUE,
            WA_MinWidth,200,WA_MinHeight,160,WA_MaxWidth,-1,WA_MaxHeight,-1,TAG_DONE);
        CHECK(w);io->io_Data=w;io->io_Length=sizeof(*w);
        CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",kind,(struct IORequest*)io,0));opened=1;
        for(hidden=0;hidden<2;hidden++)for(split=0;split<2;split++)for(st=0;st<2;st++)for(ctrl=0;ctrl<32;ctrl++) {
            UBYTE seq[64];ULONG n=0;
            if(ctrl==7||ctrl==24||ctrl==26||ctrl==27)continue;
            CHECK(TEXT("\x1b]2;Anchor\7"));
            CHECK(send(hidden?"\x1b[>9h":"\x1b[>9l",5,0));
            memcpy(seq,"\x1b]2;Control",11);n=11;seq[n++]=ctrl;
            memcpy(seq+n," PASS",5);n+=5;seq[n++]=st?27:7;if(st)seq[n++]='\\';
            CHECK(send(seq,n,split));
            CHECK(!strcmp((char*)w->Title,hidden?"Anchor":"Control PASS"));
            CHECK(TEXT("\x1b[>9l"));CHECK(!strcmp((char*)w->Title,"Control PASS"));
            /* Force a different size first, so ignoring a malformed font request
             * cannot accidentally pass just because the old font already matched. */
            CHECK(TEXT("\x1b]50;DejaVuSansMono,16\7"));
            CHECK(((struct ConUnit*)io->io_Unit)->cu_YRSize==16);
            n=sizeof("\x1b]50;DejaVu")-1;memcpy(seq,"\x1b]50;DejaVu",n);seq[n++]=ctrl;
            memcpy(seq+n,"SansMono,12\7",sizeof("SansMono,12\7")-1);n+=sizeof("SansMono,12\7")-1;CHECK(send(seq,n,split));
            CHECK(((struct ConUnit*)io->io_Unit)->cu_YRSize==12);
            cases++;
        }
        for(int overflow=0;overflow<2;overflow++) {
            UBYTE body[800];memset(body,overflow?'x':0,sizeof(body));
            CHECK(TEXT("\x1b]2;Anchor\7"));CHECK(TEXT("\x1b]2;"));
            CHECK(send(body,sizeof(body),0));memset(body,0,sizeof(body));CHECK(send(body,sizeof(body),0));
            CHECK(TEXT("Capacity PASS\7"));CHECK(!strcmp((char*)w->Title,overflow?"Anchor":"Capacity PASS"));cases++;
        }
        printf("OSC_CONTROL_UNIT kind=%d cases=%d PASS\n",kind,cases);fflush(stdout);
        CHECK(TEXT("\x1b[?25l\x1b]2;ConsoleNG - binary OSC controls\7\x1b[2J\x1b[HPASS: ignored control bytes preserve OSC payloads.\r\nAll 28 controls, BEL/ST and fragmented writes.\r\nVisible and hidden titles; native DejaVu font requests.\r\nLong ignored runs and overflow discard verified.\r\nBoth standard and retained console units tested."));
        if(kind){printf("OSC_CONTROL_PASS cases=%d\n",cases);fflush(stdout);Delay(600);}
        CloseDevice((struct IORequest*)io);opened=0;CloseWindow(w);w=NULL;
    }
    rc=0;
out:
    if(opened)CloseDevice((struct IORequest*)io);
    if(w)CloseWindow(w);
    if(io)DeleteIORequest((struct IORequest*)io);
    if(p)DeleteMsgPort(p);
    printf("OSC_CONTROL_SCRIPT rc=%d\n",rc);return rc;
}
