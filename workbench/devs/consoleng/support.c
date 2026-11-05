/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc: Support functions for console.device
*/

#define SDEBUG 0
#define DEBUG 0
#include <aros/debug.h>

#include <proto/exec.h>
#include <exec/lists.h>
#include <exec/io.h>
#include <exec/errors.h>
#include <exec/memory.h>

#include <proto/intuition.h>
#include <intuition/classes.h>

#include <devices/conunit.h>
#include <string.h>
#include <stdio.h>

#include "console_gcc.h"

#include "consoleif.h"

static BOOL getparamcommand(BYTE *cmd_ptr, UBYTE ** writestr_ptr,
    UBYTE *numparams_ptr, LONG toparse, IPTR *p_tab, Object *unit,
    struct ConsoleBase *ConsoleDevice);
static BOOL getansimodecommand(BYTE *cmd_ptr, UBYTE **writestr_ptr,
    UBYTE *numparams_ptr, LONG toparse, Object *unit);
static BOOL getstatusrequest(BYTE *command, UBYTE **input, UBYTE *count,
    LONG length, IPTR *params, BOOL native);
static BOOL getscrollregion(BYTE *command, UBYTE **input, UBYTE *count,
    LONG length, IPTR *params);
static BOOL getabsolutecommand(BYTE *command, UBYTE **input, UBYTE *count,
    LONG length, IPTR *params);
static BOOL getmoderequest(BYTE *cmd_ptr, UBYTE **writestr_ptr,
    UBYTE *numparams_ptr, LONG toparse, Object *unit, struct ConsoleBase *ConsoleDevice);
static BOOL getcursorstylecommand(BYTE *cmd_ptr, UBYTE **writestr_ptr,
    UBYTE *numparams_ptr, LONG toparse, IPTR *params);
static BOOL getdecprivatecommand(BYTE *cmd_ptr, UBYTE ** writestr_ptr,
    LONG toparse, Object *unit);
static BOOL string2command(BYTE *cmd_ptr, UBYTE ** writestr_ptr,
    UBYTE *numparams_ptr, LONG toparse, IPTR *p_tab, Object *unit,
    struct ConsoleBase *ConsoleDevice);

#define ESC 0x1B
#define CSI 0x9B

#define NIL             0x00
#define BELL            0x07
#define BACKSPACE       0x08
#define HTAB            0x09
#define LINEFEED        0x0A
#define VTAB            0x0B
#define FORMFEED        0x0C
#define CARRIAGE_RETURN 0x0D
#define SHIFT_OUT       0x0E
#define SHIFT_IN        0x0F
#define INDEX           0x84
#define NEXT_LINE       0x85
#define H_TAB_SET       0x88
#define REVERSE_INDEX   0x8D


#define FIRST_CSI_CMD 0x40

/***********************
**  writeToConsole()  **
***********************/

/*
** SGR is the command with most params: 4
**   stegerg: RKRMs say it can have any number of parameters in any order. So instead of 4
**            we assume and hope that there will never be more than 16 params :-\
*/

#define MAX_COMMAND_PARAMS 16

/* Lenient UTF-8 decoder that always makes forward progress.  A byte that
   is not part of a well-formed sequence (stray continuation byte, lead byte
   without its continuations) is taken as ONE Latin-1 character, so a
   legacy 8-bit producer writing to a UTF-8 unit (the default for this
   device) still shows its accented letters instead of '?'; only a
   sequence cut off at the end of the span, or a complete sequence that
   is overlong or does not encode a Unicode scalar, becomes '?'.
   WriteToConsole() stashes incomplete tails across CMD_WRITEs before they get
   here.  graphics.library TextUTF8() keeps its stricter '?' policy. */
ULONG con_utf8_decode(const UBYTE **sp, LONG *left)
{
    const UBYTE *p = *sp;
    LONG n = *left;
    ULONG cp;
    WORD need, i;
    UBYTE b = *p;

    if (b < 0x80)
    {
        cp = b;
        need = 1;
    }
    else if ((b & 0xE0) == 0xC0)
    {
        cp = b & 0x1F;
        need = 2;
    }
    else if ((b & 0xF0) == 0xE0)
    {
        cp = b & 0x0F;
        need = 3;
    }
    else if ((b & 0xF8) == 0xF0)
    {
        cp = b & 0x07;
        need = 4;
    }
    else
    {
        /* 0x80-0xBF continuation without a lead, or 0xF8-0xFF: Latin-1 */
        *sp = p + 1;
        *left = n - 1;
        return b;
    }

    if (need > n)
    {
        *sp = p + n;
        *left = 0;
        return '?';
    }

    for (i = 1; i < need; i++)
    {
        if ((p[i] & 0xC0) != 0x80)
        {
            /* lead byte not followed by continuations: Latin-1, and the
               bytes after it get their own turn */
            *sp = p + 1;
            *left = n - 1;
            return b;
        }
        cp = (cp << 6) | (p[i] & 0x3F);
    }

    *sp = p + need;
    *left = n - need;
    /* Complete encodings must be shortest-form Unicode scalars. Keep the
     * separate legacy-byte fallback above; malformed encodings must not
     * create NUL cells, surrogate glyph requests or out-of-range values. */
    if ((need == 2 && cp < 0x80) || (need == 3 && cp < 0x800) ||
        (need == 4 && cp < 0x10000) || (cp >= 0xd800 && cp <= 0xdfff) ||
        cp > 0x10ffff)
        return '?';
    return cp;
}

ULONG con_utf8_encode(UBYTE *buf, ULONG cp)
{
    if (cp < 0x80)
    {
        buf[0] = cp;
        return 1;
    }
    if (cp < 0x800)
    {
        buf[0] = 0xC0 | (cp >> 6);
        buf[1] = 0x80 | (cp & 0x3F);
        return 2;
    }
    if (cp < 0x10000)
    {
        buf[0] = 0xE0 | (cp >> 12);
        buf[1] = 0x80 | ((cp >> 6) & 0x3F);
        buf[2] = 0x80 | (cp & 0x3F);
        return 3;
    }
    buf[0] = 0xF0 | (cp >> 18);
    buf[1] = 0x80 | ((cp >> 12) & 0x3F);
    buf[2] = 0x80 | ((cp >> 6) & 0x3F);
    buf[3] = 0x80 | (cp & 0x3F);
    return 4;
}

ULONG con_utf8_count(const UBYTE *s, LONG len)
{
    ULONG n = 0;

    while (len > 0)
    {
        con_utf8_decode(&s, &len);
        n++;
    }
    return n;
}

/* Display cell width (wcwidth-style): emoji and East Asian Wide take
   two cells; ZWJ/variation selectors/combining marks take none. */
WORD con_cp_width(ULONG cp)
{
    /* zero-width: ZWJ, VS15/16, combining diacriticals */
    if (cp == 0x200D || cp == 0xFE0E || cp == 0xFE0F ||
        (cp >= 0x0300 && cp <= 0x036F))
        return 0;

    /* East Asian Wide / CJK */
    if ((cp >= 0x1100 && cp <= 0x115F) ||
        (cp >= 0x2E80 && cp <= 0x9FFF) ||
        (cp >= 0xAC00 && cp <= 0xD7A3) ||
        (cp >= 0xF900 && cp <= 0xFAFF) ||
        (cp >= 0xFF00 && cp <= 0xFF60))
        return 2;

    /* emoji planes */
    if (cp >= 0x1F000 && cp <= 0x1FAFF)
        return 2;

    /* BMP emoji with EastAsianWidth=Wide (the common ones) */
    if (cp == 0x231A || cp == 0x231B || (cp >= 0x23E9 && cp <= 0x23EC) ||
        cp == 0x25FD || cp == 0x25FE || cp == 0x2614 || cp == 0x2615 ||
        (cp >= 0x2648 && cp <= 0x2653) || cp == 0x267F || cp == 0x2693 ||
        cp == 0x26A1 || cp == 0x26AA || cp == 0x26AB || cp == 0x26BD ||
        cp == 0x26BE || cp == 0x26C4 || cp == 0x26C5 || cp == 0x26CE ||
        cp == 0x26D4 || cp == 0x26EA || cp == 0x26F2 || cp == 0x26F3 ||
        cp == 0x26F5 || cp == 0x26FA || cp == 0x26FD || cp == 0x2705 ||
        cp == 0x270A || cp == 0x270B || cp == 0x2728 || cp == 0x274C ||
        cp == 0x274E || (cp >= 0x2753 && cp <= 0x2755) || cp == 0x2757 ||
        (cp >= 0x2795 && cp <= 0x2797) || cp == 0x27B0 || cp == 0x27BF ||
        cp == 0x2B1B || cp == 0x2B1C || cp == 0x2B50 || cp == 0x2B55)
        return 2;

    return 1;
}

/* Cells (columns) a UTF-8 span will occupy */
ULONG con_utf8_cellcount(const UBYTE *s, LONG len)
{
    ULONG cells = 0;

    while (len > 0)
        cells += con_cp_width(con_utf8_decode(&s, &len));
    return cells;
}

static WORD utf8_seqlen(UBYTE lead)
{
    if ((lead & 0xE0) == 0xC0)
        return 2;
    if ((lead & 0xF0) == 0xE0)
        return 3;
    if ((lead & 0xF8) == 0xF0)
        return 4;
    return 1;
}

/* Unsupported device/application/privacy strings share a bounded discard
 * path. Legacy C1 introducers must never steal UTF-8 continuation bytes. */
static LONG discard_prefix(UBYTE *str, LONG length, BOOL utf8)
{
    if (length >= 2 && str[0] == ESC &&
        (str[1] == 'P' || str[1] == 'X' || str[1] == '^' || str[1] == '_'))
        return 2;
    if (length >= 1 && !utf8 &&
        (str[0] == 0x90 || str[0] == 0x98 || str[0] == 0x9e || str[0] == 0x9f))
        return 1;
    return 0;
}

/* Unsupported DCS/SOS/PM/APC strings are discarded without buffering.
 * ESC returns to normal escape dispatch, which handles ST and split escapes.
 * BEL is payload here, unlike OSC. C1 ST is only a control outside UTF-8. */
