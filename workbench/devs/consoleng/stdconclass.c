/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc: Code for CONU_STANDARD console units.
*/

#define SDEBUG 0
#define DEBUG 0
#include <aros/debug.h>

#include <proto/graphics.h>
#include <proto/cybergraphics.h>
#include <cybergraphx/cybergraphics.h>
#include <proto/intuition.h>
#include <proto/utility.h>

#include <intuition/intuition.h>
#include <graphics/rastport.h>
#include <exec/rawfmt.h>
#include <dos/dos.h>
#include <aros/asmcall.h>

#include <string.h>

#include "console_gcc.h"
#include "clipboard_prefs.h"
#include "consoleif.h"

/* Full nonnegative WORD coordinate range; public ConUnit layout stays fixed. */
#include "standard_widths.h"

#define STDCON_TAB_COLUMNS 32768
struct stdcondata
{
    struct con_width_grid widths;
    UBYTE tabBits[STDCON_TAB_COLUMNS / 8];
    UWORD tabMirror[MAXTABS];
    struct DrawInfo *dri;
    WORD rendercursorcount;
    BOOL cursorvisible;
    BOOL windowactive;
    UBYTE cursorstyle;          /* DECSCUSR 0..6; default0 remains steady */
    BOOL resetClearPending, alignmentPending;
    UBYTE resetCursorStyle;     /* preference, independent of application overrides */
    BOOL cursorpainted;
    ULONG resetCursorRGB, cursorPrefRGB;
    ULONG cursorRGB;            /* CONPEN_RGB key; zero selects legacy inversion */
    BOOL cursorColored, cursorIndexed;
    LONG cursorPen;
    struct BitMap *cursorSaved;
    ULONG cursorSavedWidth, cursorSavedHeight;
    struct Library *cursorCyberBase;
    BOOL cursorQueryReady, cursorPatterned;
    LONG cursorLeft, cursorTop, cursorRight, cursorBottom;
    BOOL cursorTimerReady;
    ULONG cursorInterval, cursorTimeout, cursorStart;
    ULONG cursorStamp;          /* milliseconds from input.device events */

    UWORD pens[CONUNIT_PEN_MAX];
    UWORD *penmap[CONUNIT_PEN_MAX];

    /* ObtainBestPen results for the 8 ANSI colors (-1 = not obtained) */
    LONG obtainedpens[8];
    struct ColorMap *cm;

    /* Lazily obtained xterm-256 palette pens (-1 = not yet) */
    LONG xtermpens[256];
    UWORD xtermCount, xtermNext, xtermLast;

    /* xterm dynamic colours: OSC 10/11 default fg/bg ([0]/[1]) and OSC 4
       palette entries 0-255. rgb 0 = not set, else a CONPEN_RGB key. */
    ULONG schemeRGB;            /* configured background; OSC overrides excluded */
    ULONG defrgb[2];
    LONG defpen[2];
    ULONG ansirgb[256];
    LONG ansipen[256];

    /* Small truecolor pen cache, round-robin eviction */
#define STDCON_TCPENS 64
    struct
    {
        ULONG rgb;              /* 0x01000000 | RRGGBB key, 0 = empty */
        LONG pen;
    } tcpens[STDCON_TCPENS];
    UWORD tcnext;

    /* MERGE (upstream 128f15fc05): single-character scratch raster reused across
       Text() calls via InitTmpRas -- a render-buffer optimisation, orthogonal to
       our UCS-4 text layout, so kept alongside the pen caches above (my conflict
       resolution dropped these two members while its usage auto-merged in). */
    PLANEPTR charScratch;
    UWORD    charScratchHeight;

    /* Only the unfinished styled run in a plain console, never scrollback. */
    UBYTE *styleRun;
    ULONG styleLength, styleCapacity;
    LONG styleX, styleY, styleEnd;

    /* Libraries */
    struct Library *scd_GfxBase;
};

/* xterm's default palette: SGR 30-37/40-47 and xterm entries 0-7 */
static const UBYTE ansi_rgb[8][3] =
{
    {0x00, 0x00, 0x00},         /* black */
    {0xCD, 0x00, 0x00},         /* red */
    {0x00, 0xCD, 0x00},         /* green */
    {0xCD, 0xCD, 0x00},         /* yellow */
    {0x00, 0x00, 0xEE},         /* blue */
    {0xCD, 0x00, 0xCD},         /* magenta */
    {0x00, 0xCD, 0xCD},         /* cyan */
    {0xE5, 0xE5, 0xE5},         /* white */
};

/* SGR 90-97/100-107 and xterm entries 8-15 */
static const UBYTE ansi_bright_rgb[8][3] =
{
    {0x7F, 0x7F, 0x7F},
    {0xFF, 0x00, 0x00},
    {0x00, 0xFF, 0x00},
    {0xFF, 0xFF, 0x00},
    {0x5C, 0x5C, 0xFF},
    {0xFF, 0x00, 0xFF},
    {0x00, 0xFF, 0xFF},
    {0xFF, 0xFF, 0xFF},
};

/* xterm-256 palette entry -> RGB */
static void xterm_to_rgb(UWORD n, UBYTE *r, UBYTE *g, UBYTE *b)
{
    static const UBYTE lvl[6] = { 0, 95, 135, 175, 215, 255 };

    if (n < 8)
    {
        *r = ansi_rgb[n][0]; *g = ansi_rgb[n][1]; *b = ansi_rgb[n][2];
    }
    else if (n < 16)
    {
        *r = ansi_bright_rgb[n - 8][0];
        *g = ansi_bright_rgb[n - 8][1];
        *b = ansi_bright_rgb[n - 8][2];
    }
    else if (n < 232)
    {
        n -= 16;
        *r = lvl[n / 36]; *g = lvl[(n / 6) % 6]; *b = lvl[n % 6];
    }
    else
    {
        UBYTE v = 8 + (n - 232) * 10;
        *r = v; *g = v; *b = v;
    }
}

/* 0x01010101 * component: replicate an 8-bit value to 32 bits */
static LONG stdcon_bestpen(struct stdcondata *data, UBYTE r, UBYTE g,
    UBYTE b)
{
    struct Library *GfxBase = data->scd_GfxBase;

    return ObtainBestPenA(data->cm,
        (ULONG) r * 0x01010101UL,
        (ULONG) g * 0x01010101UL,
        (ULONG) b * 0x01010101UL, NULL);
}


#undef ConsoleDevice
#define ConsoleDevice ((struct ConsoleBase *)cl->cl_UserData)

/***********  StdCon::New()  **********************/

static Object *stdcon_new(Class *cl, Object *o, struct opSet *msg)
{
    EnterFunc(bug("StdCon::New()\n"));
    o = (Object *) DoSuperMethodA(cl, o, (Msg) msg);
    if (o)
    {
        struct stdcondata *data = INST_DATA(cl, o);
        STACKULONG dispmid = OM_DISPOSE;
        int i;

        /* Clear for checking inside dispose() whether stuff was allocated.
           Basically this is bug-prevention.
         */
        SetMem(data, 0, sizeof(struct stdcondata));
        /* One default stop every eight columns, including beyond legacy table. */
        memset(data->tabBits, 1, sizeof(data->tabBits));
        memcpy(data->tabMirror, CU(o)->cu_TabStops, sizeof(data->tabMirror));
        for (i = 0; i < CONUNIT_PEN_MAX; i ++)
        {
            data->pens[i] = i;
            data->penmap[i] = &data->pens[i];
        }

        data->scd_GfxBase = TaggedOpenLibrary(TAGGEDOPEN_GRAPHICS);
        if (data->scd_GfxBase)
        {
            /* MERGE: AllocRaster below (upstream scratch-raster) needs GfxBase
               in scope; its declaration was in the conflict side I resolved to
               ours. dispose() already has its own. */
            struct Library *GfxBase = data->scd_GfxBase;
            data->dri = GetScreenDrawInfo(CU(o)->cu_Window->WScreen);
            if (data->dri)
            {
                /* SGR 30-37/40-47 stay on the SYSTEM PENS - pens[i] == i, as
                   set above and as the stock console.device does.

                   This device used to ObtainBestPen() true ANSI RGB for these
                   eight, which is more faithful to a terminal and is the wrong
                   default here. AROS's own prompt is written against pen
                   INDICES, not against ANSI colours (workbench/s/Shell-Startup:
                   "*E[42m*E[31m%N.*E[43m*E[32m%s*E[42m*E[31m>*E[40m*E[31m "),
                   so on the stock console those numbers pick muted Workbench
                   theme colours. Resolving them to real red/green instead made
                   an ordinary Shell prompt come out vivid red-on-green - the
                   device looked broken next to the console it sits beside.
                   terminills: it should look like a normal Console by default.

                   Nothing is lost. An application that genuinely wants colour
                   asks for it explicitly with xterm-256 (SGR 38;5;n) or
                   truecolor (SGR 38;2;r;g;b), and those pens are still
                   obtained lazily on first use, below and in
                   Console_GetColorPen(). Only the eight legacy slots that
                   predate any of that are left alone. */
                data->cm = CU(o)->cu_Window->WScreen->ViewPort.ColorMap;
                for (i = 0; i < 256; i++)
                    data->xtermpens[i] = -1;
                for (i = 0; i < 8; i++)
                    data->obtainedpens[i] = -1;
                for (i = 0; i < 256; i++)
                    data->ansipen[i] = -1;
                data->defpen[0] = data->defpen[1] = -1;

                data->penmap[CONUNIT_PEN_DEFBG] =
                    &data->dri->dri_Pens[BACKGROUNDPEN];
                data->penmap[CONUNIT_PEN_DEFFG] =
                    &data->dri->dri_Pens[TEXTPEN];

                CU(o)->cu_BgPen = (BYTE)*(data->penmap[CONUNIT_PEN_DEFBG]);
                CU(o)->cu_FgPen = (BYTE)*(data->penmap[CONUNIT_PEN_DEFFG]);

                data->charScratchHeight = RASTPORT(o)->Font->tf_YSize;
                data->charScratch = AllocRaster(8, data->charScratchHeight);

                data->cursorCyberBase = OpenLibrary((CONST_STRPTR)"cybergraphics.library", 0);
                data->cursorInterval = 500;
                data->cursorvisible = TRUE;
                data->windowactive =
                    (CU(o)->cu_Window->Flags & WFLG_WINDOWACTIVE) != 0;
                Console_RenderCursor(o);
                data->cursorQueryReady = TRUE; /* Child storage is ready after this constructor returns. */

                ReturnPtr("StdCon::New", Object *, o);
            }
        }
        CoerceMethodA(cl, o, (Msg) &dispmid);
    }
    ReturnPtr("StdCon::New", Object *, NULL);

}

/***********  StdCon::Dispose()  **************************/

static VOID stdcon_dispose(Class *cl, Object *o, Msg msg)
{
    con_sync_dispose(CU(o));
    con_keyboard_dispose(CU(o));
    con_osc_clipboard_reset(ICU(o));
    struct stdcondata *data = INST_DATA(cl, o);
    struct Library *GfxBase = data->scd_GfxBase;

    if (data->cursorIndexed) ReleasePen(data->cm, data->cursorPen);
    if (data->cursorSaved) FreeBitMap(data->cursorSaved);
    if (data->cursorCyberBase) CloseLibrary(data->cursorCyberBase);
    FreeVec(data->styleRun);
    FreeVec(data->widths.cells);
    if (data->charScratch)
        FreeRaster(data->charScratch, 8, data->charScratchHeight);

    if (data->cm && data->scd_GfxBase)
    {
        struct Library *GfxBase = data->scd_GfxBase;
        int i;

        for (i = 0; i < 8; i++)
        {
            if (data->obtainedpens[i] != -1)
                ReleasePen(data->cm, data->obtainedpens[i]);
        }
        for (i = 0; i < 256; i++)
        {
            if (data->xtermpens[i] != -1)
                ReleasePen(data->cm, data->xtermpens[i]);
        }
        for (i = 0; i < STDCON_TCPENS; i++)
        {
            if (data->tcpens[i].rgb)
                ReleasePen(data->cm, data->tcpens[i].pen);
        }
        for (i = 0; i < 256; i++)
        {
            if (data->ansipen[i] != -1)
                ReleasePen(data->cm, data->ansipen[i]);
        }
        for (i = 0; i < 2; i++)
        {
            if (data->defpen[i] != -1)
                ReleasePen(data->cm, data->defpen[i]);
        }
    }

    if (data->scd_GfxBase)
        CloseLibrary(data->scd_GfxBase);

    if (data->dri)
        FreeScreenDrawInfo(CU(o)->cu_Window->WScreen, data->dri);

    /* Let superclass free its allocations */
    DoSuperMethodA(cl, o, msg);

    return;
}

VOID setabpen(struct Library *GfxBase, struct RastPort *rp, UBYTE tflags,
    UBYTE FgPen, UBYTE BgPen)
{
    UBYTE fg = FgPen, bg = BgPen;
    UBYTE style = JAM2;

    if (tflags & CON_TXTFLAGS_CONCEALED)
        fg = bg;
    else if (tflags & CON_TXTFLAGS_REVERSED)
        style |= INVERSVID;
    SetABPenDrMd(rp, fg, bg, style);
}

/* Match the reference's default dim factor without floating-point state.
 * Indexed cube/grayscale colors are unchanged; low indexed colors follow
 * dim/normal slots when not bold. No logical rendition key is overwritten. */
UBYTE ConsoleNGFaintPen(Object *o, struct Library *GfxBase, UBYTE flags,
    UBYTE foreground, ULONG key)
{
    ULONG rgb[3];
    UBYTE pen = foreground;
    if (!(flags & CON_TXTFLAGS_FAINT) || (flags & CON_TXTFLAGS_CONCEALED))
        return foreground;
    if (CONPEN_ISBRIGHT(key))
    {
        Console_GetColorPen(o, CONPEN_XTERM(CONPEN_BRIGHTIDX(key)), &pen);
        return pen;
    }
    if (CONPEN_ISXTERM(key))
    {
        ULONG index = CONPEN_XTERMIDX(key);
        if (index >= 16 || (flags & CON_TXTFLAGS_BOLD)) return foreground;
        if (index >= 8)
        {
            Console_GetColorPen(o, CONPEN_XTERM(index - 8), &pen);
            return pen;
        }
    }
    if (!CU(o)->cu_Window || !CU(o)->cu_Window->WScreen) return foreground;
    GetRGB32(CU(o)->cu_Window->WScreen->ViewPort.ColorMap, foreground, 1, rgb);
    ULONG dim = CONPEN_RGB((rgb[0] >> 24) * 66 / 100,
        (rgb[1] >> 24) * 66 / 100, (rgb[2] >> 24) * 66 / 100);
    Console_GetColorPen(o, dim, &pen);
    return pen;
}

