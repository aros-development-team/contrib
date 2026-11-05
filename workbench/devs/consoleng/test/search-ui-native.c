/* Native retained-history search through the real console input task. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/cybergraphics.h>
#include <dos/dosextens.h>
#include <devices/rawkeycodes.h>
#include <stdio.h>
#include <string.h>
#include "../console_gcc.h"
struct Device *ConsoleDevice;
struct Library *CyberGfxBase;
static struct ConsoleBase *base;
static Object *unit;
static BPTR stream;
static struct Window *window;
static ULONG cursor_pixel(void)
{
    return ReadRGBPixel(window->RPort,CP_X(unit),CP_Y(unit));
}
static int write_text(const char *s){return Write(stream,s,strlen(s))==(LONG)strlen(s);}
static int key(UWORD code,UWORD qual)
{
    for(int up=0;up<2;up++){
        struct cdihMessage *m=AllocMem(sizeof(*m),MEMF_PUBLIC|MEMF_CLEAR);
        if(!m)return 0;
        m->msg.mn_Length=sizeof(*m);m->unit=unit;m->unitGeneration=ICU(unit)->inputGeneration;
        m->ie.ie_Class=IECLASS_RAWKEY;m->ie.ie_Code=code|(up?IECODE_UP_PREFIX:0);m->ie.ie_Qualifier=qual;
        ObtainSemaphore(&base->consoleTaskLock);PutMsg(base->consIHData.inputPort,(struct Message*)m);ReleaseSemaphore(&base->consoleTaskLock);
    }
    Delay(3);return 1;
}
static int found(void)
{
    for(int i=0;i<100;i++){
        if(window->Title&&strstr((const char*)window->Title,"[found;"))return 1;
        Delay(2);
    }
    return 0;
}
static int query(void)
{
    static const UWORD codes[]={0x36,0x12,0x12,0x22,0x28,0x12}; /* needle */
    if(!key(0x23,IEQUALIFIER_CONTROL|IEQUALIFIER_LSHIFT))return 0;
    for(unsigned i=0;i<sizeof(codes)/sizeof(*codes);i++)if(!key(codes[i],0))return 0;
    return found();
}
#define CHECK(c) do{if(!(c)){printf("SEARCH_UI_FAIL line=%d title=%s\n",__LINE__,window&&window->Title?(char*)window->Title:"none");fflush(stdout);Delay(500);goto out;}}while(0)
int main(void)
{
    int rc=20;struct InfoData info;char text[100],got;
    stream=Open((CONST_STRPTR)"RGCON:",MODE_OLDFILE);CHECK(stream);
    struct FileHandle *fh=BADDR(stream);
    CHECK(DoPkt(fh->fh_Type,ACTION_DISK_INFO,(SIPTR)MKBADDR(&info),0,0,0,0));
    struct IOStdReq *io=(void*)(IPTR)info.id_InUse;CHECK(io&&io->io_Unit);
    unit=(Object*)io->io_Unit;base=(void*)io->io_Device;ConsoleDevice=io->io_Device;window=CU(unit)->cu_Window;CHECK(window);
    CyberGfxBase=OpenLibrary((CONST_STRPTR)"cybergraphics.library",0);CHECK(CyberGfxBase);
    SetWindowTitles(window,(CONST_STRPTR)"ConsoleNG search native regression",(CONST_STRPTR)-1);
    MoveWindow(window,30-window->LeftEdge,30-window->TopEdge);SizeWindow(window,730-window->Width,450-window->Height);Delay(50);
    ActivateWindow(window);Delay(10);CHECK(SetMode(stream,TRUE));
    CHECK(write_text("\x1b%G\x1b[?25h\x1b[6 q\x1b]50;DejaVuSansMono,16\7\x1b[2J\x1b[H"));
    CHECK(write_text("needle on live screen\r\n"));
    ULONG live_pixel=cursor_pixel();
    CHECK(live_pixel!=ReadRGBPixel(window->RPort,CP_X(unit)+CU(unit)->cu_XRSize-1,CP_Y(unit)));
    CHECK(query());CHECK(cursor_pixel()==live_pixel);
    CHECK(key(RAWKEY_ESCAPE,0));CHECK(cursor_pixel()==live_pixel);
    CHECK(write_text("\x1b[2J\x1b[H"));
    for(int i=0;i<80;i++){
        snprintf(text,sizeof(text),"History %02d: %s\r\n",i,i==12?"NEEDLE first match":i==58?"needle second match":"retained output remains searchable");
        CHECK(write_text(text));
    }
    live_pixel=cursor_pixel();
    CHECK(query());CHECK(!WaitForChar(stream,20000));
    puts("SEARCH_UI_SHOT first");fflush(stdout);Delay(100);
    CHECK(key(RAWKEY_RETURN,0));CHECK(found());CHECK(!WaitForChar(stream,20000));
    CHECK(cursor_pixel()==live_pixel);
    puts("SEARCH_UI_SHOT next");fflush(stdout);Delay(100);
    CHECK(key(RAWKEY_RETURN,IEQUALIFIER_LSHIFT));CHECK(found());
    puts("SEARCH_UI_SHOT previous");fflush(stdout);Delay(100);
    CHECK(key(RAWKEY_RETURN,IEQUALIFIER_LSHIFT));CHECK(found());
    CHECK(cursor_pixel()==live_pixel);
    puts("SEARCH_UI_SHOT wrapped");fflush(stdout);Delay(100);
    CHECK(key(RAWKEY_ESCAPE,0));CHECK(!strcmp((char*)window->Title,"ConsoleNG search native regression"));CHECK(!WaitForChar(stream,20000));
    CHECK(key(0x32,0));CHECK(WaitForChar(stream,5000000)&&Read(stream,&got,1)==1&&got=='x');CHECK(!WaitForChar(stream,20000));
    CHECK(query());CHECK(write_text("new output cancels borrowed search state\r\n"));CHECK(!strcmp((char*)window->Title,"ConsoleNG search native regression"));
    CHECK(query());SizeWindow(window,-40,-20);Delay(30);CHECK(!strcmp((char*)window->Title,"ConsoleNG search native regression"));
    CHECK(query());CHECK(!WaitForChar(stream,20000));
    puts("SEARCH_UI_PASS live-cursor-pixels query next previous wrap escape no-input-leak output-cancel resize-cancel");fflush(stdout);Delay(600);rc=0;
out:
    if(stream){SetMode(stream,FALSE);Close(stream);}
    if(CyberGfxBase)CloseLibrary(CyberGfxBase);
    printf("SEARCH_UI_SCRIPT rc=%d\n",rc);return rc;
}