static LONG consume_dcs(Object *unit, UBYTE *str, LONG length)
{
    LONG used = 0;
    while (used < length)
    {
        UBYTE b = str[used];
        if (b == ESC)
        {
            ICU(unit)->dcsDiscard = FALSE;
            break;
        }
        used++;
        if (b == 0x18 || b == 0x1a ||
            (b == 0x9c && !CON_IS_UTF8(unit)))
        {
            ICU(unit)->dcsDiscard = FALSE;
            break;
        }
    }
    return used;
}

/* Accumulate an OSC string (started by ESC ] or C1 0x9D) until BEL,
   CAN/SUB or ESC, across CMD_WRITEs if necessary. ESC stays unconsumed for
   normal escape dispatch (including the ESC-backslash ST terminator). On
   completion, OSC 50 ("50;fontname[,size]") becomes C_SET_FONT with the
   NUL-terminated name (inside the unit's oscBuf) and the size as params.
   Everything else is consumed silently (C_NIL). Returns the number of
   bytes consumed; *cmd_ptr/*numparams/p_tab are set when done. */
static LONG consume_osc(Object *unit, UBYTE *str, LONG toparse,
    BYTE *cmd_ptr, UBYTE *numparams_ptr, IPTR *p_tab)
{
    struct intConUnit *icu = ICU(unit);
    LONG used = 0;
    BOOL done = FALSE, control_end = FALSE;

    *cmd_ptr = C_NIL;

    while (used < toparse && !done)
    {
        UBYTE b = str[used++];

        if (b == 0x07 || b == 0x18 || b == 0x1A)
        {
            /* CAN/SUB dispatch the collected OSC and return to ground. */
            control_end = b != 0x07;
            done = TRUE;
        }
        else if (b == ESC)
        {
            /* Dispatch at ESC, then let the normal parser consume the new
             * escape sequence (including ST). Leave ESC in the input so the
             * existing partial-sequence stash also handles split writes. */
            used--;
            control_end = TRUE;
            done = TRUE;
        }
        else if (b == 0x9C && !CON_IS_UTF8(unit))
        {
            control_end = TRUE;
            done = TRUE;
        }
        else if (b < 0x20 && b != 0x18 && b != 0x1A)
        {
            /* OSC ignores C0 controls other than its terminators and CAN/SUB.
             * In particular NUL must not truncate the eventual C string, nor
             * may ignored controls consume payload capacity. */
        }
        else if (icu->oscState == OSC_COLLECT)
        {
            if (icu->oscLen == 3 && !memcmp(icu->oscBuf, "52;", 3))
            {
                con_osc_clipboard_byte(icu, b);
                continue;
            }
            /* Keep existing title/font/color limits; OSC8 uses the static buffer. */
            ULONG limit = icu->oscLen >= 2 && icu->oscBuf[0] == '8' &&
                icu->oscBuf[1] == ';' ? CON_OSCBUF_SIZE : 256;
            if (icu->oscLen < limit - 1)
                icu->oscBuf[icu->oscLen++] = b;
            else
                icu->oscState = OSC_OVERFLOW;
        }
    }

    if (done)
    {
        BOOL st = control_end || (icu->oscState == OSC_GOT_ESC || icu->oscState == OSC_OVERFLOW_ESC);
        BOOL overflow = (icu->oscState == OSC_OVERFLOW || icu->oscState == OSC_OVERFLOW_ESC);
        ULONG code = 0;
        WORD n = 0;

        icu->oscBuf[icu->oscLen] = '\0';
        while (n < 4 && icu->oscBuf[n] >= '0' && icu->oscBuf[n] <= '9')
            code = code * 10 + (icu->oscBuf[n++] - '0');

        if (icu->oscLen == 3 && !memcmp(icu->oscBuf, "52;", 3))
        {
            *cmd_ptr = C_SET_OSC_CLIPBOARD;
            *numparams_ptr = 1;
            p_tab[0] = st;
        }
        /* A completed oversized OSC8 closes the old link, never applies a
         * truncated destination to subsequent text. */
        else if (icu->oscLen >= 2 && icu->oscBuf[0] == '8' && icu->oscBuf[1] == ';')
        {
            *cmd_ptr = C_SET_HYPERLINK;
            *numparams_ptr = 2;
            p_tab[0] = overflow ? 0 : (IPTR)(icu->oscBuf + 2);
            p_tab[1] = overflow ? 0 : icu->oscLen - 2;
        }
        /* OSC 0/2: set window/tab title (xterm) */
        else if (!overflow && icu->oscLen > 1 &&
            (icu->oscBuf[0] == '0' || icu->oscBuf[0] == '2') &&
            icu->oscBuf[1] == ';')
        {
            *cmd_ptr = C_SET_TITLE;
            *numparams_ptr = 1;
            p_tab[0] = (IPTR) (icu->oscBuf + 2);
        }
        else if (!overflow && icu->oscLen >= 15 &&
            !memcmp(icu->oscBuf, "50;CursorShape=", 15))
        {
            /* Alacritty's OSC50 shape form coexists with the native font
             * extension below. Unknown shapes are ignored, never font names. */
            if (icu->oscLen > 15 && icu->oscBuf[15] >= '0' && icu->oscBuf[15] <= '2')
            {
                *cmd_ptr = C_SET_CURSOR_SHAPE;
                *numparams_ptr = 1;
                p_tab[0] = icu->oscBuf[15] - '0';
            }
        }
        else if (!overflow && icu->oscLen > 3 &&
            icu->oscBuf[0] == '5' && icu->oscBuf[1] == '0' &&
            icu->oscBuf[2] == ';')
        {
            UBYTE *name = icu->oscBuf + 3;
            UBYTE *p = name;
            IPTR size = 0;
            BOOL valid_size = TRUE;

            while (*p && *p != ',')
                p++;
            if (*p == ',')
            {
                *p++ = '\0';
                /* TextAttr.ta_YSize is UWORD. Reject overflow and malformed
                   tails instead of wrapping IPTR or silently truncating it. */
                valid_size = *p >= '0' && *p <= '9';
                while (*p >= '0' && *p <= '9')
                {
                    ULONG digit = *p++ - '0';
                    if (size > (65535 - digit) / 10)
                        valid_size = FALSE;
                    else if (valid_size)
                        size = size * 10 + digit;
                }
                if (*p) valid_size = FALSE;
            }

            if (*name && valid_size)
            {
                *cmd_ptr = C_SET_FONT;
                *numparams_ptr = 2;
                p_tab[0] = (IPTR) name;
                p_tab[1] = size;
            }
        }
        else if (!overflow && n > 0 &&
            (icu->oscBuf[n] == ';' || icu->oscBuf[n] == '\0') &&
            (code == 4 || code == 10 || code == 11 || code == 12 || code == 104 ||
             code == 110 || code == 111 || code == 112))
        {
            *cmd_ptr = C_SET_COLOR;
            *numparams_ptr = 3;
            p_tab[0] = code;
            p_tab[1] = (IPTR) (icu->oscBuf + n + (icu->oscBuf[n] ? 1 : 0));
            p_tab[2] = st;
        }
        icu->oscState = OSC_IDLE;
        icu->oscLen = 0;
    }

    return used;
}


/* DEC Special Graphics (VT100); matches the Alacritty/vte charset table.
 * Translate character identities before rendering/retention, not font glyphs. */
static ULONG con_charset_map(ULONG cp)
{
    static const UWORD graphics[32] = {
        0x20, 0x25c6, 0x2592, 0x2409, 0x240c, 0x240d, 0x240a, 0xb0,
        0xb1, 0x2424, 0x240b, 0x2518, 0x2510, 0x250c, 0x2514, 0x253c,
        0x23ba, 0x23bb, 0x2500, 0x23bc, 0x23bd, 0x251c, 0x2524, 0x2534,
        0x252c, 0x2502, 0x2264, 0x2265, 0x3c0, 0x2260, 0xa3, 0xb7
    };
    return cp >= 0x5f && cp <= 0x7e ? graphics[cp - 0x5f] : cp;
}

/* REP retains the parser's preceding codepoint, before charset mapping.
 * Controls, saved cursors and screen swaps do not replace this history. */
static VOID con_repeat_char(Object *unit, IPTR repetitions)
{
    struct intConUnit *icu = ICU(unit);
    if (!icu->hasLastPrintable) return;
    if (repetitions <= 0) repetitions = 1;
    if (repetitions > 65535) repetitions = 65535;
    ULONG cp = icu->lastPrintable;
    if (icu->charsets & (1U << icu->activeCharset)) cp = con_charset_map(cp);
    UBYTE glyph[4], output[256];
    ULONG bytes = con_utf8_encode(glyph, cp), n;
    for (n = 0; n < 64; n++) memcpy(output + n * bytes, glyph, bytes);
    BOOL before = icu->renderUTF8;
    icu->renderUTF8 = TRUE;
    while (repetitions > 0)
    {
        ULONG count = repetitions > 64 ? 64 : repetitions;
        IPTR mapped[2] = {(IPTR)output, count * bytes};
        Console_DoCommand(unit, C_ASCII_STRING, 2, mapped);
        repetitions -= count;
    }
    icu->renderUTF8 = before;
}

static VOID con_remember_text(Object *unit, BYTE command, IPTR *params)
{
    struct intConUnit *icu = ICU(unit);
    if (command == C_ASCII)
    {
        icu->lastPrintable = (UBYTE)params[0];
        icu->hasLastPrintable = TRUE;
    }
    else if (command == C_ASCII_STRING && params[1] > 0)
    {
        const UBYTE *p = (const UBYTE *)params[0];
        LONG left = params[1];
        if (!CON_IS_UTF8(unit) || p[left - 1] < 0x80)
            icu->lastPrintable = p[left - 1];
        else
            while (left > 0) icu->lastPrintable = con_utf8_decode(&p, &left);
        icu->hasLastPrintable = TRUE;
    }
}