static void setstyle(struct Library *GfxBase, struct RastPort *rp,
    Object *o)
{
    UBYTE tflags = CU(o)->cu_TxFlags;
    setabpen(GfxBase, rp, tflags,
        ConsoleNGFaintPen(o, GfxBase, tflags, CU(o)->cu_FgPen, ICU(o)->fgColor),
        CU(o)->cu_BgPen);
    SetSoftStyle(rp, (ICU(o)->underlineStyle || ICU(o)->underlineColor) ? tflags & ~CON_TXTFLAGS_UNDERLINED : tflags, CON_TXTFLAGS_MASK);
}

static void stdcon_text(struct stdcondata *data, struct RastPort *rp,
    CONST_STRPTR text, ULONG len)
{
    struct TmpRas *savedTmpRas = rp->TmpRas;
    struct TmpRas charTmpRas;
    struct Library *GfxBase = data->scd_GfxBase;
    ULONG scratchSize = RASSIZE(8, data->charScratchHeight);

    if (len == 1 && data->charScratch &&
        (!savedTmpRas || savedTmpRas->Size < scratchSize))
        rp->TmpRas = InitTmpRas(&charTmpRas, data->charScratch, scratchSize);

    Text(rp, text, len);
    rp->TmpRas = savedTmpRas;
}

/* Native JAM2 Text() paints a run's background before its glyphs. A new
 * call can erase the preceding italic/bold overhang. Plain consoles have no
 * retained row to recompose, so keep only the current contiguous styled run.
 * Commands, geometry changes and Unicode drawing end that run.
 * Repaint decorations with it: otherwise the text background erases them.
 */
static void stdcon_native_span(struct stdcondata *data, Object *o, BOOL plain,
    CONST_STRPTR text, ULONG len)
{
    struct RastPort *rp = RASTPORT(o);
    struct Library *GfxBase = data->scd_GfxBase;
    LONG x = CP_X(o), y = CP_Y(o);
    ULONG count = len;
    ULONG capacity = CHAR_XMAX(o) + 1;
    UBYTE flags = CU(o)->cu_TxFlags;

    if (plain && len && capacity <= STDCON_TAB_COLUMNS &&
        (flags & (CON_TXTFLAGS_BOLD | CON_TXTFLAGS_ITALIC)))
    {
        if (data->styleCapacity < capacity)
        {
            UBYTE *buffer = AllocVec(capacity, MEMF_ANY);
            if (buffer)
            {
                FreeVec(data->styleRun);
                data->styleRun = buffer;
                data->styleCapacity = capacity;
            }
            data->styleLength = 0;
        }
        if (data->styleLength &&
            (x != data->styleEnd || y != data->styleY))
            data->styleLength = 0;
        if (len <= data->styleCapacity &&
            data->styleLength <= data->styleCapacity - len)
        {
            if (!data->styleLength)
            {
                data->styleX = x;
                data->styleY = y;
            }
            CopyMem(text, data->styleRun + data->styleLength, len);
            data->styleLength += len;
            data->styleEnd = x + len * XRSIZE;
            x = data->styleX;
            text = (CONST_STRPTR)data->styleRun;
            count = data->styleLength;
        }
        else data->styleLength = 0;
    }
    else data->styleLength = 0;

    Move(rp, x, y + rp->Font->tf_Baseline);
    stdcon_text(data, rp, text, count);
    ULONG fg = ConsoleNGFaintPen(o, GfxBase, flags, CU(o)->cu_FgPen, ICU(o)->fgColor);
    ConsoleNGStrike(GfxBase, rp, flags, x, y, count * XRSIZE, YRSIZE,
        fg, CU(o)->cu_BgPen);
    ConsoleNGStyledUnderline(GfxBase, rp, flags, x, y, count * XRSIZE, YRSIZE,
        fg, CU(o)->cu_BgPen, ICU(o)->underlineStyle);
    ConsoleNGColorUnderline(o, GfxBase, rp, flags, x, y, count * XRSIZE, YRSIZE,
        ICU(o)->underlineStyle, ICU(o)->underlineColor);
}

/* VT/xterm autowrap is delayed: writing the rightmost cell leaves the
   cursor there and marks wrap pending.  Only the next printable character
   moves to column zero on the next line. */
static void stdcon_prepare_print(Object *o)
{
    if ((ICU(o)->conFlags & CF_WRAP_PENDING) && CHECK_MODE(o, PMB_AWM))
    {
        ICU(o)->conFlags &= ~CF_WRAP_PENDING;
        XCP = XCCP = CHAR_XMIN(o);
        Console_Down(o, 1);
    }
}

static void stdcon_advance_print(Object *o, ULONG cells, BOOL more)
{
    while (cells)
    {
        ULONG remaining = CHAR_XMAX(o) + 1 - XCCP;
        ULONG step = MIN(cells, remaining);

        if (step < remaining)
        {
            XCP = XCCP += step;
            cells = 0;
        }
        else
        {
            XCP = XCCP = CHAR_XMAX(o);
            cells -= step;
            ICU(o)->conFlags |= CF_WRAP_PENDING;

            /* With wrapping disabled, further text replaces the last cell.
             * Keep the delayed-wrap state for a later mode enable. */
            if (!CHECK_MODE(o, PMB_AWM))
                return;
            if (cells || more)
                stdcon_prepare_print(o);
        }
    }
}

/* Call with the cursor erased; preserve all native drawing state. */
static void stdcon_width_cut(struct stdcondata *data, Object *o, LONG x, LONG y)
{
    UBYTE *row=con_width_row(&data->widths,y);
    LONG left=con_width_cut(row,data->widths.columns,x);
    if(left<0)return;
    struct Library *GfxBase=data->scd_GfxBase;
    struct RastPort *rp=RASTPORT(o);
    UBYTE fg=rp->FgPen,bg=rp->BgPen,mode=rp->DrawMode;
    SetDrMd(rp,JAM1); SetAPen(rp,(UBYTE)CU(o)->cu_BgPen);
    RectFill(rp,GFX_X(o,left),GFX_Y(o,y),GFX_X(o,left+2)-1,GFX_Y(o,y+1)-1);
    SetAPen(rp,fg); SetBPen(rp,bg); SetDrMd(rp,mode);
}
static void stdcon_width_edit(struct stdcondata *data, Object *o, LONG x, LONG y, ULONG n, int op)
{
    UBYTE *row=con_width_row(&data->widths,y);
    ULONG width=data->widths.columns;
    if(!row || x<0 || (ULONG)x>=width || !n)return;
    if(n>width-x)n=width-x;
    stdcon_width_cut(data,o,x,y);
    stdcon_width_cut(data,o,op==1 ? width-n : x+n,y);
    con_width_edit(row,width,x,n,op);
}
static void stdcon_width_print(struct stdcondata *data, Object *o, BOOL plain,
    ULONG count, BOOL wide, BOOL insert)
{
    if(!plain)return;
    if(!con_width_resize(&data->widths,CHAR_XMAX(o)+1,CHAR_YMAX(o)+1,
        XRSIZE,YRSIZE,wide))return;
    stdcon_width_edit(data,o,XCP,YCP,count,insert?1:0);
    UBYTE *row=con_width_row(&data->widths,YCP);
    if(wide && row && XCP>=0 && (ULONG)XCP+1<data->widths.columns) {
        row[XCP]=1;row[XCP+1]=2;
    }
}

/* JAM2 text replaces every pixel in the first character cell, including a
 * rendered full-cell cursor.  Update cursor nesting without first drawing
 * the redundant COMPLEMENT erase. */
static void stdcon_overwritecursor(struct stdcondata *data)
{
    data->rendercursorcount--;
    data->cursorpainted = FALSE; /* opaque text replaces the old cursor */
}

/* The caller has erased the cursor. Retained units already shifted their
 * cells and repaint the row; only plain units shift pixels here. */
static VOID stdcon_insert_print(Object *o, struct RastPort *rp,
    struct Library *GfxBase, ULONG count, BOOL insert)
{
    LONG available = CHAR_XMAX(o) + 1 - XCP;
    UBYTE oldpen = rp->FgPen;
    if (!insert || !count || available <= 0 || count >= (ULONG)available) return;
    SetAPen(rp, CU(o)->cu_BgPen);
    ScrollRaster(rp, -count * XRSIZE, 0, GFX_X(o, XCP), GFX_Y(o, YCP),
        GFX_XMAX(o), GFX_Y(o, YCP + 1) - 1);
    SetAPen(rp, oldpen);
}

/*********  xterm dynamic colours (OSC 4/10/11/104/110/111)  **********/

static LONG stdcon_hexval(const UBYTE *p, WORD n)
{
    LONG v = 0;
    WORD i;

    for (i = 0; i < n; i++)
    {
        UBYTE c = p[i];

        v <<= 4;
        if (c >= '0' && c <= '9')
            v |= c - '0';
        else if (c >= 'a' && c <= 'f')
            v |= c - 'a' + 10;
        else if (c >= 'A' && c <= 'F')
            v |= c - 'A' + 10;
        else
            return -1;
    }
    return v;
}

/* 1-4 hex digits scaled to 8 bits, as XParseColor does */
static LONG stdcon_hexcomp(const UBYTE *p, WORD n)
{
    LONG v;

    if (n < 1 || n > 4 || (v = stdcon_hexval(p, n)) < 0)
        return -1;
    return v * 255 / ((1L << (4 * n)) - 1);
}

/* "#rgb", "#rrggbb", "#rrrgggbbb", "#rrrrggggbbbb" or "rgb:r/g/b" with 1-4
   digits per component; returns a CONPEN_RGB key, 0 if not a colour */
static ULONG stdcon_parsecolor(const UBYTE *spec, WORD len)
{
    LONG c[3];
    WORD i;

    if (len > 1 && spec[0] == '#' && (len - 1) % 3 == 0 && len <= 13)
    {
        WORD n = (len - 1) / 3;

        /* Legacy # notation places supplied digits in the most significant
         * bits, unlike rgb: notation which scales the component range. */
        for (i = 0; i < 3; i++)
        {
            LONG value = stdcon_hexval(spec + 1 + i * n, n);
            if (value < 0) return 0;
            c[i] = n == 1 ? value << 4 : value >> (4 * (n - 2));
        }
        return CONPEN_RGB(c[0], c[1], c[2]);
    }
    if (len > 4 && !strncmp((const char *) spec, "rgb:", 4))
    {
        const UBYTE *p = spec + 4, *end = spec + len;

        for (i = 0; i < 3; i++)
        {
            const UBYTE *q = p;

            while (q < end && *q != '/')
                q++;
            if ((c[i] = stdcon_hexcomp(p, q - p)) < 0)
                return 0;
            if (i < 2)
            {
                if (q == end) return 0;
                p = q + 1;
            }
            else if (q != end) return 0;
        }
        return CONPEN_RGB(c[0], c[1], c[2]);
    }
    return 0;
}

static ULONG stdcon_penrgb(struct stdcondata *data, UBYTE pen)
{
    struct Library *GfxBase = data->scd_GfxBase;
    ULONG rgb[3];

    GetRGB32(data->cm, pen, 1, rgb);
    return CONPEN_RGB(rgb[0] >> 24, rgb[1] >> 24, rgb[2] >> 24);
}

/* No global dark/light setting exists here: classify the configured console
 * background by integer luma. Keep this separate from application OSC colors.
 * Protocol: contour-terminal.org/vt-extensions/color-palette-update-notifications/
 */
static VOID stdcon_schemereply(Class *cl, Object *o)
{
    struct stdcondata *data = INST_DATA(cl, o);
    ULONG rgb = data->schemeRGB;
    if (!rgb && data->cm)
        rgb = stdcon_penrgb(data, (UBYTE)*data->penmap[CONUNIT_PEN_DEFBG]);
    ULONG luma = 299 * ((rgb >> 16) & 255) +
                 587 * ((rgb >> 8) & 255) + 114 * (rgb & 255);
    const char *reply = luma < 128000 ? "\x1b[?997;1n" : "\x1b[?997;2n";
    con_inject((struct ConsoleBase *)cl->cl_UserData, CU(o),
        (const UBYTE *)reply, strlen(reply));
}

/* Replace an override pen; rgb 0 releases it (back to the default). */
static VOID stdcon_setslot(struct stdcondata *data, ULONG *rgbslot,
    LONG *penslot, ULONG rgb)
{
    struct Library *GfxBase = data->scd_GfxBase;

    LONG replacement = -1, previous;

    if (*rgbslot == rgb)
        return;
    /* Keep the old reference until the replacement is available. Failure
       must preserve the working colour, and a same-pen match must retain
       one reference after the old one is released. */
    if (rgb)
    {
        replacement = stdcon_bestpen(data, (rgb >> 16) & 0xFF,
            (rgb >> 8) & 0xFF, rgb & 0xFF);
        if (replacement == -1)
            return;
    }
    previous = *penslot;
    *penslot = replacement;
    *rgbslot = rgb;
    if (previous != -1)
        ReleasePen(data->cm, previous);
}

static VOID stdcon_colorreply(Class *cl, Object *o, ULONG code, LONG index,
    ULONG rgb, BOOL st)
{
    UBYTE reply[64];
    UBYTE r = (rgb >> 16) & 0xFF, g = (rgb >> 8) & 0xFF, b = rgb & 0xFF;

    if (index >= 0)
        NewRawDoFmt("\x1b]%lu;%ld;rgb:%02lx%02lx/%02lx%02lx/%02lx%02lx%s",
            RAWFMTFUNC_STRING, reply, code, index, (ULONG) r, (ULONG) r,
            (ULONG) g, (ULONG) g, (ULONG) b, (ULONG) b,
            st ? "\x1b\\" : "\x07");
    else
        NewRawDoFmt("\x1b]%lu;rgb:%02lx%02lx/%02lx%02lx/%02lx%02lx%s",
            RAWFMTFUNC_STRING, reply, code, (ULONG) r, (ULONG) r,
            (ULONG) g, (ULONG) g, (ULONG) b, (ULONG) b,
            st ? "\x1b\\" : "\x07");
    con_inject((struct ConsoleBase *)cl->cl_UserData, CU(o), reply, -1);
}

/* Current colour of palette entry n, for an OSC 4 query */
static ULONG stdcon_slotrgb(struct stdcondata *data, UWORD n)
{
    UBYTE r, g, b;

    if (n < 256 && data->ansirgb[n])
        return data->ansirgb[n];
    if (n < 8)
        return stdcon_penrgb(data, (UBYTE) *(data->penmap[n]));
    xterm_to_rgb(n, &r, &g, &b);
    return CONPEN_RGB(r, g, b);
}

