/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc: consoleng.device's local stand-in for graphics.library/TextUTF8().
*/

#include <proto/graphics.h>
#include <graphics/rastport.h>
#include <graphics/text.h>
#include <graphics/gfxmacros.h>

#include "console_gcc.h"

/*
 * WHY THIS EXISTS, AND WHAT IT DELIBERATELY DOES NOT DO.
 *
 * An earlier (non-ADT) tree renders codepoints above U+00FF through a new
 * graphics.library LVO, TextUTF8(), which in turn needs a UTF-8 glyph-map API
 * in diskfont.library (UTF8GlyphAdvance / UTF8RenderCP / ColorGlyphMap).
 * Neither exists in this tree, and adding them means changing two ROM
 * libraries that the whole system sits on. terminills' call: keep it out of
 * graphics.library - if it belongs there, that is an upstream merge decision,
 * not a side effect of porting a console.
 *
 * So this device carries its own stand-in instead, and the cost is bounded
 * because of how the callers are written: stdconclass.c and charmapconclass.c
 * already SPLIT each same-pen run, sending ASCII/Latin-1 cells through the
 * ordinary Text() path as bytes and calling here only for codepoints ABOVE
 * U+00FF. Everything a terminal needs structurally - escape-sequence
 * reassembly across writes, CSI semantics through UTF-8 input, deferred
 * autowrap, the redraw controls, UCS-4 charmap cells - is in the device and is
 * unaffected by this file.
 *
 * This file decodes the run and routes each codepoint:
 *   < 0x100  -> Text() as a single Latin-1 byte, which is exactly the factory
 *               ceiling, so this device is never worse than the stock one;
 *   >= 0x100 -> consolengglyph.c, which opens this device's OWN outline engine
 *               and rasterises the codepoint (diskfont closes its engine after
 *               pre-rendering 0-255, which is the whole reason a higher
 *               codepoint cannot be asked for later).
 *
 * The placeholder is now only the LAST resort - a bitmap font, a font with no
 * outline engine, or a codepoint the face does not cover. It is kept because
 * the callers have already filled the cell background and advance the cursor
 * by CELLS: something of the right width has to go where the glyph would have
 * been. Drawing the raw UTF-8 bytes through Text() instead would emit two to
 * four mojibake characters per cell and shear the whole line.
 */

/* Decode one UTF-8 sequence, advancing *cursor. Malformed input is tolerated
   the same way the device's own decoder does: a stray continuation byte
   consumes one cell rather than desynchronising the whole run. */
static ULONG ConsoleNGUTF8Next(const UBYTE **cursor, const UBYTE *end)
{
    const UBYTE *p = *cursor;
    UBYTE        b = *p;
    ULONG        cp;

    if (b < 0x80)
    {
        *cursor = p + 1;
        return b;
    }
    if (b < 0xC0)
    {
        *cursor = p + 1;         /* unexpected continuation byte */
        return '?';
    }
    if (b < 0xE0)
    {
        if (p + 2 > end) { *cursor = end; return '?'; }
        cp = ((ULONG)(b & 0x1F) << 6) | (p[1] & 0x3F);
        *cursor = p + 2;
        return cp;
    }
    if (b < 0xF0)
    {
        if (p + 3 > end) { *cursor = end; return '?'; }
        cp = ((ULONG)(b & 0x0F) << 12) | ((ULONG)(p[1] & 0x3F) << 6) |
             (p[2] & 0x3F);
        *cursor = p + 3;
        return cp;
    }
    if (p + 4 > end) { *cursor = end; return '?'; }
    cp = ((ULONG)(b & 0x07) << 18) | ((ULONG)(p[1] & 0x3F) << 12) |
         ((ULONG)(p[2] & 0x3F) << 6) | (p[3] & 0x3F);
    *cursor = p + 4;
    return cp;
}

/* Box drawing (U+2500-257F) and block elements (U+2580-259F) are drawn here
   to the exact cell, as terminals do: from a font they come out narrower than
   the cell, so rules turn into dashes and corners do not meet. */