static VOID con_emit_command(Object *unit, BYTE command, UBYTE count, IPTR *params)
{
    struct intConUnit *icu = ICU(unit);
    if (command == C_REPEAT_CHAR)
    {
        con_repeat_char(unit, params[0]);
        return;
    }
    con_remember_text(unit, command, params);
    if ((command != C_ASCII && command != C_ASCII_STRING) ||
        !(icu->charsets & (1U << icu->activeCharset)))
    {
        Console_DoCommand(unit, command, count, params);
        return;
    }
    UBYTE single = params[0], output[256];
    const UBYTE *p = command == C_ASCII ? &single : (const UBYTE *)params[0];
    LONG left = command == C_ASCII ? 1 : params[1];
    BOOL utf8 = !!CON_IS_UTF8(unit);
    while (left > 0)
    {
        ULONG used = 0;
        while (left > 0 && used <= sizeof(output) - 4)
        {
            ULONG cp;
            if (utf8) cp = con_utf8_decode(&p, &left);
            else { cp = *p++; left--; }
            used += con_utf8_encode(output + used, con_charset_map(cp));
        }
        IPTR mapped[2] = {(IPTR)output, used};
        /* Native Latin-1 consoles keep their input policy. Only this internal
         * rendering span is Unicode; input handlers never observe an encoding change. */
        BOOL before = icu->renderUTF8;
        icu->renderUTF8 = TRUE;
        Console_DoCommand(unit, C_ASCII_STRING, 2, mapped);
        icu->renderUTF8 = before;
    }
}

static BOOL getcharset(BYTE *command, UBYTE **input, LONG length, Object *unit)
{
    UBYTE *p = *input;
    if (length < 3 || p[0] != ESC || p[1] < '(' || p[1] > '+') return FALSE;
    if (p[2] < 0x20 || p[2] > 0x7e)
    {
        *command = C_NIL; *input = p + 2;
        return TRUE; /* Cancel designation; execute the following control. */
    }
    UBYTE bit = 1U << (p[1] - '(');
    if (p[2] == '0') ICU(unit)->charsets |= bit;
    else if (p[2] == 'B') ICU(unit)->charsets &= ~bit;
    /* Unsupported designations are consumed without changing the set. */
    *command = C_NIL; *input = p + 3;
    return TRUE;
}

/* An escape sequence runs <CSI> params(0x30-0x3F) intermediates(0x20-0x2F)
   final(0x40-0x7E). It is unterminated while only parameter or intermediate
   bytes have arrived, so it must not be interpreted or printed yet. */
static BOOL csi_incomplete(const UBYTE *buf, LONG len)
{
    const UBYTE *p;
    LONG rem, i;

    if (len == 2 && buf[0] == ESC &&
        ((buf[1] >= '(' && buf[1] <= '+') || buf[1] == '#' || buf[1] == '%'))
        return TRUE;
    if (len >= 1 && buf[0] == CSI)
    {
        p = buf + 1;
        rem = len - 1;
    }
    else if (len == 1 && buf[0] == ESC)
        return TRUE;
    else if (len >= 2 && buf[0] == ESC && buf[1] == '[')
    {
        p = buf + 2;
        rem = len - 2;
    }
    else
        return FALSE;

    for (i = 0; i < rem; i++)
    {
        if (p[i] < 0x20 || p[i] > 0x3F)
            return FALSE;
    }
    return TRUE;
}


ULONG con_parse_output(struct ConUnit *unit, STRPTR buf, ULONG towrite,
    struct ConsoleBase *ConsoleDevice, LONG *error)
{
    IPTR param_tab[MAX_COMMAND_PARAMS];
    struct intConUnit *icu = ICU(unit);

    BYTE command = 0;
    UBYTE numparams;
    UBYTE *orig_write_str, *write_str;
    LONG orig_towrite;
    UBYTE *allocbuf = NULL;
    ULONG accepted = towrite;
    BOOL plainText = TRUE;

    EnterFunc(bug("WriteToConsole(ioreq=%p)\n"));

    *error = 0;
    if (!towrite) return 0;
    /* Parser spans use signed LONG lengths. Reject before reading the caller's
     * buffer or changing any pending state, including on 64-bit hosts. */
    if (towrite > 0x7fffffffUL - icu->pendingCSILen)
    {
        *error = IOERR_BADLENGTH;
        return 0;
    }
    if (icu->pendingCSILen > 0)
    {
        UWORD pl = icu->pendingCSILen;
        allocbuf = AllocMem(pl + towrite, MEMF_ANY);
        if (!allocbuf)
        {
            /* No bytes were consumed: keep the prefix for a caller retry. */
            *error = IOERR_ABORTED;
            return 0;
        }
        CopyMem(icu->pendingCSI, allocbuf, pl);
        if (towrite) CopyMem(buf, allocbuf + pl, towrite);
        icu->pendingCSILen = 0;
        buf = (STRPTR) allocbuf;
        towrite += pl;
    }

    write_str = orig_write_str = (UBYTE *) buf;
    orig_towrite = towrite;

    {
        ULONG i;

        for (i = 0; i < towrite; i++)
        {
            if ((UBYTE)buf[i] < 0x20 ||
                ((UBYTE)buf[i] >= 0x7f && (UBYTE)buf[i] < 0xa0))
            {
                plainText = FALSE;
                break;
            }
        }
    }

    D(bug("Number of chars to write %d\n", towrite));

    /* Complete an UTF-8 sequence split across CMD_WRITEs */
    if (CON_IS_UTF8(unit) && ICU(unit)->utf8StashLen > 0 && towrite > 0)
    {
        WORD need = utf8_seqlen(ICU(unit)->utf8Stash[0]);
        /* A CMD_WRITE can exceed signed-WORD range. Bound in the original
         * unsigned length domain before copying into the four-byte stash. */
        ULONG take = MIN((ULONG)(need - ICU(unit)->utf8StashLen), towrite);

        ULONG valid = 0;
        while (valid < take && (write_str[valid] & 0xc0) == 0x80) valid++;
        BOOL interrupted = valid < take;
        take = valid;

        memcpy(ICU(unit)->utf8Stash + ICU(unit)->utf8StashLen, write_str,
            take);
        ICU(unit)->utf8StashLen += take;
        write_str += take;
        towrite -= take;

        if (ICU(unit)->utf8StashLen == need || interrupted)
        {
            IPTR stash_tab[2];
            UBYTE replacement = '?';

            /* The existing decoder maps a truncated sequence to '?'. Emit
             * that character directly, so immediate and retained rendering
             * both take the ordinary ASCII path. Leave the interrupting byte
             * with the parser rather than swallowing ESC, CR/LF or text. */
            stash_tab[0] = interrupted ? (IPTR)&replacement : (IPTR)ICU(unit)->utf8Stash;
            stash_tab[1] = interrupted ? 1 : ICU(unit)->utf8StashLen;
            ICU(unit)->utf8StashLen = 0;

            Console_UnRenderCursor((Object *) unit);
            con_emit_command((Object *) unit, C_ASCII_STRING, 2, stash_tab);
            Console_RenderCursor((Object *) unit);
        }
        else if (towrite == 0)
        {
            /* Still incomplete - everything consumed into the stash */
            ReturnInt("WriteToConsole", LONG, orig_towrite);
        }
    }

    /* Interpret string into a command and execute command */

    /* DEBUG aid */

#if DEBUG
    {
        UWORD i;
        for (i = 0; i < towrite; i++)
            bug("%x", write_str[i]);

        bug("\n");

    }
#endif
    if (towrite > 0 && !plainText)
        Console_UnRenderCursor((Object *) unit);

    while (towrite > 0)
    {
        if (con_sync_buffering(unit)) { accepted -= towrite; break; }
        if (csi_incomplete(write_str, towrite)
            && towrite <= (LONG) sizeof(icu->pendingCSI))
        {
            CopyMem(write_str, icu->pendingCSI, towrite);
            icu->pendingCSILen = towrite;
            write_str += towrite;
            break;
        }

        numparams = 0;

        if (!string2command(&command, &write_str, &numparams, towrite,
                param_tab, (Object *) unit, ConsoleDevice))
            break;

        con_emit_command((Object *) unit, command, numparams, param_tab);

        towrite = orig_towrite - (write_str - orig_write_str);
    } /* while (characters left to interpret) */

    if (orig_towrite > 0 && !plainText)
        Console_RenderCursor((Object *) unit);

    if (allocbuf)
        FreeMem(allocbuf, orig_towrite);

    ReturnInt("WriteToConsole", LONG, accepted);
}


/**********************
** string2command()  **
**********************/


static const UBYTE str_slm[] = {0x32, 0x30, 0x68}; /* Set linefeed mode    */
static const UBYTE str_rnm[] = {0x32, 0x30, 0x6C}; /* Reset linefeed mode  */
static const UBYTE str_ssm[] = {0x3E, 0x31, 0x68}; /* Set autoscroll mode */
static const UBYTE str_rsm[] = {0x3E, 0x31, 0x6C}; /* Reset autoscroll mode */
static const UBYTE str_swm[] = {0x3E, 0x37, 0x68}; /* Set autowrap mode */
static const UBYTE str_rwm[] = {0x3E, 0x37, 0x6C}; /* Reset autowrap mode */
static const UBYTE str_dsr[] = {0x36, 0x6E};       /* device status report */
static const UBYTE str_con[] = {' ', 'p'};         /* cursor visible */
static const UBYTE str_con2[] = {'1', ' ', 'p'};   /* cursor visible */
static const UBYTE str_cof[] = {'0', ' ', 'p'};    /* cursor invisible */
static const UBYTE str_srq1[] = {' ', 'q'};        /* window status request */
static const UBYTE str_srq2[] = {'0', ' ', 'q'};   /* window status request */
static const UBYTE str_su8[] = {0x3E, 0x38, 0x68}; /* Set UTF-8 mode (private >8h) */
static const UBYTE str_ru8[] = {0x3E, 0x38, 0x6C}; /* Reset UTF-8 mode (private >8l) */
static const UBYTE str_sob[] = {0x3E, 0x39, 0x68}; /* Obscure unit (private >9h) */
static const UBYTE str_rob[] = {0x3E, 0x39, 0x6C}; /* Reveal unit (private >9l) */