/* Pens printed with the old default colours follow the new ones, as in
   xterm, where cells refer to "default" rather than to a colour. */
static VOID stdcon_followdefault(Object *o, UBYTE oldfg, UBYTE oldbg,
    UBYTE newfg, UBYTE newbg)
{
    Console_GetColorPen(o,ICU(o)->fgColor,&CU(o)->cu_FgPen);
    Console_GetColorPen(o,ICU(o)->bgColor,&CU(o)->cu_BgPen);
}

/* Palette indices are complete decimal tokens in 0..255. Check before
 * multiplying, so arbitrarily long application output cannot wrap into an
 * array index. Empty, signed and suffixed tokens are invalid. */
static BOOL stdcon_colorindex(const UBYTE *p, const UBYTE *end, ULONG *index)
{
    ULONG value = 0;
    if (p == end) return FALSE;
    while (p < end)
    {
        if (*p < '0' || *p > '9') return FALSE;
        ULONG digit = *p++ - '0';
        if (value > (255 - digit) / 10) return FALSE;
        value = value * 10 + digit;
    }
    *index = value;
    return TRUE;
}

/* Notify retained rendering only for an actual extended palette change.
 * Allocation failure and queries leave the revision untouched. */
static VOID stdcon_setindexed(Object *o, struct stdcondata *data, ULONG n, ULONG rgb)
{
    ULONG previous = data->ansirgb[n];
    stdcon_setslot(data, &data->ansirgb[n], &data->ansipen[n], rgb);
    if (n >= 16 && previous != data->ansirgb[n])
        ICU(o)->extendedPaletteRevision++;
}

static VOID stdcon_cursorcolor(Object *o, struct stdcondata *data, ULONG rgb)
{
    if (rgb == data->cursorRGB) return;
    Console_UnRenderCursor(o);
    data->cursorRGB = rgb;
    Console_RenderCursor(o);
}

static VOID stdcon_setcolor(Class *cl, Object *o, IPTR *params)
{
    struct stdcondata *data = INST_DATA(cl, o);
    ULONG code = params[0];
    const UBYTE *p = (const UBYTE *) params[1];
    BOOL st = params[2];
    UBYTE oldfg, oldbg, newfg, newbg;

    if (!data->cm)
        return;

    Console_GetColorPen(o, CONUNIT_PEN_DEFFG, &oldfg);
    Console_GetColorPen(o, CONUNIT_PEN_DEFBG, &oldbg);

    if (code == 10 || code == 11 || code == 12)
    {
        /* OSC 10;spec;spec... sets fg, bg, cursor (xterm walks up the list) */
        WORD slot = code - 10;

        while (*p && slot < 3)
        {
            const UBYTE *q = p;

            while (*q && *q != ';')
                q++;
            if (q - p == 1 && *p == '?')
                stdcon_colorreply(cl, o, 10 + slot, -1,
                    slot == 2 ? (data->cursorRGB ? data->cursorRGB :
                        data->defrgb[0] ? data->defrgb[0] : stdcon_penrgb(data, oldfg)) :
                    data->defrgb[slot] ? data->defrgb[slot] :
                    stdcon_penrgb(data, slot ? oldbg : oldfg), st);
            else
            {
                ULONG rgb = stdcon_parsecolor(p, q - p);

                if (rgb)
                {
                    if (slot == 2) stdcon_cursorcolor(o, data, rgb);
                    else stdcon_setslot(data, &data->defrgb[slot],
                        &data->defpen[slot], rgb);
                }
            }
            slot++;
            p = *q ? q + 1 : q;
        }
    }
    else if (code == 112)
        stdcon_cursorcolor(o, data, data->resetCursorRGB);
    else if (code == 110 || code == 111)
    {
        WORD slot = code - 110;

        stdcon_setslot(data, &data->defrgb[slot], &data->defpen[slot], 0);
    }
    else if (code == 4)
    {
        /* OSC 4;n;spec[;n;spec...] */
        while (*p)
        {
            const UBYTE *q = p, *r;
            ULONG n;
            while (*q && *q != ';') q++;
            if (!*q) break;
            BOOL valid = stdcon_colorindex(p, q, &n);
            r = ++q;
            while (*r && *r != ';')
                r++;
            if (!valid)
            {
                p = *r ? r + 1 : r;
                continue;
            }
            if (r - q == 1 && *q == '?')
                stdcon_colorreply(cl, o, 4, n, stdcon_slotrgb(data, n), st);
            else
            {
                ULONG rgb = stdcon_parsecolor(q, r - q);

                if (rgb)
                    stdcon_setindexed(o, data, n, rgb);
            }
            p = *r ? r + 1 : r;
        }
    }
    else if (code == 104)
    {
        /* OSC 104 resets the whole palette, OSC 104;n;n... single entries */
        if (!*p)
        {
            WORD n;

            for (n = 0; n < 256; n++)
                stdcon_setindexed(o, data, n, 0);
        }
        while (*p)
        {
            ULONG n;
            const UBYTE *q = p;

            while (*q && *q != ';') q++;
            if (stdcon_colorindex(p, q, &n))
                stdcon_setindexed(o, data, n, 0);
            p = *q ? q + 1 : q;
        }
    }

    Console_GetColorPen(o, CONUNIT_PEN_DEFFG, &newfg);
    Console_GetColorPen(o, CONUNIT_PEN_DEFBG, &newbg);
    stdcon_followdefault(o, oldfg, oldbg, newfg, newbg);
}

/* Reserve every replacement before publishing any color. Failure leaves the
 * complete palette and its pen references unchanged. */
static BOOL stdcon_colorprefs(Class *cl,Object *o,struct ConsolengColorPrefs *p,BOOL set)
{
    struct stdcondata *data=INST_DATA(cl,o);
    struct Library *GfxBase=data->scd_GfxBase;
    LONG pens[CONSOLENG_COLOR_COUNT];
    ULONG i;
    if (!data->cm) return FALSE;
    if (!set) {
        *p=(struct ConsolengColorPrefs){sizeof(*p),1,0,{0}};
        for(i=0;i<CONSOLENG_COLOR_COUNT;i++) {
            ULONG rgb=i<2?data->defrgb[i]:data->ansirgb[i-2];
            if(rgb) p->cp_Mask|=1UL<<i;
            else if(i<2) rgb=stdcon_penrgb(data,(UBYTE)*data->penmap[i?CONUNIT_PEN_DEFBG:CONUNIT_PEN_DEFFG]);
            else rgb=stdcon_slotrgb(data,i-2);
            p->cp_RGB[i]=rgb&0xffffffUL;
        }
        return TRUE;
    }
    for(i=0;i<CONSOLENG_COLOR_COUNT;i++) pens[i]=-1;
    for(i=0;i<CONSOLENG_COLOR_COUNT;i++) if(p->cp_Mask&(1UL<<i)) {
        ULONG rgb=p->cp_RGB[i];
        pens[i]=stdcon_bestpen(data,(rgb>>16)&255,(rgb>>8)&255,rgb&255);
        if(pens[i]<0) {
            for(ULONG j=0;j<i;j++) if(pens[j]>=0) ReleasePen(data->cm,pens[j]);
            return FALSE;
        }
    }
    BOOL changed = FALSE;
    UBYTE oldfg,oldbg,newfg,newbg;
    Console_GetColorPen(o,CONUNIT_PEN_DEFFG,&oldfg);
    Console_GetColorPen(o,CONUNIT_PEN_DEFBG,&oldbg);
    for(i=0;i<CONSOLENG_COLOR_COUNT;i++) {
        LONG *pen=i<2?&data->defpen[i]:&data->ansipen[i-2];
        ULONG *rgb=i<2?&data->defrgb[i]:&data->ansirgb[i-2];
        LONG previous=*pen;
        ULONG replacement = pens[i]<0 ? 0 : 0x01000000UL|p->cp_RGB[i];
        if (*rgb != replacement) changed = TRUE;
        *pen=pens[i];*rgb=pens[i]<0?0:0x01000000UL|p->cp_RGB[i];
        if(previous>=0) ReleasePen(data->cm,previous);
    }
    Console_GetColorPen(o,CONUNIT_PEN_DEFFG,&newfg);
    Console_GetColorPen(o,CONUNIT_PEN_DEFBG,&newbg);
    stdcon_followdefault(o,oldfg,oldbg,newfg,newbg);
    data->schemeRGB = data->defrgb[1];
    if (changed && (ICU(o)->conFlags & CF_COLOR_SCHEME_NOTIFY))
        stdcon_schemereply(cl, o);
    return TRUE;
}

/* DECSCUSR pairs share a shape: odd blinks, even stays steady. Default0
 * currently denotes the existing steady block and normalizes to1/2. */
static UBYTE stdcon_blink_style(UBYTE style, BOOL blink)
{
    if (!style) style = 2;
    return ((style - 1) / 2) * 2 + (blink ? 1 : 2);
}

/*********  StdCon::DoCommand()  ****************************/

/* Clamp before narrowing to the native WORD coordinates. Application
 * parameters are unsigned decimal values and may exceed SHRT_MAX. */
static WORD stdcon_absolute_coordinate(IPTR parameter, WORD minimum, WORD maximum)
{
    IPTR position = parameter > 0 ? parameter - 1 : 0;
    if (position < minimum) position = minimum;
    if (position > maximum) position = maximum;
    return (WORD)position;
}

static VOID stdcon_absolute_position(Object *o, BYTE command, const IPTR *params)
{
    if (command != C_CURSOR_ROW)
        XCCP = XCP = stdcon_absolute_coordinate(
            params[command == C_CURSOR_POS ? 1 : 0], CHAR_XMIN(o), CHAR_XMAX(o));
    if (command != C_CURSOR_COLUMN)
    {
        WORD top = (ICU(o)->conFlags & CF_ORIGIN_MODE) ? CON_SCROLL_TOP(o) : CHAR_YMIN(o);
        WORD bottom = (ICU(o)->conFlags & CF_ORIGIN_MODE) ? CON_SCROLL_BOTTOM(o) : CHAR_YMAX(o);
        YCCP = YCP = top + stdcon_absolute_coordinate(params[0], 0, bottom - top);
    }
    ICU(o)->conFlags &= ~CF_WRAP_PENDING;
}

/* Cursor-motion CSI commands clamp; only output/index operations scroll. */
/* Import explicit legacy-table edits, and mirror the first MAXTABS-1 stops.
 * All scans are bounded even if an application omits the sentinel. */
static void stdcon_tab_import(struct stdcondata *data, const UWORD *table)
{
    int i;
    if (!memcmp(table, data->tabMirror, sizeof(data->tabMirror))) return;
    memset(data->tabBits, 0, sizeof(data->tabBits));
    for (i = 0; i < MAXTABS && table[i] != (UWORD)-1; i++)
        if (table[i] < STDCON_TAB_COLUMNS)
            data->tabBits[table[i] >> 3] |= 1U << (table[i] & 7);
    memcpy(data->tabMirror, table, sizeof(data->tabMirror));
}

static void stdcon_tab_export(struct stdcondata *data, UWORD *table)
{
    int x, n = 0;
    for (x = 0; x < STDCON_TAB_COLUMNS && n < MAXTABS - 1; x++)
        if (data->tabBits[x >> 3] & (1U << (x & 7))) table[n++] = x;
    while (n < MAXTABS) table[n++] = (UWORD)-1;
    memcpy(data->tabMirror, table, sizeof(data->tabMirror));
}

static void stdcon_tab_command(struct stdcondata *data, Object *o,
    BYTE command, const IPTR *params)
{
    int x = XCP, direction = command == C_CURSOR_BACKTAB ? -1 : 1;
    IPTR count;
    stdcon_tab_import(data, CU(o)->cu_TabStops);
    if (command == C_H_TAB_SET || command == C_CURSOR_TAB_CTRL || command == C_CLEAR_TABS)
    {
        IPTR action = command == C_H_TAB_SET ? 0 : params[0];
        if (command == C_CLEAR_TABS) action = action == 0 ? 2 : action == 3 ? 5 : -1;
        if (action == 0) data->tabBits[x >> 3] |= 1U << (x & 7);
        else if (action == 2) data->tabBits[x >> 3] &= ~(1U << (x & 7));
        else if (action == 5) memset(data->tabBits, 0, sizeof(data->tabBits));
        else return;
        stdcon_tab_export(data, CU(o)->cu_TabStops);
        return;
    }
    count = command == C_HTAB ? 1 : params[0];
    if (count <= 0) count = 1;
    /* Each iteration advances a column; cost is bounded by screen width,
       not by an untrusted CSI repeat count. Tab motion never scrolls. */
    while (count > 0 && (direction > 0 ? x < CHAR_XMAX(o) : x > CHAR_XMIN(o)))
    {
        x += direction;
        if (data->tabBits[x >> 3] & (1U << (x & 7))) count--;
    }
    XCCP = XCP = x;
    ICU(o)->conFlags &= ~CF_WRAP_PENDING;
}

static WORD stdcon_relative_coordinate(WORD position, WORD minimum, WORD maximum,
    IPTR count, BOOL forward)
{
    LONG available = forward ? maximum - position : position - minimum;
    if (count <= 0) count = 1;
    if (count > available) count = available;
    return forward ? position + count : position - count;
}

static VOID stdcon_relative_position(Object *o, BYTE command, const IPTR *params)
{
    IPTR count = (command == C_BACKSPACE || command == C_CARRIAGE_RETURN) ? 1 : params[0];
    switch (command)
    {
    case C_BACKSPACE: case C_CURSOR_BACKWARD:
        XCCP = XCP = stdcon_relative_coordinate(XCP, CHAR_XMIN(o), CHAR_XMAX(o), count, FALSE); break;
    case C_CURSOR_FORWARD:
        XCCP = XCP = stdcon_relative_coordinate(XCP, CHAR_XMIN(o), CHAR_XMAX(o), count, TRUE); break;
    case C_CURSOR_PREV_LINE:
        XCCP = XCP = CHAR_XMIN(o);
        /* fall through */
    case C_CURSOR_UP:
        YCCP = YCP = stdcon_relative_coordinate(YCP, YCP < CON_SCROLL_TOP(o) ? CHAR_YMIN(o) : CON_SCROLL_TOP(o), CHAR_YMAX(o), count, FALSE); break;
    case C_CURSOR_NEXT_LINE:
        XCCP = XCP = CHAR_XMIN(o);
        /* fall through */
    case C_CURSOR_DOWN:
        YCCP = YCP = stdcon_relative_coordinate(YCP, CHAR_YMIN(o), YCP > CON_SCROLL_BOTTOM(o) ? CHAR_YMAX(o) : CON_SCROLL_BOTTOM(o), count, TRUE); break;
    case C_CARRIAGE_RETURN:
        XCCP = XCP = CHAR_XMIN(o); break;
    }
    ICU(o)->conFlags &= ~CF_WRAP_PENDING;
}

