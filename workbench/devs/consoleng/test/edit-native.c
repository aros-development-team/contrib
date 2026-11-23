/* Visual native-device EDIT regression. Compare Actual with Expected on screen.
 * Kept open for capture through a resize and 120-second hold for owned-VM mouse resize testing. */
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
    const char *page =
        "\x1b[2J\x1b[HConsoleNG native device: immediate cell editing\r\n\r\n"
        "Insert three cells:\r\n"
        " Actual: ABCDEFGH\x1b[5D\x1b[3@!\r\n"
        " Expect: ABC!  DEFGH\r\n\r\n"
        "Delete three cells:\r\n"
        " Actual: ABCDEFGH\x1b[5D\x1b[3P!\r\n"
        " Expect: ABC!H\r\n\r\n"
        "Full-span insertion (must clear before resize):\r\n"
        "\x1b[13;1HNEXT ROW MUST REMAIN UNCHANGED"
        "\x1b[12;1H Actual: ABCDEFGH\x1b[5D\x1b[65535@"
        "\x1b[14;1H Expect: ABC\r\n\r\n"
        "Full-span deletion (must clear before resize):\r\n"
        "\x1b[18;1HNEXT ROW MUST REMAIN UNCHANGED"
        "\x1b[17;1H Actual: ABCDEFGH\x1b[5D\x1b[65535P"
        "\x1b[19;1H Expect: ABC\r\n\r\n"
        "Native retained-cell redraw; no terminal-port window.\r\n";
    window = OpenWindowTags(NULL, WA_Left,30, WA_Top,35,
        WA_Width,720, WA_Height,430,
        WA_Title,(IPTR)"ConsoleNG - native EDIT regression",
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
    Delay(250);
    SizeWindow(window, 20, 0);
    Delay(50);
    SizeWindow(window, -20, 0);
    puts("EDIT_NATIVE_RESIZED"); fflush(stdout);
    Delay(6000);
    CloseDevice((struct IORequest *)io);DeleteIORequest((struct IORequest *)io);
    DeleteMsgPort(port);CloseWindow(window);
    return 0;
}
