/* Native half-page shortcuts compared with repeated one-line viewport moves. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <stdio.h>
#include <string.h>
#include "../console_gcc.h"
/* This standalone probe opens its window before opening the console device. */
#undef IntuitionBase
struct Device *ConsoleDevice;
static struct ConsoleBase *base;
static struct IOStdReq *io;
static struct Window *window;
static Object *unit;
static int put(const char *s)
{
    io->io_Command=CMD_WRITE;io->io_Data=(APTR)s;io->io_Length=strlen(s);
    return !DoIO((struct IORequest *)io);
}
static int event(UWORD code,UWORD qualifiers)
{
    struct cdihMessage *m=AllocMem(sizeof(*m),MEMF_PUBLIC|MEMF_CLEAR);
    if(!m)return 0;
    m->msg.mn_Length=sizeof(*m);m->unit=unit;
    m->unitGeneration=ICU(unit)->inputGeneration;
    m->ie.ie_Class=IECLASS_RAWKEY;m->ie.ie_Code=code;m->ie.ie_Qualifier=qualifiers;
    ObtainSemaphore(&base->consoleTaskLock);
    PutMsg(base->consIHData.inputPort,(struct Message *)m);
    ReleaseSemaphore(&base->consoleTaskLock);
    Delay(3);return 1;
}
static int key(UWORD code){return event(code,0)&&event(code|IECODE_UP_PREFIX,0);}
static ULONG pixels(void)
{
    struct ConUnit *u=CU(unit);ULONG h=2166136261UL;
    for(int y=u->cu_YROrigin;y<=u->cu_YRExtant;y++)
        for(int x=u->cu_XROrigin;x<=u->cu_XRExtant;x++)
            h=(h^(ULONG)ReadPixel(window->RPort,x,y))*16777619UL;
    return h;
}
#define CHECK(c) do{if(!(c)){printf("HALF_HISTORY_FAIL line=%d geometry=%d cases=%d\n",__LINE__,geometry,cases);fflush(stdout);Delay(300);goto out;}}while(0)
int main(void)
{
    int rc=20,opened=0,geometry=0,cases=0;
    struct MsgPort *port=CreateMsgPort();
    io=port?(APTR)CreateIORequest(port,sizeof(*io)):NULL;CHECK(io);
    window=OpenWindowTags(NULL,WA_Left,30,WA_Top,35,WA_Width,730,WA_Height,450,
        WA_Title,(IPTR)"ConsoleNG half-page history shortcuts",
        WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_SizeGadget,TRUE,WA_SizeBRight,TRUE,
        WA_MinWidth,160,WA_MinHeight,80,WA_MaxWidth,-1,WA_MaxHeight,-1,
        WA_Activate,TRUE,TAG_DONE);CHECK(window);
    io->io_Data=window;io->io_Length=sizeof(*window);
    CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",CONU_CHARMAP,(struct IORequest *)io,0));
    opened=1;unit=(Object *)io->io_Unit;base=(APTR)io->io_Device;ConsoleDevice=io->io_Device;
    struct ConsolengBindingPrefs bindings={sizeof(bindings),9,6,{{0}}};
    const ULONG actions[]={CONSOLENG_BIND_LINEUP,CONSOLENG_BIND_LINEDOWN,
        CONSOLENG_BIND_TOP,CONSOLENG_BIND_BOTTOM,CONSOLENG_BIND_HALFUP,CONSOLENG_BIND_HALFDOWN};
    for(unsigned i=0;i<6;i++)bindings.bp_Entries[i]=(struct ConsolengKeyBinding){CONSOLENG_BIND_RAW+0x50+i,0,actions[i]};
    io->io_Command=CD_SETBINDINGPREFS;io->io_Data=&bindings;io->io_Length=sizeof(bindings);
    CHECK(!DoIO((struct IORequest *)io)&&io->io_Actual==sizeof(bindings));
    CHECK(put("\x1b[?25l\x1b]50;DejaVuSansMono,16\x07\x1b[38;2;230;230;230m\x1b[48;2;32;63;78m"));
    for(geometry=0;geometry<2;geometry++){
        if(geometry){SizeWindow(window,0,-83);Delay(25);}
        CHECK(put("\x1b[3J\x1b[2J\x1b[H"));
        for(unsigned row=0;row<120;row++){char s[100];snprintf(s,sizeof(s),"History row %03u: native half-page scrolling\r\n",row);CHECK(put(s));}
        ULONG half=((ULONG)CU(unit)->cu_YMax+1)/2;
        WORD x=CU(unit)->cu_XCP,y=CU(unit)->cu_YCP;CHECK(half>0);
        CHECK(key(0x53));ULONG bottom=pixels();
        for(ULONG n=0;n<half;n++)CHECK(key(0x50));
        ULONG expected=pixels();CHECK(expected!=bottom);CHECK(key(0x53));CHECK(key(0x54));CHECK(pixels()==expected);cases++;
        CHECK(key(0x52));ULONG top=pixels();
        for(ULONG n=0;n<half;n++)CHECK(key(0x51));
        expected=pixels();CHECK(expected!=top);CHECK(key(0x52));CHECK(key(0x55));CHECK(pixels()==expected);cases++;
        CHECK(key(0x53));
        for(ULONG n=0;n<3*half;n++)CHECK(key(0x50));
        expected=pixels();CHECK(key(0x53));
        CHECK(event(0x54,0));CHECK(event(0x54,IEQUALIFIER_REPEAT));CHECK(event(0x54,IEQUALIFIER_REPEAT));CHECK(event(0x54|IECODE_UP_PREFIX,0));
        CHECK(pixels()==expected);cases++;
        CHECK(key(0x52));CHECK(key(0x54));CHECK(pixels()==top);
        CHECK(key(0x53));CHECK(key(0x55));CHECK(pixels()==bottom);cases++;
        CHECK(CU(unit)->cu_XCP==x&&CU(unit)->cu_YCP==y&&!ICU(unit)->numStoredChars);
        CHECK(put("\x1b[>9h"));CHECK(key(0x54));CHECK(key(0x55));CHECK(pixels()==bottom);
        CHECK(put("\x1b[>9l"));CHECK(pixels()==bottom);cases++;
        CHECK(put("\x1b[?1049hAlternate screen unchanged"));ULONG alternate=pixels();
        CHECK(key(0x54));CHECK(key(0x55));CHECK(pixels()==alternate);CHECK(put("\x1b[?1049l"));cases++;
        CHECK(key(0x53));CHECK(key(0x54));
    }
    SetWindowTitles(window,(CONST_STRPTR)"Half-page history: resize / repeat / bounds PASS",(CONST_STRPTR)-1);
    puts("HALF_HISTORY_SHOT");fflush(stdout);Delay(100);
    printf("HALF_HISTORY_PASS cases=%d exact raster vs line actions, resize, held-repeat, bounds, cursor/input and hidden/alternate isolation\n",cases);fflush(stdout);
    rc=0;
out:
    if(opened)CloseDevice((struct IORequest *)io);
    if(window)CloseWindow(window);
    if(io)DeleteIORequest((struct IORequest *)io);
    if(port)DeleteMsgPort(port);
    printf("HALF_HISTORY_SCRIPT rc=%d\n",rc);return rc;
}
