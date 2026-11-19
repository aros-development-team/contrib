/* Failed writes must not consume a split control sequence or change pixels. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/cybergraphics.h>
#include <exec/errors.h>
#include <devices/conunit.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct Library *CyberGfxBase;
static struct IOStdReq *io;
static struct Window *window;
static struct ConUnit *unit;
static ULONG original[32768];
static int send(const char *s, ULONG length, int error)
{
    io->io_Command = CMD_WRITE;
    io->io_Data = (APTR)s;
    io->io_Length = length;
    io->io_Actual = 0xdeadbeef;
    LONG result = DoIO((struct IORequest *)io);
    return result == error && io->io_Error == error &&
        io->io_Actual == (error ? 0 : length);
}
static int put(const char *s) { return send(s, strlen(s), 0); }
static int putz(const char *s)
{
    io->io_Command = CMD_WRITE; io->io_Data = (APTR)s;
    io->io_Length = (ULONG)-1; io->io_Actual = 0;
    return !DoIO((struct IORequest *)io) && !io->io_Error &&
        io->io_Actual == strlen(s);
}
static int pixels(int save)
{
    ULONG at = 0;
    for (int y = 0; y < 3 * unit->cu_YRSize; y++)
        for (int x = 0; x < 20 * unit->cu_XRSize; x++) {
            ULONG p = ReadRGBPixel(window->RPort, unit->cu_XROrigin + x,
                unit->cu_YROrigin + y);
            if (at >= 32768) return 0;
            if (save) original[at] = p;
            else if (original[at] != p) { printf("RETRY_PIXEL x=%d y=%d expected=%08lx actual=%08lx\n", x, y, (unsigned long)original[at], (unsigned long)p); return 0; }
            at++;
        }
    return 1;
}
#define CHECK(c) do { if (!(c)) { printf("RETRY_FAIL line=%d font=%d sequence=%d split=%d\n", __LINE__, font, sequence, split); fflush(stdout); Delay(500); goto out; } } while (0)
int main(int argc, char **argv)
{
    int kind = argc > 1 ? atoi(argv[1]) : CONU_CHARMAP;
    int rc = 20, opened = 0, font = 0, sequence = 0, split = 0, cases = 0;
    struct MsgPort *port = CreateMsgPort();
    io = port ? (void *)CreateIORequest(port, sizeof(*io)) : NULL;
    CHECK(io);
    CyberGfxBase = OpenLibrary((CONST_STRPTR)"cybergraphics.library", 0);
    CHECK(CyberGfxBase);
    window = OpenWindowTags(NULL, WA_Left, 30, WA_Top, 30, WA_Width, 730,
        WA_Height, 470, WA_Title, (IPTR)"ConsoleNG split-write recovery",
        WA_Activate, TRUE, WA_DragBar, TRUE, WA_DepthGadget, TRUE,
        WA_SizeGadget, TRUE, WA_SizeBRight, TRUE, TAG_DONE);
    CHECK(window);
    io->io_Data = window; io->io_Length = sizeof(*window);
    CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device", kind,
        (struct IORequest *)io, 0));
    opened = 1; unit = (void *)io->io_Unit;
    const char *fonts[] = {"\x1b]50;topaz,8\7", "\x1b]50;DejaVuSansMono.font,16\7"};
    const char *sequences[] = {
        "\x1b[31mRed\x1b[0m", "\x1b[38;2;50;120;200mRGB\x1b[0m",
        "\x1b[2;3HMoved", "\x1b[4:3;58;5;10mWave\x1b[0m",
        "\x1b%G\xe4\xb8\xad\xf0\x9f\x98\x80",
        "\x1b[1;3;9mStyle\x1b[0m", "\x1b%@Latin-1: \xa3\x1b%G",
        "\x1b[1mgjpqyWf\x1b[0m", "\x1b[3mgjpqyWf\x1b[0m",
        "\x1b[1;3;4:3;58;5;10mgjpqy\x1b[0m",
        "\x1b[1;3mgjp\x1b[Dqy\x1b[0m",
        "\x1b[1;3mgjp\x1b[2Kqy\x1b[0m",
        "\x1b%@\x1b[1;3m\xa3gyp\x1b%G",
        "\x1b[1;3m\xc2\xa3gyp\x1b[0m",
        "\x1b[1;3mgj\x1b[4hpy\x1b[4l\x1b[0m",
        "\x1b[1;3mgj\x1b[2;1Hpy\x1b[0m"
    };
    const char *reset = "\x1b[?25l\x1b%G\x1b[0m\x1b[2J\x1b[H";
    for (font = 0; font < 2; font++) {
        CHECK(put(fonts[font]));
        for (sequence = 0; sequence < sizeof(sequences)/sizeof(sequences[0]); sequence++) {
            int length = strlen(sequences[sequence]);
            for (split = 1; split < length; split++) {
                CHECK(put(reset)); CHECK(putz(sequences[sequence]));
                LONG x = unit->cu_XCP, y = unit->cu_YCP;
                CHECK(pixels(1)); CHECK(put(reset));
                CHECK(send(sequences[sequence], split, 0));
                CHECK(send(sequences[sequence] + split, length - split, 0));
                CHECK(unit->cu_XCP == x && unit->cu_YCP == y);
                CHECK(pixels(0));
                CHECK(pixels(1)); CHECK(put(reset));
                CHECK(send(sequences[sequence], split, 0));
                CHECK(send("x", 0x80000000UL, IOERR_BADLENGTH));
                CHECK(send("x", 0xfffffffeUL, IOERR_BADLENGTH));
                CHECK(send(NULL, 1, IOERR_BADADDRESS));
                CHECK(send(NULL, 0, 0));
                struct Unit *saved = io->io_Unit;
                io->io_Unit = NULL;
                int rejected = send("x", 1, IOERR_BADADDRESS);
                io->io_Unit = (struct Unit *)CONU_LIBRARY;
                rejected = rejected && send("x", 1, IOERR_BADADDRESS);
                io->io_Unit = saved;
                CHECK(rejected);
                CHECK(send(sequences[sequence] + split, length - split, 0));
                CHECK(unit->cu_XCP == x && unit->cu_YCP == y && pixels(0));
                cases++;
            }
        }
    }
    CHECK(put("\x1b[0m\x1b[2J\x1b[HPASS: split-write recovery\r\n\r\nOversized / null writes reject without consuming state.\r\nEmpty writes preserve pending controls and UTF-8.\r\nRetry matches uninterrupted split writes.\r\nTopaz / DejaVu; RGB, styles, CJK and emoji.\r\nWhole and fragmented styled output also agree."));
    printf("RETRY_PASS unit=%d cases=%d\n", kind, cases); fflush(stdout); Delay(600); rc = 0;
out:
    if (opened) CloseDevice((struct IORequest *)io);
    if (window) CloseWindow(window);
    if (io) DeleteIORequest((struct IORequest *)io);
    if (port) DeleteMsgPort(port);
    if (CyberGfxBase) CloseLibrary(CyberGfxBase);
    printf("RETRY_SCRIPT rc=%d\n", rc);
    return rc;
}