#define NUM_SPECIAL_COMMANDS 16
static const struct special_cmd_descr
{
    BYTE Command;
    STRPTR CommandStr;
    BYTE Length;
} scd_tab[NUM_SPECIAL_COMMANDS] =
{
    {C_SET_LF_MODE, (STRPTR) str_slm, 3},
    {C_RESET_LF_MODE, (STRPTR) str_rnm, 3},
    {C_SET_AUTOSCROLL_MODE, (STRPTR) str_ssm, 3},
    {C_RESET_AUTOSCROLL_MODE, (STRPTR) str_rsm, 3},
    {C_SET_AUTOWRAP_MODE, (STRPTR) str_swm, 3},
    {C_RESET_AUTOWRAP_MODE, (STRPTR) str_rwm, 3},
    {C_DEVICE_STATUS_REPORT, (STRPTR) str_dsr, 2},
    {C_CURSOR_VISIBLE, (STRPTR) str_con, 2},
    {C_CURSOR_VISIBLE, (STRPTR) str_con2, 3},
    {C_CURSOR_INVISIBLE, (STRPTR) str_cof, 3},
    {C_WINDOW_STATUS_REQUEST, (STRPTR) str_srq1, 2},
    {C_WINDOW_STATUS_REQUEST, (STRPTR) str_srq2, 3},
    {C_SET_UTF8_MODE, (STRPTR) str_su8, 3},
    {C_RESET_UTF8_MODE, (STRPTR) str_ru8, 3},
    {C_SET_OBSCURED, (STRPTR) str_sob, 3},
    {C_RESET_OBSCURED, (STRPTR) str_rob, 3}
};

#if DEBUG
static UBYTE *cmd_names[NUM_CONSOLE_COMMANDS] = {

    "Ascii",                    /* C_ASCII = 0                  */

    "Esc",                      /* C_ESC                        */
    "Bell",                     /* C_BELL,                      */
    "Backspace",                /* C_BACKSPACE,                 */
    "HTab",                     /* C_HTAB,                      */
    "Linefeed",                 /* C_LINEFEED,                  */
    "VTab",                     /* C_VTAB,                      */
    "Formefeed",                /* C_FORMFEED,                  */
    "Carriage return",          /* C_CARRIAGE_RETURN,           */
    "Shift In",                 /* C_SHIFT_IN,                  */
    "Shift Out",                /* C_SHIFT_OUT,                 */
    "Index",                    /* C_INDEX,                     */
    "Nex Line",                 /* C_NEXT_LINE,                 */
    "Tab set",                  /* C_H_TAB_SET,                 */
    "Reverse Idx",              /* C_REVERSE_IDX,               */
    "Set LF Mode",              /* C_SET_LF_MODE,               */
    "Reset LF Mode",            /* C_RESET_lF_MODE,             */
    "Device Status Report",     /* C_DEVICE_STATUS_REPORT,      */

    "Insert Char",              /* C_INSERT_CHAR,               */
    "Cursor Up",                /* C_CURSOR_UP,                 */
    "Cursor Down",              /* C_CURSOR_DOWN,               */
    "Cursor Forward",           /* C_CURSOR_FORWARD,            */
    "Cursor Backward",          /* C_CURSOR_BACKWARD,           */
    "Cursor Next Line",         /* C_CURSOR_NEXT_LINE,          */
    "Cursor Prev Line",         /* C_CURSOR_PREV_LINE,          */
    "Cursor Pos",               /* C_CURSOR_POS,                */
    "Cursor Column",            /* C_CURSOR_COLUMN,             */
    "Cursor HTab",              /* C_CURSOR_HTAB,               */
    "Erase In Display",         /* C_ERASE_IN_DISPLAY,          */
    "Erase In Line",            /* C_ERASE_IN_LINE,             */
    "Insert Line",              /* C_INSERT_LINE,               */
    "Delete Line",              /* C_DELETE_LINE,               */
    "Delete Char",              /* C_DELETE_CHAR,               */
    "Scroll Up",                /* C_SCROLL_UP,                 */
    "Scroll Down",              /* C_SCROLL_DOWN,               */
    "Cursor Tab Ctrl",          /* C_CURSOR_TAB_CTRL,           */
    "Cursor Backtab",           /* C_CURSOR_BACKTAB,            */
    "Select Graphic Rendition", /* C_SELECT_GRAPHIC_RENDITION   */
    "Window Status Request",    /* C_WINDOW_STATUS_REQUEST      */
    "Cursor Visible",           /* C_CURSOR_VISIBLE,            */
    "Cursor Invisible",         /* C_CURSOR_INVISIBLE,          */
    "Set Raw Events",           /* C_SET_RAWEVENTS,             */
    "Reset Raw Events",         /* C_RESET_RAWEVENTS            */
    "Set Auto Wrap Mode",       /* C_SET_AUTOWRAP_MODE          */
    "Reset Auto Wrap Mode",     /* C_RESET_AUTOWRAP_MODE        */
    "Set Auto Scroll Mode",     /* C_SET_AUTOSCROLL_MODE        */
    "Reset Auto Scroll Mode",   /* C_RESET_AUTOSCROLL_MODE      */
    "Set Page Length",          /* C_SET_PAGE_LENGTH            */
    "Set Line Length",          /* C_SET_LINE_LENGTH            */
    "Set Left Offset",          /* C_SET_LEFT_OFFSET            */
    "Set Top Offset"            /* C_SET_TOP_OFFSET             */
};
#endif

static BOOL check_special(STRPTR string, LONG toparse, BOOL utf8)
{
    /* Native con-handler emits standalone CSI even in a UTF-8 console.
       con_text_span skips complete UTF-8 characters, so their continuation
       bytes never reach this control-byte check. Other C1 bytes stay text. */
    if (utf8 && (*(UBYTE *) string >= 0x80) && *(UBYTE *)string != CSI)
        return FALSE;

    return ((*string == CSI) ||
        (toparse >= 2 && (string[0] == ESC) && (string[1] == '[')) || /* CSI */
        (*string == NIL) ||
        (*string == BELL) ||
        (*string == BACKSPACE) ||
        (*string == HTAB) ||
        (*string == LINEFEED) ||
        (*string == VTAB) ||
        (*string == FORMFEED) ||
        (*string == CARRIAGE_RETURN) ||
        (*string == SHIFT_OUT) ||
        (*string == SHIFT_IN) ||
        (*string == ESC) ||
        (*string == INDEX) ||
        (*string == NEXT_LINE) ||
        (*string == H_TAB_SET) || (*string == REVERSE_INDEX));
}

/* Stop at controls only between UTF-8 characters. Preserve incomplete tails
 * for the existing cross-write UTF-8 stash, including CSI-valued continuations. */
static LONG con_text_span(UBYTE *text, LONG length, BOOL utf8)
{
    LONG used = 0;
    while (used < length)
    {
        if (used && check_special((STRPTR)(text + used), length - used, utf8))
            break;
        WORD step = 1;
        if (utf8)
        {
            WORD need = utf8_seqlen(text[used]);
            while (step < need && used + step < length &&
                (text[used + step] & 0xC0) == 0x80)
                step++;
        }
        used += step;
    }
    return used;
}

static BOOL getkeypadmode(BYTE *cmd_ptr, UBYTE **writestr_ptr,
    LONG toparse, Object *unit)
{
    UBYTE *p = *writestr_ptr;
    if (toparse < 2 || p[0] != ESC || (p[1] != '=' && p[1] != '>'))
        return FALSE;
    if (p[1] == '=')
        ICU(unit)->conFlags |= CF_APPLICATION_KEYPAD;
    else
        ICU(unit)->conFlags &= ~CF_APPLICATION_KEYPAD;
    *cmd_ptr = C_NIL;
    *writestr_ptr = p + 2;
    return TRUE;
}

/* Seven-bit forms of the supported C1 controls, plus DECSC/DECRC.
 * A lone ESC is buffered by the existing incomplete-sequence path. */
static BOOL getshortescape(BYTE *cmd_ptr, UBYTE **writestr_ptr, LONG toparse)
{
    UBYTE *p = *writestr_ptr;
    if (toparse < 2 || p[0] != ESC) return FALSE;
    if (p[1] == '#')
    {
        if (toparse < 3) return FALSE;
        *cmd_ptr = p[2] == '8' ? C_SCREEN_ALIGNMENT : C_NIL;
        *writestr_ptr = p + (p[2] < 0x20 || p[2] > 0x7e ? 2 : 3);
        return TRUE;
    }
    switch (p[1])
    {
    case 'D': *cmd_ptr = C_INDEX; break;
    case 'E': *cmd_ptr = C_NEXT_LINE; break;
    case 'H': *cmd_ptr = C_H_TAB_SET; break;
    case 'M': *cmd_ptr = C_REVERSE_IDX; break;
    case '7': *cmd_ptr = C_SAVE_CURSOR; break;
    case '8': *cmd_ptr = C_RESTORE_CURSOR; break;
    case 'c': *cmd_ptr = C_TERMINAL_RESET; break;
    case 'Z': *cmd_ptr = C_PRIMARY_ATTRIBUTES; break;
    case '\\': *cmd_ptr = C_NIL; break; /* ST outside a string */
    default: return FALSE;
    }
    *writestr_ptr = p + 2;
    return TRUE;
}

/* ANSI aliases are seven-bit only: native CSI u remains Set Line Length.
 * Ignore parameters like the reference, but leave private/intermediate
 * prefixes to other protocols (notably keyboard CSI ... u). */
static BOOL getansisavedcursor(BYTE *cmd_ptr, UBYTE **writestr_ptr,
    UBYTE *numparams_ptr, LONG toparse)
{
    UBYTE *p = *writestr_ptr;
    LONG i;
    if (toparse < 3 || p[0] != ESC || p[1] != '[') return FALSE;
    for (i = 2; i < toparse; i++)
    {
        if ((p[i] >= '0' && p[i] <= '9') || p[i] == ';' || p[i] == ':')
            continue;
        if (p[i] != 's' && p[i] != 'u') return FALSE;
        *cmd_ptr = p[i] == 's' ? C_SAVE_CURSOR : C_RESTORE_CURSOR;
        *numparams_ptr = 0;
        *writestr_ptr = p + i + 1;
        return TRUE;
    }
    return FALSE;
}