/* Saved cursor and logical rendition belong to the unit, including hidden tabs. */
static void stdcon_saved_cursor(Object *o, BYTE command)
{
    switch (command)
    {
    case C_SAVE_CURSOR:
        ICU(o)->savedCharsets = ICU(o)->charsets;
        ICU(o)->savedFgColor = ICU(o)->fgColor;
        ICU(o)->savedBgColor = ICU(o)->bgColor;
        ICU(o)->savedX = XCP;
        ICU(o)->savedY = YCP;
        ICU(o)->savedFgPen = CU(o)->cu_FgPen;
        ICU(o)->savedBgPen = CU(o)->cu_BgPen;
        ICU(o)->savedTxFlags = CU(o)->cu_TxFlags;
        ICU(o)->savedUnderlineStyle = ICU(o)->underlineStyle;
            ICU(o)->savedUnderlineColor = ICU(o)->underlineColor;
        ICU(o)->savedOrigin = !!(ICU(o)->conFlags & CF_ORIGIN_MODE);
        ICU(o)->savedCursor = TRUE;
        ICU(o)->savedWrapPending = !!(ICU(o)->conFlags & CF_WRAP_PENDING);
        break;

    case C_RESTORE_CURSOR:
        ICU(o)->charsets = ICU(o)->savedCursor ? ICU(o)->savedCharsets : 0;
        ICU(o)->conFlags &= ~(CF_WRAP_PENDING | CF_ORIGIN_MODE);
        if (ICU(o)->savedCursor && ICU(o)->savedOrigin)
            ICU(o)->conFlags |= CF_ORIGIN_MODE;
        {
            WORD top = (ICU(o)->conFlags & CF_ORIGIN_MODE) ? CON_SCROLL_TOP(o) : CHAR_YMIN(o);
            WORD bottom = (ICU(o)->conFlags & CF_ORIGIN_MODE) ? CON_SCROLL_BOTTOM(o) : CHAR_YMAX(o);
            /* Nothing saved: xterm restores the home position. */
            WORD x = ICU(o)->savedCursor ? ICU(o)->savedX : CHAR_XMIN(o);
            WORD y = ICU(o)->savedCursor ? ICU(o)->savedY : CHAR_YMIN(o);

            if (x < CHAR_XMIN(o))
                x = CHAR_XMIN(o);
            else if (x > CHAR_XMAX(o))
                x = CHAR_XMAX(o);
            if (y < top) y = top;
            else if (y > bottom) y = bottom;

            if (!CON_IS_OBSCURED(o)) Console_UnRenderCursor(o);
            XCCP = XCP = x;
            YCCP = YCP = y;
            if (ICU(o)->savedCursor)
            {
                if (ICU(o)->savedWrapPending && x == CHAR_XMAX(o) &&
                    x == ICU(o)->savedX && y == ICU(o)->savedY)
                    ICU(o)->conFlags |= CF_WRAP_PENDING;
                ICU(o)->fgColor = ICU(o)->savedFgColor;
                ICU(o)->bgColor = ICU(o)->savedBgColor;
                Console_GetColorPen(o,ICU(o)->savedFgColor,&CU(o)->cu_FgPen);
                Console_GetColorPen(o,ICU(o)->savedBgColor,&CU(o)->cu_BgPen);
                CU(o)->cu_TxFlags = ICU(o)->savedTxFlags;
                ICU(o)->underlineStyle = ICU(o)->savedUnderlineStyle;
                ICU(o)->underlineColor = ICU(o)->savedUnderlineColor;
            }
            if (!CON_IS_OBSCURED(o)) Console_RenderCursor(o);
            break;
        }

    }
}

/* DECSTBM ignores invalid ranges without changing margins or cursor. */
static BOOL stdcon_scroll_region(Object *o, const IPTR *params)
{
    IPTR top = params[0] > 0 ? params[0] : 1;
    IPTR bottom = params[1] > 0 ? params[1] : CHAR_YMAX(o) + 1;
    if (top >= bottom || bottom > CHAR_YMAX(o) + 1) return FALSE;
    ICU(o)->scrollTop = top - 1;
    ICU(o)->scrollBottom = bottom;
    XCCP = XCP = CHAR_XMIN(o);
    YCCP = YCP = (ICU(o)->conFlags & CF_ORIGIN_MODE) ? CON_SCROLL_TOP(o) : CHAR_YMIN(o);
    ICU(o)->conFlags &= ~CF_WRAP_PENDING;
    return TRUE;
}

static VOID stdcon_origin_mode(Object *o, BOOL enabled)
{
    if (enabled) ICU(o)->conFlags |= CF_ORIGIN_MODE;
    else ICU(o)->conFlags &= ~CF_ORIGIN_MODE;
    ICU(o)->conFlags &= ~CF_WRAP_PENDING;
    XCCP = XCP = CHAR_XMIN(o);
    YCCP = YCP = enabled ? CON_SCROLL_TOP(o) : CHAR_YMIN(o);
}

/* Alignment is a screen edit, not character input: no cursor advance, wrap,
 * rendition, charset or REP history changes. */
static VOID stdcon_alignment(struct stdcondata *data, Object *o)
{
    con_width_clear(&data->widths);
    struct RastPort *rp = RASTPORT(o);
    struct Library *GfxBase = data->scd_GfxBase;
    UBYTE fg = rp->FgPen, bg = rp->BgPen, mode = rp->DrawMode;
    UBYTE style = rp->AlgoStyle, plainfg, plainbg;
    WORD cx = rp->cp_x, cy = rp->cp_y;
    UBYTE text[64];
    memset(text, 'E', sizeof(text));
    Console_GetColorPen(o, CONUNIT_PEN_DEFFG, &plainfg);
    Console_GetColorPen(o, CONUNIT_PEN_DEFBG, &plainbg);
    SetABPenDrMd(rp, plainfg, plainbg, JAM2);
    SetSoftStyle(rp, 0, CON_TXTFLAGS_MASK);
    for (LONG y = 0; y <= CHAR_YMAX(o); y++)
        for (LONG x = 0; x <= CHAR_XMAX(o); )
        {
            LONG count = MIN(64, CHAR_XMAX(o) + 1 - x);
            Move(rp, GFX_X(o, x), GFX_Y(o, y) + rp->Font->tf_Baseline);
            stdcon_text(data, rp, text, count);
            x += count;
        }
    SetABPenDrMd(rp, fg, bg, mode);
    SetSoftStyle(rp, style, CON_TXTFLAGS_MASK);
    Move(rp, cx, cy);
}

static VOID stdcon_reset_clear(struct stdcondata *data, Object *o)
{
    con_width_clear(&data->widths);
    struct RastPort *rp = RASTPORT(o);
    struct Library *GfxBase = data->scd_GfxBase;
    UBYTE fg = rp->FgPen, bg = rp->BgPen, mode = rp->DrawMode;
    UBYTE resetpen;
    Console_GetColorPen(o, CONUNIT_PEN_DEFBG, &resetpen);
    SetDrMd(rp, JAM1);
    SetAPen(rp, resetpen);
    SetBPen(rp, resetpen);
    RectFill(rp, GFX_XMIN(o), GFX_YMIN(o), GFX_XMAX(o), GFX_YMAX(o));
    SetAPen(rp, fg); SetBPen(rp, bg); SetDrMd(rp, mode);
}

/* RIS resets application state, never native lifecycle/hidden-tab flags or
 * the user's font, palette, emoji policy and history budget. */
static VOID stdcon_reset_state(struct stdcondata *data, Object *o)
{
    ICU(o)->conFlags &= ~(CF_BRACKETED_PASTE | CF_FOCUS_REPORTING |
        CF_COLOR_SCHEME_NOTIFY | CF_WRAP_PENDING | CF_APPLICATION_CURSOR |
        CF_APPLICATION_KEYPAD | CF_INSERT_MODE | CF_ORIGIN_MODE | CF_ALTERNATE_SCROLL);
    ICU(o)->charsets = ICU(o)->savedCharsets = ICU(o)->activeCharset = 0;
    con_keyboard_dispose(CU(o));
    ICU(o)->keyboardAlternate = FALSE;
    ICU(o)->mouseMode = 0;
    ICU(o)->mouseReportedButtons = 0;
    ICU(o)->mouseSGR = ICU(o)->mouseUTF8 = ICU(o)->mouseLastValid = FALSE;
    ICU(o)->scrollTop = ICU(o)->scrollBottom = 0;
    ICU(o)->savedCursor = ICU(o)->savedOrigin = ICU(o)->savedWrapPending = FALSE;
    SET_MODE(o, PMB_AWM);
    SET_MODE(o, PMB_ASM);
    CLEAR_MODE(o, M_LNM);
    CU(o)->cu_TxFlags = 0;
    ICU(o)->underlineStyle = ICU(o)->savedUnderlineStyle = 0;
    ICU(o)->underlineColor = ICU(o)->savedUnderlineColor = 0;
    Console_GetColorPen(o, CONUNIT_PEN_DEFFG, &CU(o)->cu_FgPen);
    Console_GetColorPen(o, CONUNIT_PEN_DEFBG, &CU(o)->cu_BgPen);
    XCCP = XCP = CHAR_XMIN(o);
    YCCP = YCP = CHAR_YMIN(o);
    memset(data->tabBits, 1, sizeof(data->tabBits));
    stdcon_tab_export(data, CU(o)->cu_TabStops);
    data->cursorstyle = data->resetCursorStyle;
    data->cursorTimerReady = FALSE;
    if (!data->cursorvisible)
    {
        data->cursorvisible = TRUE;
        data->rendercursorcount--;
    }
}

static VOID stdcon_docommand(Class *cl, Object *o,
    struct P_Console_DoCommand *msg)
{
    BOOL insert = (ICU(o)->conFlags & CF_INSERT_MODE) &&
        !(msg->NumParams >= 3 && msg->Params[2]);
    struct Window *w = CU(o)->cu_Window;
    struct RastPort *rp = w->RPort;
    IPTR *params = msg->Params;
    struct stdcondata *data = INST_DATA(cl, o);
    struct Library *GfxBase = data->scd_GfxBase;

    if (msg->Command != C_ASCII && msg->Command != C_ASCII_STRING)
        data->styleLength = 0;

    EnterFunc(bug("StdCon::DoCommand(o=%p, cmd=%d, params=%p)\n",
            o, msg->Command, params));

    /* BEL must not call Intuition while output holds console/layer locks.
     * A signal also coalesces bursts and retains no unit/window pointer. */
    if (msg->Command == C_BELL)
    {
        if (!ICU(o)->bellDisabled && ConsoleDevice->consoleTask)
            Signal(ConsoleDevice->consoleTask, SIGBREAKF_CTRL_E);
        return;
    }

    if (msg->Command == C_SCREEN_ALIGNMENT)
    {
        data->resetClearPending = FALSE;
        data->alignmentPending = !!CON_IS_OBSCURED(o);
        if (!data->alignmentPending)
        {
            Console_UnRenderCursor(o);
            stdcon_alignment(data, o);
            Console_RenderCursor(o);
        }
        return;
    }

    if (msg->Command == C_TERMINAL_RESET)
    {
        Console_UnRenderCursor(o);
        stdcon_reset_state(data, o);
        data->alignmentPending = FALSE;
        data->resetClearPending = !!CON_IS_OBSCURED(o);
        if (!data->resetClearPending) stdcon_reset_clear(data, o);
        Console_DoCommand(o, C_CLEAR_TITLE_STACK, 0, NULL);
        /* A fresh terminal has no application-supplied title. */
        IPTR title = (IPTR)"";
        Console_DoCommand(o, C_SET_TITLE, 1, &title);
        Console_RenderCursor(o);
        return;
    }

    if (msg->Command == C_SET_ORIGIN_MODE)
    {
        if (!CON_IS_OBSCURED(o)) Console_UnRenderCursor(o);
        stdcon_origin_mode(o, params[0] != 0);
        if (!CON_IS_OBSCURED(o)) Console_RenderCursor(o);
        return;
    }

    if (msg->Command == C_SET_SCROLL_REGION)
    {
        if (!CON_IS_OBSCURED(o)) Console_UnRenderCursor(o);
        stdcon_scroll_region(o, params);
        if (!CON_IS_OBSCURED(o)) Console_RenderCursor(o);
        return;
    }

    /* Position is application state even when a tab is hidden. Do not
     * paint or disturb the foreground tab's cursor in that case. */
    if (msg->Command == C_CURSOR_POS || msg->Command == C_CURSOR_COLUMN ||
        msg->Command == C_CURSOR_ROW)
    {
        if (!CON_IS_OBSCURED(o)) Console_UnRenderCursor(o);
        stdcon_absolute_position(o, msg->Command, params);
        if (!CON_IS_OBSCURED(o)) Console_RenderCursor(o);
        return;
    }

    if ((msg->Command >= C_CURSOR_UP && msg->Command <= C_CURSOR_PREV_LINE) ||
        msg->Command == C_BACKSPACE || msg->Command == C_CARRIAGE_RETURN)
    {
        if (!CON_IS_OBSCURED(o)) Console_UnRenderCursor(o);
        stdcon_relative_position(o, msg->Command, params);
        if (!CON_IS_OBSCURED(o)) Console_RenderCursor(o);
        return;
    }

    if (msg->Command == C_HTAB || msg->Command == C_CURSOR_HTAB ||
        msg->Command == C_CURSOR_BACKTAB || msg->Command == C_H_TAB_SET ||
        msg->Command == C_CURSOR_TAB_CTRL || msg->Command == C_CLEAR_TABS)
    {
        if (!CON_IS_OBSCURED(o)) Console_UnRenderCursor(o);
        stdcon_tab_command(data, o, msg->Command, params);
        if (!CON_IS_OBSCURED(o)) Console_RenderCursor(o);
        return;
    }

    if (msg->Command == C_SAVE_CURSOR || msg->Command == C_RESTORE_CURSOR)
    {
        stdcon_saved_cursor(o, msg->Command);
        return;
    }

    if (msg->Command == C_DEVICE_STATUS_REPORT && msg->NumParams >= 2 &&
        params[0] == 996)
    {
        stdcon_schemereply(cl, o);
        return;
    }

