/* Native CAN/SUB OSC termination: collected command and trailing text. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <devices/conunit.h>
#include <stdio.h>
#include <string.h>
#include "consoleng.h"
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



        struct ConsolengColorPrefs baseline,got;
        CHECK(TEXT("\x1b]4;1;#112233;2;#223344\7"));
        io->io_Command=CD_GETCOLORPREFS;io->io_Data=&baseline;io->io_Length=sizeof(baseline);CHECK(!DoIO((struct IORequest*)io));
        const char *bad[]={"-1","+1","1x","16x","256","2147483648","4294967296","18446744073709551616","999999999999999999999999999999999999999999"};
        for(hidden=0;hidden<2;hidden++)for(split=0;split<2;split++)for(st=0;st<2;st++)for(ctrl=0;ctrl<9;ctrl++) {
            io->io_Command=CD_SETCOLORPREFS;io->io_Data=&baseline;io->io_Length=sizeof(baseline);CHECK(!DoIO((struct IORequest*)io));
            CHECK(send(hidden?"\x1b[>9h":"\x1b[>9l",5,0));
            char seq[180];int n=snprintf(seq,sizeof(seq),"\x1b]4;%s;#abcdef",bad[ctrl]);seq[n++]=st?27:7;if(st)seq[n++]='\\';CHECK(send(seq,n,split));
            n=snprintf(seq,sizeof(seq),"\x1b]104;%s",bad[ctrl]);seq[n++]=st?27:7;if(st)seq[n++]='\\';CHECK(send(seq,n,split));
            CHECK(TEXT("\x1b]4;1;rgb:1/2/3/4\7"));
            io->io_Command=CD_GETCOLORPREFS;io->io_Data=&got;io->io_Length=sizeof(got);CHECK(!DoIO((struct IORequest*)io));CHECK(!memcmp(&got,&baseline,sizeof(got)));
            n=snprintf(seq,sizeof(seq),"\x1b]4;%s;#abcdef;2;#334455",bad[ctrl]);seq[n++]=st?27:7;if(st)seq[n++]='\\';CHECK(send(seq,n,split));
            io->io_Command=CD_GETCOLORPREFS;io->io_Data=&got;io->io_Length=sizeof(got);CHECK(!DoIO((struct IORequest*)io));CHECK(got.cp_RGB[4]==0x334455);
            n=snprintf(seq,sizeof(seq),"\x1b]104;%s;2",bad[ctrl]);seq[n++]=st?27:7;if(st)seq[n++]='\\';CHECK(send(seq,n,split));
            io->io_Command=CD_GETCOLORPREFS;io->io_Data=&got;io->io_Length=sizeof(got);CHECK(!DoIO((struct IORequest*)io));CHECK(!(got.cp_Mask&(1UL<<4))&&got.cp_RGB[3]==baseline.cp_RGB[3]);
            CHECK(TEXT("\x1b[>9l"));cases++;
        }
        printf("OSC_CONTROL_UNIT kind=%d cases=%d PASS\n",kind,cases);fflush(stdout);
        CHECK(TEXT("\x1b]104\7\x1b]50;DejaVuSansMono,16\7\x1b]2;ConsoleNG - palette validation\7\x1b[2J\x1b[HPASS: malformed palette indices leave colors unchanged.\r\nLong integers cannot wrap into palette array indices.\r\nValid entries after rejected entries still apply.\r\nRGB values with extra components are rejected.\r\nWhole and bytewise writes; BEL and ST terminators.\r\nBoth standard and retained units; hidden and visible."));
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
