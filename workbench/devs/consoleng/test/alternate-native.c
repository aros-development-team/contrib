/* Native scrollback trimming/round-trip fixture. Hold for owned-VM mouse tests. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <devices/conunit.h>
#include <intuition/intuition.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
    struct Window *window;
    struct MsgPort *port;
    struct IOStdReq *io;
    const char *page = "\x1b%G\x1b]50;DejaVuSansMono,14\x07\x1b[2J\x1b[H";
    window = OpenWindowTags(NULL, WA_Left,30, WA_Top,35,
        WA_Width,720, WA_Height,430,
        WA_Title,(IPTR)"ConsoleNG - alternate screen regression",
        WA_DragBar,TRUE, WA_DepthGadget,TRUE, WA_SizeGadget,TRUE,
        WA_SizeBRight,TRUE, WA_MinWidth,160, WA_MinHeight,80,
        WA_MaxWidth,-1, WA_MaxHeight,-1, WA_Activate,TRUE,TAG_DONE);
    if (!window) return 20;
    port=CreateMsgPort();
    io=port?(struct IOStdReq *)CreateIORequest(port,sizeof(*io)):NULL;
    if (!io) {if(port)DeleteMsgPort(port); CloseWindow(window);return 20;}
    io->io_Data=window;io->io_Length=sizeof(*window);
    if(OpenDevice((CONST_STRPTR)"consoleng.device",CONU_CHARMAP,(struct IORequest *)io,0)) {
        DeleteIORequest((struct IORequest *)io);DeleteMsgPort(port);CloseWindow(window);return 20;
    }
    io->io_Command=CMD_WRITE;io->io_Data=(APTR)page;io->io_Length=strlen(page);
    DoIO((struct IORequest *)io);
    printf("EDIT_NATIVE_WRITE error=%ld actual=%lu\n",(long)io->io_Error,(unsigned long)io->io_Actual);
    fflush(stdout);
    for (int n=0;n<60;n++) {
        char line[96];snprintf(line,sizeof(line),"History row %04d - safe backward traversal after trimming\r\n",n);
        io->io_Command=CMD_WRITE;io->io_Data=(APTR)line;io->io_Length=strlen(line);
        DoIO((struct IORequest *)io);
        if (io->io_Error) {printf("SCROLL_WRITE_FAILED %d\n",n);break;}
    }
    puts("PRIMARY_READY");fflush(stdout);
    Delay(1000);
    const char *alt="\x1b[?1049h\x1b[2J\x1b[HAlternate screen - primary history must survive\r\nThis text must disappear on exit.\r\n\x1b" "7";
    io->io_Data=(APTR)alt;io->io_Length=strlen(alt);DoIO((struct IORequest *)io);
    puts("ALTERNATE_READY");fflush(stdout);Delay(1500);
    const char *back="\x1b[?1049lRESTORED PRIMARY CURSOR";
    io->io_Data=(APTR)back;io->io_Length=strlen(back);DoIO((struct IORequest *)io);
    puts("PRIMARY_RESTORED");fflush(stdout);Delay(3000);
    CloseDevice((struct IORequest *)io);DeleteIORequest((struct IORequest *)io);
    DeleteMsgPort(port);CloseWindow(window);
    return 0;
}