    /* Reports are unit state, not drawing: hidden tabs must still answer. */
    if (msg->Command == C_DEVICE_STATUS_REPORT || msg->Command == C_WINDOW_STATUS_REQUEST ||
        msg->Command == C_PRIMARY_ATTRIBUTES || msg->Command == C_SECONDARY_ATTRIBUTES ||
        msg->Command == C_REPORT_CELL_GEOMETRY || msg->Command == C_REPORT_PIXEL_GEOMETRY)
    {
        UBYTE reply[32];
        if (msg->Command == C_PRIMARY_ATTRIBUTES)
            strcpy((char *)reply, "\x1b[?6c");
        else if (msg->Command == C_SECONDARY_ATTRIBUTES)
        {
            struct Library *library = &((struct ConsoleBase *)cl->cl_UserData)->device.dd_Library;
            /* Native version.revision encoded as version * 100 + revision.
             * This is ConsoleNG's firmware identifier, not Alacritty's. */
            ULONG version = (ULONG)library->lib_Version * 100 + library->lib_Revision;
            NewRawDoFmt("\x1b[>0;%lu;1c", RAWFMTFUNC_STRING, reply, version);
        }
        else if (msg->Command == C_REPORT_CELL_GEOMETRY)
            NewRawDoFmt("\x1b[8;%d;%dt", RAWFMTFUNC_STRING, reply,
                CU(o)->cu_YMax + 1, CU(o)->cu_XMax + 1);
        else if (msg->Command == C_REPORT_PIXEL_GEOMETRY)
            NewRawDoFmt("\x1b[4;%lu;%lut", RAWFMTFUNC_STRING, reply,
                (ULONG)(CU(o)->cu_YMax + 1) * CU(o)->cu_YRSize,
                (ULONG)(CU(o)->cu_XMax + 1) * CU(o)->cu_XRSize);
        else if (msg->Command == C_WINDOW_STATUS_REQUEST)
            NewRawDoFmt("\x9b" "1;1;%d;%d r", RAWFMTFUNC_STRING, reply,
                CU(o)->cu_YMax + 1, CU(o)->cu_XMax + 1);
        else
        {
            BOOL native = msg->NumParams < 2 || params[1];
            if (msg->NumParams >= 2 && params[0] == 5)
                strcpy((char *)reply, native ? "\x9b" "0n" : "\x1b[0n");
            else
                NewRawDoFmt(native ? "\x9b" "%d;%dR" : "\x1b[%d;%dR",
                    RAWFMTFUNC_STRING, reply, CU(o)->cu_YCP + 1 -
                        (!native && (ICU(o)->conFlags & CF_ORIGIN_MODE) ? CON_SCROLL_TOP(o) : 0),
                    CU(o)->cu_XCP + 1);
        }
        con_inject((struct ConsoleBase *)cl->cl_UserData, CU(o), reply, -1);
        return;
    }

    /* Visibility belongs to the unit even when its tab is hidden. Cursor
     * methods retain nesting and suppress raster access while obscured. */
    if (msg->Command == C_CURSOR_VISIBLE || msg->Command == C_CURSOR_INVISIBLE)
    {
        BOOL visible = msg->Command == C_CURSOR_VISIBLE;
        if (visible && !data->cursorvisible)
        {
            data->cursorvisible = TRUE;
            data->rendercursorcount--;
            Console_RenderCursor(o);
        }
        else if (!visible && data->cursorvisible)
        {
            Console_UnRenderCursor(o);
            data->cursorvisible = FALSE;
            data->rendercursorcount++;
        }
        return;
    }

    if (msg->Command == C_SET_SYNC_OUTPUT)
    {
        con_sync_set(CU(o), ConsoleDevice, !!params[0]);
        return;
    }

    if (msg->Command == C_GET_MODE_STATE)
    {
        if (params[0] && (params[1] == 12 || params[1] == 25))
        {
            BOOL enabled = params[1] == 12 ? (data->cursorstyle & 1) : data->cursorvisible;
            *(ULONG *)params[2] = enabled ? 1 : 2;
        }
        else if (params[0] && params[1] == 2026)
            *(ULONG *)params[2] = con_sync_active(CU(o)) ? 1 : 2;
        else DoSuperMethodA(cl, o, (Msg)msg);
        return;
    }

    if (msg->Command == C_GET_CURSOR_PREFS || msg->Command == C_SET_CURSOR_PREFS)
    {
        struct ConsolengCursorPrefs *prefs = (struct ConsolengCursorPrefs *)params[0];
        if (msg->Command == C_GET_CURSOR_PREFS)
        {
            prefs->cp_Size = sizeof(*prefs);
            prefs->cp_Version = CONSOLENG_CURSOR_PREFS_VERSION;
            prefs->cp_Style = data->cursorstyle;
            prefs->cp_BlinkIntervalMS = data->cursorInterval;
            prefs->cp_IdleTimeoutMS = data->cursorTimeout;
        }
        else
        {
            Console_UnRenderCursor(o);
            data->cursorstyle = data->resetCursorStyle = prefs->cp_Style;
            data->cursorInterval = prefs->cp_BlinkIntervalMS;
            data->cursorTimeout = prefs->cp_IdleTimeoutMS;
            Console_RenderCursor(o);
        }
        return;
    }

    if (msg->Command == C_SET_OBSCURED)
    {
        if (!CON_IS_OBSCURED(o))
        {
            Console_UnRenderCursor(o);
            DoSuperMethodA(cl, o, (Msg)msg);
            Console_RenderCursor(o);
        }
        return;
    }
    if (msg->Command == C_RESET_OBSCURED)
    {
        if (CON_IS_OBSCURED(o))
        {
            data->cursorpainted = FALSE;
            data->cursorTimerReady = FALSE;
            DoSuperMethodA(cl, o, (Msg)msg);
            if (data->resetClearPending)
            {
                stdcon_reset_clear(data, o);
                data->resetClearPending = FALSE;
            }
            if (data->alignmentPending)
            {
                stdcon_alignment(data, o);
                data->alignmentPending = FALSE;
            }
        }
        return;
    }

    if (msg->Command == C_SET_CURSOR_BLINK)
    {
        Console_UnRenderCursor(o);
        data->cursorstyle = stdcon_blink_style(data->cursorstyle, params[0] != 0);
        Console_RenderCursor(o);
        return;
    }

    if (msg->Command == C_SET_CURSOR_STYLE || msg->Command == C_SET_CURSOR_SHAPE)
    {
        BOOL shape_only = msg->Command == C_SET_CURSOR_SHAPE;
        if (params[0] >= 0 && params[0] <= (shape_only ? 2 : 6))
        {
            ULONG style = params[0];
            if (shape_only)
            {
                static const UBYTE shapes[] = {2, 6, 4}; /* block, beam, underline */
                style = shapes[style] - (data->cursorstyle & 1);
            }
            Console_UnRenderCursor(o);
            data->cursorstyle = style;
            Console_RenderCursor(o);
        }
        return;
    }

    if (msg->Command == C_SET_OSC_CLIPBOARD)
    {
        if (!con_osc_clipboard_read(ConsoleDevice, ICU(o), params[0]))
            con_osc_clipboard_apply(ConsoleDevice, ICU(o));
        con_osc_clipboard_reset(ICU(o));
        return;
    }
    if (msg->Command == C_GET_CLIPBOARD_PREFS || msg->Command == C_SET_CLIPBOARD_PREFS)
    {
        struct ConsolengClipboardPrefs *prefs = (APTR)params[0];
        if (msg->Command == C_SET_CLIPBOARD_PREFS)
        {
            ULONG policy = con_clipboard_policy(prefs);
            con_osc_read_cancel(o);
            if (ICU(o)->oscCopyDisabled != policy || (policy & 1))
            {
                con_osc_clipboard_reset(ICU(o));
                /* Discard any transfer already in progress, even if enabled
                 * again before its terminator arrives. */
                ICU(o)->oscClipboardInvalid = TRUE;
            }
            ICU(o)->oscCopyDisabled = policy;
        }
        else *prefs = (struct ConsolengClipboardPrefs){sizeof(*prefs),
            (ICU(o)->oscCopyDisabled & 2)?2:1,
            (!(ICU(o)->oscCopyDisabled & 1)) | (ICU(o)->oscCopyDisabled & 2)};
        params[1] = TRUE;
        return;
    }
    if (msg->Command == C_GET_BELL_PREFS || msg->Command == C_SET_BELL_PREFS)
    {
        struct ConsolengBellPrefs *prefs = (APTR)params[0];
        if (msg->Command == C_SET_BELL_PREFS)
            ICU(o)->bellDisabled = !prefs->bp_Enabled;
        else *prefs = (struct ConsolengBellPrefs){sizeof(*prefs),1,!ICU(o)->bellDisabled};
        params[1] = TRUE;
        return;
    }
    if (msg->Command == C_GET_EMOJI_PREFS || msg->Command == C_SET_EMOJI_PREFS)
    {
        struct ConsolengEmojiPrefs *prefs = (APTR)params[0];
        if (msg->Command == C_SET_EMOJI_PREFS)
            ICU(o)->emojiDisabled = !prefs->ep_Enabled;
        else *prefs = (struct ConsolengEmojiPrefs){sizeof(*prefs),1,!ICU(o)->emojiDisabled};
        params[1] = TRUE;
        return;
    }
    if (msg->Command == C_GET_CURSOR_COLOR_PREFS || msg->Command == C_SET_CURSOR_COLOR_PREFS)
    {
        struct ConsolengCursorColorPrefs *prefs = (APTR)params[0];
        if (msg->Command == C_SET_CURSOR_COLOR_PREFS)
        {
            data->cursorPrefRGB = prefs->ccp_RGB;
            data->resetCursorRGB = prefs->ccp_Enabled ? 0x01000000UL | prefs->ccp_RGB : 0;
            stdcon_cursorcolor(o, data, data->resetCursorRGB);
        }
        else *prefs = (struct ConsolengCursorColorPrefs){sizeof(*prefs),1,
            !!data->cursorRGB,data->cursorRGB ? data->cursorRGB & 0xffffffUL : data->cursorPrefRGB};
        params[1] = TRUE;
        return;
    }
    if (msg->Command == C_GET_COLOR_PREFS || msg->Command == C_SET_COLOR_PREFS)
    {
        params[1]=stdcon_colorprefs(cl,o,(APTR)params[0],msg->Command==C_SET_COLOR_PREFS);
        return;
    }
    if (msg->Command == C_SET_COLOR)
    {
        stdcon_setcolor(cl, o, params);
        return;
    }

    /* Obscured (background tab): keep all cursor/mode/state logic but
       never touch the shared window's RastPort. */
    if (CON_IS_OBSCURED(o))
    {
        switch (msg->Command)
        {
        case C_FORMFEED:
            ICU(o)->conFlags &= ~CF_WRAP_PENDING;
            XCCP = XCP = CHAR_XMIN(o);
            YCCP = YCP = CHAR_YMIN(o);
            break;
        case C_LINEFEED:
        case C_INDEX:
        case C_NEXT_LINE:
            ICU(o)->conFlags &= ~CF_WRAP_PENDING;
            Console_Down(o, 1);
            if (msg->Command == C_NEXT_LINE ||
                (msg->Command == C_LINEFEED && CHECK_MODE(o, M_LNM)))
                XCCP = XCP = CHAR_XMIN(o);
            break;
        case C_REVERSE_IDX:
        case C_VTAB:
            ICU(o)->conFlags &= ~CF_WRAP_PENDING;
            Console_Up(o, 1);
            break;

        case C_ASCII:
            stdcon_prepare_print(o);
            stdcon_advance_print(o, 1, FALSE);
            break;

        case C_ASCII_STRING:
            if (CON_TEXT_IS_UTF8(o))
            {
                const UBYTE *p = (const UBYTE *)params[0];
                LONG left = params[1];
                while (left > 0)
                {
                    WORD width = con_cp_width(con_utf8_decode(&p, &left));
                    if (!width) continue;
                    stdcon_prepare_print(o);
                    if (width > CHAR_XMAX(o) + 1 - XCCP)
                    {
                        if (!CHECK_MODE(o, PMB_AWM) || CHAR_XMAX(o) < 1)
                        {
                            ICU(o)->conFlags |= CF_WRAP_PENDING;
                            continue;
                        }
                        stdcon_advance_print(o, 1, FALSE);
                        stdcon_prepare_print(o);
                    }
                    stdcon_advance_print(o, width, FALSE);
                }
            }
            else
            {
                stdcon_prepare_print(o);
                stdcon_advance_print(o, (ULONG)params[1], FALSE);
            }
            break;

        case C_RESET_FONT:
        case C_SET_FONT:
            /* The base updates this unit's font/grid without painting the
               shared window. Reveal installs that font before redraw. */
            DoSuperMethodA(cl, o, (Msg)msg);
            break;

        default:
            /* pure state logic lives in the base class */
            DoSuperMethodA(cl, o, (Msg) msg);
            break;
        }
        ReturnVoid("StdCon::DoCommand(obscured)");
    }