/* Window title and geometry extensions are seven-bit; preserve native CSI t. */
static BOOL getwindowcommand(BYTE *command, UBYTE **input, UBYTE *count, LONG length)
{
    UBYTE *p = *input;
    LONG i = 2;
    ULONG selector = 0;
    if (length < 3 || p[0] != ESC || p[1] != '[') return FALSE;
    while (i < length && p[i] >= '0' && p[i] <= '9')
    {
        /* Saturate unknown selectors without letting them fall through to
         * native CSI t (Set Page Length), including oversized numbers. */
        if (selector < 24) selector = selector * 10 + p[i] - '0';
        if (selector > 24) selector = 24;
        i++;
    }
    /* Reference ignores the optional title selector (icon names unsupported). */
    while (i < length && (p[i] == ';' || p[i] == ':' || (p[i] >= '0' && p[i] <= '9'))) i++;
    if (i >= length || p[i] != 't') return FALSE;
    *command = selector == 14 ? C_REPORT_PIXEL_GEOMETRY :
        selector == 18 ? C_REPORT_CELL_GEOMETRY :
        selector == 22 ? C_PUSH_TITLE : selector == 23 ? C_POP_TITLE : C_NIL;
    *count = 0;
    *input = p + i + 1;
    return TRUE;
}

/* Colon subparameters form one SGR group, never separate rendition codes.
 * Handle underline and indexed/RGB colors; ignore unsupported colon groups. */
static BOOL getcolonsgr(BYTE *command, UBYTE **input, UBYTE *count,
    LONG length, IPTR *params, BOOL modern)
{
    const UBYTE *p = *input;
    LONG end, start;
    BOOL colon = FALSE, defaults = FALSE, overflow = FALSE;
    UBYTE used = 0;
    IPTR values[MAX_COMMAND_PARAMS];
    for (end = 0; end < length && p[end] != 'm'; end++)
    {
        if (p[end] == ':') colon = TRUE;
        else if (p[end] == ';')
        {
            if (!end || p[end - 1] == ';') defaults = TRUE;
        }
        else if (p[end] < '0' || p[end] > '9') return FALSE;
    }
    if (end == length) return FALSE;
    if (end && p[end - 1] == ';') defaults = TRUE;
    if (!colon && !(modern && defaults)) return FALSE;
    for (start = 0; start <= end; )
    {
        LONG next = start, i = start;
        ULONG value = 0;
        while (next < end && p[next] != ';') next++;
        while (i < next && p[i] != ':')
        {
            value = value * 10 + p[i++] - '0';
            if (value > 65535) value = 65535;
        }
        if (i == next)
        {
            if (used < MAX_COMMAND_PARAMS) values[used++] = value;
            else overflow = TRUE;
        }
        else if (value == 4)
        {
            ++i;
            ULONG style = 0;
            while (i < next && p[i] != ':')
            {
                style = style * 10 + p[i++] - '0';
                if (style > 65535) style = 65535;
            }
            /* VTE defaults an empty subparameter to zero. Only exact
             * two-part selectors have special meanings; other [4, ..]
             * groups fall back to single underline. Negative internal tags
             * carry extended styles without aliasing public numeric SGR. */
            if (i != next || style > 5) style = 1;
            if (used < MAX_COMMAND_PARAMS)
                values[used++] = style > 2 ? -(IPTR)style : style == 0 ? 24 : style == 1 ? 4 : 21;
            else overflow = TRUE;
        }
        else if (value == 38 || value == 48 || value == 58)
        {
            ULONG parts[5] = {0};
            ULONG missing = 0;
            LONG fields = 0;
            while (i < next)
            {
                LONG begin = ++i;
                ULONG part = 0;
                while (i < next && p[i] != ':')
                {
                    part = part * 10 + p[i++] - '0';
                    if (part > 65535) part = 65535;
                }
                if (fields < 5)
                {
                    parts[fields] = part;
                    if (i == begin) missing |= 1UL << fields;
                }
                fields++;
            }
            BOOL indexed = fields == 2 && !missing && parts[0] == 5 && parts[1] <= 255;
            LONG rgb = fields == 5 ? 2 : 1;
            BOOL direct = (fields == 4 || fields == 5) && parts[0] == 2 &&
                (fields == 4 ? !missing : !(missing & ~2UL) && parts[1] == 0) &&
                parts[rgb] <= 255 && parts[rgb+1] <= 255 && parts[rgb+2] <= 255;
            UBYTE needed = indexed ? 3 : direct ? 5 : 0;
            if (used + needed > MAX_COMMAND_PARAMS) overflow = TRUE;
            else if (needed)
            {
                values[used++] = value;
                values[used++] = indexed ? 5 : 2;
                if (indexed) values[used++] = parts[1];
                else for (LONG c = 0; c < 3; c++) values[used++] = parts[rgb+c];
            }
        }
        start = next + 1;
    }
    *input += end + 1;
    *count = overflow ? 0 : used;
    *command = !overflow && used ? C_SELECT_GRAPHIC_RENDITION : C_NIL;
    if (!overflow) for (UBYTE n = 0; n < used; n++) params[n] = values[n];
    return TRUE;
}

static BOOL string2command(BYTE *cmd_ptr, UBYTE ** writestr_ptr,
    UBYTE *numparams_ptr, LONG toparse, IPTR *p_tab, Object *unit,
    struct ConsoleBase *ConsoleDevice)
{
    UBYTE *write_str = *writestr_ptr;
    UBYTE *csi_str = write_str;
    LONG csi_toparse = 0;
    BOOL utf8 = CON_IS_UTF8(unit) ? TRUE : FALSE;

    BOOL found = FALSE, csi = FALSE;

    EnterFunc(bug("StringToCommand(toparse=%d)\n", toparse));

    if (ICU(unit)->dcsDiscard)
    {
        *cmd_ptr = C_NIL;
        *writestr_ptr += consume_dcs(unit, write_str, toparse);
        ReturnBool("StringToCommand", TRUE);
    }

    /* OSC string in progress (possibly split across CMD_WRITEs)? */
    if (ICU(unit)->oscState != OSC_IDLE)
    {
        write_str += consume_osc(unit, write_str, toparse, cmd_ptr,
            numparams_ptr, p_tab);
        *writestr_ptr = write_str;
        ReturnBool("StringToCommand", TRUE);
    }

    /* ISO 2022 / xterm UTF-8 selection: ESC % G on, ESC % @ off
       (the private CSI >8h/>8l remains as a synonym) */
    if (*write_str == ESC && toparse >= 3 && write_str[1] == '%')
    {
        if (write_str[2] == 'G' || write_str[2] == '@')
        {
            *cmd_ptr = (write_str[2] == 'G') ?
                C_SET_UTF8_MODE : C_RESET_UTF8_MODE;
            *writestr_ptr = write_str + 3;
            ReturnBool("StringToCommand", TRUE);
        }
    }

    if (getcharset(cmd_ptr, writestr_ptr, toparse, unit))
        ReturnBool("StringToCommand", TRUE);

    if (getkeypadmode(cmd_ptr, writestr_ptr, toparse, unit))
        ReturnBool("StringToCommand", TRUE);

    if (getshortescape(cmd_ptr, writestr_ptr, toparse))
        ReturnBool("StringToCommand", TRUE);

    if (getwindowcommand(cmd_ptr, writestr_ptr, numparams_ptr, toparse))
        ReturnBool("StringToCommand", TRUE);

    if (getansisavedcursor(cmd_ptr, writestr_ptr, numparams_ptr, toparse))
        ReturnBool("StringToCommand", TRUE);

    /* Unsupported control strings never become ordinary console text. */
    LONG discard = discard_prefix(write_str, toparse, utf8);
    if (discard)
    {
        LONG skip = discard;
        ICU(unit)->dcsDiscard = TRUE;
        *cmd_ptr = C_NIL;
        *writestr_ptr += skip + consume_dcs(unit, write_str + skip, toparse - skip);
        ReturnBool("StringToCommand", TRUE);
    }

    /* OSC start: ESC ] (or C1 0x9D outside UTF-8 mode) */
    if ((*write_str == ESC && toparse >= 2 && write_str[1] == ']') ||
        (!utf8 && *write_str == 0x9D))
    {
        LONG skip = (*write_str == ESC) ? 2 : 1;

        con_osc_clipboard_reset(ICU(unit));
        ICU(unit)->oscState = OSC_COLLECT;
        ICU(unit)->oscLen = 0;
        write_str += skip;
        write_str += consume_osc(unit, write_str, toparse - skip, cmd_ptr,
            numparams_ptr, p_tab);
        *writestr_ptr = write_str;
        ReturnBool("StringToCommand", TRUE);
    }

    /* Standalone native CSI is a control at this codepoint boundary. */
    if (*write_str == CSI)
    {
        csi_str++;
        csi = TRUE;
        csi_toparse = toparse - 1;
    }
    else if (toparse >= 2)
    {
        if ((write_str[0] == ESC) && (write_str[1] == '['))
        {
            csi_str += 2;
            csi_toparse = toparse - 2;
            csi = TRUE;
        }
    }

