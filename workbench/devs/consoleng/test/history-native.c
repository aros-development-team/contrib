/* Visual native-device EDIT regression. Compare Actual with Expected on screen.
 * Kept open for capture through a resize and 30-second hold for owned-VM mouse resize testing. */
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
        "\x1b%G\x1b]50;DejaVuSansMono,14\x07\x1b[2J\x1b[H"
        "ConsoleNG: clear saved history, preserve live screen\r\n\r\n"
        "This text and the coffee glyph must survive ED 3.\r\n"
        "Control: A☕BC\r\n\r\n"
        "The scrollbar should lose its old history.\r\n"
        "The live viewport and cursor must stay put.\r\n";
    window = OpenWindowTags(NULL, WA_Left,30, WA_Top,35,
        WA_Width,720, WA_Height,430,
        WA_Title,(IPTR)"ConsoleNG - native saved-history regression",
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
    for (int n=0;n<60;n++) {
        const char *old="OLD HISTORY MUST BE REMOVED\r\n";
        io->io_Command=CMD_WRITE;io->io_Data=(APTR)old;io->io_Length=strlen(old);
        if (DoIO((struct IORequest *)io)) return 20;
    }
    io->io_Command=CMD_WRITE;io->io_Data=(APTR)page;io->io_Length=strlen(page);
    DoIO((struct IORequest *)io);
    printf("EDIT_NATIVE_WRITE error=%ld actual=%lu\n",(long)io->io_Error,(unsigned long)io->io_Actual);
    fflush(stdout);
    Delay(250);
    io->io_Command=CMD_WRITE;io->io_Data=(APTR)"\x1b[3J";io->io_Length=4;
    DoIO((struct IORequest *)io);
    printf("HISTORY_ERASE error=%ld actual=%lu\n",(long)io->io_Error,(unsigned long)io->io_Actual);fflush(stdout);
    SizeWindow(window, 20, 0);
    Delay(50);
    SizeWindow(window, -20, 0);
    puts("EDIT_NATIVE_RESIZED"); fflush(stdout);
    Delay(1500);
    CloseDevice((struct IORequest *)io);DeleteIORequest((struct IORequest *)io);
    DeleteMsgPort(port);CloseWindow(window);
    return 0;
}