    switch (msg->Command)
    {
    case C_NIL:
        /* do nothing */
        break;

    case C_ASCII:
        stdcon_prepare_print(o);

        D(bug("Writing char %c at (%d, %d)\n",
                params[0], CP_X(o), CP_Y(o) + rp->Font->tf_Baseline));

        if (insert || (data->cursorpainted &&
            (data->cursorLeft != CP_X(o) || data->cursorRight >= CP_X(o) + XRSIZE)))
            Console_UnRenderCursor(o);
        else stdcon_overwritecursor(data);

        setstyle(GfxBase, rp, o);
        stdcon_width_print(data, o, OCLASS(o) == cl, 1, FALSE, insert);
        stdcon_insert_print(o, rp, GfxBase, 1, insert);
        Move(rp, CP_X(o), CP_Y(o) + rp->Font->tf_Baseline);
        {
            UBYTE c = params[0];
            stdcon_native_span(data, o, OCLASS(o) == cl, &c, 1);
        }

        stdcon_advance_print(o, 1, FALSE);

        /* Rerender the cursor */
        Console_RenderCursor(o);

        break;

    case C_ASCII_STRING:
        D(bug("Writing string %.*s at (%d, %d)\n",
                (int)params[1], (char *)params[0], CP_X(o),
                CP_Y(o) + rp->Font->tf_Baseline));

        if (insert || (data->cursorpainted &&
            (data->cursorLeft != CP_X(o) || data->cursorRight >= CP_X(o) + XRSIZE)))
            Console_UnRenderCursor(o);
        else stdcon_overwritecursor(data);

        setstyle(GfxBase, rp, o);

        if (CON_TEXT_IS_UTF8(o))
        {
            /* The span is UTF-8: ASCII runs go through Text() unchanged,
               multi-byte runs render via TextUTF8() over a background
               fill. Cursor advance counts CELLS (codepoints), not bytes. */
            const UBYTE *p = (const UBYTE *) params[0];
            const UBYTE *end = p + params[1];

            while (p < end)
            {
                stdcon_prepare_print(o);
                ULONG remaining_space = CHAR_XMAX(o) + 1 - XCCP;

                if (remaining_space == 0)
                    remaining_space = 1;        /* paranoia: never stall */

                if (*p < 0x80)
                {
                    ULONG n = 0;

                    while (p + n < end && n < remaining_space && p[n] < 0x80)
                        n++;

                    /* Pixel insertion must have the same cell boundaries
                     * for whole and fragmented styled writes. */
                    if (insert && OCLASS(o) == cl &&
                        (CU(o)->cu_TxFlags & (CON_TXTFLAGS_BOLD | CON_TXTFLAGS_ITALIC)))
                        n = 1;
                    stdcon_width_print(data, o, OCLASS(o) == cl, n, FALSE, insert);
                    stdcon_insert_print(o, rp, GfxBase, n, insert);
                    Move(rp, CP_X(o), CP_Y(o) + rp->Font->tf_Baseline);
                    stdcon_native_span(data, o, OCLASS(o) == cl, p, n);
                    p += n;
                    stdcon_advance_print(o, n, p < end);
                }
                else
                {
                    /* one codepoint at a time so wide glyphs (emoji,
                       CJK) get their two cells and zero-width marks
                       consume none */
                    const UBYTE *q = p;
                    LONG qleft = end - p;
                    ULONG cp = con_utf8_decode(&q, &qleft);
                    WORD cw = con_cp_width(cp);
                    UBYTE tflags = CU(o)->cu_TxFlags;
                    UBYTE ufg = CU(o)->cu_FgPen, ubg = CU(o)->cu_BgPen, swp;

                    /* Latin-1 already has a native Text() glyph. Use the
                     * normal raster style just like retained redraw and
                     * byte-oriented writes; the Unicode JAM1 path changes
                     * antialiased pixels compared with native JAM2 text. */
                    if (cp < 0x100 && cw == 1)
                    {
                        UBYTE byte = cp;
                        stdcon_width_print(data, o, OCLASS(o) == cl, 1, FALSE, insert);
                        stdcon_insert_print(o, rp, GfxBase, 1, insert);
                        Move(rp, CP_X(o), CP_Y(o) + rp->Font->tf_Baseline);
                        stdcon_native_span(data, o, OCLASS(o) == cl, &byte, 1);
                        p = q;
                        stdcon_advance_print(o, 1, p < end);
                        continue;
                    }

                    data->styleLength = 0;

                    if (cw == 0)
                    {
                        /* combining marks/selectors: not yet composed */
                        p = q;
                        continue;
                    }
                    if ((ULONG) cw > remaining_space)
                    {
                        if (!CHECK_MODE(o, PMB_AWM) || CHAR_XMAX(o) < 1)
                        {
                            ICU(o)->conFlags |= CF_WRAP_PENDING;
                            p = q;
                            continue;
                        }
                        Move(rp, CP_X(o), CP_Y(o) + rp->Font->tf_Baseline);
                        stdcon_width_print(data, o, OCLASS(o) == cl, 1, FALSE, FALSE);
                        stdcon_text(data, rp, (CONST_STRPTR)" ", 1);
                        stdcon_advance_print(o, 1, FALSE);
                        stdcon_prepare_print(o);
                    }

                    stdcon_width_print(data, o, OCLASS(o) == cl, cw, cw == 2, insert);
                    stdcon_insert_print(o, rp, GfxBase, cw, insert);
                    ufg = ConsoleNGFaintPen(o, GfxBase, tflags, ufg, ICU(o)->fgColor);
                    if (tflags & CON_TXTFLAGS_REVERSED)
                    {
                        swp = ufg;
                        ufg = ubg;
                        ubg = swp;
                    }
                    if (tflags & CON_TXTFLAGS_CONCEALED)
                        ufg = ubg;

                    /* Unicode pens are already swapped above; do not apply INVERSVID twice. */
                    SetDrMd(rp, JAM1);
                    SetAPen(rp, ubg);
                    RectFill(rp, CP_X(o), CP_Y(o),
                        CP_X(o) + cw * XRSIZE - 1, CP_Y(o) + YRSIZE - 1);
                    SetAPen(rp, ufg);
                    Move(rp, CP_X(o), CP_Y(o) + rp->Font->tf_Baseline);
                    if (!(tflags & CON_TXTFLAGS_CONCEALED))
                        TextUTF8(rp, p, q - p);
                    ConsoleNGStrike(GfxBase, rp, tflags, CP_X(o), CP_Y(o),
                        cw * XRSIZE, YRSIZE,
                        ConsoleNGFaintPen(o, GfxBase, CU(o)->cu_TxFlags, CU(o)->cu_FgPen, ICU(o)->fgColor),
                        CU(o)->cu_BgPen);
                    ConsoleNGStyledUnderline(GfxBase, rp, tflags, CP_X(o), CP_Y(o),
                        cw * XRSIZE, YRSIZE,
                        ConsoleNGFaintPen(o, GfxBase, CU(o)->cu_TxFlags, CU(o)->cu_FgPen, ICU(o)->fgColor),
                        CU(o)->cu_BgPen, ICU(o)->underlineStyle);
                    ConsoleNGColorUnderline(o, GfxBase, rp, tflags, CP_X(o), CP_Y(o),
                        cw * XRSIZE, YRSIZE, ICU(o)->underlineStyle, ICU(o)->underlineColor);
                    setstyle(GfxBase, rp, o);
                    p = q;
                    stdcon_advance_print(o, cw, p < end);
                }
            }
        }
        else
        {
            ULONG len = params[1];
            STRPTR str = (STRPTR) params[0];

            while (len)
            {
                stdcon_prepare_print(o);
                ULONG remaining_space = CHAR_XMAX(o) + 1 - XCCP;
                ULONG line_len =
                    len < remaining_space ? len : remaining_space;

                if (insert && OCLASS(o) == cl &&
                    (CU(o)->cu_TxFlags & (CON_TXTFLAGS_BOLD | CON_TXTFLAGS_ITALIC)))
                    line_len = 1;
                stdcon_width_print(data, o, OCLASS(o) == cl, line_len, FALSE, insert);
                stdcon_insert_print(o, rp, GfxBase, line_len, insert);
                Move(rp, CP_X(o), CP_Y(o) + rp->Font->tf_Baseline);
                stdcon_native_span(data, o, OCLASS(o) == cl, str, line_len);

                len -= line_len;
                str += line_len;
                stdcon_advance_print(o, line_len, len != 0);
            }
        }

        /* Rerender the cursor */
        Console_RenderCursor(o);

        break;

    case C_FORMFEED:
        ICU(o)->conFlags &= ~CF_WRAP_PENDING;
        {
            /* Clear the console */

            UBYTE oldpen = rp->FgPen;
            IPTR newcurpos[2] = { 0, 0 };

            Console_UnRenderCursor(o);
            con_width_clear(&data->widths);

            SetAPen(rp, CU(o)->cu_BgPen);
            RectFill(rp, CU(o)->cu_XROrigin, CU(o)->cu_YROrigin,
                CU(o)->cu_XRExtant, CU(o)->cu_YRExtant);

            SetAPen(rp, oldpen);

            Console_DoCommand(o, C_CURSOR_POS, 2, newcurpos);

            Console_RenderCursor(o);

            break;
        }

    case C_DELETE_CHAR:
        {
            IPTR count = params[0] > 0 ? params[0] : 1;
            LONG available = CHAR_XMAX(o) - XCP + 1;
            UBYTE oldpen = rp->FgPen, oldbg = rp->BgPen, oldmode = rp->DrawMode;
            if (available <= 0) break;
            if (count > available) count = available;
            Console_UnRenderCursor(o);
            stdcon_width_edit(data, o, XCP, YCP, count, -1);
            SetDrMd(rp, JAM1);
            SetAPen(rp, (UBYTE)CU(o)->cu_BgPen);
            SetBPen(rp, (UBYTE)CU(o)->cu_BgPen);
            /* A full-span shift has no surviving pixels. Clear explicitly;
             * native ScrollRaster need not move a region by its whole width. */
            if (count == available)
                RectFill(rp, GFX_X(o, XCP), GFX_Y(o, YCP),
                    GFX_XMAX(o), GFX_Y(o, YCP + 1) - 1);
            else
                ScrollRaster(rp, count * XRSIZE, 0,
                    GFX_X(o, XCP), GFX_Y(o, YCP),
                    GFX_XMAX(o), GFX_Y(o, YCP + 1) - 1);
            SetAPen(rp, oldpen);
            SetBPen(rp, oldbg);
            SetDrMd(rp, oldmode);
            Console_RenderCursor(o);
        }
        break;

    case C_LINEFEED:
        ICU(o)->conFlags &= ~CF_WRAP_PENDING;
        D(bug("Got linefeed command\n"));
        /*Console_ClearCell(o, XCCP, YCCP); */
        Console_UnRenderCursor(o);

        Console_Down(o, 1);

        /* Check for linefeed mode (LF or LF+CR) */

        D(bug("conflags: %d\n", ICU(o)->conFlags));

        /* if (ICU(o)->conFlags & CF_LF_MODE_ON) */
        if (CHECK_MODE(o, M_LNM))
        {
            CU(o)->cu_XCP = CHAR_XMIN(o);
            CU(o)->cu_XCCP = CHAR_XMIN(o);
        }
        Console_RenderCursor(o);
        break;

    case C_VTAB:
        ICU(o)->conFlags &= ~CF_WRAP_PENDING;
        Console_UnRenderCursor(o);
        Console_Up(o, 1);
        Console_RenderCursor(o);
        break;

    case C_INDEX:
        ICU(o)->conFlags &= ~CF_WRAP_PENDING;
        Console_Down(o, 1);
        break;

    case C_NEXT_LINE:
        ICU(o)->conFlags &= ~CF_WRAP_PENDING;
        D(bug("Got NEXT LINE cmd\n"));
        Console_Down(o, 1);
        Console_Left(o, XCP);
        break;

    case C_REVERSE_IDX:
        ICU(o)->conFlags &= ~CF_WRAP_PENDING;
        Console_Up(o, 1);
        break;

    case C_ERASE_CHAR:
        {
            IPTR count = params[0] > 0 ? params[0] : 1;
            LONG available = CHAR_XMAX(o) - XCP + 1;
            UBYTE oldpen = rp->FgPen;
            if (available <= 0) break;
            if (count > available) count = available;
            Console_UnRenderCursor(o);
            stdcon_width_edit(data, o, XCP, YCP, count, 0);
            SetAPen(rp, CU(o)->cu_BgPen);
            RectFill(rp, GFX_X(o, XCP), GFX_Y(o, YCP),
                GFX_X(o, XCP + count) - 1, GFX_Y(o, YCP + 1) - 1);
            SetAPen(rp, oldpen);
            Console_RenderCursor(o);
        }
        break;

    case C_ERASE_IN_LINE:
        {
            UBYTE oldpen = rp->FgPen;
            WORD xmin, xmax;

            Console_UnRenderCursor(o);

            switch (params[0])
            {
            case 1:
                xmin = CU(o)->cu_XROrigin;
                xmax = GFX_X(o, XCP + 1) - 1;
                break;
            case 2:
                xmin = CU(o)->cu_XROrigin;
                xmax = CU(o)->cu_XRExtant;
                break;
            case 0:
            default:
                xmin = GFX_X(o, XCP);
                xmax = CU(o)->cu_XRExtant;
                break;
            }

            LONG first=params[0]!=1 && params[0]!=2 ? XCP : 0;
            LONG last=params[0]==1 ? XCP+1 : CHAR_XMAX(o)+1;
            stdcon_width_edit(data,o,first,YCP,last-first,0);
            SetAPen(rp, CU(o)->cu_BgPen);
            SetDrMd(rp, JAM2);

            RectFill(rp, xmin,
                CU(o)->cu_YROrigin + YCP * YRSIZE, xmax,
                CU(o)->cu_YROrigin + (YCP + 1) * YRSIZE - 1);


            SetAPen(rp, oldpen);

            Console_RenderCursor(o);

        }
        break;

    case C_ERASE_IN_DISPLAY:
        {
            UBYTE oldpen = rp->FgPen;
            WORD ymin;

            Console_UnRenderCursor(o);

            if(params[0]!=3) {
                LONG first=params[0]==1 || params[0]==2 ? 0 : YCP;
                LONG last=params[0]==1 ? YCP : CHAR_YMAX(o);
                for(LONG y=first;y<=last;y++) {
                    LONG x=y==YCP && params[0]!=1 && params[0]!=2 ? XCP : 0;
                    LONG end=y==YCP && params[0]==1 ? XCP+1 : CHAR_XMAX(o)+1;
                    stdcon_width_edit(data,o,x,y,end-x,0);
                }
            }
            SetAPen(rp, CU(o)->cu_BgPen);
            SetDrMd(rp, JAM2);

            switch (params[0])
            {
            case 1:
                if (YCP > CHAR_YMIN(o))
                    RectFill(rp, CU(o)->cu_XROrigin,
                        CU(o)->cu_YROrigin, CU(o)->cu_XRExtant,
                        GFX_Y(o, YCP) - 1);
                RectFill(rp, CU(o)->cu_XROrigin, GFX_Y(o, YCP),
                    GFX_X(o, XCP + 1) - 1, GFX_Y(o, YCP + 1) - 1);
                break;
            case 3:
                /* No saved history in the standard unit; preserve viewport. */
                break;
            case 2:
                RectFill(rp, CU(o)->cu_XROrigin, CU(o)->cu_YROrigin,
                    CU(o)->cu_XRExtant, CU(o)->cu_YRExtant);
                break;
            case 0:
            default:
                RectFill(rp, GFX_X(o, XCP), GFX_Y(o, YCP),
                    CU(o)->cu_XRExtant, GFX_Y(o, YCP + 1) - 1);
                ymin = GFX_Y(o, YCP + 1);
                if (ymin <= CU(o)->cu_YRExtant)
                    RectFill(rp, CU(o)->cu_XROrigin, ymin,
                        CU(o)->cu_XRExtant, CU(o)->cu_YRExtant);
                break;
            }

            SetAPen(rp, oldpen);

            Console_RenderCursor(o);

            break;
        }

    case C_INSERT_CHAR:
        {
            IPTR count = params[0] > 0 ? params[0] : 1;
            LONG available = CHAR_XMAX(o) - XCP + 1;
            UBYTE oldpen = rp->FgPen, oldbg = rp->BgPen, oldmode = rp->DrawMode;
            if (available <= 0) break;
            if (count > available) count = available;
            Console_UnRenderCursor(o);
            stdcon_width_edit(data, o, XCP, YCP, count, 1);
            SetDrMd(rp, JAM1);
            SetAPen(rp, (UBYTE)CU(o)->cu_BgPen);
            SetBPen(rp, (UBYTE)CU(o)->cu_BgPen);
            /* A full-span shift has no surviving pixels. Clear explicitly;
             * native ScrollRaster need not move a region by its whole width. */
            if (count == available)
                RectFill(rp, GFX_X(o, XCP), GFX_Y(o, YCP),
                    GFX_XMAX(o), GFX_Y(o, YCP + 1) - 1);
            else
                ScrollRaster(rp, -count * XRSIZE, 0,
                    GFX_X(o, XCP), GFX_Y(o, YCP),
                    GFX_XMAX(o), GFX_Y(o, YCP + 1) - 1);
            SetAPen(rp, oldpen);
            SetBPen(rp, oldbg);
            SetDrMd(rp, oldmode);
            Console_RenderCursor(o);
        }
        break;

    case C_INSERT_LINE:
    case C_DELETE_LINE:
    case C_SCROLL_DOWN:
        {
            IPTR count = params[0] > 0 ? params[0] : 1;
            LONG firstRow = msg->Command == C_SCROLL_DOWN ? CON_SCROLL_TOP(o) : YCP;
            LONG bottom = CON_SCROLL_BOTTOM(o);
            LONG available = bottom - firstRow + 1;
            if (firstRow < CON_SCROLL_TOP(o) || firstRow > bottom) break;
            UBYTE oldpen = rp->FgPen, oldbg = rp->BgPen, oldmode = rp->DrawMode;
            if (available <= 0) break;
            if (count > available) count = available;
            Console_UnRenderCursor(o);
            SetDrMd(rp, JAM1);
            SetAPen(rp, CU(o)->cu_BgPen);
            SetBPen(rp, CU(o)->cu_BgPen);
            if (count == available)
                RectFill(rp, GFX_XMIN(o), GFX_Y(o, firstRow), GFX_XMAX(o), GFX_Y(o, bottom + 1) - 1);
            else
                ScrollRaster(rp, 0,
                    (msg->Command != C_DELETE_LINE ? -1 : 1) * YRSIZE * count,
                    GFX_XMIN(o), GFX_Y(o, firstRow), GFX_XMAX(o), GFX_Y(o, bottom + 1) - 1);
            con_width_scroll(&data->widths,firstRow,bottom,
                msg->Command == C_DELETE_LINE ? count : -(LONG)count);
            SetAPen(rp, oldpen);
            SetBPen(rp, oldbg);
            SetDrMd(rp, oldmode);
            Console_RenderCursor(o);
            break;
        }

    case C_SCROLL_UP:
        {
            IPTR count = params[0] > 0 ? params[0] : 1;
            LONG top = CON_SCROLL_TOP(o), bottom = CON_SCROLL_BOTTOM(o);
            LONG available = bottom - top + 1;
            UBYTE oldpen = rp->FgPen, oldbg = rp->BgPen, oldmode = rp->DrawMode;
            if (available <= 0) break;
            if (count > available) count = available;
            Console_UnRenderCursor(o);
            SetDrMd(rp, JAM1);
            SetAPen(rp, CU(o)->cu_BgPen);
            SetBPen(rp, CU(o)->cu_BgPen);
            if (count == available)
                RectFill(rp, GFX_XMIN(o), GFX_Y(o, top), GFX_XMAX(o), GFX_Y(o, bottom + 1) - 1);
            else
                ScrollRaster(rp, 0, YRSIZE * count, GFX_XMIN(o),
                    GFX_Y(o, top), GFX_XMAX(o), GFX_Y(o, bottom + 1) - 1);
            con_width_scroll(&data->widths,top,bottom,count);
            SetAPen(rp, oldpen);
            SetBPen(rp, oldbg);
            SetDrMd(rp, oldmode);
            Console_RenderCursor(o);
            break;
        }

    case C_SET_TOP_OFFSET:
        Console_UnRenderCursor(o);
        CU(o)->cu_YROrigin = params[0];
        CU(o)->cu_YMax =
            (w->Height - (CU(o)->cu_YROrigin +
                w->BorderBottom)) / CU(o)->cu_YRSize - 1;
        Console_RenderCursor(o);
        Console_NewWindowSize(o);
        break;

    case C_SET_PAGE_LENGTH:
        Console_UnRenderCursor(o);
        CU(o)->cu_YMax = params[0];
        // FIXME: Need to set something that prevents NewWindowSize to
        // change YMax
        Console_RenderCursor(o);
        Console_NewWindowSize(o);
        break;

    case C_SET_FONT:
    case C_RESET_FONT:
    case C_ZOOM_FONT:
        DoSuperMethodA(cl, o, (Msg)msg);
        if (ICU(o)->fontSetOK) con_width_clear(&data->widths);
        break;

    default:
        DoSuperMethodA(cl, o, (Msg) msg);
        break;
    }