/* Arms of U+2500-257F: two bits each for left, right, up, down;
   0 none, 1 light, 2 heavy, 3 double. Dashes, diagonals and arcs are
   handled separately and are 0 here. */
#define BX(l, r, u, d) (((l) << 6) | ((r) << 4) | ((u) << 2) | (d))
static const UBYTE box_arms[128] =
{
    BX(1,1,0,0), BX(2,2,0,0), BX(0,0,1,1), BX(0,0,2,2), 0, 0, 0, 0,
    0, 0, 0, 0, BX(0,1,0,1), BX(0,2,0,1), BX(0,1,0,2), BX(0,2,0,2),
    BX(1,0,0,1), BX(2,0,0,1), BX(1,0,0,2), BX(2,0,0,2),
    BX(0,1,1,0), BX(0,2,1,0), BX(0,1,2,0), BX(0,2,2,0),
    BX(1,0,1,0), BX(2,0,1,0), BX(1,0,2,0), BX(2,0,2,0),
    BX(0,1,1,1), BX(0,2,1,1), BX(0,1,2,1), BX(0,1,1,2),
    BX(0,1,2,2), BX(0,2,2,1), BX(0,2,1,2), BX(0,2,2,2),
    BX(1,0,1,1), BX(2,0,1,1), BX(1,0,2,1), BX(1,0,1,2),
    BX(1,0,2,2), BX(2,0,2,1), BX(2,0,1,2), BX(2,0,2,2),
    BX(1,1,0,1), BX(2,1,0,1), BX(1,2,0,1), BX(2,2,0,1),
    BX(1,1,0,2), BX(2,1,0,2), BX(1,2,0,2), BX(2,2,0,2),
    BX(1,1,1,0), BX(2,1,1,0), BX(1,2,1,0), BX(2,2,1,0),
    BX(1,1,2,0), BX(2,1,2,0), BX(1,2,2,0), BX(2,2,2,0),
    BX(1,1,1,1), BX(2,1,1,1), BX(1,2,1,1), BX(2,2,1,1),
    BX(1,1,2,1), BX(1,1,1,2), BX(1,1,2,2), BX(2,1,2,1),
    BX(1,2,2,1), BX(2,1,1,2), BX(1,2,1,2), BX(2,2,2,1),
    BX(2,2,1,2), BX(2,1,2,2), BX(1,2,2,2), BX(2,2,2,2),
    0, 0, 0, 0,
    BX(3,3,0,0), BX(0,0,3,3), BX(0,3,0,1), BX(0,1,0,3),
    BX(0,3,0,3), BX(3,0,0,1), BX(1,0,0,3), BX(3,0,0,3),
    BX(0,3,1,0), BX(0,1,3,0), BX(0,3,3,0), BX(3,0,1,0),
    BX(1,0,3,0), BX(3,0,3,0), BX(0,3,1,1), BX(0,1,3,3),
    BX(0,3,3,3), BX(3,0,1,1), BX(1,0,3,3), BX(3,0,3,3),
    BX(3,3,0,1), BX(1,1,0,3), BX(3,3,0,3), BX(3,3,1,0),
    BX(1,1,3,0), BX(3,3,3,0), BX(3,3,1,1), BX(1,1,3,3),
    BX(3,3,3,3), BX(0,1,0,1), BX(1,0,0,1), BX(1,0,1,0),
    BX(0,1,1,0), 0, 0, 0,
    BX(1,0,0,0), BX(0,0,1,0), BX(0,1,0,0), BX(0,0,0,1),
    BX(2,0,0,0), BX(0,0,2,0), BX(0,2,0,0), BX(0,0,0,2),
    BX(1,2,0,0), BX(0,0,1,2), BX(2,1,0,0), BX(0,0,2,1),
};
#undef BX

static void cell_fill(struct Library *GfxBase, struct RastPort *rp,
    LONG x0, LONG y0, LONG x1, LONG y1)
{
    if (x1 >= x0 && y1 >= y0)
        RectFill(rp, x0, y0, x1, y1);
}

/* One arm from the cell edge to the centre (overlapping the centre line so
   the arms of a junction meet). */
