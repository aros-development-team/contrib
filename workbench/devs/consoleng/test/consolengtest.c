/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: Open consoleng.device on a window and render into it, so that "the
          device works" is something you can look at rather than a build log.
*/

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>

#include <exec/io.h>
#include <exec/memory.h>
#include <devices/conunit.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>

#include <stdio.h>
#include <string.h>

/* Written straight into the unit, so each line is exactly what the device's
 * own parser sees. Kept deliberately mixed: plain text, an SGR colour run,
 * a cursor-position CSI, and a UTF-8 run above Latin-1 - the last one is the
 * interesting case, because that is the path the device's local TextUTF8
 * stand-in handles. */
static const char *lines[] =
{
    "consoleng.device -- second console, stock console untouched\r\n",
    "\r\n",
    "plain ASCII: The quick brown fox jumps over the lazy dog 0123456789\r\n",
    "\x1b[1mbold\x1b[0m  \x1b[31mred\x1b[0m  \x1b[32mgreen\x1b[0m  "
        "\x1b[34mblue\x1b[0m  \x1b[7mreverse\x1b[0m\r\n",
    "\r\n",
    "Latin-1 still goes through Text(): \xc3\xa4 \xc3\xb6 \xc3\xbc \xc3\x9f\r\n",
    "\r\n",

    /* Symbols above U+00FF. Every character here must appear as a real glyph;
     * anything that comes out as a bare capital letter is a REASON CODE, not a
     * character - see CONSOLENG_GLYPH_* in console_gcc.h ('E' = the face never
     * opened, 'F' = the face is fine but has no such glyph).
     *
     * Worth remembering how this line was chosen. An earlier version used
     * Greek alpha/beta/gamma/delta and Cyrillic A/Be/Ve/Ge against Vera Mono,
     * they all came back 'F', and that looked like the glyph path failing.
     * It was not: Bitstream Vera is a Latin-only face (282 codepoints, checked
     * by parsing its cmap) and does not contain them - while pi and omega, the
     * only two Greek letters it does have, rendered perfectly. The engine was
     * right and the TEST was wrong. That is why this file now states which
     * face is expected to cover what, instead of assuming. */
    "Symbols above Latin-1:\r\n",
    "  \xcf\x80 \xce\xa9 \xe2\x82\xac \xe2\x84\xa2 \xe2\x88\x9e \xe2\x88\x91 "
        "\xe2\x88\x9a \xe2\x89\xa4 \xe2\x89\xa5 \xe2\x89\xa0 \xe2\x80\xa2 "
        "\xe2\x80\xa6 \xc5\x82 \xef\xac\x81\r\n",
    "  (pi omega euro tm infinity sigma root le ge ne bullet "
        "ellipsis l-stroke fi)\r\n",
    "\r\n",

    /* THE POINT OF ALL OF THIS - a real TUI frame. This is the shape Ink
     * draws, and Ink is what the Claude Code CLI renders with, so a box that
     * joins up here is the actual goal rather than a glyph-by-glyph demo. */
    "A real TUI frame (what Ink draws):\r\n",
    "  \xe2\x94\x8c\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
        "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
        "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
        "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x90\r\n",
    "  \xe2\x94\x82 consoleng \xe2\x9c\x93 ready \xe2\x94\x82\r\n",
    "  \xe2\x94\x9c\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
        "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
        "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
        "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\xa4\r\n",
    "  \xe2\x94\x82 \xe2\x96\x88\xe2\x96\x88\xe2\x96\x88\xe2\x96\x92\xe2\x96\x92"
        "\xe2\x96\x91\xe2\x96\x91 62%  \xe2\x86\x92    \xe2\x94\x82\r\n",
    "  \xe2\x94\x94\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
        "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
        "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
        "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x98\r\n",
    "\r\n",
    "Greek/Cyrillic: \xce\xb1 \xce\xb2 \xce\xb3 \xce\xb4  "
        "\xd0\x90 \xd0\x91 \xd0\x92 \xd0\x93\r\n",

    /* The control. Braille is the ONE thing DejaVu Sans Mono still lacks
     * (verified against its cmap), and Ink's default spinner is braille - so
     * this 'F' is a real, known limitation rather than a defect, and keeping
     * it on screen stops it being rediscovered as a bug later. */
    "Braille (absent even from DejaVu - Ink's spinner): "
        "\xe2\xa0\x8b\xe2\xa0\x99\xe2\xa0\xb9\r\n",
    NULL
};