    if (csi)
    {
        D(bug("CSI found, getting command\n"));
        ULONG keyboard = con_keyboard_csi(CU(unit), csi_str, csi_toparse, ConsoleDevice);
        if (keyboard) {
            *cmd_ptr = C_NIL;
            *writestr_ptr = csi_str + keyboard;
            ReturnBool("StringToCommand", TRUE);
        }

        if (!found)
            found = getcolonsgr(cmd_ptr, &csi_str, numparams_ptr,
                csi_toparse, p_tab, *write_str != CSI);

        if (!found)
            found = getmoderequest(cmd_ptr, &csi_str, numparams_ptr,
                csi_toparse, unit, ConsoleDevice);

        if (!found)
            found = getstatusrequest(cmd_ptr, &csi_str, numparams_ptr,
                csi_toparse, p_tab, *write_str == CSI);

        if (!found)
            found = getscrollregion(cmd_ptr, &csi_str, numparams_ptr,
                csi_toparse, p_tab);

        if (!found)
            found = getabsolutecommand(cmd_ptr, &csi_str, numparams_ptr,
                csi_toparse, p_tab);

        if (!found)
            found = getcursorstylecommand(cmd_ptr, &csi_str, numparams_ptr,
                csi_toparse, p_tab);

        /* DEC private modes use a '?' parameter prefix, which the classic
           Amiga console grammar does not understand.  Parse them before the
           generic parameter commands so the complete CSI is consumed rather
           than leaving its body to be rendered as text. */
        if (!found)
            found = getdecprivatecommand(cmd_ptr, &csi_str, csi_toparse,
                unit);

        if (!found)
            found = getansimodecommand(cmd_ptr, &csi_str, numparams_ptr,
                csi_toparse, unit);

        /* Search for the longest commands first */

        if (!found)
        {
            BYTE i;
            /* Look for some special commands */
            for (i = 0; ((i < NUM_SPECIAL_COMMANDS) && (!found)); i++)
            {
                /* Check whether command sequence is longer than input */
                if (scd_tab[i].Length > csi_toparse)
                    continue;   /* if so, check next command sequence */

                D(bug
                    ("Comparing for special command %d, idx %d, cmdstr %p, len %d, csistr %p \n",
                        scd_tab[i].Command, i, scd_tab[i].CommandStr,
                        scd_tab[i].Length, csi_str));
                /* Command match ? */
                if (0 == strncmp(csi_str, scd_tab[i].CommandStr,
                        scd_tab[i].Length))
                {
                    D(bug("Special command found\n"));
                    csi_str += scd_tab[i].Length;
                    *cmd_ptr = scd_tab[i].Command;

                    found = TRUE;
                }
            } /* for (each special command) */
        }

        /* A parameter command? (I.e. one of the commands that take
         * parameters) */
        if (!found)
            found =
                getparamcommand(cmd_ptr, &csi_str, numparams_ptr,
                csi_toparse, p_tab, unit, ConsoleDevice);

        /* ECMA-48 says unsupported control functions are ignored.  Once a
           syntactically complete CSI has reached us, consume it as one unit
           instead of interpreting ESC alone and printing the parameter body.
           This also safely handles xterm secondary-device/version queries
           (CSI > ... final) until KCON grows an input reply path. */
        if (!found)
        {
            LONG i;

            for (i = 0; i < csi_toparse; i++)
            {
                if (csi_str[i] >= 0x40 && csi_str[i] <= 0x7E)
                {
                    csi_str += i + 1;
                    *cmd_ptr = C_NIL;
                    found = TRUE;
                    break;
                }
                if (csi_str[i] < 0x20 || csi_str[i] > 0x3F)
                    break;
            }
        }

    } /* if (CSI was found) */

    if (found)
        write_str = csi_str;
    else if (utf8 && *write_str >= 0x80)
    {
        /* UTF-8 lead/continuation byte - plain text, fall through below */
    }
    else
    {
        /* Look for standalone codes */
        switch (*write_str)
        {
        case NIL:
            *cmd_ptr = C_NIL;
            found = TRUE;
            break;

        case BELL:
            *cmd_ptr = C_BELL;
            found = TRUE;
            break;

        case BACKSPACE:
            *cmd_ptr = C_BACKSPACE;
            found = TRUE;
            break;

        case HTAB:
            *cmd_ptr = C_HTAB;
            found = TRUE;
            break;

        case LINEFEED:
            *cmd_ptr = C_LINEFEED;
            found = TRUE;
            break;

        case VTAB:
            *cmd_ptr = C_VTAB;
            found = TRUE;
            break;

        case FORMFEED:
            *cmd_ptr = C_FORMFEED;
            found = TRUE;
            break;

        case CARRIAGE_RETURN:
            *cmd_ptr = C_CARRIAGE_RETURN;
            found = TRUE;
            break;

        case SHIFT_OUT:
            *cmd_ptr = C_SHIFT_OUT;
            found = TRUE;
            break;

        case SHIFT_IN:
            *cmd_ptr = C_SHIFT_IN;
            found = TRUE;
            break;

        case ESC:
            *cmd_ptr = C_ESC;
            found = TRUE;
            break;

        case INDEX:
            *cmd_ptr = C_INDEX;
            found = TRUE;
            break;

        case NEXT_LINE:
            *cmd_ptr = C_NEXT_LINE;
            found = TRUE;
            break;

        case H_TAB_SET:
            *cmd_ptr = C_H_TAB_SET;
            found = TRUE;
            break;

        case REVERSE_INDEX:
            *cmd_ptr = C_REVERSE_IDX;
            found = TRUE;
            break;
        } /* (switch) */

        if (found)
        {
            /* Found special char. Increase pointer */

            write_str++;
        }
    }

    if (!found) /* Still not any found? Try to print as plain text */
    {
        *cmd_ptr = C_ASCII_STRING;

        p_tab[0] = (IPTR) write_str;
        *numparams_ptr = 2;
        found = TRUE;

        LONG span = con_text_span(write_str, toparse, utf8);
        toparse -= span;
        write_str += span;

        /* store the string length */
        p_tab[1] = (IPTR) (write_str - (UBYTE *) p_tab[0]);

        /* UTF-8: an incomplete trailing sequence at the END of the buffer
           (never mid-buffer - controls cannot split a sequence) is consumed
           into the unit stash and completed by the next CMD_WRITE. */
        if (utf8 && toparse == 0)
        {
            UBYTE *base = (UBYTE *) p_tab[0];
            UBYTE *p = write_str;
            WORD back = 0;

            while (back < 3 && p > base && (p[-1] & 0xC0) == 0x80)
            {
                p--;
                back++;
            }

            if (p > base && (p[-1] & 0xC0) == 0xC0)
            {
                WORD have = back + 1;

                if (have < utf8_seqlen(p[-1]))
                {
                    ICU(unit)->utf8StashLen = have;
                    memcpy(ICU(unit)->utf8Stash, p - 1, have);
                    p_tab[1] -= have;
                    if (p_tab[1] == 0)
                        *cmd_ptr = C_NIL;
                }
            }
        }
    }

    D(bug("FOUND CMD: %s\n", cmd_names[*cmd_ptr]));

    /* Return pointer to first character AFTER last interpreted char */
    *writestr_ptr = write_str;

    ReturnBool("StringToCommand", found);
}


/************************
** getdecprivatecommand()
************************/

/* Absolute positioning permits omitted fields (CSI ;6H and CSI 5;f).
 * Keep that grammar separate from the legacy Amiga parameter extensions. */
/* DSR accepts one numeric selector; DA1 accepts omitted/zero selector.
 * Commit only a complete supported query. */
static BOOL getstatusrequest(BYTE *command, UBYTE **input, UBYTE *count,
    LONG length, IPTR *params, BOOL native)
{
    UBYTE *p = *input;
    /* Color-scheme query is private DSR, not the cursor-position report.
       Commit only the complete selector; fragmented writes remain buffered. */
    if (length > 0 && p[0] == '?')
    {
        LONG i = 1;
        IPTR selector = 0;
        while (i < length && p[i] >= '0' && p[i] <= '9')
        {
            if (selector > 996) return FALSE;
            selector = selector * 10 + p[i++] - '0';
        }
        if (i < length && p[i] == 'n' && selector == 996)
        {
            params[0] = 996;
            params[1] = native;
            *count = 2;
            *command = C_DEVICE_STATUS_REPORT;
            *input = p + i + 1;
            return TRUE;
        }
        return FALSE;
    }
    /* Native Amiga window reports overlap ANSI DECSCUSR. Keep the native
       spelling available without stealing ESC-[ cursor-style commands. */
    if (native && ((length >= 2 && p[0] == ' ' && p[1] == 'q') ||
        (length >= 3 && p[0] == '0' && p[1] == ' ' && p[2] == 'q')))
    {
        *input = p + (p[0] == '0' ? 3 : 2);
        *command = C_WINDOW_STATUS_REQUEST;
        *count = 0;
        return TRUE;
    }
    BOOL secondary = length > 0 && p[0] == '>';
    LONG i = secondary ? 1 : 0;
    IPTR selector = 0;
    while (i < length && p[i] >= '0' && p[i] <= '9')
    {
        if (selector > 6) return FALSE;
        selector = selector * 10 + p[i++] - '0';
    }
    if (i < length && p[i] == 'c' && selector == 0)
    {
        *command = secondary ? C_SECONDARY_ATTRIBUTES : C_PRIMARY_ATTRIBUTES;
        *count = 0;
        *input = p + i + 1;
        return TRUE;
    }
    if (secondary || !i || i >= length || p[i] != 'n' ||
        (selector != 5 && selector != 6)) return FALSE;
    params[0] = selector;
    params[1] = native;
    *count = 2;
    *command = C_DEVICE_STATUS_REPORT;
    *input = p + i + 1;
    return TRUE;
}

static BOOL getscrollregion(BYTE *command, UBYTE **input, UBYTE *count,
    LONG length, IPTR *params)
{
    UBYTE *p = *input;
    IPTR values[2] = {0, 0};
    LONG i = 0;
    UBYTE field = 0;
    while (i < length)
    {
        if (p[i] >= '0' && p[i] <= '9')
            values[field] = MIN(values[field] * 10 + p[i] - '0', 65535);
        else if (p[i] == ';')
        {
            if (++field > 1) return FALSE;
        }
        else break;
        i++;
    }
    if (i == length || p[i] != 'r') return FALSE;
    params[0] = values[0]; params[1] = values[1];
    *count = 2; *command = C_SET_SCROLL_REGION; *input = p + i + 1;
    return TRUE;
}