static void box_arm(struct Library *GfxBase, struct RastPort *rp, WORD side,
    UBYTE weight, LONG x, LONG y, LONG w, LONG h)
{
    LONG lt = (h >= 20) ? 2 : 1;                /* light stroke */
    LONG t = (weight == 2) ? lt * 2 + 1 : lt;
    LONG cx = x + w / 2, cy = y + h / 2;
    LONG gap = lt + 1;
    LONG off[2];
    WORD lines = 1, i;

    if (weight == 3)
    {
        lines = 2;
        off[0] = -gap;
        off[1] = gap;
        t = lt;
    }
    else
        off[0] = 0;

    for (i = 0; i < lines; i++)
    {
        LONG o = off[i];
        LONG a0 = -(t / 2), a1 = a0 + t - 1;

        switch (side)
        {
        case 0:     /* left */
            cell_fill(GfxBase, rp, x, cy + o + a0, cx + (lines > 1 ? gap : t / 2),
                cy + o + a1);
            break;
        case 1:     /* right */
            cell_fill(GfxBase, rp, cx - (lines > 1 ? gap : t / 2), cy + o + a0,
                x + w - 1, cy + o + a1);
            break;
        case 2:     /* up */
            cell_fill(GfxBase, rp, cx + o + a0, y, cx + o + a1,
                cy + (lines > 1 ? gap : t / 2));
            break;
        case 3:     /* down */
            cell_fill(GfxBase, rp, cx + o + a0, cy - (lines > 1 ? gap : t / 2),
                cx + o + a1, y + h - 1);
            break;
        }
    }
}

/* n dashes along the cell, horizontal or vertical */
static void box_dashes(struct Library *GfxBase, struct RastPort *rp,
    BOOL vertical, WORD n, BOOL heavy, LONG x, LONG y, LONG w, LONG h)
{
    LONG lt = (h >= 20) ? 2 : 1;
    LONG t = heavy ? lt * 2 + 1 : lt;
    LONG len = vertical ? h : w;
    WORD i;

    for (i = 0; i < n; i++)
    {
        LONG s = len * i / n, e = len * (i + 1) / n;
        LONG gap = (e - s) / 4;

        if (gap < 1)
            gap = 1;
        if (vertical)
            cell_fill(GfxBase, rp, x + w / 2 - t / 2, y + s,
                x + w / 2 - t / 2 + t - 1, y + e - gap - 1);
        else
            cell_fill(GfxBase, rp, x + s, y + h / 2 - t / 2,
                x + e - gap - 1, y + h / 2 - t / 2 + t - 1);
    }
}

/* Unicode Braille uses dots 1,2,3,7 down the left column and 4,5,6,8
 * down the right. Render the pattern directly, including blank U+2800,
 * so spinners do not depend on outline-font coverage. Each dot stays inside
 * its quarter-height, half-width tile even at very small font sizes. */
static void braille_pattern(struct Library *GfxBase, struct RastPort *rp,
    UBYTE pattern, LONG x, LONG y, LONG w, LONG h)
{
    static const UBYTE bits[4][2] = {{0,3}, {1,4}, {2,5}, {6,7}};
    WORD row, col;

    for (row = 0; row < 4; row++)
        for (col = 0; col < 2; col++)
        {
            LONG x0 = x + col * w / 2, x1 = x + (col + 1) * w / 2;
            LONG y0 = y + row * h / 4, y1 = y + (row + 1) * h / 4;
            LONG dx = (x1 - x0) / 4, dy = (y1 - y0) / 4;

            if (pattern & (1U << bits[row][col]))
                cell_fill(GfxBase, rp, x0 + dx, y0 + dy,
                    x1 - dx - 1, y1 - dy - 1);
        }
}

