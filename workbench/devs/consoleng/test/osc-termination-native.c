/* Native CAN/SUB OSC termination: collected command and trailing text. */
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

        for(hidden=0;hidden<2;hidden++)for(split=0;split<2;split++)for(ctrl=24;ctrl<=26;ctrl+=2)for(st=0;st<2;st++) {
            UBYTE seq[900];ULONG n;
            CHECK(TEXT("\x1b[>9l\x1b]2;Anchor\7\x1b[2J\x1b[H"));
            CHECK(send(hidden?"\x1b[>9h":"\x1b[>9l",5,0));
            const char *prefix="\x1b]2;";n=strlen(prefix);memcpy(seq,prefix,n);
            if(st){memset(seq+n,'x',800);n+=800;}
            else {memcpy(seq+n,"Terminated PASS",15);n+=15;}
            seq[n++]=ctrl;seq[n++]='X';
            CHECK(send(seq,n,split));
            CHECK(((struct ConUnit*)io->io_Unit)->cu_XCP==1);
            CHECK(!strcmp((char*)w->Title,hidden||st?"Anchor":"Terminated PASS"));
            CHECK(TEXT("\x1b[>9l"));CHECK(!strcmp((char*)w->Title,st?"Anchor":"Terminated PASS"));
            CHECK(TEXT("\x1b]50;DejaVuSansMono,16\7\x1b[H"));
            CHECK(((struct ConUnit*)io->io_Unit)->cu_YRSize==16);
            prefix="\x1b]50;DejaVuSansMono,12";n=strlen(prefix);memcpy(seq,prefix,n);seq[n++]=ctrl;seq[n++]='X';
            CHECK(send(seq,n,split));CHECK(((struct ConUnit*)io->io_Unit)->cu_YRSize==12);
            CHECK(((struct ConUnit*)io->io_Unit)->cu_XCP==1);
            cases++;
        }

        /* Every two-piece boundary, including lone ESC and partial CSI. */
        const char *tails[]={"\x1b[3;4HX","\x1b" "EX","\x1b\\X","\x1b]2;Second\7X"};
        for(ctrl=0;ctrl<4;ctrl++)for(hidden=0;hidden<2;hidden++)for(st=0;st<2;st++) {
            char seq[900];strcpy(seq,"\x1b]2;");
            if(st){memset(seq+4,'a',300);seq[304]=0;}else strcat(seq,"First");
            strcat(seq,tails[ctrl]);int length=strlen(seq);
            for(split=0;split<=length;split++){
                CHECK(TEXT("\x1b[>9l\x1b]2;Anchor\7\x1b[H"));
                if(hidden)CHECK(TEXT("\x1b[>9h"));
                CHECK(send(seq,split,0)&&send(seq+split,length-split,0));
                struct ConUnit *u=(struct ConUnit*)io->io_Unit;
                CHECK(u->cu_XCP==(ctrl==0?4:1)&&u->cu_YCP==(ctrl==0?2:ctrl==1?1:0));
                const char *title=ctrl==3?"Second":st?"Anchor":"First";
                CHECK(!strcmp((char*)w->Title,hidden?"Anchor":title));
                CHECK(TEXT("\x1b[>9l"));CHECK(!strcmp((char*)w->Title,title));cases++;
            }
        }
        printf("OSC_CONTROL_UNIT kind=%d cases=%d PASS\n",kind,cases);fflush(stdout);
        CHECK(TEXT("\x1b[?25l\x1b]50;DejaVuSansMono,16\7\x1b]2;ConsoleNG - OSC termination\7\x1b[2J\x1b[HPASS: CAN and SUB end OSC strings.\r\nFollowing text remains visible and advances the cursor.\r\nTitle and font requests apply before text resumes.\r\nOverflowed strings discard safely and resume output.\r\nWhole and bytewise writes; visible and hidden titles.\r\nESC preserves CSI, next-line, ST and a following OSC.\r\nBoth standard and retained console units tested."));
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