static BOOL getabsolutecommand(BYTE *command, UBYTE **input, UBYTE *count,
    LONG length, IPTR *params)
{
    UBYTE *p = *input;
    IPTR values[2] = {1, 1};
    LONG i = 0;
    UBYTE field = 0;
    BOOL digits = FALSE;
    BYTE cmd;
    while (i < length)
    {
        if (p[i] >= '0' && p[i] <= '9')
        {
            if (!digits) values[field] = 0;
            digits = TRUE;
            values[field] = MIN(values[field] * 10 + p[i] - '0', 65535);
        }
        else if (p[i] == ';')
        {
            if (++field > 1) return FALSE;
            digits = FALSE;
        }
        else break;
        i++;
    }
    if (i == length) return FALSE;
    switch (p[i])
    {
    case 'H': case 'f': cmd = C_CURSOR_POS; break;
    case 'G': case '`': cmd = C_CURSOR_COLUMN; break;
    case 'd': cmd = C_CURSOR_ROW; break;
    default: return FALSE;
    }
    if (cmd != C_CURSOR_POS && field) return FALSE;
    params[0] = values[0]; params[1] = values[1];
    *count = cmd == C_CURSOR_POS ? 2 : 1;
    *command = cmd; *input = p + i + 1;
    return TRUE;
}

/* Validate the full ANSI mode list before applying any changes. Private
 * Amiga/DEC sequences remain with their existing parsers. */
static BOOL getansimodecommand(BYTE *cmd_ptr, UBYTE **writestr_ptr,
    UBYTE *numparams_ptr, LONG toparse, Object *unit)
{
    UBYTE *p = *writestr_ptr;
    LONG end = 0, i;
    ULONG mode = 0;
    BOOL overflow = FALSE, set;
    while (end < toparse && ((p[end] >= '0' && p[end] <= '9') || p[end] == ';')) end++;
    if (end == 0 || end >= toparse || (p[end] != 'h' && p[end] != 'l')) return FALSE;
    set = p[end] == 'h';
    for (i = 0; i <= end; i++)
    {
        if (i < end && p[i] != ';')
        {
            if (mode > (65535UL - (p[i] - '0')) / 10) overflow = TRUE;
            if (!overflow) mode = mode * 10 + p[i] - '0';
        }
        else
        {
            if (!overflow && mode == 4)
            {
                if (set) ICU(unit)->conFlags |= CF_INSERT_MODE;
                else ICU(unit)->conFlags &= ~CF_INSERT_MODE;
            }
            else if (!overflow && mode == 20)
                Console_DoCommand(unit, set ? C_SET_LF_MODE : C_RESET_LF_MODE, 0, NULL);
            mode = 0; overflow = FALSE;
        }
    }
    *writestr_ptr = p + end + 1; *cmd_ptr = C_NIL; *numparams_ptr = 0;
    return TRUE;
}

/* DECRQM accepts a single decimal mode and a dollar intermediate. Validate
 * the complete sequence before querying or emitting an unframed input reply. */
static BOOL getmoderequest(BYTE *cmd_ptr, UBYTE **writestr_ptr,
    UBYTE *numparams_ptr, LONG toparse, Object *unit, struct ConsoleBase *ConsoleDevice)
{
    UBYTE *p = *writestr_ptr;
    LONG i = 0;
    ULONG mode = 0, state = 0;
    BOOL privateMode = toparse > 0 && p[0] == '?', overflow = FALSE;
    if (privateMode) i++;
    while (i < toparse && p[i] >= '0' && p[i] <= '9')
    {
        if (mode > (65535UL - (p[i] - '0')) / 10) overflow = TRUE;
        if (!overflow) mode = mode * 10 + p[i] - '0';
        i++;
    }
    if (i + 1 >= toparse || p[i] != '$' || p[i + 1] != 'p') return FALSE;
    *writestr_ptr = p + i + 2; *cmd_ptr = C_NIL; *numparams_ptr = 0;
    if (!overflow)
    {
        IPTR params[3] = {privateMode, mode, (IPTR)&state};
        char reply[32];
        Console_DoCommand(unit, C_GET_MODE_STATE, 3, params);
        LONG length = snprintf(reply, sizeof(reply), "\x1b[%s%lu;%lu$y",
            privateMode ? "?" : "", (unsigned long)mode, (unsigned long)state);
        con_inject(ConsoleDevice, CU(unit), (const UBYTE *)reply, length);
    }
    return TRUE;
}

/* DECSCUSR has a space intermediate; it must not be parsed as plain CSI q.
 * Validate the entire sequence before changing any output state. */
static BOOL getcursorstylecommand(BYTE *cmd_ptr, UBYTE **writestr_ptr,
    UBYTE *numparams_ptr, LONG toparse, IPTR *params)
{
    UBYTE *p = *writestr_ptr;
    LONG i = 0;
    ULONG style = 0;
    while (i < toparse && p[i] >= '0' && p[i] <= '9')
    {
        style = MIN(style * 10 + p[i] - '0', 7);
        i++;
    }
    if (i + 1 >= toparse || p[i] != ' ' || p[i + 1] != 'q')
        return FALSE;
    *writestr_ptr = p + i + 2;
    *cmd_ptr = C_NIL;
    *numparams_ptr = 0;
    if (style <= 6)
    {
        params[0] = style;
        *numparams_ptr = 1;
        *cmd_ptr = C_SET_CURSOR_STYLE;
    }
    return TRUE;
}

static BOOL getdecprivatecommand(BYTE *cmd_ptr, UBYTE ** writestr_ptr,
    LONG toparse, Object *unit)
{
    UBYTE *p = *writestr_ptr;
    ULONG value = 0;
    BOOL havevalue = FALSE;
    BOOL set;
    LONG i;

    if (toparse < 2 || *p != '?')
        return FALSE;

    /* Locate and validate the final byte before changing any mode state. */
    for (i = 1; i < toparse; i++)
    {
        if (p[i] >= 0x40 && p[i] <= 0x7E)
            break;
        if (p[i] < 0x30 || p[i] > 0x3F)
            return FALSE;
    }
    if (i >= toparse)
        return FALSE;

    /* Unknown DEC private controls are still valid CSI and must be ignored
       whole.  Mode set/reset is the only family with state to retain here. */
    if (p[i] != 'h' && p[i] != 'l')
    {
        *cmd_ptr = C_NIL;
        *writestr_ptr = p + i + 1;
        return TRUE;
    }

    set = p[i] == 'h';
    *cmd_ptr = C_NIL;

    for (i = 1; ; i++)
    {
        UBYTE c = p[i];

        if (c >= '0' && c <= '9')
        {
            value = value * 10 + (c - '0');
            if (value > 65535)
                value = 65535;
            havevalue = TRUE;
            continue;
        }

        if (havevalue)
        {
            switch (value)
            {
            case 1:
                if (set)
                    ICU(unit)->conFlags |= CF_APPLICATION_CURSOR;
                else
                    ICU(unit)->conFlags &= ~CF_APPLICATION_CURSOR;
                break;
            case 3:
                {
                    /* DECCOLM follows the reference's reset side effects;
                     * native window/font geometry remains unchanged. */
                    IPTR region[2] = {0, 0}, erase = 2;
                    Console_DoCommand(unit, C_SET_SCROLL_REGION, 2, region);
                    Console_DoCommand(unit, C_ERASE_IN_DISPLAY, 1, &erase);
                }
                break;
            case 6:
                {
                    IPTR origin = set;
                    Console_DoCommand(unit, C_SET_ORIGIN_MODE, 1, &origin);
                }
                break;
            case 7:
                Console_DoCommand(unit, set ? C_SET_AUTOWRAP_MODE :
                    C_RESET_AUTOWRAP_MODE, 0, NULL);
                break;
            case 12:
                {
                    IPTR blink = set;
                    Console_DoCommand(unit, C_SET_CURSOR_BLINK, 1, &blink);
                }
                break;
            case 25:
                *cmd_ptr = set ? C_CURSOR_VISIBLE : C_CURSOR_INVISIBLE;
                break;
            case 1049:
                {
                    IPTR mode = set;
                    Console_DoCommand(unit, C_ALTERNATE_SCREEN, 1, &mode);
                }
                break;
            case 2026:
                {
                    IPTR mode = set;
                    Console_DoCommand(unit, C_SET_SYNC_OUTPUT, 1, &mode);
                }
                break;
            case 2004:
                if (set)
                    ICU(unit)->conFlags |= CF_BRACKETED_PASTE;
                else
                    ICU(unit)->conFlags &= ~CF_BRACKETED_PASTE;
                break;
            case 1000:
            case 1002:
            case 1003:
                if ((set && ICU(unit)->mouseMode != value) ||
                    (!set && ICU(unit)->mouseMode == value))
                    ICU(unit)->mouseReportedButtons = 0;
                if (set) ICU(unit)->mouseMode = value;
                else if (ICU(unit)->mouseMode == value) ICU(unit)->mouseMode = 0;
                ICU(unit)->mouseLastValid = FALSE;
                break;
            case 1005:
                ICU(unit)->mouseUTF8 = set;
                if (set) ICU(unit)->mouseSGR = FALSE;
                break;
            case 1006:
                ICU(unit)->mouseSGR = set;
                if (set) ICU(unit)->mouseUTF8 = FALSE;
                break;
            case 1007:
                if (set) ICU(unit)->conFlags |= CF_ALTERNATE_SCROLL;
                else ICU(unit)->conFlags &= ~CF_ALTERNATE_SCROLL;
                break;
            case 1004:
                if (set)
                    ICU(unit)->conFlags |= CF_FOCUS_REPORTING;
                else
                    ICU(unit)->conFlags &= ~CF_FOCUS_REPORTING;
                break;
            case 2031:
                if (set)
                    ICU(unit)->conFlags |= CF_COLOR_SCHEME_NOTIFY;
                else
                    ICU(unit)->conFlags &= ~CF_COLOR_SCHEME_NOTIFY;
                break;
            }
        }

        value = 0;
        havevalue = FALSE;
        if (c == ';')
            continue;

        *writestr_ptr = p + i + 1;
        return TRUE;
    }
}


/************************
**  getparamcommand()  **
************************/