static BOOL builtin_glyph(struct Library *GfxBase, struct RastPort *rp,
    ULONG cp)
{
    struct TextFont *tf = rp->Font;
    LONG w = tf->tf_XSize, h = tf->tf_YSize;
    LONG x = rp->cp_x, y = rp->cp_y - tf->tf_Baseline;
    UBYTE oldmode = rp->DrawMode;

    /* These spacing characters intentionally have no ink. Outline engines
     * may report their empty bitmap as an unknown glyph; never turn that
     * into a missing-glyph box. The caller paints the cell background and
     * handles wide-cell advance; decorations are applied after this helper.
     * Ogham space (U+1680) is excluded because its glyph can contain a line. */
    if (w > 0 && ((cp >= 0x2000 && cp <= 0x200a) ||
        cp == 0x202f || cp == 0x205f || cp == 0x3000)) {
        Move(rp, x + w, rp->cp_y);
        return TRUE;
    }

    if (((cp < 0x2500 || cp > 0x259F) && (cp < 0x2800 || cp > 0x28FF))
        || w < 2 || h < 2)
        return FALSE;

    SetDrMd(rp, JAM1);

    if (cp >= 0x2800)
        braille_pattern(GfxBase, rp, (UBYTE)(cp - 0x2800), x, y, w, h);
    else if (cp < 0x2580)
    {
        UBYTE arms = box_arms[cp - 0x2500];
        LONG lt = (h >= 20) ? 2 : 1;

        if (arms)
        {
            WORD side;

            for (side = 0; side < 4; side++)
            {
                UBYTE weight = (arms >> (6 - side * 2)) & 3;

                if (weight)
                    box_arm(GfxBase, rp, side, weight, x, y, w, h);
            }
        }
        else if (cp >= 0x2504 && cp <= 0x250B)
            box_dashes(GfxBase, rp, (cp & 2) != 0, cp < 0x2508 ? 3 : 4,
                cp & 1, x, y, w, h);
        else if (cp >= 0x254C && cp <= 0x254F)
            box_dashes(GfxBase, rp, (cp & 2) != 0, 2, cp & 1, x, y, w, h);
        else if (cp >= 0x2571 && cp <= 0x2573)
        {
            LONG i;

            for (i = 0; i < lt; i++)
            {
                if (cp != 0x2572)
                {
                    Move(rp, x + i, y + h - 1);
                    Draw(rp, x + w - 1 + i, y);
                }
                if (cp != 0x2571)
                {
                    Move(rp, x + i, y);
                    Draw(rp, x + w - 1 + i, y + h - 1);
                }
            }
        }
    }
    else if (cp == 0x2580)
        cell_fill(GfxBase, rp, x, y, x + w - 1, y + h / 2 - 1);
    else if (cp >= 0x2581 && cp <= 0x2588)
        cell_fill(GfxBase, rp, x, y + h - h * (cp - 0x2580) / 8,
            x + w - 1, y + h - 1);
    else if (cp >= 0x2589 && cp <= 0x258F)
        cell_fill(GfxBase, rp, x, y, x + w * (0x2590 - cp) / 8 - 1,
            y + h - 1);
    else if (cp == 0x2590)
        cell_fill(GfxBase, rp, x + w / 2, y, x + w - 1, y + h - 1);
    else if (cp >= 0x2591 && cp <= 0x2593)
    {
        static UWORD shade[3][2] =
        {
            { 0x8888, 0x2222 }, { 0xAAAA, 0x5555 }, { 0x7777, 0xDDDD }
        };

        SetAfPt(rp, shade[cp - 0x2591], 1);
        cell_fill(GfxBase, rp, x, y, x + w - 1, y + h - 1);
        SetAfPt(rp, NULL, 0);
    }
    else if (cp == 0x2594)
        cell_fill(GfxBase, rp, x, y, x + w - 1, y + h / 8 - 1);
    else if (cp == 0x2595)
        cell_fill(GfxBase, rp, x + w - w / 8, y, x + w - 1, y + h - 1);
    else
    {
        /* quadrants U+2596-259F: UL 1, UR 2, LL 4, LR 8 */
        static const UBYTE quad[10] =
            { 4, 8, 1, 1 | 4 | 8, 1 | 8, 1 | 2 | 4, 1 | 2 | 8, 2, 2 | 4,
              2 | 4 | 8 };
        UBYTE q = quad[cp - 0x2596];
        LONG mx = x + w / 2, my = y + h / 2;

        if (q & 1)
            cell_fill(GfxBase, rp, x, y, mx - 1, my - 1);
        if (q & 2)
            cell_fill(GfxBase, rp, mx, y, x + w - 1, my - 1);
        if (q & 4)
            cell_fill(GfxBase, rp, x, my, mx - 1, y + h - 1);
        if (q & 8)
            cell_fill(GfxBase, rp, mx, my, x + w - 1, y + h - 1);
    }

    SetDrMd(rp, oldmode);
    Move(rp, x + w, rp->cp_y);
    return TRUE;
}

