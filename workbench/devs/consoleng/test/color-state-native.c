/* Native retained palette state across alternate screen and saved cursor. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <devices/conunit.h>
#include <stdio.h>
#include <string.h>
static int put(struct IOStdReq *io,const char *s)
{
    io->io_Command=CMD_WRITE;io->io_Data=(APTR)s;io->io_Length=strlen(s);
    return !DoIO((struct IORequest*)io);
}
static int background(struct IOStdReq *io,ULONG expected)
{
    struct ConUnit *unit=(void*)io->io_Unit;ULONG rgb[3];
    GetRGB32(unit->cu_Window->WScreen->ViewPort.ColorMap,(UBYTE)unit->cu_BgPen,1,rgb);
    ULONG actual=((rgb[0]>>24)<<16)|((rgb[1]>>24)<<8)|(rgb[2]>>24);
    printf("COLOR_BACKGROUND actual=%06lx expected=%06lx\n",(unsigned long)actual,(unsigned long)expected);fflush(stdout);
    return actual==expected;
}
#define CHECK(x) do {if(!(x)){printf("COLOR_FAIL line=%d\n",__LINE__);goto out;}}while(0)
int main(void)
{
    int rc=20,opened=0;struct Window *w=NULL;struct MsgPort *p=CreateMsgPort();
    struct IOStdReq *io=p?(void*)CreateIORequest(p,sizeof(*io)):NULL;
    CHECK(io);
    w=OpenWindowTags(NULL,WA_Left,30,WA_Top,30,WA_Width,730,WA_Height,500,
        WA_Title,(IPTR)"ConsoleNG palette and saved-screen regression",WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_SizeGadget,TRUE,
 WA_MinWidth,200,WA_MinHeight,120,WA_MaxWidth,~0UL,WA_MaxHeight,~0UL,WA_Activate,TRUE,TAG_DONE);CHECK(w);
    io->io_Data=w;io->io_Length=sizeof(*w);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",CONU_CHARMAP,(struct IORequest*)io,0));opened=1;
    CHECK(put(io,"\x1b]10;#e0e0e0\x07\x1b]11;#203040\x07\x1b[2J\x1b[HPRIMARY: retained text must follow the palette change.\r\n\x1b" "7"));
    CHECK(background(io,0x203040));
    CHECK(put(io,"\x1b[?1049h\x1b]11;#402050\x07\x1b[2J\x1b[HALTERNATE: background changed while primary is dormant.\r\n\x1b[?1049l"));
    CHECK(background(io,0x402050));
    CHECK(put(io,"NEW OUTPUT: must have the same background as PRIMARY above.\r\n\x1b" "7\x1b]11;#204050\x07\x1b" "8"));
    CHECK(background(io,0x204050));
    CHECK(put(io,"SAVED CURSOR: restored rendition follows the updated palette.\r\n\r\nPASS: primary and saved cursor retain the current colors.\r\n"));
    puts("COLOR_NATIVE_PASS");fflush(stdout);Delay(50*12);
    CHECK(put(io,"\x1b[2J\x1b[H"));
    for(int i=0;i<150;i++){char line[90];snprintf(line,sizeof(line),"History row %03d - palette query must preserve the browsed viewport.\r\n",i);CHECK(put(io,line));}
    puts("COLOR_QUERY_READY");fflush(stdout);Delay(50*25);
    CHECK(put(io,"\x1b]11;?\x07"));
    puts("COLOR_QUERY_DONE");fflush(stdout);Delay(50*20);rc=0;
out:
    if(opened)CloseDevice((struct IORequest*)io);
    if(w)CloseWindow(w);
    if(io)DeleteIORequest((struct IORequest*)io);
    if(p)DeleteMsgPort(p);
    printf("COLOR_SCRIPT rc=%d\n",rc);return rc;
}
