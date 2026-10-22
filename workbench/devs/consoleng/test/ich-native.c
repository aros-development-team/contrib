/* Visual native-device ICH regression. Compare Actual with Expected on screen.
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
        "\x1b[2J\x1b[HConsoleNG native device: counted insertion\r\n\r\n"
        "Insert three cells without moving cursor:\r\n"
        " Actual: ABCDEFGH\x1b[5D\x1b[3@!\r\n"
        " Expect: ABC!  DEFGH\r\n\r\n"
        "Zero means one cell:\r\n"
        " Actual: ABCDEFGH\x1b[5D\x1b[0@\r\n"
        " Expect: ABC DEFGH\r\n\r\n"
        "Default means one cell:\r\n"
        " Actual: ABCDEFGH\x1b[5D\x1b[@\r\n"
        " Expect: ABC DEFGH\r\n\r\n"
        "Large count clips at right edge:\r\n"
        "\x1b[18;1HNEXT ROW MUST REMAIN UNCHANGED"
        "\x1b[17;1H Actual: ABCDEFGH\x1b[5D\x1b[65535@"
        "\x1b[19;1H Expect: ABC\r\n\r\n"
        "Native device I/O; resize checks retained text.\r\n";
    window = OpenWindowTags(NULL, WA_Left,30, WA_Top,35,
        WA_Width,720, WA_Height,430,
        WA_Title,(IPTR)"ConsoleNG - native ICH regression",
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
    printf("ICH_NATIVE_WRITE error=%ld actual=%lu\n",(long)io->io_Error,(unsigned long)io->io_Actual);
    fflush(stdout);
    Delay(250);
    SizeWindow(window, 20, 0);
    Delay(50);
    SizeWindow(window, -20, 0);
    puts("ICH_NATIVE_RESIZED"); fflush(stdout);
    Delay(1500);
    CloseDevice((struct IORequest *)io);DeleteIORequest((struct IORequest *)io);
    DeleteMsgPort(port);CloseWindow(window);
    return 0;
}