/* Cell-bounded decoration shared by immediate output and retained redraws.
 * Explicit logical pens avoid inheriting INVERSVID or a Unicode background pen.
 * Never decorate concealed text; preserve every raster state we modify. */
void ConsoleNGStrike(struct Library *GfxBase, struct RastPort *rp,
    UBYTE flags, LONG x, LONG y, LONG width, LONG height, UBYTE fg, UBYTE bg)
{
    UBYTE pen, mode;
    if (!rp || !(flags & (CON_TXTFLAGS_STRUCK | CON_TXTFLAGS_DOUBLEUNDERLINED)) ||
        (flags & CON_TXTFLAGS_CONCEALED) || width <= 0 || height <= 0)
        return;
    pen = rp->FgPen;
    mode = rp->DrawMode;
    SetDrMd(rp, JAM1);
    SetAPen(rp, (flags & CON_TXTFLAGS_REVERSED) ? bg : fg);
    if (flags & CON_TXTFLAGS_STRUCK)
        RectFill(rp, x, y + (height - 1) / 2,
            x + width - 1, y + (height - 1) / 2);
    if (flags & CON_TXTFLAGS_DOUBLEUNDERLINED)
    {
        LONG upper = y + (height > 2 ? height - 3 : 0);
        LONG lower = y + height - 1;
        RectFill(rp, x, upper, x + width - 1, upper);
        if (lower != upper)
            RectFill(rp, x, lower, x + width - 1, lower);
    }
    SetAPen(rp, pen);
    SetDrMd(rp, mode);
}

/* The local Unicode paths bypass graphics/Text(), including its underline
 * raster. Paint the same baseline-relative stroke after every Unicode glyph,
 * across its full cell width. Callers already resolve reverse/conceal colors. */
static void ConsolengUnderline(struct Library *GfxBase, struct RastPort *rp,
                               ULONG cp, LONG x, LONG baseline)
{
    if (!rp || !rp->Font ||
        !((rp->AlgoStyle | rp->Font->tf_Style) & FSF_UNDERLINED)) return;
    LONG cells=con_cp_width(cp), h=rp->Font->tf_YSize;
    LONG width=rp->Font->tf_XSize*cells;
    LONG row=rp->Font->tf_Baseline+1;
    if (cells<=0 || width<=0 || h<=0) return;
    if (row<h-1) row++;
    if (row>=h) return;
    UBYTE mode=rp->DrawMode;
    SetDrMd(rp,JAM1);
    LONG y=baseline-rp->Font->tf_Baseline+row;
    RectFill(rp,x,y,x+width-1,y);
    SetDrMd(rp,mode);
}

/* A neutral missing-glyph box, independent of font coverage. Diagnostic return
 * codes stay internal; drawing them as letters changes the apparent text. */
static void ConsolengMissingGlyph(struct Library *GfxBase, struct RastPort *rp,
                                  ULONG cp)
{
    LONG w, h, x0, y0, x1, y1;
    UBYTE mode;
    WORD cells;
    if (!rp || !rp->Font) return;
    cells = con_cp_width(cp);
    if (cells <= 0 || !rp->Font->tf_XSize || !rp->Font->tf_YSize) return;
    w = (LONG)rp->Font->tf_XSize * cells;
    h = rp->Font->tf_YSize;
    x0 = rp->cp_x + (w > 2 ? 1 : 0);
    y0 = rp->cp_y - rp->Font->tf_Baseline + (h > 2 ? 1 : 0);
    x1 = rp->cp_x + w - 1 - (w > 2 ? 1 : 0);
    y1 = rp->cp_y - rp->Font->tf_Baseline + h - 1 - (h > 2 ? 1 : 0);
    mode = rp->DrawMode;
    SetDrMd(rp, JAM1);
    RectFill(rp, x0, y0, x1, y0);
    RectFill(rp, x0, y1, x1, y1);
    RectFill(rp, x0, y0, x0, y1);
    RectFill(rp, x1, y0, x1, y1);
    SetDrMd(rp, mode);
}