/* !!! IMPORTANT !!!
   If you add a command here, you should also add default values for
   its parameters in Console::GetDefaultParams()
*/
static const struct Command
{
    BYTE Command;
    UBYTE MaxParams;

} csi2command[] =
{
    {C_INSERT_CHAR,              1},                     /* 0x40 @ */
    {C_CURSOR_UP,                1},                     /* 0x41 A */
    {C_CURSOR_DOWN,              1},                     /* 0x42 B */
    {C_CURSOR_FORWARD,           1},                     /* 0x43 C */
    {C_CURSOR_BACKWARD,          1},                     /* 0x44 D */
    {C_CURSOR_NEXT_LINE,         1},                     /* 0x45 E */
    {C_CURSOR_PREV_LINE,         1},                     /* 0x46 F */
    {C_CURSOR_COLUMN,            1},                     /* 0x47 G */
    {C_CURSOR_POS,               2},                     /* 0x48 H */
    {C_CURSOR_HTAB,              1},                     /* 0x49 I */
    {C_ERASE_IN_DISPLAY,         1},                     /* 0x4A J */
    {C_ERASE_IN_LINE,            1},                     /* 0x4B K */
    {C_INSERT_LINE,              1},                     /* 0x4C L */
    {C_DELETE_LINE,              1},                     /* 0x4D M */
    {-1,                         },                      /* 0x4E N */
    {-1,                         },                      /* 0x4F O */
    {C_DELETE_CHAR,              1},                     /* 0x50 P */
    {-1,                         },                      /* 0x51 Q */
    {-1,                         },                      /* 0x52 R */
    {C_SCROLL_UP,                1},                     /* 0x53 S */
    {C_SCROLL_DOWN,              1},                     /* 0x54 T */
    {-1,                         },                      /* 0x55 U */
    {-1,                         },                      /* 0x56 V */
    {C_CURSOR_TAB_CTRL,          1},                     /* 0x57 W */
    {C_ERASE_CHAR,               1},                     /* 0x58 X */
    {-1,                         },                      /* 0x59 Y */
    {C_CURSOR_BACKTAB,           1},                     /* 0x5A Z */
    {-1,                         },                      /* 0x5B [ */
    {-1,                         },                      /* 0x5C \ */
    {-1,                         },                      /* 0x5D ] */
    {-1,                         },                      /* 0x5E ^ */
    {-1,                         },                      /* 0x5F _ */
    {C_CURSOR_COLUMN,            1},                     /* 0x60 ` */
    {C_CURSOR_FORWARD,           1},                     /* 0x61 a */
    {C_REPEAT_CHAR,              1},                     /* 0x62 b */
    {-1,                         },                      /* 0x63 c */
    {C_CURSOR_ROW,               1},                     /* 0x64 d */
    {C_CURSOR_DOWN,              1},                     /* 0x65 e */
    {C_CURSOR_POS,               2},                     /* 0x66 f */
    {C_CLEAR_TABS,               1},                     /* 0x67 g */
    {-1,                         },                      /* 0x68 h */
    {-1,                         },                      /* 0x69 i */
    {-1,                         },                      /* 0x6A j */
    {-1,                         },                      /* 0x6B k */
    {-1,                         },                      /* 0x6C l */
    {C_SELECT_GRAPHIC_RENDITION, MAX_COMMAND_PARAMS},    /* 0x6D m */
    {-1,                         },                      /* 0x6E n */
    {-1,                         },                      /* 0x6F o */
    {-1,                         },                      /* 0x70 p */
    {-1,                         },                      /* 0x71 q */
    {-1,                         },                      /* 0x72 r */
    {-1,                         },                      /* 0x73 s */
    {C_SET_PAGE_LENGTH,          1},                     /* 0x74 t */
    {C_SET_LINE_LENGTH,          },                      /* 0x75 u */
    {-1,                         },                      /* 0x76 v */
    {-1,                         },                      /* 0x77 w */
    {C_SET_LEFT_OFFSET,          1},                     /* 0x78 x */
    {C_SET_TOP_OFFSET,           1},                     /* 0x79 y */
    {-1,                         },                      /* 0x7A z */
    {C_SET_RAWEVENTS,            MAX_COMMAND_PARAMS},    /* 0x7B { */
    {-1,                         },                      /* 0x7C | */
    {C_RESET_RAWEVENTS,          MAX_COMMAND_PARAMS},    /* 0x7D } */
};


#define PARAM_BUF_SIZE MAX_COMMAND_PARAMS
/* Parameters for commands are parsed and filled into this one */
struct cmd_params
{
    UBYTE numparams;            /* Parameters stored */

    /* Since parameters may be optional, only supplied parameters
       are saved, along with their number. For example
       for the command CURSOR POSITION, if only the second parameter
       (column) is specified in the write stream, then
       numparams will be 1 and for the one entry, paramno will be 1
       (C counting) and val will be <column>.
       Row will have to be set to some default value.
     */
    struct cmd_param
    {
        UBYTE paramno;          /* Starts counting at 0 */
        UWORD val;              /* was UBYTE - wrapped any value > 255 */
    } tab[PARAM_BUF_SIZE];
};


static BOOL getparamcommand(BYTE *cmd_ptr, UBYTE ** writestr_ptr,
    UBYTE *numparams_ptr, LONG toparse, IPTR *p_tab, Object *unit,
    struct ConsoleBase *ConsoleDevice)
{
    /* This function checks for a command with parameters in
     ** the string. The problem is that the parameters come
     ** before the comand ID, and the parameters are optional.
     ** This means that a parameter which has the same value
     ** as a command ID may be mistakenly taken for being
     ** end of the command. Therefore we must continue scanning
     ** even if we found a command ID.
     */

    struct cmd_params params = { };
    BYTE cmd = -1;
    BYTE cmd_next_idx = 0;      /* Index to byte after the command */

    /* write_str points to first character after <CSI> */
    UBYTE *write_str = *writestr_ptr;

    UBYTE num_params = 0;
    BOOL done = FALSE, found = FALSE;
    BOOL next_can_be_separator = TRUE,
        next_can_be_param = TRUE,
        next_can_be_commandid = TRUE, last_was_param = FALSE;
    UBYTE num_separators_found = 0;

    while (!done)
    {
        /* In case it's a parameter */

        if (toparse <= 0)
        {
            done = TRUE;
            break;
        }

        switch (*write_str)
        {
        case 0x40:
        case 0x41:
        case 0x42:
        case 0x43:
        case 0x44:
        case 0x45:
        case 0x46:
        case 0x47:
        case 0x48:
        case 0x49:

        case 0x4A:
        case 0x4B:
        case 0x4C:
        case 0x4D:

        case 0x50:
        case 0x53:
        case 0x54:
        case 0x57:
        case 0x58:
        case 0x5A:
        case 0x60:
        case 0x61:
        case 0x62:
        case 0x64:
        case 0x65:
        case 0x66:
        case 0x67:
        case 0x6D:
        case 0x74:
        case 0x75:
        case 0x78:
        case 0x79:
        case 0x7B:
        case 0x7D:
            {
                UBYTE idx = *write_str - FIRST_CSI_CMD;
                UBYTE maxparams = csi2command[idx].MaxParams;

                if (next_can_be_commandid)
                {
/* FIXME: Should also do a MinParams compare */
                    if (num_params <= maxparams) /* Valid command? */
                    {

                        /* Assure that there are not too many separators in
                         * a command  */
                        if ((num_separators_found < maxparams)
/* FIXME: 0-param commands can be moved to special-command-handlin in string2command() */
                            || ((num_separators_found == 0)
                                && (maxparams == 0)))
                        {
                            cmd = csi2command[idx].Command;

                            /* Save index to where the next command will
                             * start */
                            cmd_next_idx = write_str - *writestr_ptr + 1;

                            params.numparams = num_params;
                        }
                    }
                }

                done = TRUE;

                break;
            }

        case ';': /* parameter separator, skip it */

            if (!next_can_be_separator)
            {
                /* Error */
                done = TRUE;
                break;
            }

            next_can_be_separator = FALSE;
            next_can_be_param = TRUE;
            next_can_be_commandid = FALSE;
            last_was_param = FALSE;

            num_separators_found++;

            break;

        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
        case '>': /* because of SGR background color param :-( */
            if (!next_can_be_param)
            {
                /* Error */
                done = TRUE;
                break;
            }

            if (!last_was_param)
            {
                num_params++;
                if (num_params > MAX_COMMAND_PARAMS)
                {
                    done = TRUE;
                    break;
                }
                params.tab[num_params - 1].paramno = num_params - 1;
                params.tab[num_params - 1].val = 0;

                last_was_param = TRUE;
            }

            {
                ULONG v = (ULONG) params.tab[num_params - 1].val * 10;

                if (*write_str == '>')
                    v += 5;
                else
                    v += (*write_str) - '0';

                params.tab[num_params - 1].val = v > 65535 ? 65535 : v;
            }

            next_can_be_separator = TRUE;
            next_can_be_commandid = TRUE;
            break;

        default:
            /* Error */
            done = TRUE;
            break;
        } /* switch */

        write_str++;
        toparse--;
    } /* while (!done) */

    if (cmd != -1)
    {
        *cmd_ptr = cmd;
        found = TRUE;

        /* Continue parsing on the first byte after the command */
        *writestr_ptr += cmd_next_idx;
    }

    if (found)
    {
        UBYTE i;
        /* First fill in some default values in p_tab */
        Console_GetDefaultParams(unit, *cmd_ptr, p_tab);

        for (i = 0; i < params.numparams; i++)
        {
            /* Override with parsed values */
            D(bug("CMD %s: Setting param %d to %d\n", cmd_names[*cmd_ptr],
                    params.tab[i].paramno, params.tab[i].val));

            p_tab[params.tab[i].paramno] = params.tab[i].val;
        }

        *numparams_ptr = params.numparams;
    }

    return found;
}

VOID printstring(STRPTR string, ULONG len,
    struct ConsoleBase *ConsoleDevice)
{
    while (len--)
    {
        bug("%d/%c ", *string, *string);
        string++;
    }

    bug("\n");
}