int main(int argc, char **argv)
{
    struct MsgPort   *port  = NULL;
    struct IOStdReq  *ioreq = NULL;
    struct Window    *win   = NULL;
    BOOL              opened = FALSE;
    BOOL              selfwin = FALSE;
    int               i;

    /* "SELFWIN" exercises the other open path: hand the device NO window and
     * let it open one itself. That is exactly how rom/filesys/
     * console_handler's plain-device mode opens a device (io_Data = NULL), so
     * this argument is the standing proof that a mount can route a CON:-style
     * stream at consoleng.device without any change to that handler. */
    if (argc > 1 && argv[1] &&
        (argv[1][0] == 'S' || argv[1][0] == 's'))
        selfwin = TRUE;

    /* "CONNG:" (or any DOS path) drives the WHOLE mount chain instead of the
     * device directly: DOS -> aux-handler -> con-handler device mode ->
     * OpenDevice("consoleng.device", 3, io_Data = NULL) -> the device opens
     * its own window. That is the path a Shell or a TUI actually takes, and
     * unlike a screenshot it gives a verdict in words - useful precisely when
     * another window is covering the screen. */
    if (argc > 1 && argv[1] && strchr(argv[1], ':'))
    {
        BPTR fh = Open(argv[1], MODE_NEWFILE);

        if (!fh)
        {
            printf("CONSOLENG-TEST: FAIL Open(\"%s\") failed, ioerr=%ld\n",
                   argv[1], (long)IoErr());
            return RETURN_FAIL;
        }

        printf("CONSOLENG-TEST: PASS Open(\"%s\") succeeded\n", argv[1]);

        /* UTF-8 mode, then the outline font, then the corpus - same stream a
           terminal would send. */
        {
            static const char utf8on[]  = "\x1b[>8h";
            static const char setfont[] = "\x1b]50;DejaVuSansMono,16\x07";

            Write(fh, (APTR)utf8on,  sizeof(utf8on)  - 1);
            Write(fh, (APTR)setfont, sizeof(setfont) - 1);
        }

        for (i = 0; lines[i]; i++)
            Write(fh, (APTR)lines[i], strlen(lines[i]));

        printf("CONSOLENG-TEST: PASS wrote %d lines through %s\n", i, argv[1]);

        /* Hold the stream open so the window stays up to be looked at. */
        Delay(50 * 25);

        Close(fh);
        printf("CONSOLENG-TEST: PASS closed %s\n", argv[1]);
        return RETURN_OK;
    }

    if (selfwin)
    {
        printf("CONSOLENG-TEST: SELFWIN - passing no window, device opens its own\n");
        goto have_window;
    }

    win = OpenWindowTags(NULL,
        WA_Left,        20,
        WA_Top,         20,
        WA_Width,       600,
        WA_Height,      360,
        WA_Title,       (IPTR)"consoleng.device test",
        WA_CloseGadget, TRUE,
        WA_DragBar,     TRUE,
        WA_DepthGadget, TRUE,
        WA_Activate,    TRUE,
        WA_IDCMP,       IDCMP_CLOSEWINDOW,
        TAG_DONE);

    if (!win)
    {
        printf("CONSOLENG-TEST: FAIL could not open a window\n");
        return RETURN_FAIL;
    }

have_window:
    port = CreateMsgPort();
    if (port)
        ioreq = (struct IOStdReq *)CreateIORequest(port, sizeof(struct IOStdReq));

    if (!ioreq)
    {
        printf("CONSOLENG-TEST: FAIL no IO request\n");
        goto cleanup;
    }

    /* The whole point: open the NEW device by name. Nothing here touches
     * console.device, and this program is the only thing selecting consoleng
     * until a handler does it. */
    ioreq->io_Data = (APTR)win;
    ioreq->io_Length = sizeof(struct Window);

    if (OpenDevice("consoleng.device", CONU_CHARMAP,
                   (struct IORequest *)ioreq, 0) != 0)
    {
        printf("CONSOLENG-TEST: FAIL OpenDevice(consoleng.device) rc!=0\n");
        goto cleanup;
    }
    opened = TRUE;

    printf("CONSOLENG-TEST: opened consoleng.device unit=%p\n",
           (void *)ioreq->io_Unit);

    /* Ask the unit to interpret the stream as UTF-8. The sequence is the
     * device's own private CSI >8h (support.c: str_su8 = 0x3E 0x38 0x68) -
     * NOT >1h, which is what the first version of this test guessed and which
     * left the unit in byte mode, rendering every UTF-8 sequence as two or
     * three Latin-1 characters of mojibake. */
    {
        static const char utf8on[] = "\x1b[>8h";
        ioreq->io_Command = CMD_WRITE;
        ioreq->io_Data    = (APTR)utf8on;
        ioreq->io_Length  = sizeof(utf8on) - 1;
        DoIO((struct IORequest *)ioreq);
    }

    /* Switch to an OUTLINE font via OSC 50. This is not decoration: the
     * default console font is a bitmap font with no .otag, so it has no
     * outline engine and NOTHING above Latin-1 can be rasterised from it -
     * the glyph path correctly falls back to a placeholder. Exercising the
     * engine at all requires a font that has one.
     *
     * DejaVuSansMono, not "Vera Mono". Vera is monospaced and has an .otag,
     * but it is a Latin-only face: parsing its cmap shows 282 codepoints, and
     * box drawing, arrows, blocks, Cyrillic and all but two Greek letters are
     * simply absent - so a TUI drawn in it is blank cells, however good the
     * glyph path is. DejaVu is Vera's upstream-maintained successor with the
     * same metrics and 3260 codepoints covering the ranges a terminal needs.
     * The name is deliberately space-free so CON:-style FONT=/OSC 50 options
     * need no quoting. */
    {
        static const char setfont[] = "\x1b]50;DejaVuSansMono,16\x07";
        ioreq->io_Command = CMD_WRITE;
        ioreq->io_Data    = (APTR)setfont;
        ioreq->io_Length  = sizeof(setfont) - 1;
        DoIO((struct IORequest *)ioreq);
        printf("CONSOLENG-TEST: requested outline font via OSC 50\n");
    }

    for (i = 0; lines[i]; i++)
    {
        ioreq->io_Command = CMD_WRITE;
        ioreq->io_Data    = (APTR)lines[i];
        ioreq->io_Length  = strlen(lines[i]);
        if (DoIO((struct IORequest *)ioreq) != 0)
            printf("CONSOLENG-TEST: write %d returned error\n", i);
    }

    printf("CONSOLENG-TEST: PASS rendered %d lines into consoleng.device\n", i);

    /*
     * INPUT. Everything above this point is the write path; a TUI also has to
     * READ. consoleng inherits the stock console's input chain (the device's
     * input handler fills a per-unit buffer from real key events, and CMD_READ
     * drains it), but "inherits" is an assumption until a keystroke has
     * actually come back out, so this asks for one.
     *
     * SendIO rather than DoIO: a console read blocks until something is typed,
     * and a test that hangs forever when input is broken is worse than one
     * that reports nothing arrived. Poll, then abort cleanly on timeout.
     *
     * Keys are injected by the harness (AROS_ACTIONS_FILE "key <rawcode>"),
     * which delivers them to the ACTIVE window - hence the ActivateWindow
     * below. In SELFWIN mode the device owns the window and this program has
     * no pointer to it, so the read test only runs when we opened it.
     */
    if (win)
    {
        static UBYTE inbuf[64];
        LONG waited = 0;
        LONG got = -1;

        ActivateWindow(win);

        {
            static const char prompt[] =
                "\r\nINPUT TEST - type something (harness injects keys):\r\n";
            ioreq->io_Command = CMD_WRITE;
            ioreq->io_Data    = (APTR)prompt;
            ioreq->io_Length  = sizeof(prompt) - 1;
            DoIO((struct IORequest *)ioreq);
        }

        ioreq->io_Command = CMD_READ;
        ioreq->io_Data    = (APTR)inbuf;
        ioreq->io_Length  = sizeof(inbuf) - 1;
        SendIO((struct IORequest *)ioreq);

        /* ~20s, which is inside the harness deadline and well after the
           injected keys are due. */
        while (waited < 400 && !CheckIO((struct IORequest *)ioreq))
        {
            Delay(5);
            waited++;
        }

        if (CheckIO((struct IORequest *)ioreq))
        {
            WaitIO((struct IORequest *)ioreq);
            got = ioreq->io_Actual;
        }
        else
        {
            AbortIO((struct IORequest *)ioreq);
            WaitIO((struct IORequest *)ioreq);
        }

        if (got > 0)
        {
            LONG n;

            inbuf[got] = '\0';
            printf("CONSOLENG-TEST: PASS read %ld byte(s) from consoleng:",
                   (long)got);
            for (n = 0; n < got; n++)
                printf(" %02x", inbuf[n]);
            printf(" (\"%s\")\n", inbuf);

            /* Echo it back so the screenshot shows the round trip too. */
            ioreq->io_Command = CMD_WRITE;
            ioreq->io_Data    = (APTR)"read back: ";
            ioreq->io_Length  = 11;
            DoIO((struct IORequest *)ioreq);
            ioreq->io_Command = CMD_WRITE;
            ioreq->io_Data    = (APTR)inbuf;
            ioreq->io_Length  = got;
            DoIO((struct IORequest *)ioreq);
        }
        else
        {
            printf("CONSOLENG-TEST: FAIL no input arrived within %ld ticks"
                   " (read aborted)\n", (long)waited);
        }
    }

    /* Hold the window so a screenshot can be taken, but do not hang a test
     * harness forever: give up after a bounded wait if nobody closes it. */
    {
        ULONG sigs;
        ULONG waited = 0;

        while (waited < 120)
        {
            sigs = CheckSignal(1L << port->mp_SigBit);
            (void)sigs;
            /* In SELFWIN mode we own no window, so there is no UserPort to
               poll - just hold for the full wait so the device's own window
               can be seen and screenshotted. */
            if (win && GetMsg(win->UserPort))
                break;
            Delay(50);              /* 1s */
            waited++;
        }
    }

cleanup:
    if (opened)
        CloseDevice((struct IORequest *)ioreq);
    if (ioreq)
        DeleteIORequest((struct IORequest *)ioreq);
    if (port)
        DeleteMsgPort(port);
    if (win)
        CloseWindow(win);

    return RETURN_OK;
}