void ConsoleNGTextUTF8(struct Library *GfxBase,
                       struct ConsoleBase *ConsoleDevice,
                       struct RastPort *rp,
                       CONST_STRPTR string, ULONG byteLen, BOOL emoji)
{
    const UBYTE *p   = (const UBYTE *)string;
    const UBYTE *end = p + byteLen;

    if (!rp || !string || !byteLen)
        return;

    while (p < end)
    {
        const UBYTE *prev = p;
        ULONG        cp   = ConsoleNGUTF8Next(&p, end);
        LONG glyphX=rp->cp_x, glyphY=rp->cp_y;

        if (cp < 0x100)
        {
            /* Defensive Latin-1 handling for callers of this helper. The
             * immediate and retained console paths now route these cells
             * directly through native Text() with their normal raster style.
             * diskfont pre-renders glyphs 0-255, so these characters need no
             * live outline engine and must not become fallback boxes. */
            UBYTE byte = (UBYTE)cp;

            Text(rp, (CONST_STRPTR)&byte, 1);
        }
        else if (builtin_glyph(GfxBase, rp, cp))
        {
        }
        else
        {
            /* Above Latin-1: ask this device's own GlyphEngine, which exists
               precisely because diskfont closes its own at the end of
               OTAG_ReadOutlineFont (bullet.c:1048). The pen position has
               already been set by the caller, so hand the glyph path the
               current position explicitly and advance by one cell.

               Fall back to the placeholder only when there really is no glyph
               to be had - a bitmap font, a font with no outline engine, or a
               codepoint the face does not cover. */
            LONG why = ConsolengGlyphDraw(GfxBase, ConsoleDevice, rp, cp,
                                          rp->cp_x, rp->cp_y, emoji);

            if (why != CONSOLENG_GLYPH_OK)
                ConsolengMissingGlyph(GfxBase, rp, cp);
            /* The callers account for wide cells. Both glyph paths leave the
             * RastPort pen fixed, matching the original one-character step. */
            Move(rp, rp->cp_x + rp->Font->tf_XSize, rp->cp_y);
        }

        if (cp>=0x100)
            ConsolengUnderline(GfxBase,rp,cp,glyphX,glyphY);

        if (p <= prev)
            break;               /* never loop on a zero-width step */
    }
}

/* Private SGR 4:3/4:4/4:5 decoration, bounded to the cell rectangle.
 * Screen-relative phase makes separately written and retained runs agree. */
void ConsoleNGStyledUnderline(struct Library *GfxBase, struct RastPort *rp,
    UBYTE flags, LONG x, LONG y, LONG width, LONG height,
    UBYTE fg, UBYTE bg, UBYTE style)
{
    UBYTE pen, mode;
    LONG offset, period;
    if (!rp || !(flags & CON_TXTFLAGS_UNDERLINED) ||
        (flags & CON_TXTFLAGS_CONCEALED) || width <= 0 || height <= 0 ||
        style < 1 || style > 3)
        return;
    pen = rp->FgPen;
    mode = rp->DrawMode;
    SetDrMd(rp, JAM1);
    SetAPen(rp, (flags & CON_TXTFLAGS_REVERSED) ? bg : fg);
    period = style == 1 ? 4 : style == 2 ? 2 : 6;
    for (offset = 0; offset < width; offset++)
    {
        LONG phase = ((x % period) + (offset % period) + period) % period;
        LONG row = y + height - 1;
        if (style == 1)
        {
            LONG rise = phase == 3 ? 1 : phase;
            if (rise >= height) rise = height - 1;
            row -= rise;
        }
        else if ((style == 2 && phase != 0) || (style == 3 && phase >= 4))
            continue;
        RectFill(rp, x + offset, row, x + offset, row);
    }
    SetAPen(rp, pen);
    SetDrMd(rp, mode);
}