    ReturnVoid("StdCon::DoCommand");
}

/*********  StdCon::RenderCursor()  ****************************/
/* Preserve the original bitmap, not a reconstructed glyph or a second RGB
 * conversion. This also restores antialiasing and text underneath bar cursors.
 * Direct-color block cursors XOR a fixed RGB mask relative to the text background:
 * background becomes the requested color and distinct glyph pixels stay distinct.
 * Indexed screens XOR pen identities relative to the background, preserving
 * distinct glyph pens; hold the allocated color until the saved pixels return.
 * Unsupported screens or allocation failure keep the legacy invert cursor.
 */
static BOOL stdcon_coloredcursor(Object *o, struct stdcondata *data)
{
    struct RastPort *rp = RASTPORT(o), saved;
    struct Library *GfxBase = data->scd_GfxBase;
    struct Library *CyberGfxBase = data->cursorCyberBase;
    LONG left = data->cursorLeft, top = data->cursorTop;
    ULONG width = data->cursorRight - left + 1;
    ULONG height = data->cursorBottom - top + 1;

    if (data->cursorpainted)
    {
        if (!data->cursorColored) return FALSE;
        InitRastPort(&saved);
        saved.BitMap = data->cursorSaved;
        UBYTE oldmask = rp->Mask;
        SetWriteMask(rp, 0xff);
        ClipBlit(&saved, 0, 0, rp, left, top, width, height, 0xc0);
        SetWriteMask(rp, oldmask);
        if (data->cursorIndexed) ReleasePen(data->cm, data->cursorPen);
        data->cursorIndexed = FALSE;
        data->cursorColored = FALSE;
        return TRUE;
    }
    data->cursorColored = FALSE;
    ULONG depth = GetBitMapAttr(rp->BitMap, BMA_DEPTH);
    BOOL indexed = depth >= 1 && depth <= 8;
    if (!data->cursorRGB || !width || !height ||
        (indexed ? !data->cm : (!CyberGfxBase ||
            (depth != 15 && depth != 16 && depth < 24) ||
            !GetCyberMapAttr(rp->BitMap, CYBRMATTR_ISCYBERGFX))))
        return FALSE;
    if (!data->cursorSaved || data->cursorSavedWidth != width ||
        data->cursorSavedHeight != height ||
        GetBitMapAttr(data->cursorSaved, BMA_DEPTH) != depth)
    {
        struct BitMap *bitmap = AllocBitMap(width, height,
            GetBitMapAttr(rp->BitMap, BMA_DEPTH), BMF_CLEAR, rp->BitMap);
        if (!bitmap) return FALSE;
        if (data->cursorSaved) FreeBitMap(data->cursorSaved);
        data->cursorSaved = bitmap;
        data->cursorSavedWidth = width;
        data->cursorSavedHeight = height;
    }
    if (indexed)
    {
        ULONG rgb = data->cursorRGB;
        LONG pen = stdcon_bestpen(data, (rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255);
        if (pen < 0) return FALSE;
        if ((ULONG)pen >= (1UL << depth))
        {
            ReleasePen(data->cm, pen);
            return FALSE;
        }
        data->cursorPen = pen;
    }
    InitRastPort(&saved);
    saved.BitMap = data->cursorSaved;
    ClipBlit(rp, left, top, &saved, 0, 0, width, height, 0xc0);
    if (indexed)
    {
        UBYTE oldpen = rp->FgPen, oldmode = rp->DrawMode;
        ULONG oldmask = rp->Mask;
        SetWriteMask(rp, 0xff);
        ULONG background = (CU(o)->cu_TxFlags & CON_TXTFLAGS_REVERSED) ?
            CU(o)->cu_FgPen : CU(o)->cu_BgPen;
        ULONG mask = (background ^ data->cursorPen) & ((1UL << depth) - 1);
        SetDrMd(rp, JAM1);
        for (ULONG y = 0; y < height; y++)
            for (ULONG x = 0; x < width; x++)
            {
                if (data->cursorPatterned && ((left + x + top + y) & 1)) continue;
                ULONG pen = data->cursorstyle <= 2 ? ReadPixel(&saved, x, y) ^ mask : data->cursorPen;
                SetAPen(rp, pen);
                WritePixel(rp, left + x, top + y);
            }
        SetAPen(rp, oldpen);
        SetDrMd(rp, oldmode);
        SetWriteMask(rp, oldmask);
        data->cursorIndexed = data->cursorColored = TRUE;
        return TRUE;
    }
    ULONG background = stdcon_penrgb(data, (CU(o)->cu_TxFlags & CON_TXTFLAGS_REVERSED) ?
        CU(o)->cu_FgPen : CU(o)->cu_BgPen) & 0xffffff;
    ULONG mask = background ^ (data->cursorRGB & 0xffffff);
    for (ULONG y = 0; y < height; y++)
        for (ULONG x = 0; x < width; x++)
        {
            if (data->cursorPatterned && ((left + x + top + y) & 1)) continue;
            ULONG pixel = ReadRGBPixel(&saved, x, y);
            ULONG color = data->cursorstyle <= 2 ? pixel ^ mask :
                (pixel & 0xff000000) | (data->cursorRGB & 0xffffff);
            WriteRGBPixel(rp, left + x, top + y, color);
        }
    data->cursorColored = TRUE;
    return TRUE;
}

static VOID stdcon_drawcursor(Object *o, struct stdcondata *data)
{
    static const UWORD inactive_pattern[] = { 0xAAAA, 0x5555 };
    struct RastPort *rp = RASTPORT(o);
    const UWORD *oldpattern = rp->AreaPtrn;
    BYTE oldpatternsize = rp->AreaPtSz;
    UBYTE oldmode = rp->DrawMode;
    struct Library *GfxBase = data->scd_GfxBase;

    LONG left, top, right, bottom;
    if (!data->cursorpainted)
    {
        IPTR span = data->cursorQueryReady ? Console_CursorSpan(o) : 1;
        LONG cells = span == 2 || span == 3 ? 2 : 1;
        left = CP_X(o) - (span == 3 ? XRSIZE : 0);
        top = CP_Y(o);
        right = left + cells * XRSIZE - 1;
        bottom = top + YRSIZE - 1;
        if (data->cursorstyle == 3 || data->cursorstyle == 4)
            top = bottom - MAX(1, YRSIZE / 8) + 1;
        else if (data->cursorstyle == 5 || data->cursorstyle == 6)
            right = left + MAX(1, XRSIZE / 8) - 1;
        data->cursorLeft = left; data->cursorTop = top;
        data->cursorRight = right; data->cursorBottom = bottom;
        data->cursorPatterned = !data->windowactive;
    }
    /* Erase exactly what was painted, even if retained cells or live cursor
     * geometry changed before the parent receives an overwrite command. */
    left = data->cursorLeft; top = data->cursorTop;
    right = data->cursorRight; bottom = data->cursorBottom;
    if (stdcon_coloredcursor(o, data))
    {
        data->cursorpainted = !data->cursorpainted;
        return;
    }
    SetDrMd(rp, COMPLEMENT);
    rp->AreaPtrn = NULL;
    rp->AreaPtSz = 0;
    if (data->cursorPatterned)
    {
        rp->AreaPtrn = inactive_pattern;
        rp->AreaPtSz = 1;
    }
    RectFill(rp, left, top, right, bottom);
    rp->AreaPtrn = oldpattern;
    rp->AreaPtSz = oldpatternsize;
    SetDrMd(rp, oldmode);
    data->cursorpainted = !data->cursorpainted;
}

static VOID stdcon_rendercursor(Class *cl, Object *o,
    struct P_Console_RenderCursor *msg)
{
    struct stdcondata *data = INST_DATA(cl, o);

    /* SetAPen(rp, data->dri->dri_Pens[FILLPEN]); */

    data->rendercursorcount++;

    if (data->cursorvisible && (data->rendercursorcount == 1)
        && !CON_IS_OBSCURED(o))
    {
        data->cursorTimerReady = FALSE;
        if (!data->cursorpainted)
            stdcon_drawcursor(o, data);
    }
}

/*********  StdCon::UnRenderCursor()  ****************************/
static VOID stdcon_unrendercursor(Class *cl, Object *o,
    struct P_Console_UnRenderCursor *msg)
{
    struct stdcondata *data = INST_DATA(cl, o);

    data->rendercursorcount--;

    /* SetAPen(rp, data->dri->dri_Pens[FILLPEN]); */

    if (data->cursorpainted && (data->rendercursorcount == 0)
        && !CON_IS_OBSCURED(o))
    {
        stdcon_drawcursor(o, data);
    }
}

static VOID stdcon_setactive(Class *cl, Object *o,
    struct P_Console_HandleGadgets *msg)
{
    struct stdcondata *data = INST_DATA(cl, o);
    struct InputEvent *event = msg->Event;
    if (event->ie_Class == IECLASS_RAWKEY)
    {
        data->cursorTimerReady = FALSE;
        if (data->windowactive && data->cursorvisible &&
            data->rendercursorcount == 1 && !CON_IS_OBSCURED(o) &&
            !data->cursorpainted)
            stdcon_drawcursor(o, data);
        return;
    }
    if (event->ie_Class == IECLASS_TIMER)
    {
        if (!(data->cursorstyle & 1) || !data->windowactive ||
            !data->cursorvisible || data->rendercursorcount != 1 ||
            CON_IS_OBSCURED(o))
        {
            data->cursorTimerReady = FALSE;
            return;
        }
        ULONG now = (ULONG)event->ie_TimeStamp.tv_secs * 1000 +
            event->ie_TimeStamp.tv_micro / 1000;
        if (!data->cursorTimerReady)
        {
            data->cursorStamp = data->cursorStart = now;
            data->cursorTimerReady = TRUE;
        }
        else if (data->cursorTimeout &&
            (ULONG)(now - data->cursorStart) >= data->cursorTimeout)
        {
            if (!data->cursorpainted) stdcon_drawcursor(o, data);
        }
        else if ((ULONG)(now - data->cursorStamp) >= data->cursorInterval)
        {
            stdcon_drawcursor(o, data);
            data->cursorStamp = now;
        }
        return;
    }
    if (event->ie_Class != IECLASS_ACTIVEWINDOW &&
        event->ie_Class != IECLASS_INACTIVEWINDOW)
        return;
    BOOL active = event->ie_Class == IECLASS_ACTIVEWINDOW;
    if (active == data->windowactive)
        return;

    if (data->cursorpainted && !CON_IS_OBSCURED(o))
        stdcon_drawcursor(o, data);
    data->windowactive = active;
    data->cursorTimerReady = FALSE;
    if (data->cursorvisible && data->rendercursorcount == 1 &&
        !CON_IS_OBSCURED(o))
        stdcon_drawcursor(o, data);
}

/**************************
**  StdCon::ClearCell()  **
**************************/
static VOID stdcon_clearcell(Class *cl, Object *o,
    struct P_Console_ClearCell *msg)
{
    struct RastPort *rp = RASTPORT(o);
    struct stdcondata *data = INST_DATA(cl, o);
    struct Library *GfxBase = data->scd_GfxBase;
    data->styleLength = 0;

    SetAPen(rp, data->dri->dri_Pens[BACKGROUNDPEN]);
    SetDrMd(rp, JAM1);
    RectFill(rp, GFX_X(o, msg->X), GFX_Y(o, msg->Y), GFX_X(o,
            msg->X) + XRSIZE - 1, GFX_Y(o, msg->Y) + YRSIZE - 1);
}

/*******************************
**  StdCon::NewWindowSize()  **
*******************************/
static VOID stdcon_newwindowsize(Class *cl, Object *o,
    struct P_Console_NewWindowSize *msg)
{
    struct RastPort *rp = RASTPORT(o);
    struct stdcondata *data = INST_DATA(cl, o);
    struct Library *GfxBase = data->scd_GfxBase;
    data->styleLength = 0;
    WORD old_xmax = CHAR_XMAX(o);
    WORD old_ymax = CHAR_YMAX(o);
    WORD old_xcp = XCP;
    WORD old_ycp = YCP;

    WORD x1, y1, x2, y2;

    DoSuperMethodA(cl, o, (Msg) msg);
    if (old_xmax != CHAR_XMAX(o) || old_ymax != CHAR_YMAX(o))
        ICU(o)->scrollTop = ICU(o)->scrollBottom = 0;
    con_width_resize(&data->widths,CHAR_XMAX(o)+1,CHAR_YMAX(o)+1,
        XRSIZE,YRSIZE,FALSE);
    if (CON_IS_OBSCURED(o)) return;
    /* Shrinking through a wide glyph must not leave its visible head behind. */
    if(data->widths.cells) {
        UBYTE fg=rp->FgPen,bg=rp->BgPen,mode=rp->DrawMode;
        SetDrMd(rp,JAM1); SetAPen(rp,(UBYTE)CU(o)->cu_BgPen);
        for(ULONG y=0;y<data->widths.rows;y++) {
            UBYTE *row=con_width_row(&data->widths,y);
            ULONG x=data->widths.columns-1;
            if(row[x]==1) {
                row[x]=0;
                RectFill(rp,GFX_X(o,x),GFX_Y(o,y),GFX_X(o,x+1)-1,GFX_Y(o,y+1)-1);
            }
        }
        SetAPen(rp,fg); SetBPen(rp,bg); SetDrMd(rp,mode);
    }

    if (CHAR_XMAX(o) < old_xmax)
    {
        x1 = GFX_XMAX(o) + 1;
        y1 = GFX_YMIN(o);
        x2 = GFX_XMAX(o);
        y2 = GFX_YMAX(o) - 1;

        if ((x2 >= x1) && (y2 >= y1))
        {
            SetAPen(rp, 0);
            SetDrMd(rp, JAM2), RectFill(rp, x1, y1, x2, y2);
        }
    }

    if (CHAR_YMAX(o) < old_ymax)
    {
        x1 = GFX_XMIN(o);
        y1 = GFX_YMAX(o) + 1;
        x2 = WINDOW(o)->Width - WINDOW(o)->BorderRight - 1;
        y2 = WINDOW(o)->Height - WINDOW(o)->BorderBottom - 1;

        if ((x2 >= x1) && (y2 >= y1))
        {
            SetAPen(rp, 0);
            SetDrMd(rp, JAM2), RectFill(rp, x1, y1, x2, y2);
        }
    }

    if ((old_xcp != XCP) || (old_ycp != YCP))
    {
        con_width_clear(&data->widths);
        SetAPen(rp, 0);
        SetDrMd(rp, JAM2);
        RectFill(rp, GFX_XMIN(o), GFX_YMIN(o), GFX_XMAX(o), GFX_YMAX(o));
        data->cursorpainted = FALSE;
        data->rendercursorcount--;
        Console_RenderCursor(o);
    }
    return;
}

/* Extended indexed colors retain logical keys, so their physical pens can
 * be a bounded cache. Keep the active rendition and the previous lookup alive
 * while resolving a foreground/background pair for a dormant retained row. */
static VOID stdcon_trimxterm(Object *o, struct stdcondata *data, UWORD added)
{
    struct Library *GfxBase = data->scd_GfxBase;
    data->xtermCount++;
    if (data->xtermCount <= 64) return;
    for (UWORD scanned = 0; scanned < 240; scanned++)
    {
        UWORD victim = 16 + data->xtermNext;
        data->xtermNext = (data->xtermNext + 1) % 240;
        if (victim == added || victim == data->xtermLast ||
            CONPEN_XTERM(victim) == ICU(o)->fgColor ||
            CONPEN_XTERM(victim) == ICU(o)->bgColor ||
            data->xtermpens[victim] == -1) continue;
        ReleasePen(data->cm, data->xtermpens[victim]);
        data->xtermpens[victim] = -1;
        data->xtermCount--;
        return;
    }
}

IPTR stdcon_getpencolor(Class *cl, Object *o, struct P_Console_GetColorPen *msg)
{
    struct stdcondata *data = INST_DATA(cl, o);
    ULONG idx = msg->ColorIdx;
    if (msg->PenPtr == (UBYTE *)&CU(o)->cu_FgPen) ICU(o)->fgColor = idx;
    if (msg->PenPtr == (UBYTE *)&CU(o)->cu_BgPen) ICU(o)->bgColor = idx;

    /* Retain named provenance above; resolve through the shared palette. */
    if (CONPEN_ISBRIGHT(idx)) idx = CONPEN_XTERM(CONPEN_BRIGHTIDX(idx) + 8);

    if ((idx == CONUNIT_PEN_DEFFG || idx == CONUNIT_PEN_DEFBG)
        && data->defpen[idx == CONUNIT_PEN_DEFBG] != -1)
    {
        *msg->PenPtr = (UBYTE) data->defpen[idx == CONUNIT_PEN_DEFBG];
        return TRUE;
    }
    if (idx < 8 && data->ansipen[idx] != -1)
    {
        *msg->PenPtr = (UBYTE) data->ansipen[idx];
        return TRUE;
    }
    if (CONPEN_ISXTERM(idx)
        && data->ansipen[CONPEN_XTERMIDX(idx)] != -1)
    {
        *msg->PenPtr = (UBYTE) data->ansipen[CONPEN_XTERMIDX(idx)];
        return TRUE;
    }

    if (CONPEN_ISRGB(idx) && data->cm)
    {
        /* 24-bit truecolor - small round-robin pen cache */
        UWORD i;
        LONG pen;

        for (i = 0; i < STDCON_TCPENS; i++)
        {
            if (data->tcpens[i].rgb == idx)
            {
                /* Do not immediately evict a just-resolved foreground when
                 * the next lookup resolves the background of the same run. */
                data->tcnext = (i + 1) % STDCON_TCPENS;
                *msg->PenPtr = (UBYTE) data->tcpens[i].pen;
                return TRUE;
            }
        }

        pen = stdcon_bestpen(data, (idx >> 16) & 0xFF, (idx >> 8) & 0xFF,
            idx & 0xFF);
        if (pen == -1)
        {
            *msg->PenPtr = (BYTE)*(data->penmap[CONUNIT_PEN_DEFFG]);
            return TRUE;
        }

        i = data->tcnext;
        /* Current SGR colors remain live even when only the other component
         * changes repeatedly. At most two entries are pinned in this cache. */
        while (data->tcpens[i].rgb &&
            (data->tcpens[i].rgb == ICU(o)->fgColor ||
             data->tcpens[i].rgb == ICU(o)->bgColor))
            i = (i + 1) % STDCON_TCPENS;
        data->tcnext = (i + 1) % STDCON_TCPENS;
        if (data->tcpens[i].rgb)
        {
            struct Library *GfxBase = data->scd_GfxBase;
            ReleasePen(data->cm, data->tcpens[i].pen);
        }
        data->tcpens[i].rgb = idx;
        data->tcpens[i].pen = pen;
        *msg->PenPtr = (UBYTE) pen;
        return TRUE;
    }

    if (CONPEN_ISXTERM(idx) && data->cm)
    {
        UWORD n = CONPEN_XTERMIDX(idx);

        if (data->xtermpens[n] == -1)
        {
            UBYTE r, g, b;

            xterm_to_rgb(n, &r, &g, &b);
            data->xtermpens[n] = stdcon_bestpen(data, r, g, b);
            if (n >= 16 && data->xtermpens[n] != -1)
                stdcon_trimxterm(o, data, n);
        }
        if (n >= 16) data->xtermLast = n;
        if (data->xtermpens[n] != -1)
            *msg->PenPtr = (UBYTE) data->xtermpens[n];
        else
            *msg->PenPtr = (BYTE)*(data->penmap[CONUNIT_PEN_DEFFG]);
        return TRUE;
    }

    if (idx < CONUNIT_PEN_MAX)
        *msg->PenPtr = (BYTE)*(data->penmap[idx]);
    return TRUE;
}

AROS_UFH3S(IPTR, dispatch_stdconclass,
    AROS_UFHA(Class *, cl, A0),
    AROS_UFHA(Object *, o, A2), AROS_UFHA(Msg, msg, A1))
{
    AROS_USERFUNC_INIT

    IPTR retval = 0UL;

    switch (msg->MethodID)
    {
    case M_Console_GetColorPen:
        retval = (IPTR) stdcon_getpencolor(cl, o, (struct P_Console_GetColorPen *)msg);
        break;

    case OM_NEW:
        retval = (IPTR) stdcon_new(cl, o, (struct opSet *)msg);
        break;

    case OM_DISPOSE:
        stdcon_dispose(cl, o, msg);
        break;

    case M_Console_DoCommand:
        stdcon_docommand(cl, o, (struct P_Console_DoCommand *)msg);
        break;

    case M_Console_RenderCursor:
        stdcon_rendercursor(cl, o, (struct P_Console_RenderCursor *)msg);
        break;

    case M_Console_UnRenderCursor:
        stdcon_unrendercursor(cl, o,
            (struct P_Console_UnRenderCursor *)msg);
        break;

    case M_Console_ClearCell:
        stdcon_clearcell(cl, o, (struct P_Console_ClearCell *)msg);
        break;

    case M_Console_NewWindowSize:
        stdcon_newwindowsize(cl, o, (struct P_Console_NewWindowSize *)msg);
        break;

    case M_Console_FilterInput:
        /* Plain consoles do not own local search/vi input. */
        retval = FALSE;
        break;

    case M_Console_HandleGadgets:
        stdcon_setactive(cl, o,
            (struct P_Console_HandleGadgets *)msg);
        break;

    default:
        retval = DoSuperMethodA(cl, o, msg);
        break;
    }

    return retval;

    AROS_USERFUNC_EXIT
}

#undef ConsoleDevice

Class *makeStdConClass(struct ConsoleBase *ConsoleDevice)
{
    Class *cl;

    cl = MakeClass(NULL, NULL, CONSOLECLASSPTR, sizeof(struct stdcondata),
        0UL);
    if (cl)
    {
        cl->cl_Dispatcher.h_Entry = (APTR) dispatch_stdconclass;
        cl->cl_Dispatcher.h_SubEntry = NULL;

        cl->cl_UserData = (IPTR) ConsoleDevice;

        return (cl);
    }
    return NULL;
}

/* Explicit underline colors are independent of reverse text colors. The
 * existing strike remains foreground-colored; only underline pixels change. */
void ConsoleNGColorUnderline(Object *o, struct Library *GfxBase, struct RastPort *rp,
    UBYTE flags, LONG x, LONG y, LONG width, LONG height, UBYTE style, ULONG color)
{
    UBYTE pen, oldpen, oldmode;
    LONG row;
    if (!color || !rp || width <= 0 || height <= 0 ||
        (flags & CON_TXTFLAGS_CONCEALED) ||
        !(flags & (CON_TXTFLAGS_UNDERLINED | CON_TXTFLAGS_DOUBLEUNDERLINED))) return;
    Console_GetColorPen(o, color - 1, &pen);
    pen = ConsoleNGFaintPen(o, GfxBase, flags, pen, color - 1);
    flags &= ~CON_TXTFLAGS_REVERSED;
    if (flags & CON_TXTFLAGS_DOUBLEUNDERLINED)
    {
        ConsoleNGStrike(GfxBase, rp, CON_TXTFLAGS_DOUBLEUNDERLINED,
            x, y, width, height, pen, pen);
        return;
    }
    if (style)
    {
        ConsoleNGStyledUnderline(GfxBase, rp, flags, x, y, width, height, pen, pen, style);
        return;
    }
    row = rp->Font ? rp->Font->tf_Baseline + 2 : height - 1;
    if (row >= height) row = height - 1;
    if (row < 0) row = 0;
    oldpen = rp->FgPen; oldmode = rp->DrawMode;
    SetDrMd(rp, JAM1); SetAPen(rp, pen);
    RectFill(rp, x, y + row, x + width - 1, y + row);
    SetAPen(rp, oldpen); SetDrMd(rp, oldmode);
}
