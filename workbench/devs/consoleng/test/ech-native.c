/* Visual native-device ECH regression. Compare Actual with Expected on screen.
 * Kept open for capture through a resize and 30-second hold; no input or shared desktop required. */
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
        "\x1b[2J\x1b[HConsoleNG native device: character erasure\r\n\r\n"
        "Three cells, cursor stays put:\r\n"
        " Actual: ABCDEFGH\x1b[5D\x1b[3X!\r\n"
        " Expect: ABC!  GH\r\n\r\n"
        "Zero means one cell:\r\n"
        " Actual: ABCDEFGH\x1b[5D\x1b[0X\r\n"
        " Expect: ABC EFGH\r\n\r\n"
        "Default means one cell:\r\n"
        " Actual: ABCDEFGH\x1b[5D\x1b[X\r\n"
        " Expect: ABC EFGH\r\n\r\n"
        "Short line: colored erased cells must survive redraw\r\n"
        " Actual: \x1b[44m\x1b[12X\x1b[0m\r\n\r\n"
        "Device I/O, not a terminal-port window.\r\n";
    window = OpenWindowTags(NULL, WA_Left,30, WA_Top,35,
        WA_Width,720, WA_Height,430,
        WA_Title,(IPTR)"ConsoleNG - native ECH regression",
        WA_DragBar,TRUE, WA_DepthGadget,TRUE, WA_Activate,TRUE,TAG_DONE);
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
    printf("ECH_NATIVE_WRITE error=%ld actual=%lu\n",(long)io->io_Error,(unsigned long)io->io_Actual);
    fflush(stdout);
    Delay(250);
    SizeWindow(window, 20, 0);
    Delay(50);
    SizeWindow(window, -20, 0);
    puts("ECH_NATIVE_RESIZED"); fflush(stdout);
    Delay(1500);
    CloseDevice((struct IORequest *)io);DeleteIORequest((struct IORequest *)io);
    DeleteMsgPort(port);CloseWindow(window);
    return 0;
}
