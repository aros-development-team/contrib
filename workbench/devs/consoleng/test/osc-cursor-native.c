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


        CHECK(TEXT("\x1b]50;DejaVuSansMono,16\7"));
        for(hidden=0;hidden<2;hidden++)for(split=0;split<2;split++)for(ctrl=0;ctrl<=6;ctrl++)for(st=0;st<2;st++)for(int shape=0;shape<4;shape++) {
            struct ConsolengCursorPrefs prefs={sizeof(prefs),1,ctrl,350,8000},got;
            io->io_Command=CD_SETCURSORPREFS;io->io_Data=&prefs;io->io_Length=sizeof(prefs);CHECK(!DoIO((struct IORequest*)io));
            CHECK(send(hidden?"\x1b[>9h":"\x1b[>9l",5,0));
            char seq[64];int n=snprintf(seq,sizeof(seq),"\x1b]50;CursorShape=%d",shape);
            seq[n++]=st?27:7;if(st)seq[n++]='\\';
            CHECK(send(seq,n,split));
            io->io_Command=CD_GETCURSORPREFS;io->io_Data=&got;io->io_Length=sizeof(got);CHECK(!DoIO((struct IORequest*)io));
            static const ULONG styles[]={2,6,4};
            prefs.cp_Style=shape<3?styles[shape]-(ctrl&1):ctrl;
            CHECK(!memcmp(&prefs,&got,sizeof(got)));
            CHECK(((struct ConUnit*)io->io_Unit)->cu_YRSize==16);
            CHECK(TEXT("\x1b[>9l"));cases++;
        }
        CHECK(TEXT("\x1b" "c"));
        struct ConsolengCursorPrefs got;
        io->io_Command=CD_GETCURSORPREFS;io->io_Data=&got;io->io_Length=sizeof(got);CHECK(!DoIO((struct IORequest*)io));CHECK(got.cp_Style==6);
        printf("OSC_CONTROL_UNIT kind=%d cases=%d PASS\n",kind,cases);fflush(stdout);
        CHECK(TEXT("\x1b]50;DejaVuSansMono,16\7\x1b]2;ConsoleNG - OSC cursor shapes\7\x1b[2J\x1b[HPASS: OSC 50 CursorShape=0 / 1 / 2\r\nBlock, beam and underline preserve blink settings.\r\nInvalid shapes leave the cursor unchanged.\r\nFont, blink interval and idle timeout stay unchanged.\r\nWhole and bytewise writes; BEL and ST terminators.\r\nBoth standard and retained units; hidden and visible.\r\nRIS restores the configured cursor default.\r\n\x1b]50;CursorShape=1\7"));
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
