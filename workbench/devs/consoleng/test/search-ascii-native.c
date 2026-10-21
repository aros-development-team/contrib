/* Native named ASCII class search through the real console input task. */
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
static BOOL negated;
static int multiline;
static BOOL crlf, ascii_mode, named_mode, property_mode, category_mode, short_property_mode, script_mode, binary_mode, special_mode, value_mode, set_mode;
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
    /* [[:digit:]]+ using the native US keymap, not direct search injection. */
    static const UWORD codes[]={0x1a,0x1a,0x29,0x22,0x17,0x24,0x17,0x14,0x29,0x1b,0x1b,0x0c};
    static const UWORD quals[]={0,0,IEQUALIFIER_LSHIFT,0,0,0,0,0,IEQUALIFIER_LSHIFT,0,0,IEQUALIFIER_LSHIFT};
    static int regex_enabled;
    if(!key(0x23,IEQUALIFIER_CONTROL|IEQUALIFIER_LSHIFT))return 0;
    if(!regex_enabled){if(!key(0x13,IEQUALIFIER_CONTROL))return 0;regex_enabled=1;}
    if(set_mode){
        /* [0-9&&[1-8]]+ through the native keymap. */
        const UWORD c[]={0x1a,0x0a,0x0b,0x09,0x07,0x07,0x1a,0x01,0x0b,0x08,0x1b,0x1b,0x0c};
        for(unsigned i=0;i<sizeof(c)/sizeof(*c);i++)if(!key(c[i],(i==4||i==5||i==12)?IEQUALIFIER_LSHIFT:0))return 0;
        return found();
    }
    if(value_mode){
        const UWORD c[]={0x0d,0x19,0x1a,0x20,0x24,0x12,0x0c,0x01,RAWKEY_PERIOD,0x01,0x1b,0x0c};
        const UWORD q[]={0,0,IEQUALIFIER_LSHIFT,IEQUALIFIER_LSHIFT,0,0,0,0,0,0,IEQUALIFIER_LSHIFT,IEQUALIFIER_LSHIFT};
        for(unsigned i=0;i<sizeof(c)/sizeof(*c);i++)if(!key(c[i],q[i]))return 0;
        return found();
    }
    if(special_mode){
        const UWORD c[]={0x0d,0x19,0x1a,0x20,0x21,0x33,0x17,0x17,0x1b,0x0c};
        for(unsigned i=0;i<sizeof(c)/sizeof(*c);i++)if(!key(c[i],i<2?0:IEQUALIFIER_LSHIFT))return 0;
        return found();
    }
    if(script_mode || binary_mode){
        /* \p{Latin}+ through the native US keymap. */
        const UWORD c[]={0x0d,0x19,0x1a,binary_mode?0x20:0x28,binary_mode?0x28:0x20,binary_mode?0x19:0x14,binary_mode?0x25:0x17,binary_mode?0x20:0x36,0x1b,0x0c};
        const UWORD q[]={0,0,IEQUALIFIER_LSHIFT,IEQUALIFIER_LSHIFT,0,0,0,0,IEQUALIFIER_LSHIFT,IEQUALIFIER_LSHIFT};
        for(unsigned i=0;i<sizeof(c)/sizeof(*c);i++)if(!key(c[i],q[i]))return 0;
        return found();
    }
    if(property_mode || category_mode || short_property_mode){
        /* \p{Nd}+ or category \p{N}+ through the native US keymap. */
        const UWORD c[]={0x0d,0x19,0x1a,0x36,0x22,0x1b,0x0c};
        const UWORD q[]={0,0,IEQUALIFIER_LSHIFT,IEQUALIFIER_LSHIFT,0,IEQUALIFIER_LSHIFT,IEQUALIFIER_LSHIFT};
        for(unsigned i=0;i<sizeof(c)/sizeof(*c);i++){if((category_mode || short_property_mode) && i==4)continue;if(short_property_mode && (i==2 || i==5))continue;if(!key(c[i],q[i]))return 0;}
        return found();
    }
    if(multiline){
        if(!key(0x09,IEQUALIFIER_LSHIFT)||!key(0x3a,IEQUALIFIER_LSHIFT))return 0; /* (? */
        if(named_mode){
            if(!key(0x38,IEQUALIFIER_LSHIFT)||!key(0x36,0)||!key(0x39,IEQUALIFIER_LSHIFT))return 0; /* <n> */
        }else{
        if((multiline<0||ascii_mode)&&!key(0x0b,0))return 0; /* - */
        if(!key(ascii_mode?0x16:0x37,0))return 0; /* u or m */
        if(crlf&&!key(0x13,IEQUALIFIER_LSHIFT))return 0; /* R */
        if(!key(0x29,IEQUALIFIER_LSHIFT))return 0; /* : */
        }
        if(!key(0x06,IEQUALIFIER_LSHIFT))return 0; /* ^ */
    }
    for(unsigned i=0;i<sizeof(codes)/sizeof(*codes);i++){
        if(negated&&i==3&&!key(0x06,IEQUALIFIER_LSHIFT))return 0; /* ^ */
        if(!key(codes[i],quals[i]))return 0;
    }
    if(multiline&&!key(0x0a,IEQUALIFIER_LSHIFT))return 0; /* ) */
    return found();
}
#define CHECK(c) do{if(!(c)){printf("SEARCH_ASCII_FAIL line=%d title=%s\n",__LINE__,window&&window->Title?(char*)window->Title:"none");fflush(stdout);Delay(500);goto out;}}while(0)
int main(int argc,char **argv)
{
    negated=argc>1&&!strcmp(argv[1],"negated");
    crlf=argc>1&&!strcmp(argv[1],"crlf");
    ascii_mode=argc>1&&!strcmp(argv[1],"ascii");
    property_mode=argc>1&&!strcmp(argv[1],"property");
    special_mode=argc>1&&!strcmp(argv[1],"special");
    set_mode=argc>1&&!strcmp(argv[1],"sets");
    value_mode=argc>1&&!strcmp(argv[1],"value");
    binary_mode=argc>1&&!strcmp(argv[1],"binary");
    script_mode=argc>1&&!strcmp(argv[1],"script");
    short_property_mode=argc>1&&!strcmp(argv[1],"shortproperty");
    category_mode=argc>1&&!strcmp(argv[1],"category");
    named_mode=argc>1&&!strcmp(argv[1],"named");
    multiline=argc>1&&(!strcmp(argv[1],"multiline")||!strcmp(argv[1],"crlf")||ascii_mode||named_mode)?1:
        argc>1&&!strcmp(argv[1],"singleline")?-1:0;
    int rc=20;struct InfoData info;char text[100],got;
    stream=Open((CONST_STRPTR)"RGCON:",MODE_OLDFILE);CHECK(stream);
    struct FileHandle *fh=BADDR(stream);
    CHECK(DoPkt(fh->fh_Type,ACTION_DISK_INFO,(SIPTR)MKBADDR(&info),0,0,0,0));
    struct IOStdReq *io=(void*)(IPTR)info.id_InUse;CHECK(io&&io->io_Unit);
    unit=(Object*)io->io_Unit;base=(void*)io->io_Device;ConsoleDevice=io->io_Device;window=CU(unit)->cu_Window;CHECK(window);
    CyberGfxBase=OpenLibrary((CONST_STRPTR)"cybergraphics.library",0);CHECK(CyberGfxBase);
    SetWindowTitles(window,(CONST_STRPTR)"ConsoleNG named ASCII class search",(CONST_STRPTR)-1);
    MoveWindow(window,30-window->LeftEdge,30-window->TopEdge);SizeWindow(window,730-window->Width,450-window->Height);Delay(50);
    ActivateWindow(window);Delay(10);CHECK(SetMode(stream,TRUE));
    CHECK(write_text("\x1b%G\x1b[?25h\x1b[6 q\x1b]50;DejaVuSansMono,16\7\x1b[2J\x1b[H"));
    CHECK(write_text("12345 on live screen\r\n"));
    ULONG live_pixel=cursor_pixel();
    CHECK(live_pixel!=ReadRGBPixel(window->RPort,CP_X(unit)+CU(unit)->cu_XRSize-1,CP_Y(unit)));
    CHECK(query());CHECK(cursor_pixel()==live_pixel);
    CHECK(key(RAWKEY_ESCAPE,0));CHECK(cursor_pixel()==live_pixel);
    CHECK(write_text("\x1b[2J\x1b[H"));
    for(int i=0;i<80;i++){
        snprintf(text,sizeof(text),multiline?"%s\r\n":"History: %s\r\n",i==12?"67890 first match":i==58?"12345 second match":"retained output remains searchable");
        CHECK(write_text(text));
    }
    live_pixel=cursor_pixel();
    LONG live_x=CU(unit)->cu_XCP,live_y=CU(unit)->cu_YCP;
    /* A complement or script match can highlight the sampled pixel. Preserve
     * the logical application cursor in that mode; retain the stronger pixel
     * check for the original digit-only fixture. */
#define CURSOR_UNCHANGED() ((negated || script_mode || binary_mode || special_mode || value_mode) ? (CU(unit)->cu_XCP==live_x&&CU(unit)->cu_YCP==live_y) : cursor_pixel()==live_pixel)
    CHECK(query());CHECK(!WaitForChar(stream,20000));
    puts("SEARCH_ASCII_SHOT first");fflush(stdout);Delay(100);
    CHECK(key(RAWKEY_RETURN,0));CHECK(found());CHECK(!WaitForChar(stream,20000));
    CHECK(CURSOR_UNCHANGED());
    puts("SEARCH_ASCII_SHOT next");fflush(stdout);Delay(100);
    CHECK(key(RAWKEY_RETURN,IEQUALIFIER_LSHIFT));CHECK(found());
    puts("SEARCH_ASCII_SHOT previous");fflush(stdout);Delay(100);
    CHECK(key(RAWKEY_RETURN,IEQUALIFIER_LSHIFT));CHECK(found());
    CHECK(CURSOR_UNCHANGED());
    puts("SEARCH_ASCII_SHOT wrapped");fflush(stdout);Delay(100);
    CHECK(key(RAWKEY_ESCAPE,0));CHECK(!strcmp((char*)window->Title,"ConsoleNG named ASCII class search"));CHECK(!WaitForChar(stream,20000));
    if(script_mode || binary_mode || special_mode || value_mode)CHECK(cursor_pixel()==live_pixel);
    CHECK(key(0x32,0));CHECK(WaitForChar(stream,5000000)&&Read(stream,&got,1)==1&&got=='x');CHECK(!WaitForChar(stream,20000));
    CHECK(query());CHECK(write_text("new output cancels borrowed search state\r\n"));CHECK(!strcmp((char*)window->Title,"ConsoleNG named ASCII class search"));
    CHECK(query());SizeWindow(window,-40,-20);Delay(30);CHECK(!strcmp((char*)window->Title,"ConsoleNG named ASCII class search"));
    CHECK(query());CHECK(!WaitForChar(stream,20000));
    printf("SEARCH_ASCII_PASS %s query next previous wrap escape no-input-leak output-cancel resize-cancel\n",(negated||script_mode||binary_mode||special_mode||value_mode)?"logical-cursor":"live-cursor-pixels");fflush(stdout);Delay(1500);rc=0;
out:
    if(stream){SetMode(stream,FALSE);Close(stream);}
    if(CyberGfxBase)CloseLibrary(CyberGfxBase);
    printf("SEARCH_ASCII_SCRIPT rc=%d\n",rc);return rc;
}
