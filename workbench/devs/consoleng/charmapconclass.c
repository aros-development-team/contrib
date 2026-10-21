/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc: Code for CONU_CHARMAP console units.
*/

#define SDEBUG 0
//#define DEBUG 1
#define DEBUG 0
#include <aros/debug.h>

#include <proto/utility.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/alib.h>

#include <exec/ports.h>
#include <intuition/intuition.h>
#include <intuition/imageclass.h>
#include <intuition/gadgetclass.h>
#include <intuition/sghooks.h>
#include <libraries/gadtools.h>
#include <graphics/rastport.h>
#include <aros/asmcall.h>

#include <string.h>
#include <stdlib.h>

#include "console_gcc.h"
#include "consoleif.h"
#include "search_regex.h"

#include "charmap.h"
#include "search.h"
#include "navigation.h"
#include "search_origin.h"
#include "search_seed.h"
#include "search_anchor.h"
#include "copy_encode.h"
#include <proto/keymap.h>
#include <devices/rawkeycodes.h>

#undef ConsoleDevice
#define ConsoleDevice ((struct ConsoleBase *)cl->cl_UserData)

#define PROP_FLAGS \
        AUTOKNOB | FREEVERT | PROPNEWLOOK | PROPBORDERLESS

struct Scroll
{
    struct Gadget scroller;     /* proportionnal gadget */
    struct Gadget down;         /* down gadget */
    struct Gadget up;           /* up gadget */
    struct PropInfo pinfo;      /* PropInfo for scroller */
    struct Image simage;        /* image for scroller */
    struct Image *upimage;      /* Boopsi image for up arrow */
    struct Image *downimage;    /* ditto for down arrow */
};

/* Search UI state is private to one retained unit and allocated on demand. */
struct charmap_search_ui
{
    struct con_search_selection selection;
    struct con_regex_selection regex_selection;
    BOOL regex_mode;
    struct con_search_match match;
    const struct charmap_line *seek, *view;
    ULONG query[CON_SEARCH_MAX], length, row, first_row, last_row, view_row;
    ULONG history[16][CON_SEARCH_MAX], history_length[16], history_count, history_at;
    ULONG draft[CON_SEARCH_MAX], draft_length;
    UBYTE swallowed[16];
    UQUAD revision;
    BOOL active, scanning, locating, found;
    UBYTE *saved_title;
    const char *paste_status; /* survives background search progress */
    char title[384];
    BOOL reverse_search;
    struct con_nav nav, nav_origin, copy_nav;
    BOOL nav_copy, nav_copy_clear;
    struct con_copy_encode copy_encoder;
    UBYTE *copy_buffer;
    ULONG copy_size,copy_phase;
    struct con_search_origin origin_search;
    struct con_search_seed seed;
    BOOL nav_seed, seed_reverse, nav_anchor;
    struct con_search_anchor seed_anchor;
    const char *nav_notice;
    const struct charmap_line *nav_origin_view;
    ULONG nav_origin_view_row;
    BOOL nav_query, nav_search, nav_confirm, nav_query_miss;
    BOOL nav_mode, nav_dirty, nav_view, nav_find_pending;
    ULONG nav_view_row;
};

// FIXME: Abstract out the non-GUI aspects
struct charmapcondata
{
    struct charmap_search_ui *search;
    UBYTE *prepared_copy;
    ULONG prepared_copy_size;
    struct con_link_pool link_pool;
    struct con_link *current_link, *saved_link, *primary_link;

    /* Primary buffer retained while DEC 1049 alternate screen is active. */
    BOOL alternate;
    struct charmap_line *primary_head, *primary_window;
    ULONG primary_size, primary_pos, primary_max;
    WORD primary_x, primary_y;
    ULONG primary_width;
    BOOL primary_pending, primary_origin;
    BYTE primary_fg, primary_bg;
    ULONG primary_fgColor, primary_bgColor, primary_underlineColor;
    UBYTE primary_tx, primary_charsets, primary_underline;

    /* Start of the scrollback buffer */
    struct charmap_line *top_of_scrollback;
    /* The line currently displayed at the top of the screen */
    struct charmap_line *top_of_window;
    /* Saved position for the top of the screen at the end of
       the buffer; where the buffer is reset to if there is
       output while scrolling */
    struct charmap_line *saved_top_of_window;
    ULONG saved_scrollback_pos;

    ULONG scrollback_size;      /* Total size of the scrollback buffer */
    ULONG scrollback_pos;       /* Position of the top of the window */

    ULONG scrollback_max;       /* Maximum number of lines in scrollback
                                   buffer on top of CHAR_YMAX(o) */

    BOOL unrendered;            /* Unrendered cursor while scrolled back? */

    /* FIXME: Belongs in snipmap class */
    /* Current selection */
    LONG select_x_min;
    LONG select_y_min;
    struct charmap_line *select_line_min;
    LONG select_x_max;
    LONG select_y_max;
    struct charmap_line *select_line_max;
    BOOL active_selection;      /* If true, mouse move will affect the selection */
    BOOL selection_block;
    BOOL selection_exact_end; /* Search endpoint excludes an unmatched final newline. */
    BOOL ignore_drag;


    UBYTE boopsigad;            /* Type of right prop gadget of window */
    struct Scroll *prop;

    /* Active gadget */
    APTR activeGad;

    /* Libraries */
    struct Library *ccd_GfxBase;
};

static VOID charmap_search_close(Class *cl, Object *o, BOOL redraw);
static BOOL charmap_history_clear_action(Class *cl, Object *o);
static BOOL charmap_selection_clear_action(Class *cl, Object *o);
static VOID charmapcon_clear_selection(Class *cl, Object *o);
static VOID charmapcon_refresh_lines(Class *cl, Object *o, LONG fromLine, LONG toLine);


/* Template used to quickly fill constant fields */
CONST struct Scroll ScrollBar = {
    {
        /* PropGadget */
        NULL, 0, 0, 0, 0,
        GFLG_RELRIGHT | GFLG_RELHEIGHT,
        GACT_RIGHTBORDER | GACT_FOLLOWMOUSE | GACT_IMMEDIATE |
        GACT_RELVERIFY,
        GTYP_PROPGADGET,
        NULL, NULL, NULL, 0, NULL,
        0, NULL
    },
    {
        /* Up-gadget */
        NULL, 0, 0, 0, 0,
        GFLG_RELRIGHT | GFLG_RELBOTTOM | GFLG_GADGHIMAGE |
        GFLG_GADGIMAGE,
        GACT_RIGHTBORDER | GACT_RELVERIFY | GACT_IMMEDIATE,
        GTYP_BOOLGADGET,
        NULL, NULL, NULL, 0, NULL,
        1, NULL
    },
    {
        /* Down-Gadget */
        NULL, 0, 0, 0, 0,
        GFLG_RELRIGHT | GFLG_RELBOTTOM | GFLG_GADGHIMAGE |
        GFLG_GADGIMAGE,
        GACT_RIGHTBORDER | GACT_RELVERIFY | GACT_IMMEDIATE,
        GTYP_BOOLGADGET,
        NULL, NULL, NULL, 0, NULL,
        2, NULL
    },
    {
        /* PropInfo */
        PROP_FLAGS,
        MAXPOT, MAXPOT,
        MAXBODY, MAXBODY
    }
    /* Other values may be NULL */
};


#ifdef GfxBase
#undef GfxBase
#endif
//#define GfxBase (((struct charmapcondata *)INST_DATA(cl, o))->ccd_GfxBase)

static VOID charmapcon_refresh(Class *cl, Object *o, LONG off);

/*** Allocate and attach a prop gadget to the window ***/
static VOID charmapcon_add_prop(Class *cl, Object *o)
{
    struct charmapcondata *data = INST_DATA(cl, o);
    struct Scroll *pg;
    struct Image *dummy;
    struct Window *win = CU(o)->cu_Window;

    UWORD height, size_width, size_height;

    /* If the window is a backdrop'ed one, use a simplified BOOPSI propgadget
     * because the next propgadget aspect depends on window activated state */
    if (win->Flags & WFLG_BACKDROP)
    {
        /* Yes this is actually a (struct Gadget *)... */
        if ((data->prop = pg =
                (struct Scroll *)NewObject(NULL, "propgclass", GA_Top, 0,
                    GA_Left, win->Width - 10, GA_Width, 10, GA_Height,
                    win->Height, GA_RelVerify, TRUE, GA_FollowMouse, TRUE,
                    GA_Immediate, TRUE, PGA_VertPot, MAXPOT, PGA_VertBody,
                    MAXBODY, PGA_Freedom, FREEVERT, PGA_NewLook, TRUE,
                    TAG_END)))
        {
            /* And finally, add it to the window */
            AddGList(win, (struct Gadget *)pg, 0, 1, NULL);
            RefreshGList((struct Gadget *)pg, win, NULL, 1);
        }

        data->boopsigad = TRUE;
        return;
    }
    data->boopsigad = FALSE;

    /* Get memory */
    if ((data->prop = pg = (void *)AllocMem(sizeof(*pg), MEMF_PUBLIC)))
    {
        /* Copy default flags/modes/etc. */
        CopyMem(&ScrollBar, pg, sizeof(*pg));
        pg->pinfo.Flags = PROP_FLAGS;
        struct DrawInfo *di;

        di = (void *)GetScreenDrawInfo(win->WScreen);

        /* We need to get size-gadget height, to adjust properly arrows */
        if ((dummy = (struct Image *)NewObject(NULL, "sysiclass",
                    SYSIA_Which, SIZEIMAGE,
                    SYSIA_DrawInfo, (IPTR) di, TAG_END)))
        {
            size_width = dummy->Width;  /* width of up/down-gadgets */
            size_height = dummy->Height;        /* bottom offset */

            /* We don't need the image anymore */
            DisposeObject(dummy);

            /* Get the boopsi image of the up and down arrow */
            if ((pg->upimage = (struct Image *)NewObject(NULL, "sysiclass",
                        SYSIA_Which, UPIMAGE,
                        SYSIA_DrawInfo, (IPTR) di, TAG_END)))
            {
                pg->up.GadgetRender = pg->up.SelectRender =
                    (APTR) pg->upimage;
                height = pg->upimage->Height;

                if ((pg->downimage =
                        (struct Image *)NewObject(NULL, "sysiclass",
                            SYSIA_Which, DOWNIMAGE, SYSIA_DrawInfo,
                            (IPTR) di, TAG_END)))
                {
                    struct Gadget *G = (void *)pg;
                    WORD hoffset = size_width / 4;

                    pg->down.GadgetRender = pg->down.SelectRender =
                        (APTR) pg->downimage;

                    /* Release drawinfo */
                    FreeScreenDrawInfo(win->WScreen, di);

                    /* Now init all sizes/positions relative to window's
                     * borders */
                    G->Height =
                        -(win->BorderTop + size_height + 2 * height + 2);
                    G->TopEdge = win->BorderTop + 1;
                    G->Width = size_width - hoffset * 2 + 2;
                    G->LeftEdge = -(size_width - hoffset);
                    G++;
                    pg->up.LeftEdge = G->LeftEdge = -(size_width - 1);
                    G->Width = pg->up.Width = size_width;
                    G->Height = pg->up.Height = height;
                    G->TopEdge = -(size_height + height - 1);
                    pg->up.TopEdge = G->TopEdge - height;

                    /* Other fields */
                    pg->scroller.GadgetRender = (APTR) &pg->simage;
                    pg->scroller.SpecialInfo = (APTR) &pg->pinfo;

                    /* Link gadgets */
                    pg->scroller.NextGadget = &pg->up;
                    pg->up.NextGadget = &pg->down;

                    /* And finally, add them to the window */
                    AddGList(win, &pg->scroller, 0, 3, NULL);
                    RefreshGList(&pg->scroller, win, NULL, 3);

                    return;
                }
                DisposeObject(pg->upimage);
            }
        }
        FreeMem(pg, sizeof(*pg));
        FreeScreenDrawInfo(win->WScreen, di);
    }
    return;
}

/* Blank trailing rows need not have allocated charmap lines. The live
 * viewport, not the allocated tail, defines the scrollbar's endpoint. */
static ULONG charmap_history_limit(struct charmapcondata *data)
{
    return data->unrendered ? data->saved_scrollback_pos : data->scrollback_pos;
}

static VOID charmapcon_adj_prop(Class *cl, Object *o)
{
    struct charmapcondata *data = INST_DATA(cl, o);

    /* background tabs must not touch the shared window's gadgetry */
    if (CON_IS_OBSCURED(o))
        return;
    struct Window *w = CU(o)->cu_Window;
    ULONG VertBody, VertPot;

    ULONG hidden = charmap_history_limit(data);

    if (hidden > 0)
    {
        VertPot = (data->scrollback_pos) * MAXPOT / hidden;
        VertBody = (CHAR_YMAX(o) + 1) * MAXBODY /
            (hidden + CHAR_YMAX(o) + 1);
    }
    else
    {
        VertPot = 0;
        VertBody = MAXBODY;
    }

    if (VertPot > MAXPOT)
    {
        VertPot = MAXPOT;
        D(bug("VERTPOT SET TOO HIGH. Adjusted\n"));
    }

    NewModifyProp((struct Gadget *)&(data->prop->scroller), w, NULL,
        ((struct PropInfo *)data->prop->scroller.SpecialInfo)->Flags,
        MAXPOT, VertPot, MAXBODY, VertBody, 1);
}

/*** Free resources allocated for scroller ***/
void charmapcon_free_prop(Class *cl, Object *o)
{
    struct charmapcondata *data = INST_DATA(cl, o);
    struct Window *win = CU(o)->cu_Window;

    if (data->prop)
    {
        if (win)
        {
            if (data->boopsigad)
                RemoveGadget(win, (struct Gadget *)data->prop);
            else
                RemoveGList(win, &data->prop->scroller, 3);
        }
        if (data->boopsigad)
            DisposeObject(data->prop);
        else
        {
            /* Free elements */
            DisposeObject(data->prop->upimage);
            DisposeObject(data->prop->downimage);

            /* Free struct */
            FreeMem(data->prop, sizeof(*data->prop));
        }
    }
}



/* Exclusive right edge of the console area, excluding every scroller
 * gadget (the arrow buttons can be wider than the track). */
static LONG charmapcon_content_right(Class *cl, Object *o)
{
    struct charmapcondata *data = INST_DATA(cl, o);
    struct Window *win = CU(o)->cu_Window;
    LONG origin = (win->Flags & WFLG_GIMMEZEROZERO) ? win->BorderLeft : 0;
    LONG right = win->Width - win->BorderRight - origin;
    if (data->prop)
    {
        struct Gadget *gadgets[3];
        WORD i, count = data->boopsigad ? 1 : 3;
        gadgets[0] = data->boopsigad ? (struct Gadget *)data->prop : &data->prop->scroller;
        if (!data->boopsigad)
        {
            gadgets[1] = &data->prop->up;
            gadgets[2] = &data->prop->down;
        }
        for (i = 0; i < count; i++)
        {
            LONG edge = gadgets[i]->LeftEdge;
            if (gadgets[i]->Flags & GFLG_RELRIGHT) edge += win->Width;
            edge -= origin;
            if (edge < right) right = edge;
        }
    }
    return right;
}

static VOID charmapcon_update_text_width(Class *cl, Object *o)
{
    LONG columns;
    if (XRSIZE <= 0) return;
    columns = (charmapcon_content_right(cl, o) - CU(o)->cu_XROrigin) / XRSIZE;
    if (columns > 0 && columns - 1 < CHAR_XMAX(o))
    {
        CU(o)->cu_XMax = columns - 1;
        CU(o)->cu_XRExtant = GFX_X(o, columns) - 1;
    }
}

/* The character grid is rounded down; paint its remaining interior pixels
 * too, without extending into the window frame or scroller controls. */
static VOID charmapcon_clear_margins(Class *cl, Object *o)
{
    struct charmapcondata *data = INST_DATA(cl, o);
    struct Window *win = CU(o)->cu_Window;
    struct RastPort *rp = win->RPort;
    struct Library *GfxBase = data->ccd_GfxBase;
    UBYTE oldpen = rp->FgPen, oldmode = rp->DrawMode;
    LONG right = charmapcon_content_right(cl, o) - 1;
    LONG bottom = win->Height - win->BorderBottom - 1;
    LONG left = GFX_XMIN(o), top = GFX_YMIN(o);
    LONG gridright = GFX_XMAX(o), gridbottom = GFX_YMAX(o);
    if (CON_IS_OBSCURED(o)) return;
    if (win->Flags & WFLG_GIMMEZEROZERO) bottom -= win->BorderTop;
    if (right < left || bottom < top) return;
    SetAPen(rp, CU(o)->cu_BgPen);
    SetDrMd(rp, JAM1);
    if (gridright < right)
        RectFill(rp, MAX(left, gridright + 1), top, right, bottom);
    if (gridbottom < bottom && gridright >= left)
        RectFill(rp, left, MAX(top, gridbottom + 1), MIN(gridright, right), bottom);
    SetAPen(rp, oldpen);
    SetDrMd(rp, oldmode);
}

/***********  CharMapCon::New()  **********************/

static Object *charmapcon_new(Class *cl, Object *o, struct opSet *msg)
{
    EnterFunc(bug("CharMapCon::New()\n"));
    APTR newGfxBase = TaggedOpenLibrary(TAGGEDOPEN_GRAPHICS);
    if (newGfxBase == NULL)
        return NULL;

    o = (Object *) DoSuperMethodA(cl, o, (Msg) msg);
    if (o)
    {
        struct charmapcondata *data = INST_DATA(cl, o);

        /* Clear for checking inside dispose() whether stuff was allocated.
           Basically this is bug-prevention.
         */
        SetMem(data, 0, sizeof(struct charmapcondata));

        data->scrollback_max = 1000;    /* FIXME: Don't hardcode it */
        data->ccd_GfxBase = newGfxBase;
        charmapcon_add_prop(cl, o);
        charmapcon_update_text_width(cl, o);

        ReturnPtr("CharMapCon::New", Object *, o);
    }

    CloseLibrary(newGfxBase);
    ReturnPtr("CharMapCon::New", Object *, NULL);

}

/***********  CharMapCon::Dispose()  **************************/

static VOID charmapcon_dispose(Class *cl, Object *o, Msg msg)
{
    struct charmapcondata *data = INST_DATA(cl, o);

    charmap_search_close(cl, o, FALSE);
    if (data->search) FreeMem(data->search, sizeof(*data->search));
    charmap_dispose_lines(data->top_of_scrollback);
    if (data->alternate)
        charmap_dispose_lines(data->primary_head);
    con_link_release(data->current_link);
    con_link_release(data->saved_link);
    con_link_release(data->primary_link);
    charmapcon_free_prop(cl, o);

    CloseLibrary(data->ccd_GfxBase);

    DoSuperMethodA(cl, o, msg);
}

/*********  CharMapCon::DoCommand()  ****************************/

static VOID charmap_trim_selection(struct charmapcondata *data)
{
        /* Preserve endpoints when the oldest retained line is removed. */
        if (data->select_line_max == data->select_line_min &&
            data->select_line_min == data->top_of_scrollback)
        {
            /* The entire selection has scrolled out, but we keep it in case
               it is still active
             */
            data->select_line_min = data->select_line_min->next;
            data->select_line_max = data->select_line_min;
            data->select_x_min = 0;
            data->select_x_max = 0;
            data->select_y_min += 1;
            data->select_y_max += 1;
        }
        else if (data->top_of_scrollback == data->select_line_min)
        {
            data->select_line_min = data->select_line_min->next;
            data->select_y_min += 1;
            data->select_x_min = 0;
        }
        else if (data->top_of_scrollback == data->select_line_max)
        {
            /* "Reversed" selection */
            data->select_line_max = data->select_line_max->next;
            data->select_y_max += 1;
            data->select_x_max = 0;
        }

}

static struct charmap_line *charmapcon_find_line(Class *cl, Object *o,
    ULONG ycp)
{
    struct charmapcondata *data = INST_DATA(cl, o);

    // Find the line. This is inefficient but the number of lines on screen
    // should never be very high.
    // FIXME: Optimizing the case of appending to the end (e.g. know what line
    // is on the last line of the buffer).

    struct charmap_line *line = data->top_of_window;
    if (!line)
    {
        D(bug("Initializing charmap\n"));
        data->top_of_window = data->top_of_scrollback = line =
            charmap_newline(0, 0);
        if (!line) return NULL;
        data->scrollback_size = 1;
    }

    D(bug("Finding line %ld\n", ycp));
    while (ycp > 0)
    {
        if (!line->next)
        {
            if (!charmap_newline(0, line)) return NULL;
            data->scrollback_size += 1;
        }
        line = line->next;
        ycp -= 1;
    }

    while (data->scrollback_size > data->scrollback_max + CHAR_YMAX(o) &&
        data->top_of_window != data->top_of_scrollback)
    {
        data->scrollback_size -= 1;
        data->scrollback_pos -= 1;
        if (data->unrendered && data->saved_scrollback_pos)
            data->saved_scrollback_pos -= 1;

        charmap_trim_selection(data);

        data->top_of_scrollback =
            charmap_dispose_line(data->top_of_scrollback);
    }

    return line;
}

/* Store a byte span into the cell map. In UTF-8 mode the span is decoded
   to codepoints (one per cell); in legacy mode each byte maps 1:1 to a
   cell (Latin-1 identity). Return TRUE when an old wide partner outside
   the written span was blanked, or styled overhang needs recomposition. */
static BOOL charmap_ascii(Class *cl, Object *o, ULONG xcp, ULONG ycp,
    char *str, ULONG len, BOOL utf8)
{
    struct charmap_line *line = charmapcon_find_line(cl, o, ycp);
    if (!line) return FALSE;
    ULONG oldsize = line->size;
    ULONG ncells = utf8 ? con_utf8_cellcount((const UBYTE *)str, len) : len;
    ncells = MIN(ncells, CHAR_XMAX(o) + 1 - xcp);
    ULONG newsize = xcp + ncells;
    ULONG i;
    BOOL repaired = con_url_in_row(line);

    /* Native bold/italic glyphs can extend into neighboring cells. A new
     * JAM2 Text span can erase those strokes even when its logical cells do
     * not overlap the previous span. Recompose the retained row, preserving
     * full glyph shapes instead of clipping them to individual cells.
     * Include the old rendition so overwriting its last styled cell also
     * removes stale overhang. Plain unstyled rows keep the immediate path. */
    if (ncells)
    {
        if (CU(o)->cu_TxFlags & (CON_TXTFLAGS_BOLD | CON_TXTFLAGS_ITALIC))
            repaired = TRUE;
        for (i = 0; !repaired && i < oldsize; i++)
            if (line->flags[i] != CHARMAP_ERASED &&
                (line->flags[i] & (CON_TXTFLAGS_BOLD | CON_TXTFLAGS_ITALIC)))
                repaired = TRUE;
    }

    /* Extending within the existing allocation needs no clearing here: all
     * newly exposed cells are initialized below, including any gap between
     * the old end and xcp. */
    if (line->size < newsize)
    {
        if (newsize <= line->capacity)
            line->size = newsize;
        else
            charmap_resize(ConsoleDevice, line, newsize);

        if (line->size < newsize)
            return FALSE;
    }

    if (newsize >= oldsize)
    {
        line->wrapColumns = 0;
        line->wrapPadding = 0;
    }

    /* Only partners outside this write span can become orphaned. Preserve
     * their rendition while blanking their character, as a wide glyph owns
     * both cells even when only one is overwritten. */
    if (ncells && xcp && xcp < oldsize &&
        line->text[xcp] == CELL_WIDE_CONT &&
        con_cp_width(line->text[xcp - 1]) == 2)
    {
        line->text[xcp - 1] = ' ';
        if (line->links) con_link_fill(line->links+xcp-1,1,NULL);
        repaired = TRUE;
    }
    if (ncells && newsize < oldsize &&
        line->text[newsize] == CELL_WIDE_CONT &&
        con_cp_width(line->text[newsize - 1]) == 2)
    {
        line->text[newsize] = ' ';
        if (line->links) con_link_fill(line->links+newsize,1,NULL);
        repaired = TRUE;
    }

    /* Failed metadata allocation renders ordinary text, never an old target. */
    struct charmapcondata *data = INST_DATA(cl, o);
    if (!charmap_links(line, xcp, ncells, data->current_link) && line->links)
        con_link_fill(line->links + xcp, ncells, NULL);

    /* A console's interactive hot path is a one-character write. */
    if (!utf8 && len == 1)
    {
        line->fgpen[xcp] = CU(o)->cu_FgPen;
        line->bgpen[xcp] = CU(o)->cu_BgPen;
        line->flags[xcp] = CU(o)->cu_TxFlags;
        line->text[xcp] = (UBYTE)str[0];
    }
    else if (utf8)
    {
        const UBYTE *p = (const UBYTE *)str;
        LONG left = len;

        SetMem(line->fgpen + xcp, CU(o)->cu_FgPen, ncells);
        SetMem(line->bgpen + xcp, CU(o)->cu_BgPen, ncells);
        SetMem(line->flags + xcp, CU(o)->cu_TxFlags, ncells);
        for (i = 0; i < ncells && left > 0; )
        {
            ULONG cp = con_utf8_decode(&p, &left);
            WORD cw = con_cp_width(cp);

            if (cw == 0)
                continue;
            line->text[xcp + i++] = cp;
            if (cw == 2 && i < ncells)
                line->text[xcp + i++] = CELL_WIDE_CONT;
        }
    }
    else
    {
        SetMem(line->fgpen + xcp, CU(o)->cu_FgPen, ncells);
        SetMem(line->bgpen + xcp, CU(o)->cu_BgPen, ncells);
        SetMem(line->flags + xcp, CU(o)->cu_TxFlags, ncells);
        for (i = 0; i < ncells; i++)
            line->text[xcp + i] = (UBYTE)str[i];
    }

    if (line->roles) for (i=xcp;i<newsize;i++) line->roles[i]=CHARMAP_ROLES(o);
    if (!charmap_underline_colors(line,xcp,ncells,ICU(o)->underlineColor)) repaired = TRUE;

    // If cursor output is moved further right on the screen than
    // the last output, we need to fill the line
    if (oldsize < xcp)
    {
        SetMem(line->fgpen + oldsize, CU(o)->cu_FgPen, xcp - oldsize);
        SetMem(line->bgpen + oldsize, CU(o)->cu_BgPen, xcp - oldsize);
        /* Cursor motion did not print these cells. Current text styles,
         * especially reverse and underline, must not decorate the gap. */
        SetMem(line->flags + oldsize, 0, xcp - oldsize);
        if (line->roles) for (i=oldsize;i<xcp;i++) line->roles[i]=CHARMAP_ROLES(o);
        if (line->underlineColors) SetMem(line->underlineColors+oldsize,0,(xcp-oldsize)*sizeof(ULONG));
        for (i = oldsize; i < xcp; i++)
            line->text[i] = ' ';
    }
    return repaired || con_url_in_row(line);
}

static VOID charmap_scroll_up(Class *cl, Object *o, ULONG y)
{
    struct charmapcondata *data = INST_DATA(cl, o);

    if (!data->top_of_window)
        return;

    while (y--)
    {
        if (!data->top_of_window->next)
        {
            if (!charmap_newline(0, data->top_of_window)) break;
            data->scrollback_size += 1;
        }
        data->top_of_window = data->top_of_window->next;
        data->scrollback_pos += 1;
        data->select_y_max -= 1;
        data->select_y_min -= 1;
    }

    if (data->unrendered &&
        data->scrollback_pos >= data->saved_scrollback_pos)
    {
        Console_RenderCursor(o);
        data->unrendered = 0;
    }

    while (data->scrollback_size > data->scrollback_max + CHAR_YMAX(o) &&
        data->top_of_window != data->top_of_scrollback)
    {
        data->scrollback_size -= 1;
        data->scrollback_pos -= 1;
        if (data->unrendered && data->saved_scrollback_pos)
            data->saved_scrollback_pos -= 1;
        charmap_trim_selection(data);
        data->top_of_scrollback =
            charmap_dispose_line(data->top_of_scrollback);
    }
}

static VOID charmap_scroll_down(Class *cl, Object *o, ULONG y)
{
    // FIXME: Need to adjust cursor position or reset to bottom when editing.
    struct charmapcondata *data = INST_DATA(cl, o);
    if (!data->unrendered)
    {
        Console_UnRenderCursor(o);
        data->unrendered = 1;
        data->saved_scrollback_pos = data->scrollback_pos;
        data->saved_top_of_window = data->top_of_window;

        // FIXME: Select position.
    }
    if (data->top_of_window)
    {
        while (y-- && data->top_of_window->prev)
        {
            data->top_of_window = data->top_of_window->prev;
            data->scrollback_pos -= 1;
            data->select_y_max += 1;
            data->select_y_min += 1;
        }
    }
}

static VOID charmapcon_scroll_to(Class *cl, Object *o, ULONG y)
{
    struct charmapcondata *data = INST_DATA(cl, o);
    struct Library *GfxBase = data->ccd_GfxBase;
    struct Window *w = CU(o)->cu_Window;
    struct RastPort *rp = w->RPort;
    LONG off = data->scrollback_pos - y;
    LONG old_pos = data->scrollback_pos;

    if (off == 0)
        return;

    Console_UnRenderCursor(o);

    if (off > 0)
        charmap_scroll_down(cl, o, off);
    else
        charmap_scroll_up(cl, o, -off);

    /* Correct offset to account for the fact we might reach the
     * top or bottom of the buffer:
     */
    off = old_pos - data->scrollback_pos;

    /* A whole screenful? If so we have no choice but a full refresh
     * (though we could double buffer... Not sure that's worth the
     * memory cost)
     */
    if (abs(off) > CHAR_YMAX(o))
    {
        charmapcon_refresh(cl, o, 0);
        Console_RenderCursor(o);
        return;
    }

    /* Avoid a full refresh by scrolling the rastport.
     * Use "standard" background to reduce flicker
     */
    SetBPen(rp, CU(o)->cu_BgPen);
    if (off > 0)
    {
        ScrollRaster(rp,
            0,
            -YRSIZE * off,
            GFX_XMIN(o), GFX_YMIN(o), GFX_XMAX(o), GFX_YMAX(o));
    }
    else
    {
        ScrollRaster(rp,
            0,
            -YRSIZE * off,
            GFX_XMIN(o), GFX_YMIN(o), GFX_XMAX(o), GFX_YMAX(o));

    }

    /* Partial refresh */
    charmapcon_refresh(cl, o, off);

    Console_RenderCursor(o);
}


/* Viewport-only shortcuts: never create history or disturb alternate screens. */
static BOOL charmap_history_action(Class *cl, Object *o, ULONG action)
{
    if(action==CONSOLENG_BIND_CLEARHISTORY)return charmap_history_clear_action(cl,o);
    if(action==CONSOLENG_BIND_CLEARSELECTION)return charmap_selection_clear_action(cl,o);
    struct charmapcondata *data = INST_DATA(cl, o);
    if ((action < CONSOLENG_BIND_LINEUP || action > CONSOLENG_BIND_BOTTOM) &&
        action != CONSOLENG_BIND_HALFUP && action != CONSOLENG_BIND_HALFDOWN)
        return FALSE;
    if (data->alternate || CON_IS_OBSCURED(o)) return TRUE;
    ULONG limit = charmap_history_limit(data), pos = data->scrollback_pos;
    ULONG step = (action == CONSOLENG_BIND_PAGEUP || action == CONSOLENG_BIND_PAGEDOWN)
        ? (ULONG)CHAR_YMAX(o) + 1 : 1;
    if (action == CONSOLENG_BIND_HALFUP || action == CONSOLENG_BIND_HALFDOWN)
        step = ((ULONG)CHAR_YMAX(o) + 1) / 2;
    switch (action) {
    case CONSOLENG_BIND_LINEUP:
    case CONSOLENG_BIND_HALFUP:
    case CONSOLENG_BIND_PAGEUP: pos = step > pos ? 0 : pos - step; break;
    case CONSOLENG_BIND_LINEDOWN:
    case CONSOLENG_BIND_HALFDOWN:
    case CONSOLENG_BIND_PAGEDOWN: pos = step > limit - pos ? limit : pos + step; break;
    case CONSOLENG_BIND_TOP: pos = 0; break;
    case CONSOLENG_BIND_BOTTOM: pos = limit; break;
    }
    charmapcon_scroll_to(cl, o, pos);
    charmapcon_adj_prop(cl, o);
    return TRUE;
}

/* Cell editing may cut a two-column glyph at either edge of the moved
 * span. Keep intact pairs and blank fragments rather than rendering a wide
 * glyph over the following independent cell. No extra cells are shifted. */
static VOID charmap_repair_wide_cells(Object *o, struct charmap_line *line,
    ULONG width)
{
    ULONG i;
    if (!CON_TEXT_IS_UTF8(o)) return;
    for (i = 0; i < width; i++)
    {
        ULONG cp = line->text[i];
        if (cp != CELL_WIDE_CONT && con_cp_width(cp) == 2)
        {
            if (i + 1 < width && line->text[i + 1] == CELL_WIDE_CONT)
            {
                i++;
                continue;
            }
        }
        else if (cp != CELL_WIDE_CONT)
            continue;
        line->text[i] = ' ';
        if (line->links) con_link_fill(line->links+i,1,NULL);
        if (line->roles) line->roles[i]=CHARMAP_ROLES(o);
        if (line->underlineColors) line->underlineColors[i] = 0;
        line->fgpen[i] = CU(o)->cu_FgPen;
        line->bgpen[i] = CU(o)->cu_BgPen;
        line->flags[i] = 0;
    }
}

static BOOL charmap_delete_char(Class *cl, Object *o, ULONG x, ULONG y,
    IPTR requested)
{
    struct charmap_line *line = charmapcon_find_line(cl, o, y);
    ULONG width = CHAR_XMAX(o) + 1;
    ULONG count, move, i, oldsize;
    if (!line || x >= width) return FALSE;
    count = requested > 0 ? requested : 1;
    if (count > width - x) count = width - x;
    oldsize = line->size;
    if (oldsize < width)
    {
        charmap_resize(ConsoleDevice, line, width);
        if (line->size < width) return FALSE;
        for (i = oldsize; i < width; i++) line->text[i] = ' ';
    }
    line->wrapColumns = 0; line->wrapPadding = 0;
    move = width - x - count;
    if (line->links) {
        if (!con_link_copy(line->links+x,line->links+x+count,move)) return FALSE;
        con_link_fill(line->links+width-count,count,NULL);
    }
    if (line->roles) memmove(line->roles+x,line->roles+x+count,move*sizeof(*line->roles));
    if (line->underlineColors) memmove(line->underlineColors+x,line->underlineColors+x+count,move*sizeof(*line->underlineColors));
    memmove(line->fgpen + x, line->fgpen + x + count, move);
    memmove(line->bgpen + x, line->bgpen + x + count, move);
    memmove(line->flags + x, line->flags + x + count, move);
    memmove(line->text + x, line->text + x + count, move * sizeof(ULONG));
    for (i = width - count; i < width; i++)
    {
        if (line->roles) line->roles[i]=CHARMAP_ROLES(o);
        if (line->underlineColors) line->underlineColors[i] = 0;
        line->fgpen[i] = CU(o)->cu_FgPen;
        line->bgpen[i] = CU(o)->cu_BgPen;
        line->flags[i] = 0;
        line->text[i] = ' ';
    }
    charmap_repair_wide_cells(o, line, width);
    return TRUE;
}

static BOOL charmap_insert_char(Class *cl, Object *o, ULONG x, ULONG y,
    IPTR requested)
{
    struct charmap_line *line = charmapcon_find_line(cl, o, y);
    ULONG width = CHAR_XMAX(o) + 1;
    ULONG count, move, i, oldsize;
    if (!line || x >= width) return FALSE;
    count = requested > 0 ? requested : 1;
    if (count > width - x) count = width - x;
    oldsize = line->size;
    if (oldsize < width)
    {
        charmap_resize(ConsoleDevice, line, width);
        if (line->size < width) return FALSE;
        /* Newly materialized blanks must keep the current palette roles,
         * not physical pen zero, before the shift makes them retained cells. */
        for (i = oldsize; i < width; i++)
        {
            line->text[i] = ' ';
            line->fgpen[i] = CU(o)->cu_FgPen;
            line->bgpen[i] = CU(o)->cu_BgPen;
            line->flags[i] = 0;
            if (line->roles) line->roles[i] = CHARMAP_ROLES(o);
            if (line->underlineColors) line->underlineColors[i] = 0;
        }
    }
    line->wrapColumns = 0; line->wrapPadding = 0;
    move = width - x - count;
    if (line->links) {
        if (!con_link_copy(line->links+x+count,line->links+x,move)) return FALSE;
        con_link_fill(line->links+x,count,NULL);
    }
    if (line->roles) memmove(line->roles+x+count,line->roles+x,move*sizeof(*line->roles));
    if (line->underlineColors) memmove(line->underlineColors+x+count,line->underlineColors+x,move*sizeof(*line->underlineColors));
    memmove(line->fgpen + x + count, line->fgpen + x, move);
    memmove(line->bgpen + x + count, line->bgpen + x, move);
    memmove(line->flags + x + count, line->flags + x, move);
    memmove(line->text + x + count, line->text + x, move * sizeof(ULONG));
    for (i = x; i < x + count; i++)
    {
        if (line->roles) line->roles[i]=CHARMAP_ROLES(o);
        if (line->underlineColors) line->underlineColors[i] = 0;
        line->fgpen[i] = CU(o)->cu_FgPen;
        line->bgpen[i] = CU(o)->cu_BgPen;
        line->flags[i] = 0;
        line->text[i] = ' ';
    }
    charmap_repair_wide_cells(o, line, width);
    return TRUE;
}

static VOID charmap_formfeed(Class *cl, Object *o)
{
    struct charmapcondata *data = INST_DATA(cl, o);
    struct charmap_line *line = data->top_of_window;

    while (line)
    {
        charmap_resize(ConsoleDevice, line, 0);
        line = line->next;
    }
}

static VOID charmap_erase_line(Class *cl, Object *o, ULONG x, ULONG y,
    IPTR mode)
{
    struct charmap_line *line = charmapcon_find_line(cl, o, y);
    ULONG width = CHAR_XMAX(o) + 1;
    ULONG first, end, oldsize, i;
    if (!line || x >= width) return;
    first = mode == 1 || mode == 2 ? 0 : x;
    end = mode == 1 ? x + 1 : width;
    oldsize = line->size;
    /* Erasure has a background rendition even when no text remains. Retain
     * blank cells so subsequent SGR changes, scrolling and reflow cannot
     * recolor them. Allocate before changing the old row. */
    if (oldsize < width)
    {
        charmap_resize(ConsoleDevice, line, width);
        if (line->size < width) return;
        for (i = oldsize; i < width; i++)
        {
            line->text[i] = ' ';
            line->fgpen[i] = CU(o)->cu_FgPen;
            line->bgpen[i] = CU(o)->cu_BgPen;
            line->flags[i] = CHARMAP_ERASED;
            if (line->roles) line->roles[i] = CHARMAP_ROLES(o);
            if (line->underlineColors) line->underlineColors[i] = 0;
        }
    }
    line->wrapColumns = 0; line->wrapPadding = 0;
    if (line->links) con_link_fill(line->links+first,end-first,NULL);
    for (i = first; i < end; i++)
    {
        line->text[i] = ' ';
        line->fgpen[i] = CU(o)->cu_FgPen;
        line->bgpen[i] = CU(o)->cu_BgPen;
        line->flags[i] = CHARMAP_ERASED;
        if (line->roles) line->roles[i] = CHARMAP_ROLES(o);
        if (line->underlineColors) line->underlineColors[i] = 0;
    }
    charmap_repair_wide_cells(o, line, width);
}

/* DoCommand restores the live viewport before handling output. ED 3 removes
 * only the linked history preceding it; visible cells and cursor stay put. */
static VOID charmap_clear_history(Class *cl, Object *o)
{
    struct charmapcondata *data = INST_DATA(cl, o);
    if (!data->top_of_window || data->top_of_scrollback == data->top_of_window)
        return;

    struct charmap_line *line;
    BOOL discard_selection = FALSE;
    for (line = data->top_of_scrollback;
         line && line != data->top_of_window; line = line->next)
    {
        if (line == data->select_line_min || line == data->select_line_max)
        {
            discard_selection = TRUE;
            break;
        }
    }
    if (discard_selection)
    {
        if (!CON_IS_OBSCURED(o)) charmapcon_clear_selection(cl, o);
        data->select_line_min = data->select_line_max = data->top_of_window;
        data->select_x_min = data->select_x_max = 0;
        data->select_y_min = data->select_y_max = 0;
        data->active_selection = FALSE;
        data->ignore_drag = FALSE;
    }

    while (data->top_of_scrollback &&
           data->top_of_scrollback != data->top_of_window)
    {
        data->top_of_scrollback = charmap_dispose_line(data->top_of_scrollback);
        if (data->scrollback_size) data->scrollback_size--;
    }
    data->top_of_window->prev = NULL;
    data->scrollback_pos = data->saved_scrollback_pos = 0;
    data->saved_top_of_window = data->top_of_window;
}

static VOID charmap_erase_display(Class *cl, Object *o, ULONG x, ULONG y,
    IPTR mode)
{
    ULONG row;

    switch (mode)
    {
    case 1:
        for (row = CHAR_YMIN(o); row < y; row++)
            charmap_erase_line(cl, o, 0, row, 2);
        charmap_erase_line(cl, o, x, y, 1);
        break;
    case 2:
        for (row = CHAR_YMIN(o); row <= (ULONG) CHAR_YMAX(o); row++)
            charmap_erase_line(cl, o, 0, row, 2);
        break;
    case 3:
        charmap_clear_history(cl, o);
        break;
    case 0:
    default:
        charmap_erase_line(cl, o, x, y, 0);
        for (row = y + 1; row <= (ULONG) CHAR_YMAX(o); row++)
            charmap_erase_line(cl, o, 0, row, 2);
        break;
    }
}

/* The pens a dynamic colour change (OSC 4/10/11/...) can move: the default
   foreground and background, eight legacy ANSI slots, xterm 0-15 and
   distinct named bright roles (which share the bright palette pens) */
#define CHARMAP_COLORPENS (2 + 8 + 16 + 8)

static VOID charmapcon_colorpens(Object *o, UBYTE *pens)
{
    WORD i;

    Console_GetColorPen(o, CONUNIT_PEN_DEFFG, &pens[0]);
    Console_GetColorPen(o, CONUNIT_PEN_DEFBG, &pens[1]);
    for (i = 0; i < 8; i++)
        Console_GetColorPen(o, i, &pens[2 + i]);
    for (i = 0; i < 16; i++)
        Console_GetColorPen(o, CONPEN_XTERM(i), &pens[10 + i]);
    for (i = 0; i < 8; i++)
        Console_GetColorPen(o, CONPEN_BRIGHT(i), &pens[26 + i]);
}

static VOID charmap_remap_colors(struct charmap_line *line, const UBYTE *map, const UBYTE *slots)
{
    for (; line; line = line->next)
    {
        ULONG x;
        if (!line->fgpen || !line->bgpen)
            continue;
        for (x = 0; x < line->size; x++)
        {
            if (line->roles) {
                UBYTE fg=(line->roles[x]>>8)&0xbf,bg=line->roles[x]&0xbf;
                if(fg && fg<=CHARMAP_COLORPENS) line->fgpen[x]=(BYTE)slots[fg-1];
                if(bg && bg<=CHARMAP_COLORPENS) line->bgpen[x]=(BYTE)slots[bg-1];
            } else {
                line->fgpen[x] = (BYTE)map[(UBYTE)line->fgpen[x]];
                line->bgpen[x] = (BYTE)map[(UBYTE)line->bgpen[x]];
            }
        }
    }
}

/* Cells keep logical roles alongside rendering pens. Resolve each role
   independently so shared physical pens do not couple unrelated colors. */
static VOID charmapcon_setcolor(Class *cl, Object *o,
    struct P_Console_DoCommand *msg)
{
    struct charmapcondata *data = INST_DATA(cl, o);
    UBYTE before[CHARMAP_COLORPENS], after[CHARMAP_COLORPENS];
    UBYTE map[256];
    BOOL changed = FALSE;
    ULONG revision = ICU(o)->extendedPaletteRevision;
    WORD i;

    charmapcon_colorpens(o, before);
    DoSuperMethodA(cl, o, (Msg) msg);
    charmapcon_colorpens(o, after);
    changed = revision != ICU(o)->extendedPaletteRevision;

    for (i = 0; i < 256; i++)
        map[i] = (UBYTE) i;
    for (i = 0; i < CHARMAP_COLORPENS; i++)
    {
        if (before[i] != after[i])
        {
            map[before[i]] = after[i];
            changed = TRUE;
        }
    }
    if (!changed)
        return;

    /* Resolve retained colors, including dormant primary storage and
       both saved cursor renditions. Releasing an old override must not leave
       references that reappear on DECRC or alternate-screen exit. */
    charmap_remap_colors(data->top_of_scrollback, map, after);
    if (data->alternate)
    {
        charmap_remap_colors(data->primary_head, map, after);
        Console_GetColorPen(o,data->primary_fgColor,(UBYTE *)&data->primary_fg);
        Console_GetColorPen(o,data->primary_bgColor,(UBYTE *)&data->primary_bg);
    }
    Console_GetColorPen(o,ICU(o)->fgColor,&CU(o)->cu_FgPen);
    Console_GetColorPen(o,ICU(o)->bgColor,&CU(o)->cu_BgPen);
    if (ICU(o)->savedCursor)
    {
        Console_GetColorPen(o,ICU(o)->savedFgColor,(UBYTE *)&ICU(o)->savedFgPen);
        Console_GetColorPen(o,ICU(o)->savedBgColor,(UBYTE *)&ICU(o)->savedBgPen);
    }

    if (!CON_IS_OBSCURED(o))
        charmapcon_refresh(cl, o, 0);
}

/* Cursor coordinates belong to the live screen, even while history is shown. */
static BOOL charmap_restore_live(struct charmapcondata *data)
{
    if (!data->unrendered)
        return FALSE;
    LONG old_pos = data->scrollback_pos;
    data->unrendered = 0;
    data->scrollback_pos = data->saved_scrollback_pos;
    data->select_y_min += old_pos - (LONG)data->scrollback_pos;
    data->select_y_max += old_pos - (LONG)data->scrollback_pos;
    data->top_of_window = data->saved_top_of_window;
    return TRUE;
}

/* Keyboard clearing preserves the live grid even when history is displayed.
 * Cancel every borrowed-row search before releasing any retained row. */
static BOOL charmap_history_clear_action(Class *cl, Object *o)
{
    struct charmapcondata *data = INST_DATA(cl, o);
    if(data->alternate || CON_IS_OBSCURED(o))return TRUE;
    charmap_search_close(cl,o,FALSE);
    Console_UnRenderCursor(o);
    charmap_restore_live(data);
    charmap_clear_history(cl,o);
    charmapcon_adj_prop(cl,o);
    charmapcon_refresh(cl,o,0);
    Console_RenderCursor(o);
    return TRUE;
}

/* Allocate before touching primary state, so failed entry leaves it intact. */
static BOOL charmap_reflow_primary(Class *cl, Object *o,
    ULONG oldx, ULONG oldy, BOOL pending);

static VOID charmap_alternate_screen(Class *cl, Object *o, BOOL set)
{
    struct charmapcondata *data = INST_DATA(cl, o);
    struct charmap_line *blank = NULL;
    if (!!set == !!data->alternate)
        return;
    if (set)
    {
        blank = AllocMem(sizeof(*blank), MEMF_ANY | MEMF_CLEAR);
        if (!blank)
            return;
    }
    Console_UnRenderCursor(o);
    if (!CON_IS_OBSCURED(o))
        charmapcon_clear_selection(cl, o);
    if (set)
    {
        con_link_fill(&data->primary_link, 1, data->current_link);
        data->primary_head = data->top_of_scrollback;
        data->primary_window = data->top_of_window;
        data->primary_size = data->scrollback_size;
        data->primary_pos = data->scrollback_pos;
        data->primary_max = data->scrollback_max;
        data->primary_x = XCP; data->primary_y = YCP;
        data->primary_width = CHAR_XMAX(o) + 1;
        data->primary_pending = !!(ICU(o)->conFlags & CF_WRAP_PENDING);
        data->primary_origin = !!(ICU(o)->conFlags & CF_ORIGIN_MODE);
        data->primary_fgColor=ICU(o)->fgColor;data->primary_bgColor=ICU(o)->bgColor;
        data->primary_fg = CU(o)->cu_FgPen;
        data->primary_bg = CU(o)->cu_BgPen;
        data->primary_tx = CU(o)->cu_TxFlags;
        data->primary_underline = ICU(o)->underlineStyle;
        data->primary_underlineColor = ICU(o)->underlineColor;
        data->primary_charsets = ICU(o)->charsets;
        data->top_of_scrollback = data->top_of_window = blank;
        data->scrollback_size = 1;
        data->scrollback_pos = 0;
        data->scrollback_max = 0;
        data->alternate = TRUE;
    }
    else
    {
        charmap_dispose_lines(data->top_of_scrollback);
        con_link_fill(&data->current_link, 1, data->primary_link);
        data->top_of_scrollback = data->primary_head;
        data->top_of_window = data->primary_window;
        data->scrollback_size = data->primary_size;
        data->scrollback_pos = data->primary_pos;
        data->scrollback_max = data->primary_max;
        XCCP = XCP = MIN(MAX(data->primary_x, CHAR_XMIN(o)), CHAR_XMAX(o));
        ICU(o)->conFlags &= ~CF_ORIGIN_MODE;
        if (data->primary_origin) ICU(o)->conFlags |= CF_ORIGIN_MODE;
        WORD top = data->primary_origin ? CON_SCROLL_TOP(o) : CHAR_YMIN(o);
        WORD bottom = data->primary_origin ? CON_SCROLL_BOTTOM(o) : CHAR_YMAX(o);
        YCCP = YCP = MIN(MAX(data->primary_y, top), bottom);
        ICU(o)->fgColor = data->primary_fgColor;
        ICU(o)->bgColor = data->primary_bgColor;
        Console_GetColorPen(o,data->primary_fgColor,&CU(o)->cu_FgPen);
        Console_GetColorPen(o,data->primary_bgColor,&CU(o)->cu_BgPen);
        CU(o)->cu_TxFlags = data->primary_tx;
        ICU(o)->underlineStyle = data->primary_underline;
        ICU(o)->underlineColor = data->primary_underlineColor;
        ICU(o)->charsets = data->primary_charsets;
        data->primary_head = data->primary_window = NULL;
        data->alternate = FALSE;
    }
    con_keyboard_switch(CU(o), set);
    con_link_fill(&data->saved_link, 1, data->primary_link);
    if (!set) con_link_fill(&data->primary_link, 1, NULL);
    /* The primary save slot must survive ESC 7/8 inside the alternate screen. */
    ICU(o)->savedFgColor=data->primary_fgColor;ICU(o)->savedBgColor=data->primary_bgColor;
    ICU(o)->savedX = data->primary_x; ICU(o)->savedY = data->primary_y;
    ICU(o)->savedFgPen = data->primary_fg; ICU(o)->savedBgPen = data->primary_bg;
    ICU(o)->savedUnderlineStyle = data->primary_underline;
    ICU(o)->savedUnderlineColor = data->primary_underlineColor;
    ICU(o)->savedTxFlags = data->primary_tx; ICU(o)->savedCursor = TRUE;
    ICU(o)->savedWrapPending = data->primary_pending;
    ICU(o)->savedOrigin = data->primary_origin;
    ICU(o)->savedCharsets = data->primary_charsets;
    ICU(o)->conFlags &= ~CF_WRAP_PENDING;
    data->saved_top_of_window = data->top_of_window;
    data->saved_scrollback_pos = data->scrollback_pos;
    data->select_line_min = data->select_line_max = data->top_of_window;
    data->active_selection = FALSE;
    data->ignore_drag = FALSE;
    data->select_x_min = data->select_x_max = 0;
    data->select_y_min = data->select_y_max = 0;
    if (!set)
    {
        if (data->primary_width != (ULONG)CHAR_XMAX(o) + 1)
            charmap_reflow_primary(cl, o, data->primary_x, data->primary_y,
                data->primary_pending);
        else if (data->primary_pending) ICU(o)->conFlags |= CF_WRAP_PENDING;
    }
    if (!CON_IS_OBSCURED(o))
        charmapcon_refresh(cl, o, 0);
    charmapcon_adj_prop(cl, o);
    Console_RenderCursor(o);
}

/* Limit a retained/drawn span to the current row without splitting UTF-8.
 * The caller handles wide characters which cannot fit at the margin. */
static ULONG charmap_print_span(const UBYTE *p, ULONG len, ULONG available,
    BOOL utf8)
{
    const UBYTE *start = p;
    LONG left = len;
    ULONG cells = 0;
    if (!utf8)
        return MIN(len, available);
    while (left > 0)
    {
        const UBYTE *next = p;
        LONG remaining = left;
        WORD width = con_cp_width(con_utf8_decode(&next, &remaining));
        if (cells && cells + width > available)
            break;
        p = next;
        left = remaining;
        cells += width;
        if (cells >= available)
            break;
    }
    return p - start;
}

/* Reset blanks have default color roles of their own. Later SGR must not
 * recolor them when a hidden screen is revealed or a window is resized. */
static struct charmap_line *charmap_reset_grid(Class *cl, Object *o)
{
    struct charmap_line anchor = {0}, *line;
    UBYTE fg, bg;
    Console_GetColorPen(o, CONUNIT_PEN_DEFFG, &fg);
    Console_GetColorPen(o, CONUNIT_PEN_DEFBG, &bg);
    if (!charmap_append_blanks(ConsoleDevice, &anchor, CHAR_YMAX(o) + 1,
            CHAR_XMAX(o) + 1, fg, bg, 0x0102)) return NULL;
    anchor.next->prev = NULL;
    for (line = anchor.next; line; line = line->next)
        memset(line->flags, CHARMAP_ERASED, line->size);
    return anchor.next;
}

/* Replace only live screen rows; retain history and any off-screen tail.
 * Allocate the complete replacement before altering existing links. */
static BOOL charmap_alignment(Class *cl, Object *o)
{
    struct charmapcondata *data = INST_DATA(cl, o);
    struct charmap_line *head = charmap_reset_grid(cl, o), *tail, *line;
    struct charmap_line *old = data->top_of_window, *before = old ? old->prev : NULL;
    struct charmap_line *after = old, *last = NULL;
    ULONG removed = 0, rows = CHAR_YMAX(o) + 1;
    if (!head) return FALSE;
    for (line = head; line; line = line->next)
    {
        for (ULONG x = 0; x < line->size; x++)
        {
            line->text[x] = 'E'; line->flags[x] = 0;
        }
        tail = line;
    }
    while (after && removed < rows) { last = after; after = after->next; removed++; }
    if (!CON_IS_OBSCURED(o)) charmapcon_clear_selection(cl, o);
    if (last) last->next = NULL;
    if (old) old->prev = NULL;
    if (before) before->next = head;
    else data->top_of_scrollback = head;
    head->prev = before;
    tail->next = after;
    if (after) after->prev = tail;
    data->top_of_window = data->saved_top_of_window = head;
    data->saved_scrollback_pos = data->scrollback_pos;
    data->scrollback_size += rows - removed;
    data->select_line_min = data->select_line_max = head;
    data->select_x_min = data->select_x_max = 0;
    data->select_y_min = data->select_y_max = 0;
    data->active_selection = data->ignore_drag = FALSE;
    charmap_dispose_lines(old);
    return TRUE;
}

static VOID charmapcon_docommand(Class *cl, Object *o,
    struct P_Console_DoCommand *msg)
{
    IPTR *params = msg->Params;
    struct charmapcondata *data = INST_DATA(cl, o);

    EnterFunc(bug
        ("CharMapCon::DoCommand(o=%p, cmd=%d, params=%p) x=%d, y=%d, ymax=%d\n",
            o, msg->Command, params, XCP, YCP, CHAR_YMAX(o)));

    if (msg->Command == C_TERMINAL_RESET)
    {
        charmap_search_close(cl, o, TRUE);
        /* Allocate before discarding either screen: OOM leaves the old state
         * intact. A fresh row also drops every scrollback/selection pointer. */
        struct charmap_line *blank = charmap_reset_grid(cl, o);
        if (!blank) return;
        Console_UnRenderCursor(o);
        if (data->unrendered)
        {
            data->unrendered = FALSE;
            Console_RenderCursor(o);
        }
        charmap_dispose_lines(data->top_of_scrollback);
        if (data->alternate)
        {
            charmap_dispose_lines(data->primary_head);
            data->scrollback_max = data->primary_max;
        }
        con_link_fill(&data->current_link, 1, NULL);
        con_link_fill(&data->saved_link, 1, NULL);
        con_link_fill(&data->primary_link, 1, NULL);
        data->alternate = FALSE;
        data->primary_head = data->primary_window = NULL;
        data->top_of_scrollback = data->top_of_window = blank;
        data->saved_top_of_window = blank;
        data->scrollback_size = CHAR_YMAX(o) + 1;
        data->scrollback_pos = data->saved_scrollback_pos = 0;
        data->select_line_min = data->select_line_max = blank;
        data->select_x_min = data->select_x_max = 0;
        data->select_y_min = data->select_y_max = 0;
        data->active_selection = data->ignore_drag = FALSE;
        DoSuperMethodA(cl, o, (Msg)msg);
        if (!CON_IS_OBSCURED(o)) charmapcon_clear_margins(cl, o);
        charmapcon_adj_prop(cl, o);
        Console_RenderCursor(o);
        return;
    }

    if (msg->Command == C_SET_HYPERLINK)
    {
        con_link_open(&data->link_pool, &data->current_link,
            (const UBYTE *)params[0], params[1]);
        return; /* Metadata alone does not move or repaint the viewport. */
    }
    if (msg->Command == C_SAVE_CURSOR)
        con_link_fill(&data->saved_link, 1, data->current_link);
    else if (msg->Command == C_RESTORE_CURSOR)
        con_link_fill(&data->current_link, 1,
            ICU(o)->savedCursor ? data->saved_link : NULL);

    /* Budget edits must not restore the viewport, trim the selection, or
       introduce alternate-screen history. Existing trimming handles the
       new budget on the next append to the primary screen. */
    if (msg->Command == C_GET_HISTORY_PREFS || msg->Command == C_SET_HISTORY_PREFS)
    {
        struct ConsolengHistoryPrefs *prefs = (APTR)params[0];
        ULONG *limit = data->alternate ? &data->primary_max : &data->scrollback_max;
        if (msg->Command == C_SET_HISTORY_PREFS) *limit = prefs->hp_Lines;
        else *prefs = (struct ConsolengHistoryPrefs) {
            sizeof(*prefs), CONSOLENG_HISTORY_PREFS_VERSION, *limit
        };
        params[1] = TRUE;
        return;
    }

    if (msg->Command == C_GET_EMOJI_PREFS || msg->Command == C_SET_EMOJI_PREFS)
    {
        BOOL before = ICU(o)->emojiDisabled;
        DoSuperMethodA(cl, o, (Msg)msg);
        if (before != ICU(o)->emojiDisabled && !CON_IS_OBSCURED(o))
            charmapcon_refresh(cl, o, 0);
        return; /* Preserve a browsed viewport, selection and cursor. */
    }

    /* OSC palette queries/changes are not new text output. Keep a browsed
       viewport in place and repaint that viewport only when pens changed. */
    if (msg->Command == C_SET_COLOR || msg->Command == C_SET_COLOR_PREFS)
    {
        charmapcon_setcolor(cl, o, msg);
        return;
    }

    if (msg->Command == C_GET_MODE_STATE)
    {
        if (params[0] && params[1] == 1049)
            *(ULONG *)params[2] = data->alternate ? 1 : 2;
        else DoSuperMethodA(cl, o, (Msg)msg);
        return; /* Queries must not restore or redraw a browsed viewport. */
    }

    /* A consumed query/unsupported escape is not output. */
    if (msg->Command == C_NIL) return;

    /* A preferences query/edit must not move a browsed history viewport. */
    if (msg->Command == C_GET_CURSOR_PREFS || msg->Command == C_SET_CURSOR_PREFS ||
        msg->Command == C_GET_COLOR_PREFS || msg->Command == C_SET_CURSOR_SHAPE ||
        msg->Command == C_GET_CURSOR_COLOR_PREFS || msg->Command == C_SET_CURSOR_COLOR_PREFS)
    {
        DoSuperMethodA(cl, o, (Msg)msg);
        return;
    }

    charmap_search_close(cl, o, TRUE);

    // This is a bit of a hack: Set position to bottom in order to prevent
    // output while scrolled.

    ULONG old_scrollback_size = data->scrollback_size;
    ULONG old_scrollback_pos = data->scrollback_pos;

    if (charmap_restore_live(data))
    {
        charmapcon_refresh(cl, o, 0);
        Console_RenderCursor(o);
    }

    switch (msg->Command)
    {
    case C_SCREEN_ALIGNMENT:
        Console_UnRenderCursor(o);
        if (charmap_alignment(cl, o))
        {
            if (!CON_IS_OBSCURED(o)) charmapcon_refresh(cl, o, 0);
            charmapcon_adj_prop(cl, o);
        }
        Console_RenderCursor(o);
        break;
    case C_ALTERNATE_SCREEN:
        charmap_alternate_screen(cl, o, params[0] != 0);
        break;
    case C_ASCII:
        {
            BOOL repaired;
            BOOL insert = !!(ICU(o)->conFlags & CF_INSERT_MODE);
            struct P_Console_DoCommand part = *msg;
            IPTR handled[3] = {params[0], 0, TRUE};
            charmapcon_clear_selection(cl, o);
            if ((ICU(o)->conFlags & CF_WRAP_PENDING) && CHECK_MODE(o, PMB_AWM))
            {
                struct charmap_line *wrapped = charmapcon_find_line(cl, o, YCP);
                if (wrapped) wrapped->wrapColumns = CHAR_XMAX(o) + 1;
                ICU(o)->conFlags &= ~CF_WRAP_PENDING;
                XCP = XCCP = CHAR_XMIN(o);
                Console_Down(o, 1);
            }
            if (insert && !charmap_insert_char(cl, o, XCP, YCP, 1)) break;
            repaired = charmap_ascii(cl, o, XCP, YCP, (char *)&params[0], 1, FALSE);
            repaired |= insert;
            part.NumParams = 3; part.Params = handled;
            DoSuperMethodA(cl, o, (Msg)&part);
            if (repaired && !CON_IS_OBSCURED(o))
                charmapcon_refresh_lines(cl, o, YCP, YCP);
            break;
        }

    case C_ASCII_STRING:
        {
            const UBYTE *p = (const UBYTE *)params[0];
            ULONG left = params[1];
            charmapcon_clear_selection(cl, o);
            /* Store and draw the same row-sized span before advancing. A
             * single Write may wrap through many rows (including scrolling),
             * or repeatedly overwrite the last cell with autowrap disabled. */
            while (left)
            {
                struct P_Console_DoCommand part = *msg;
                IPTR span[3] = {0, 0, TRUE};
                part.NumParams = 3;
                ULONG n;
                BOOL repaired;
                if ((ICU(o)->conFlags & CF_WRAP_PENDING) && CHECK_MODE(o, PMB_AWM))
                {
                    struct charmap_line *wrapped = charmapcon_find_line(cl, o, YCP);
                    if (wrapped) wrapped->wrapColumns = CHAR_XMAX(o) + 1;
                    ICU(o)->conFlags &= ~CF_WRAP_PENDING;
                    XCP = XCCP = CHAR_XMIN(o);
                    Console_Down(o, 1);
                }
                if (CON_TEXT_IS_UTF8(o))
                {
                    const UBYTE *next = p;
                    LONG remaining = left;
                    WORD width = con_cp_width(con_utf8_decode(&next, &remaining));
                    if (width > CHAR_XMAX(o) + 1 - XCP)
                    {
                        if (!CHECK_MODE(o, PMB_AWM) || CHAR_XMAX(o) < 1)
                        {
                            /* No half glyph: preserve the existing last cell. */
                            ICU(o)->conFlags |= CF_WRAP_PENDING;
                            p = next;
                            left = remaining;
                            continue;
                        }
                        /* Fill the last cell, then retry the full glyph on
                         * the next row. Store and draw the spacer together. */
                        repaired = charmap_ascii(cl, o, XCP, YCP, " ", 1, FALSE);
                        struct charmap_line *wrapped = charmapcon_find_line(cl, o, YCP);
                        if (wrapped) wrapped->wrapPadding = 1;
                        span[0] = (IPTR)" ";
                        span[1] = 1;
                        part.Params = span;
                        DoSuperMethodA(cl, o, (Msg)&part);
                        if (repaired && !CON_IS_OBSCURED(o))
                            charmapcon_refresh_lines(cl, o, YCP, YCP);
                        continue;
                    }
                }
                n = charmap_print_span(p, left, CHAR_XMAX(o) + 1 - XCP,
                    CON_TEXT_IS_UTF8(o));
                ULONG cells = CON_TEXT_IS_UTF8(o) ? con_utf8_cellcount(p, n) : n;
                BOOL insert = (ICU(o)->conFlags & CF_INSERT_MODE) && cells;
                if (insert && !charmap_insert_char(cl, o, XCP, YCP, cells)) break;
                repaired = charmap_ascii(cl, o, XCP, YCP, (char *)p, n, CON_TEXT_IS_UTF8(o));
                repaired |= insert;
                span[0] = (IPTR)p;
                span[1] = n;
                part.Params = span;
                DoSuperMethodA(cl, o, (Msg)&part);
                if (repaired && !CON_IS_OBSCURED(o))
                    charmapcon_refresh_lines(cl, o, YCP, YCP);
                p += n;
                left -= n;
            }
        }
        break;

    case C_LINEFEED:
    case C_NEXT_LINE:
    case C_INDEX:
        {
            struct charmap_line *line = charmapcon_find_line(cl, o, YCP);
            if (line) { line->wrapColumns = 0; line->wrapPadding = 0; }
            DoSuperMethodA(cl, o, (Msg)msg);
            break;
        }

    case C_SET_OBSCURED:
        /* going to background: the shared window's scroller belongs to
           the visible tab - take ours out. Edge-triggered: only on a
           real visible->hidden transition (double RemoveGList would
           corrupt the window's gadget list). */
        if (!CON_IS_OBSCURED(o))
        {
            struct Window *win = CU(o)->cu_Window;

            if (data->prop && win)
            {
                if (data->boopsigad)
                    RemoveGadget(win, (struct Gadget *) data->prop);
                else
                    RemoveGList(win, &data->prop->scroller, 3);
            }
        }
        DoSuperMethodA(cl, o, (Msg) msg);
        break;

    case C_RESET_OBSCURED:
        /* tab brought to front. Edge-triggered like SET: re-attach the
           scroller only if we really were hidden (a reveal of an
           already-visible unit must not AddGList twice - that deadlocks
           Intuition on a corrupted gadget list). */
        {
            BOOL washidden = CON_IS_OBSCURED(o) ? TRUE : FALSE;

            DoSuperMethodA(cl, o, (Msg) msg);

            if (washidden)
            {
                struct Window *win = CU(o)->cu_Window;

                if (data->prop && win)
                {
                    if (data->boopsigad)
                        AddGadget(win, (struct Gadget *) data->prop, 0);
                    else
                        AddGList(win, &data->prop->scroller, 0, 3, NULL);
                    RefreshGList(data->boopsigad ?
                        (struct Gadget *) data->prop :
                        &data->prop->scroller,
                        win, NULL, data->boopsigad ? 1 : 3);
                }
            }
        }
        charmapcon_refresh(cl, o, 0);
        charmapcon_adj_prop(cl, o);
        break;

    case C_FORMFEED:
        charmap_formfeed(cl, o);
        DoSuperMethodA(cl, o, (Msg) msg);
        charmapcon_clear_margins(cl, o);
        break;

    case C_DELETE_CHAR:
        if (!charmap_delete_char(cl, o, XCP, YCP, params[0])) break;
        /* Draw from retained cells: ScrollRaster can leave a full-span shift
           unchanged and damage the adjacent scroller on this native path. */
        if (!CON_IS_OBSCURED(o)) charmapcon_refresh_lines(cl, o, YCP, YCP);
        break;

    case C_INSERT_CHAR:
        if (!charmap_insert_char(cl, o, XCP, YCP, params[0])) break;
        /* Draw from retained cells: ScrollRaster can leave a full-span shift
           unchanged and damage the adjacent scroller on this native path. */
        if (!CON_IS_OBSCURED(o)) charmapcon_refresh_lines(cl, o, YCP, YCP);
        break;

    case C_ERASE_CHAR:
        {
            struct charmap_line *line = charmapcon_find_line(cl, o, YCP);
            IPTR count = params[0] > 0 ? params[0] : 1;
            LONG available = CHAR_XMAX(o) - XCP + 1;
            ULONG i;
            if (available > 0 && line)
            {
                ULONG oldsize = line->size;
                ULONG end;
                if (count > available) count = available;
                end = XCP + count;
                if (end > line->size)
                {
                    charmap_resize(ConsoleDevice, line, end);
                    /* Never paint an erase that cannot survive a redraw. */
                    if (line->size < end) break;
                    for (i = oldsize; i < (ULONG)XCP; i++)
                        line->text[i] = ' ';
                }
                line->wrapColumns = 0; line->wrapPadding = 0;
                if (line->links) con_link_fill(line->links+XCP,count,NULL);
                for (i = XCP; i < XCP + count; i++)
                {
                    line->text[i] = ' ';
                    if (line->roles) line->roles[i]=CHARMAP_ROLES(o);
                    if (line->underlineColors) line->underlineColors[i] = 0;
                    line->fgpen[i] = CU(o)->cu_FgPen;
                    line->bgpen[i] = CU(o)->cu_BgPen;
                    line->flags[i] = 0;
                }
                ULONG width = CHAR_XMAX(o) + 1;
                if (width > line->size) width = line->size;
                charmap_repair_wide_cells(o, line, width);
                /* Repair can erase a partner outside the requested span. */
                if (!CON_IS_OBSCURED(o))
                    charmapcon_refresh_lines(cl, o, YCP, YCP);
            }
        }
        break;

    case C_ERASE_IN_LINE:
        charmap_erase_line(cl, o, XCP, YCP, params[0]);
        if (!CON_IS_OBSCURED(o)) charmapcon_refresh_lines(cl, o, YCP, YCP);
        break;

    case C_ERASE_IN_DISPLAY:
        charmap_erase_display(cl, o, XCP, YCP, params[0]);
        DoSuperMethodA(cl, o, (Msg) msg);
        if (params[0] == 2) charmapcon_clear_margins(cl, o);
        /* ED can split a wide pair on the cursor row just like EL. */
        if ((params[0] == 0 || params[0] == 1) && !CON_IS_OBSCURED(o))
            charmapcon_refresh_lines(cl, o, YCP, YCP);
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
            if (available <= 0) break;
            if (count > available) count = available;
            struct charmap_line *last = charmapcon_find_line(cl, o, bottom);
            struct charmap_line *first = charmapcon_find_line(cl, o, firstRow);
            if (!first || !last || !charmap_edit_lines(ConsoleDevice, first, last,
                    count, msg->Command != C_DELETE_LINE, CHAR_XMAX(o) + 1,
                    CU(o)->cu_FgPen, CU(o)->cu_BgPen, CHARMAP_ROLES(o))) break;
            data->select_x_max = data->select_x_min;
            data->select_y_max = data->select_y_min;
            data->select_line_max = data->select_line_min;
            data->active_selection = data->ignore_drag = FALSE;
            if (!CON_IS_OBSCURED(o)) charmapcon_refresh_lines(cl, o, 0, CHAR_YMAX(o));
            break;
        }

    case C_SCROLL_UP:
        {
            IPTR count = params[0] > 0 ? params[0] : 1;
            LONG top = CON_SCROLL_TOP(o), bottom = CON_SCROLL_BOTTOM(o);
            LONG available = bottom - top + 1;
            if (available <= 0) break;
            if (count > available) count = available;
            if (top || bottom != CHAR_YMAX(o))
            {
                struct charmap_line *first = charmapcon_find_line(cl, o, top);
                struct charmap_line *last = charmapcon_find_line(cl, o, bottom);
                if (!first || !last || !charmap_edit_lines(ConsoleDevice, first, last,
                        count, FALSE, CHAR_XMAX(o) + 1, CU(o)->cu_FgPen,
                        CU(o)->cu_BgPen, CHARMAP_ROLES(o))) break;
                data->select_x_max = data->select_x_min;
                data->select_y_max = data->select_y_min;
                data->select_line_max = data->select_line_min;
                data->active_selection = data->ignore_drag = FALSE;
                if (!CON_IS_OBSCURED(o)) charmapcon_refresh_lines(cl, o, top, bottom);
                break;
            }
            struct charmap_line *last = charmapcon_find_line(cl, o, CHAR_YMAX(o));
            if (!last || !charmap_append_blanks(ConsoleDevice, last, count,
                    CHAR_XMAX(o) + 1, CU(o)->cu_FgPen, CU(o)->cu_BgPen,
                    CHARMAP_ROLES(o))) break;
            data->scrollback_size += count;
            charmap_scroll_up(cl, o, count);
            /* Pass the bounded count without modifying the caller's parameters. */
            struct P_Console_DoCommand bounded = *msg;
            bounded.Params = &count;
            DoSuperMethodA(cl, o, (Msg)&bounded);
            break;
        }

    default:
        DoSuperMethodA(cl, o, (Msg) msg);
        break;
    }

    if (old_scrollback_size != data->scrollback_size ||
        old_scrollback_pos != data->scrollback_pos)
        charmapcon_adj_prop(cl, o);

    ReturnVoid("CharMapCon::DoCommand");
}

/**************************
**  CharMapCon::ClearCell()  **
**************************/
static VOID charmapcon_clearcell(Class *cl, Object *o,
    struct P_Console_ClearCell *msg)
{
    // FIXME, insert space.
    DoSuperMethodA(cl, o, (Msg) msg);
}

/* Decode the retained foreground identity without consulting current SGR. */
static ULONG charmap_foreground_key(const struct charmap_line *line, ULONG x)
{
    if (!line->roles) return 0xffffffffUL;
    UQUAD colors = line->roles[x];
    UBYTE role = (colors >> 8) & 0xbf;
    if (role == 1) return CONUNIT_PEN_DEFFG;
    if (role == 2) return CONUNIT_PEN_DEFBG;
    if (role >= 3 && role <= 10) return role - 3;
    if (role >= 11 && role <= 26) return CONPEN_XTERM(role - 11);
    if (role >= 27 && role <= 34) return CONPEN_BRIGHT(role - 27);
    if (role == 0x80) return 0x01000000 | ((colors >> 16) & 0xffffff);
    if (role == 0x81) return (colors >> 16) & 0x1ff;
    return 0xffffffffUL;
}

/* Truecolor cache pens may be recycled between writes or while this screen
 * is dormant. Resolve original RGB immediately before each raster batch. */
static VOID charmap_resolve_rgb(Object *o, struct charmap_line *line, ULONG x)
{
    if (!line->roles) return;
    UQUAD colors = line->roles[x];
    if (((colors >> 8) & 0xbf) == 0x80)
        Console_GetColorPen(o, 0x01000000 | ((colors >> 16) & 0xffffff),
            (UBYTE *)&line->fgpen[x]);
    if (((colors >> 8) & 0xbf) == 0x81)
        Console_GetColorPen(o, (colors >> 16) & 0x1ff,
            (UBYTE *)&line->fgpen[x]);
    if ((colors & 0xbf) == 0x80)
        Console_GetColorPen(o, 0x01000000 | ((colors >> 40) & 0xffffff),
            (UBYTE *)&line->bgpen[x]);
    if ((colors & 0xbf) == 0x81)
        Console_GetColorPen(o, (colors >> 40) & 0x1ff,
            (UBYTE *)&line->bgpen[x]);
}

/* A reflowed hard row may end in a partial run of retained blanks. Extend
 * that run's background while painting, without manufacturing text cells. */
static UBYTE charmap_blank_tail_pen(const struct charmap_line *line, UBYTE fallback)
{
    if (!line || !line->size || !line->text ||
        line->text[line->size - 1] != ' ' || !line->bgpen) return fallback;
    ULONG last = line->size - 1;
    if (line->flags && (line->flags[last] & CON_TXTFLAGS_REVERSED) && line->fgpen)
        return (UBYTE)line->fgpen[last];
    return (UBYTE)line->bgpen[last];
}

/* Rectangular boundaries include a complete glyph when touching a wide cell. */
static VOID charmap_block_span(const struct charmap_line *line,ULONG *left,ULONG *right)
{
    if(!line || !line->text)return;
    if(*left && *left<line->size && line->text[*left]==0xfffffffeUL)(*left)--;
    if(*right<line->size && line->text[*right]==0xfffffffeUL)(*right)++;
}

/* Overlay only: stop boundaries split render batches without changing cells. */
static UBYTE charmap_search_flags(const struct charmap_search_ui *search,
    const struct charmap_line *line, ULONG row, ULONG column, ULONG width, ULONG *stop, UBYTE flags)
{
    if (search && search->found && row >= search->first_row && row <= search->last_row)
    {
        ULONG left = row == search->first_row ? search->match.first.column : 0;
        ULONG right = row == search->last_row ? search->match.end.column : width;
        if(search->nav_mode && search->nav.selecting && search->nav.blockwise){
            left=search->match.first.column;right=search->match.end.column;
            charmap_block_span(line,&left,&right);
        }
        if (column < left) *stop = MIN(*stop, left);
        else if (column < right)
        {
            *stop = MIN(*stop, right);
            flags ^= CON_TXTFLAGS_REVERSED;
        }
    }
    return flags;
}

static VOID charmapcon_refresh_lines(Class *cl, Object *o, LONG fromLine,
    LONG toLine)
{
    struct Window *w = CU(o)->cu_Window;
    struct RastPort *rp = w->RPort;
    struct charmapcondata *data = INST_DATA(cl, o);
    struct Library *GfxBase = data->ccd_GfxBase;
    struct TmpRas localTmpRas;
    struct TmpRas *savedTmpRas = rp->TmpRas;
    PLANEPTR scratch = NULL;
    UWORD scratchWidth = GFX_XMAX(o) - GFX_XMIN(o) + 1;
    UWORD scratchHeight = rp->Font->tf_YSize;
    ULONG scratchSize = RASSIZE(scratchWidth, scratchHeight);
    BOOL cursor_in_range;

    if (fromLine < CHAR_YMIN(o))
        fromLine = CHAR_YMIN(o);
    if (toLine > CHAR_YMAX(o))
        toLine = CHAR_YMAX(o);

    D(bug("fromLine: %ld, toLine: %ld, char_ymax: %ld\n", fromLine, toLine,
            CHAR_YMAX(o)));

    if (toLine < fromLine)
        return;

    /* A URL may start or end on an adjacent automatically wrapped row. */
    struct charmap_line *edge=charmapcon_find_line(cl,o,fromLine);
    while (fromLine>CHAR_YMIN(o) && edge && edge->prev && edge->prev->wrapColumns)
    { edge=edge->prev; fromLine--; }
    edge=charmapcon_find_line(cl,o,toLine);
    while (toLine<CHAR_YMAX(o) && edge && edge->wrapColumns && edge->next)
    { edge=edge->next; toLine++; }

    cursor_in_range = YCP >= fromLine && YCP <= toLine;

    if (!savedTmpRas || savedTmpRas->Size < scratchSize)
    {
        scratch = AllocRaster(scratchWidth, scratchHeight);
        if (scratch)
            rp->TmpRas = InitTmpRas(&localTmpRas, scratch, scratchSize);
    }

    if (cursor_in_range)
        Console_UnRenderCursor(o);

    D(bug("Rendering charmap\n"));

    struct charmap_line *line = charmapcon_find_line(cl, o, fromLine);
    ULONG y = GFX_YMIN(o) + fromLine * YRSIZE + rp->Font->tf_Baseline;
    ULONG yc = fromLine;
    UBYTE flags = 0;
    BOOL styleSet = FALSE;
    UBYTE previousDecoration = 255;
    ULONG previousUnderlineColor = 0;

    LONG selectstart_x, selectstart_y, selectend_x, selectend_y;
    if (data->select_y_min == data->select_y_max)
    {
        selectstart_y = selectend_y = data->select_y_min;
        selectstart_x = MIN(data->select_x_min, data->select_x_max);
        selectend_x = MAX(data->select_x_min, data->select_x_max);
    }
    else if (data->select_y_min < data->select_y_max)
    {
        selectstart_y = data->select_y_min;
        selectstart_x = data->select_x_min;
        selectend_y = data->select_y_max;
        selectend_x = data->select_x_max;
    }
    else
    {
        selectstart_y = data->select_y_max;
        selectstart_x = data->select_x_max;
        selectend_y = data->select_y_min;
        selectend_x = data->select_x_min;
    }

    while (line && yc <= toLine)
    {
        const ULONG *str = line->text;
        ULONG start = 0;
        ULONG remaining_space = CHAR_XMAX(o) + 1;
        ULONG urlEnd=0;
        BOOL urlUnderline=FALSE;
        Move(rp, GFX_XMIN(o), y);
        /* Clear the blank tail before painting glyphs. Italic glyphs may
         * overhang their last cell; clearing afterwards clips those pixels. */
        ULONG tailStart = 0;
        while (tailStart < line->size && tailStart <= (ULONG)CHAR_XMAX(o) && str[tailStart])
            tailStart++;
        if (tailStart <= (ULONG)CHAR_XMAX(o))
        {
            /* The helper returns the effective background; do not invert it again. */
            SetDrMd(rp, JAM1);
            if (line->size) charmap_resolve_rgb(o, line, line->size - 1);
            UBYTE tail = charmap_blank_tail_pen(line, CU(o)->cu_BgPen);
            if (line->size && line->flags &&
                (line->flags[line->size - 1] & CON_TXTFLAGS_REVERSED))
                tail = ConsoleNGFaintPen(o, GfxBase, line->flags[line->size - 1],
                    tail, charmap_foreground_key(line, line->size - 1));
            SetAPen(rp, tail);
            RectFill(rp,
                GFX_X(o, tailStart), GFX_Y(o, yc),
                GFX_XMAX(o), GFX_Y(o, yc + 1) - 1);
            /* A logical-line overlay also covers unallocated blank columns. */
            ULONG at=tailStart,width=(ULONG)CHAR_XMAX(o)+1;
            while(at<width){
                ULONG stop=width;
                UBYTE overlay=charmap_search_flags(data->search,line,data->scrollback_pos+yc,at,width,&stop,0);
                if(overlay & CON_TXTFLAGS_REVERSED){
                    UBYTE selected=CU(o)->cu_FgPen;
                    if(line->size && line->text[line->size-1]==' ' && line->fgpen && line->bgpen){
                        ULONG last=line->size-1;
                        selected=line->flags && (line->flags[last]&CON_TXTFLAGS_REVERSED)?line->bgpen[last]:line->fgpen[last];
                    }
                    SetAPen(rp,selected);
                    RectFill(rp,GFX_X(o,at),GFX_Y(o,yc),GFX_X(o,stop)-1,GFX_Y(o,yc+1)-1);
                }
                if(stop<=at)break;
                at=stop;
            }
        }

        /* Erased cells are retained backgrounds, not printed spaces. Paint
         * those runs first so they cannot wipe the adjacent glyph's overhang.
         * Keep explicit spaces in text order, including their decorations. */
        for (unsigned pass = 0; pass < 2; pass++)
        {
        start = 0;
        remaining_space = CHAR_XMAX(o) + 1;
        urlEnd = 0;
        while (line->size > start && remaining_space > 0 && str[start])
        {
            /* Identify a batch of characters with the same fgpen/bgpen
               to avoid having to move/set pens and do Text() on single
               characters */

            charmap_resolve_rgb(o, line, start);
            UBYTE fgpen = line->fgpen[start];
            UBYTE bgpen = line->bgpen[start];

            /* Is any part of this line part of a selection?
             * If so, we bake in a state transition on "stop".
             * This code is messy - there must be a nicer way.
             */
            ULONG stop = 9999999;
            BOOL in_selection = 0;

            if (yc > selectstart_y && yc < selectend_y)
            {
                in_selection = 1;
            }
            else if (yc == selectstart_y)
            {
                if (yc == selectend_y)
                {
                    if (start >= selectstart_x && start < selectend_x)
                    {
                        in_selection = 1;
                        stop = selectend_x;
                    }
                    else if (start < selectstart_x)
                    {
                        stop = selectstart_x;
                    }
                }
                else
                {
                    if (start >= selectstart_x)
                        in_selection = 1;
                    else
                        stop = selectstart_x;
                }
            }
            else if (yc == selectend_y)
            {
                /* In this case, the selection *ends* on selectend_x */
                if (start < selectend_x)
                {
                    in_selection = 1;
                    stop = selectend_x;
                }
            }

            if (stop == start)
                stop += 1;

            fgpen = ConsoleNGFaintPen(o, GfxBase, line->flags[start], fgpen,
                charmap_foreground_key(line, start));
            if (in_selection)
            {
                UBYTE selectedForeground = fgpen;
                fgpen = bgpen;
                bgpen = selectedForeground;
            }

            if (start>=urlEnd)
                urlUnderline=con_url_run(line,start,&urlEnd);
            if (urlEnd<stop) stop=urlEnd;
            UBYTE drawFlags=line->flags[start] == CHARMAP_ERASED ? 0 : line->flags[start];
            UBYTE decoration = line->roles ? ((line->roles[start] >> 6) & 1) | ((line->roles[start] >> 13) & 2) : 0;
            ULONG underlineColor = line->underlineColors ? line->underlineColors[start] : 0;
            drawFlags = charmap_search_flags(data->search, line, data->scrollback_pos + yc,
                start, line->size, &stop, drawFlags);
            if (urlUnderline) drawFlags|=CON_TXTFLAGS_UNDERLINED;

            ULONG len = 0;
            while (line->size > start + len && str[start + len] &&
                len < remaining_space &&
                (line->roles ? line->roles[start] == line->roles[start + len] &&
                    (((line->roles[start] >> 8) & 0xbf) != 0 || line->fgpen[start] == line->fgpen[start + len]) &&
                    ((line->roles[start] & 0xbf) != 0 || line->bgpen[start] == line->bgpen[start + len]) :
                    line->fgpen[start] == line->fgpen[start + len] &&
                    line->bgpen[start] == line->bgpen[start + len]) &&
                (!line->underlineColors || line->underlineColors[start] == line->underlineColors[start + len]) &&
                line->flags[start] == line->flags[start + len] &&
                start + len < stop)
                len += 1;

            if ((line->flags[start] == CHARMAP_ERASED) != (pass == 0))
            {
                start += len;
                remaining_space -= len;
                continue;
            }

            setabpen(GfxBase, rp, drawFlags, fgpen, bgpen);
            if (!styleSet || decoration != previousDecoration || underlineColor != previousUnderlineColor || (drawFlags & CON_TXTFLAGS_MASK) !=
                (flags & CON_TXTFLAGS_MASK))
            {
                SetSoftStyle(rp, (decoration || underlineColor) ? drawFlags & ~CON_TXTFLAGS_UNDERLINED : drawFlags, CON_TXTFLAGS_MASK);
                styleSet = TRUE;
                previousDecoration = decoration;
                previousUnderlineColor = underlineColor;
            }
            flags = drawFlags;

            /* Render the same-pen run in sub-batches: Latin-1 cells go
               through Text() as bytes, higher codepoints re-encode to
               UTF-8 and render via TextUTF8() over a background fill
               (the alpha blit does not paint cell backgrounds). */
            {
                ULONG idx = start;

                while (idx < start + len)
                {
                    if (str[idx] < 0x100)
                    {
                        UBYTE bytebuf[128];
                        ULONG n = 0;

                        while (idx + n < start + len && n < sizeof(bytebuf)
                            && str[idx + n] < 0x100)
                        {
                            bytebuf[n] = str[idx + n];
                            n++;
                        }
                        Move(rp, GFX_X(o, idx), y);
                        Text(rp, bytebuf, n);
                        ConsoleNGStrike(GfxBase, rp, flags, GFX_X(o, idx), GFX_Y(o, yc),
                            n * XRSIZE, YRSIZE, fgpen, bgpen);
                        ConsoleNGStyledUnderline(GfxBase, rp, flags, GFX_X(o, idx), GFX_Y(o, yc),
                            n * XRSIZE, YRSIZE, fgpen, bgpen, decoration);
                        ConsoleNGColorUnderline(o, GfxBase, rp, flags, GFX_X(o, idx), GFX_Y(o, yc),
                            n * XRSIZE, YRSIZE, decoration, underlineColor);
                        idx += n;
                    }
                    else
                    {
                        /* one glyph per call: wide glyphs own two cells
                           (their CELL_WIDE_CONT partner is skipped) */
                        ULONG cp = str[idx];
                        UBYTE encbuf[8];
                        ULONG bytes;
                        WORD cw;
                        UBYTE ufg = fgpen, ubg = bgpen, swp;

                        if (cp == CELL_WIDE_CONT)
                        {
                            idx++;
                            continue;
                        }

                        cw = con_cp_width(cp);
                        if (cw < 1)
                            cw = 1;
                        if (idx + cw > start + len)
                            cw = start + len - idx;

                        bytes = con_utf8_encode(encbuf, cp);

                        if (flags & CON_TXTFLAGS_REVERSED)
                        {
                            swp = ufg;
                            ufg = ubg;
                            ubg = swp;
                        }
                        if (flags & CON_TXTFLAGS_CONCEALED)
                            ufg = ubg;

                        /* Unicode pens are already swapped above; do not apply INVERSVID twice. */
                        SetDrMd(rp, JAM1);
                        SetAPen(rp, ubg);
                        RectFill(rp, GFX_X(o, idx), GFX_Y(o, yc),
                            GFX_X(o, idx + cw) - 1, GFX_Y(o, yc + 1) - 1);
                        SetAPen(rp, ufg);
                        Move(rp, GFX_X(o, idx), y);
                        if (!(flags & CON_TXTFLAGS_CONCEALED))
                            TextUTF8(rp, encbuf, bytes);
                        ConsoleNGStrike(GfxBase, rp, flags, GFX_X(o, idx), GFX_Y(o, yc),
                            cw * XRSIZE, YRSIZE, fgpen, bgpen);
                        ConsoleNGStyledUnderline(GfxBase, rp, flags, GFX_X(o, idx), GFX_Y(o, yc),
                            cw * XRSIZE, YRSIZE, fgpen, bgpen, decoration);
                        ConsoleNGColorUnderline(o, GfxBase, rp, flags, GFX_X(o, idx), GFX_Y(o, yc),
                            cw * XRSIZE, YRSIZE, decoration, underlineColor);
                        setabpen(GfxBase, rp, flags, fgpen, bgpen);
                        idx += cw;
                    }
                }
            }

            start += len;
            remaining_space -= len;
        }

        }
        y += YRSIZE;
        yc++;

        /* We want to make sure we have lines covering the window once
           there's something to scroll back, as that simplies resize handling
           etc.
         */
        if (!line->next && yc <= toLine)
        {
            if (charmap_newline(0, line)) data->scrollback_size += 1;
        }
        line = line->next;
    }

    if (yc < toLine)
    {
        SetAPen(rp, CU(o)->cu_BgPen);
        RectFill(rp,
            GFX_XMIN(o), GFX_Y(o, yc), GFX_XMAX(o), GFX_Y(o, toLine));
    }

    if (cursor_in_range)
        Console_RenderCursor(o);

    if (scratch)
    {
        rp->TmpRas = savedTmpRas;
        FreeRaster(scratch, scratchWidth, scratchHeight);
    }

}

static VOID charmapcon_clear_selection(Class *cl, Object *o)
{
    struct charmapcondata *data = INST_DATA(cl, o);
    data->selection_exact_end = FALSE;
    data->selection_block = FALSE;

    if (data->select_x_min != data->select_x_max ||
        data->select_y_min != data->select_y_max)
    {
        LONG fromLine = MIN(data->select_y_min, data->select_y_max);
        LONG toLine = MAX(data->select_y_min, data->select_y_max);

        data->select_x_max = data->select_x_min;
        data->select_y_max = data->select_y_min;
        data->select_line_max = data->select_line_min;
        data->active_selection = FALSE;
        data->ignore_drag = FALSE;
        charmapcon_refresh_lines(cl, o, fromLine, toLine);
    }
}


/*
 * Refresh the full console unless "off" is provided.
 * If off is set to a positive value, it indicates the
 * number of rows from the top we start rendering.
 * If off is set to a negative value, it indicates the
 * number of rows from the top we stop rendering.
 * This is used for partial refreshes when the screen is
 * scrolled up/down.
 */
static VOID charmapcon_refresh(Class *cl, Object *o, LONG off)
{
    LONG fromLine = 0;
    LONG toLine = CHAR_YMAX(o) - CHAR_YMIN(o);

    if (off > 0)
        toLine = off - 1;
    if (off < 0)
        fromLine = CHAR_YMAX(o) + off + 1;

    charmapcon_refresh_lines(cl, o, fromLine, toLine);
    charmapcon_clear_margins(cl, o);
}


/* Fixed per-unit history. No row pointers, disk writes or global queries. */
static VOID charmap_search_remember(struct charmap_search_ui *s)
{
    if (!s->length || (s->history_count && s->history_length[0] == s->length &&
        !memcmp(s->history[0], s->query, s->length * sizeof(ULONG)))) return;
    ULONG count = s->history_count < 16 ? s->history_count++ : 15;
    for (ULONG i = count; i; i--)
    {
        s->history_length[i] = s->history_length[i - 1];
        memcpy(s->history[i], s->history[i - 1], s->history_length[i] * sizeof(ULONG));
    }
    s->history_length[0] = s->length;
    memcpy(s->history[0], s->query, s->length * sizeof(ULONG));
}

static BOOL charmap_search_recall(struct charmap_search_ui *s, BOOL older)
{
    if (older)
    {
        if (s->history_at >= s->history_count) return FALSE;
        if (!s->history_at)
        {
            s->draft_length = s->length;
            memcpy(s->draft, s->query, s->length * sizeof(ULONG));
        }
        s->history_at++;
    }
    else
    {
        if (!s->history_at) return FALSE;
        s->history_at--;
    }
    s->length = s->history_at ? s->history_length[s->history_at - 1] : s->draft_length;
    memcpy(s->query, s->history_at ? s->history[s->history_at - 1] : s->draft,
        s->length * sizeof(ULONG));
    return TRUE;
}

/* Intuition titles use byte Text(), not UTF-8. Keep the query lossless and
 * distinguish literal backslashes from display-only Unicode/control escapes. */
static ULONG charmap_search_title_scalar(ULONG cp, char *out)
{
    if (cp == '\\') { out[0] = out[1] = '\\'; return 2; }
    if (cp == '\n' || cp == '\r' || cp == '\t') {
        out[0] = '\\'; out[1] = cp == '\n' ? 'n' : cp == '\r' ? 'r' : 't';
        return 2;
    }
    if ((cp >= 32 && cp < 127) || (cp >= 160 && cp <= 255)) {
        out[0] = cp; return 1;
    }
    static const char hex[] = "0123456789ABCDEF";
    ULONG digits = cp > 0xfffff ? 6 : cp > 0xffff ? 5 : 4;
    out[0] = '\\'; out[1] = 'u'; out[2] = '{';
    for (ULONG i = 0; i < digits; i++)
        out[3+i] = hex[(cp >> (4 * (digits-1-i))) & 15];
    out[3+digits] = '}';
    return digits + 4;
}

static VOID charmap_search_title(Class *cl, Object *o, const char *status)
{
    struct charmapcondata *data = INST_DATA(cl, o);
    struct charmap_search_ui *s = data->search;
    if(s->nav_mode && !s->nav_query){
        strcpy(s->title,s->nav_copy?"Navigate: copying selection":s->nav_seed?"Navigate: collecting search text":s->nav_notice?s->nav_notice:s->nav_find_pending?"Navigate: find character / Esc cancel":s->nav.selecting?
            "Navigate: selecting / hjkl arrows / 0 $ / g G / y copy / Esc clear / i exit":
            "Navigate: hjkl arrows / w b e / W B E / v select / i or Ctrl+C exit");
        SetWindowTitles(WINDOW(o),(CONST_STRPTR)s->title,(CONST_STRPTR)-1);return;
    }

    ULONG at = 0;
    if (s->paste_status) status = s->paste_status;
    if(s->nav_query_miss)status="not found";
    const char *prefix = s->nav_query ? (s->reverse_search?"Navigate ?: ":"Navigate /: ") : s->regex_mode ? "Regex: " : "Find: ";
    while (*prefix) s->title[at++] = *prefix++;
    ULONG i = 0;
    for (; i < s->length; i++) {
        char scalar[10];
        ULONG count = charmap_search_title_scalar(s->query[i], scalar);
        if (at + count > 270) break;
        memcpy(s->title + at, scalar, count); at += count;
    }
    if (i < s->length) {
        memcpy(s->title + at, "...", 3); at += 3;
    }
    s->title[at++] = ' '; s->title[at++] = '[';
    while (*status && at < sizeof(s->title)-2) s->title[at++] = *status++;
    s->title[at++] = ']'; s->title[at] = 0;
    SetWindowTitles(WINDOW(o), (CONST_STRPTR)s->title, (CONST_STRPTR)-1);
}

static VOID charmap_nav_copy_cancel(struct charmap_search_ui *s)
{
    if(s->copy_buffer)FreeMem(s->copy_buffer,s->copy_size);
    s->copy_buffer=NULL;s->copy_size=s->copy_phase=0;
    s->nav_copy=s->nav_copy_clear=FALSE;
    memset(&s->copy_nav,0,sizeof(s->copy_nav));memset(&s->copy_encoder,0,sizeof(s->copy_encoder));
}

/* Selection-only action: preserve retained text, view and application cursor. */
static BOOL charmap_selection_clear_action(Class *cl, Object *o)
{
    struct charmapcondata *data = INST_DATA(cl, o);
    struct charmap_search_ui *s = data->search;
    if(s){
        charmap_nav_copy_cancel(s);
        if(s->active && s->nav_mode && !s->nav_query){
            s->nav.selecting=FALSE;s->nav_dirty=TRUE;s->found=FALSE;
            s->nav_notice=NULL;
            charmap_search_title(cl,o,"");
        }
    }
    charmapcon_clear_selection(cl,o);
    data->active_selection=FALSE;data->ignore_drag=FALSE;
    return TRUE;
}

static VOID charmap_search_close(Class *cl, Object *o, BOOL redraw)
{
    struct charmapcondata *data = INST_DATA(cl, o);
    struct charmap_search_ui *s = data->search;
    if (!s || !s->active) return;
    s->revision++;
    if(!s->nav_mode)charmap_search_remember(s);
    s->nav_mode=s->nav_dirty=s->nav_view=s->nav_find_pending=FALSE;
    charmap_nav_copy_cancel(s);
    s->nav_anchor=FALSE;memset(&s->seed_anchor,0,sizeof(s->seed_anchor));
    s->nav_seed=FALSE;s->nav_notice=NULL;memset(&s->seed,0,sizeof(s->seed));
    memset(&s->nav,0,sizeof(s->nav));
    memset(&s->nav_origin,0,sizeof(s->nav_origin));
    memset(&s->origin_search,0,sizeof(s->origin_search));
    s->nav_origin_view=NULL;s->nav_query=s->nav_search=s->nav_confirm=s->nav_query_miss=FALSE;
    s->active = s->scanning = s->locating = s->found = FALSE;
    s->paste_status = NULL;
    con_search_select_cancel(&s->selection);
    con_regex_select_cancel(&s->regex_selection);
    memset(&s->match, 0, sizeof(s->match));
    s->seek = s->view = NULL;
    /* Intuition borrows title pointers. Restore before freeing our storage,
     * but never overwrite a newer title installed externally by the owner. */
    if (WINDOW(o)->Title == (UBYTE *)s->title)
        SetWindowTitles(WINDOW(o), (CONST_STRPTR)s->saved_title, (CONST_STRPTR)-1);
    s->saved_title = NULL;
    if (redraw && !CON_IS_OBSCURED(o)) charmapcon_refresh(cl, o, 0);
}

/* Explicit search acceptance or navigation copy publishes a stable selection.
 * Mutation/resize/title-ownership cancellation must never publish borrowed rows. */
static VOID charmap_search_publish(Class *cl, Object *o)
{
    struct charmapcondata *data = INST_DATA(cl, o);
    struct charmap_search_ui *s = data->search;
    if (s && s->active && s->found && !s->scanning && !s->locating &&
        s->match.first.line && s->match.end.line &&
        (s->match.first.line != s->match.end.line ||
         s->match.first.column != s->match.end.column)) {
        data->select_line_min = (struct charmap_line *)s->match.first.line;
        data->select_line_max = (struct charmap_line *)s->match.end.line;
        data->selection_block = s->nav_mode && s->nav.selecting && s->nav.blockwise;
        data->selection_exact_end = !(s->nav_mode && s->nav.selecting && s->nav.linewise);
        data->select_x_min = s->match.first.column;
        data->select_x_max = s->match.end.column;
        data->select_y_min = (LONG)s->first_row - (LONG)data->scrollback_pos;
        data->select_y_max = (LONG)s->last_row - (LONG)data->scrollback_pos;
        data->active_selection = data->ignore_drag = FALSE;
    }
}

/* Copy a completed navigation snapshot without changing a newer cursor,
 * query or selection. Output/reflow/exit cancels its borrowed rows first. */
static VOID charmap_nav_copy_selection(Class *cl,Object *o,const struct con_nav *nav,UBYTE *buffer,ULONG size)
{
    struct charmapcondata *data=INST_DATA(cl,o);
    struct con_search_match match;ULONG first,last;
    con_nav_match(nav,&match,&first,&last);
    if(match.first.line && match.end.line &&
       (match.first.line!=match.end.line || match.first.column!=match.end.column)){
        data->select_line_min=(struct charmap_line *)match.first.line;
        data->select_line_max=(struct charmap_line *)match.end.line;
        data->selection_block=nav->blockwise;data->selection_exact_end=!nav->linewise;
        data->select_x_min=match.first.column;data->select_x_max=match.end.column;
        data->select_y_min=(LONG)first-(LONG)data->scrollback_pos;
        data->select_y_max=(LONG)last-(LONG)data->scrollback_pos;
        data->active_selection=data->ignore_drag=FALSE;
        data->prepared_copy=buffer;data->prepared_copy_size=size;
        Console_Copy(o);charmapcon_clear_selection(cl,o);
        /* A subclass may override Copy; reclaim unconsumed prepared data. */
        if(!data->prepared_copy)return;
        data->prepared_copy=NULL;data->prepared_copy_size=0;
    }
    FreeMem(buffer,size);
}

/* Each phase has a fixed budget; no retained-row or cell pass is unbounded. */
static enum con_copy_result charmap_nav_copy_step(Class *cl,Object *o,ULONG budget)
{
    struct charmapcondata *data=INST_DATA(cl,o);struct charmap_search_ui *s=data->search;
    if(!s->copy_phase){
        if(!con_nav_step(&s->copy_nav,budget))return CON_COPY_MORE;
        struct con_search_match span;ULONG first,last;
        con_nav_match(&s->copy_nav,&span,&first,&last);
        con_copy_begin(&s->copy_encoder,&span,CON_IS_UTF8(o),!s->copy_nav.linewise,s->copy_nav.blockwise);
        s->copy_phase=1;
    }
    enum con_copy_result result=con_copy_step(&s->copy_encoder,budget);
    if(result==CON_COPY_MEASURED){
        s->copy_size=s->copy_encoder.size;
        if(!s->copy_size)return CON_COPY_DONE;
        s->copy_buffer=AllocMem(s->copy_size,MEMF_ANY);
        if(!s->copy_buffer){s->nav_notice="Navigate: copy allocation failed";return CON_COPY_ERROR;}
        if(!con_copy_write_begin(&s->copy_encoder,s->copy_buffer,s->copy_size))return CON_COPY_ERROR;
        s->copy_phase=2;result=con_copy_step(&s->copy_encoder,budget);
    }
    if(result==CON_COPY_DONE){
        UBYTE *buffer=s->copy_buffer;s->copy_buffer=NULL;
        charmap_nav_copy_selection(cl,o,&s->copy_nav,buffer,s->copy_size);
    }
    if(result==CON_COPY_ERROR && !s->nav_notice)s->nav_notice="Navigate: copy encoding failed";
    return result;
}

static VOID charmap_nav_copy_finish(Class *cl,Object *o,enum con_copy_result result)
{
    struct charmapcondata *data=INST_DATA(cl,o);struct charmap_search_ui *s=data->search;
    if(result==CON_COPY_DONE && s->nav_copy_clear){
        s->nav=s->copy_nav;s->nav.selecting=FALSE;s->nav.match_ready=FALSE;
        s->nav_dirty=TRUE;s->found=FALSE;
    }
    charmap_nav_copy_cancel(s);charmap_search_title(cl,o,"");
}

static VOID charmap_search_accept(Class *cl, Object *o)
{
    charmap_search_publish(cl,o);
    charmap_search_close(cl,o,TRUE);
}

static VOID charmap_search_start(Class *cl, Object *o, BOOL reverse, BOOL next)
{
    struct charmapcondata *data = INST_DATA(cl, o);
    struct charmap_search_ui *s = data->search;
    s->revision++;
    s->paste_status = NULL;
    s->nav_anchor=FALSE;memset(&s->seed_anchor,0,sizeof(s->seed_anchor));
    struct con_search_match anchor = s->match;
    BOOL anchored = next && s->found;
    s->found = s->locating = FALSE;
    s->seek = NULL;
    con_search_select_cancel(&s->selection);
    con_regex_select_cancel(&s->regex_selection);
    if(s->nav_mode){
        struct con_nav_cursor origin=s->nav_query?s->nav_origin.cursor:s->nav.cursor;
        s->nav_search=TRUE;s->nav_dirty=FALSE;s->nav_query_miss=FALSE;
        s->scanning=con_search_origin_begin(&s->origin_search,&s->selection.scan,
            &s->regex_selection.scan,data->top_of_scrollback,s->query,s->length,
            s->regex_mode,reverse,!next,origin.row,origin.column);
        if(!s->scanning){
            s->nav_search=FALSE;
            if(s->length && s->regex_mode)s->paste_status="invalid or unsupported regex";
            if(s->nav_query){s->nav=s->nav_origin;s->nav_view=TRUE;s->nav_view_row=s->nav_origin_view_row;s->nav_dirty=TRUE;}
        }
    }else s->scanning = s->regex_mode ?
        con_regex_select_begin(&s->regex_selection, data->top_of_scrollback,
            s->query, s->length, con_regex_smart_fold(s->query, s->length),
            reverse, anchored ? &anchor : NULL) :
        con_search_select_begin(&s->selection, data->top_of_scrollback,
            s->query, s->length, TRUE, reverse, anchored ? &anchor : NULL);
    if (!CON_IS_OBSCURED(o)) charmapcon_refresh(cl, o, 0);
    charmap_search_title(cl, o, s->scanning ? "searching" : s->regex_mode && s->length ? "invalid or unsupported regex" : s->nav_query ? "Enter confirm / Esc cancel / Ctrl+R toggles regex" : "Ctrl+R toggles regex / Esc close");
}

/* Jump without trimming rows, balancing only the history cursor suppression. */
static VOID charmap_search_view(Class *cl, Object *o, struct charmap_search_ui *s)
{
    struct charmapcondata *data = INST_DATA(cl, o);
    if (!data->unrendered && s->view != data->top_of_window)
    {
        Console_UnRenderCursor(o);
        data->unrendered = TRUE;
        data->saved_scrollback_pos = data->scrollback_pos;
        data->saved_top_of_window = data->top_of_window;
    }
    BOOL live = data->unrendered && s->view == data->saved_top_of_window;
    LONG offset = (LONG)data->scrollback_pos - (LONG)s->view_row;
    data->select_y_min += offset;
    data->select_y_max += offset;
    data->top_of_window = (struct charmap_line *)s->view;
    data->scrollback_pos = s->view_row;
    if (live) data->unrendered = FALSE;
    charmapcon_adj_prop(cl, o);
    if (!CON_IS_OBSCURED(o)) charmapcon_refresh(cl, o, 0);
    if (live) Console_RenderCursor(o);
}

/* Query cancellation restores the borrowed navigation origin and viewport.
 * Finish invalidates pending clipboard replies but keeps navigation active. */
static VOID charmap_nav_query_finish(Class *cl,Object *o,BOOL cancel)
{
    struct charmapcondata *data=INST_DATA(cl,o);struct charmap_search_ui *s=data->search;
    charmap_search_remember(s);s->revision++;
    if(cancel){
        s->nav=s->nav_origin;s->nav_view=TRUE;s->nav_view_row=s->nav_origin_view_row;s->length=0;
    }
    s->nav_query=s->nav_search=s->nav_confirm=s->nav_query_miss=FALSE;
    s->scanning=s->locating=s->found=FALSE;s->seek=NULL;s->nav_dirty=TRUE;
    s->nav_origin_view=NULL;memset(&s->nav_origin,0,sizeof(s->nav_origin));
    memset(&s->origin_search,0,sizeof(s->origin_search));
    con_search_select_cancel(&s->selection);con_regex_select_cancel(&s->regex_selection);
    charmap_search_title(cl,o,"");
}

static VOID charmap_search_tick(Class *cl, Object *o)
{
    struct charmapcondata *data = INST_DATA(cl, o);
    struct charmap_search_ui *s = data->search;
    if (!s || !s->active) return;
    if (WINDOW(o)->Title != (UBYTE *)s->title)
    {
        charmap_search_close(cl, o, TRUE);
        return;
    }
    if(s->nav_copy){
        enum con_copy_result result=charmap_nav_copy_step(cl,o,512);
        if(result!=CON_COPY_MORE)charmap_nav_copy_finish(cl,o,result);
    }
    if(s->nav_seed){
        enum con_seed_result result=con_seed_step(&s->seed,1024);
        if(result==CON_SEED_MORE)return;
        s->nav_seed=FALSE;
        if(result==CON_SEED_DONE){
            struct con_nav_cursor origin=s->nav.cursor;
            memcpy(s->query,s->seed.query,s->seed.length*sizeof(ULONG));s->length=s->seed.length;
            s->reverse_search=s->seed_reverse;s->regex_mode=TRUE;s->history_at=0;
            s->nav.selecting=FALSE;s->nav.match_ready=FALSE;s->nav_view=FALSE;
            charmapcon_clear_selection(cl,o);
            charmap_search_remember(s);charmap_search_start(cl,o,s->reverse_search,FALSE);
            s->nav_anchor=s->scanning;
            if(s->nav_anchor){
                /* The source occurrence is always found forwards, even when
                 * the subsequent destination search runs backwards. */
                con_regex_ui_begin(&s->regex_selection.scan,data->top_of_scrollback,
                    s->query,s->length,con_regex_smart_fold(s->query,s->length),FALSE);
                con_search_anchor_begin(&s->seed_anchor,data->top_of_scrollback,origin.row,origin.column);
            }
        }else{
            s->nav_notice=result==CON_SEED_LIMIT?"Navigate: search text exceeds query limit":"Navigate: no search text";
            s->nav_dirty=TRUE;charmap_search_title(cl,o,"");
        }
        memset(&s->seed,0,sizeof(s->seed));return;
    }
    if(s->nav_mode && !s->nav_search && s->nav_dirty){
        if(!con_nav_step(&s->nav,1024))return;
        con_nav_match(&s->nav,&s->match,&s->first_row,&s->last_row);
        s->seek=data->top_of_scrollback;s->row=0;s->first_row=~(ULONG)0;
        s->view=NULL;s->locating=TRUE;s->nav_dirty=FALSE;
    }
    if(s->scanning && s->nav_anchor){
        struct con_search_match anchor;
        enum con_search_result result=con_search_anchor_step(&s->seed_anchor,&s->regex_selection.scan,1024,&anchor);
        if(result==CON_SEARCH_MORE)return;
        if(result==CON_SEARCH_FOUND){
            ULONG row=s->reverse_search?s->seed_anchor.result_row:s->seed_anchor.result_end_row;
            ULONG column=s->reverse_search?anchor.first.column:anchor.end.column;
            charmap_search_start(cl,o,s->reverse_search,FALSE);
            s->origin_search.origin_row=row;s->origin_search.origin_column=column;
            s->origin_search.inclusive=!s->reverse_search;
        }else{
            s->scanning=s->nav_search=s->nav_anchor=FALSE;
            memset(&s->seed_anchor,0,sizeof(s->seed_anchor));
            s->nav_notice="Navigate: not found";s->nav_dirty=TRUE;charmap_search_title(cl,o,"");
        }
        return;
    }
    if (s->scanning)
    {
        enum con_search_result result = s->nav_search ?
            con_search_origin_step(&s->origin_search,&s->selection.scan,&s->regex_selection.scan,1024,&s->match) : s->regex_mode ?
            con_regex_select_step(&s->regex_selection, 1024, &s->match) :
            con_search_select_step(&s->selection, 1024, &s->match);
        if (result == CON_SEARCH_MORE) return;
        s->scanning = FALSE;
        if (result == CON_SEARCH_DONE)
        {
            if(s->nav_search){
                s->nav_search=FALSE;s->nav_query_miss=TRUE;
                if(s->nav_query){s->nav=s->nav_origin;s->nav_view=TRUE;s->nav_view_row=s->nav_origin_view_row;s->nav_dirty=TRUE;}
                if(s->nav_confirm)charmap_nav_query_finish(cl,o,FALSE);
            }
            charmap_search_title(cl, o, "not found");
            return;
        }
        if(s->nav_search){
            s->nav.cursor=(struct con_nav_cursor){s->match.first.line,s->origin_search.result_row,s->match.first.column};
            con_nav_clamp(&s->nav);s->nav.pending=FALSE;s->nav.match_ready=FALSE;
        }
        s->seek = data->top_of_scrollback;
        s->row = 0;
        s->first_row = ~(ULONG)0;
        s->view = NULL;
        s->locating = TRUE;
    }
    if (s->locating)
    {
        for (ULONG budget = 1024; budget && s->seek; budget--)
        {
            if (s->seek == s->match.first.line) s->first_row = s->row;
            ULONG desired = s->first_row;
            if (s->nav_mode)
            {
                desired = s->nav_view ? s->nav_view_row : data->scrollback_pos;
                if (s->nav.cursor.row < desired) desired = s->nav.cursor.row;
                else if (s->nav.cursor.row - desired > (ULONG)CHAR_YMAX(o))
                    desired = s->nav.cursor.row - (ULONG)CHAR_YMAX(o);
            }
            if (s->row <= desired && s->row <= charmap_history_limit(data))
            {
                s->view = s->seek;
                s->view_row = s->row;
            }
            if (s->seek == s->match.end.line)
            {
                s->last_row = s->row;
                s->found = TRUE;
                s->locating = FALSE;
                s->seek = NULL;
                charmap_search_view(cl, o, s);
                s->nav_view=FALSE;
                if(s->nav_search){
                    s->nav.cursor=(struct con_nav_cursor){s->match.first.line,s->first_row,s->match.first.column};
                    con_nav_clamp(&s->nav);s->nav.pending=FALSE;s->nav.match_ready=FALSE;s->nav_search=FALSE;
                    if(s->nav_confirm)charmap_nav_query_finish(cl,o,FALSE);
                    else if(!s->nav_query)s->nav_dirty=TRUE;
                }
                charmap_search_title(cl, o, s->nav_query ? "found; Enter confirm / Esc cancel" : "found; Enter next / Shift+Enter previous / Esc close");
                return;
            }
            s->seek = s->seek->next;
            s->row++;
        }
    }
}

static VOID charmap_search_paste(Class *cl, Object *o, struct P_Console_QueryPaste *msg)
{
    struct charmapcondata *data = INST_DATA(cl, o);
    struct charmap_search_ui *s = data->search;
    if (!s || !s->active || s->revision != msg->Revision) return;
    if (CON_IS_OBSCURED(o) || WINDOW(o)->Title != (UBYTE *)s->title)
    {
        charmap_search_close(cl, o, FALSE);
        return;
    }
    if (msg->Valid && con_search_append(s->query, &s->length, msg->Data, msg->Length, msg->UTF8))
    {
        s->history_at = 0;
        charmap_search_start(cl, o, s->reverse_search, FALSE);
    }
    else
    {
        s->paste_status = "paste rejected: encoding or query limit";
        charmap_search_title(cl, o, s->paste_status);
    }
}

/* Unicode White_Space boundaries; query indices are scalar values, not bytes. */
static BOOL charmap_search_space(ULONG cp)
{
    return (cp >= 9 && cp <= 13) || cp == 0x20 || cp == 0x85 ||
        cp == 0xa0 || cp == 0x1680 || (cp >= 0x2000 && cp <= 0x200a) ||
        cp == 0x2028 || cp == 0x2029 || cp == 0x202f || cp == 0x205f || cp == 0x3000;
}

static VOID charmap_search_delete_word(struct charmap_search_ui *s)
{
    while (s->length && charmap_search_space(s->query[s->length - 1])) s->length--;
    while (s->length && !charmap_search_space(s->query[s->length - 1])) s->length--;
}

/* Explicit scrolling preserves the cursor's screen row for pages; line
 * scrolling only clamps it into the new viewport. Row seeking stays bounded. */
static VOID charmap_nav_scroll(Class *cl,Object *o,ULONG amount,BOOL up,BOOL page)
{
    struct charmapcondata *data=INST_DATA(cl,o);struct charmap_search_ui *s=data->search;
    ULONG limit=charmap_history_limit(data),view=data->scrollback_pos;
    ULONG height=(ULONG)CHAR_YMAX(o)+1,target=s->nav.cursor.row;
    if(view>limit)view=limit;
    view=up?(amount>view?0:view-amount):(amount>limit-view?limit:view+amount);
    if(page)target=up?(amount>target?0:target-amount):
        (amount>~(ULONG)0-target?~(ULONG)0:target+amount);
    if(target<view)target=view;
    else if(target-view>=height)target=view+height-1;
    s->nav.target_row=target;con_nav_move(&s->nav,page?CON_NAV_VIEW_ROW:CON_NAV_ROW);
    s->nav_view=TRUE;s->nav_view_row=view;s->nav_dirty=TRUE;s->found=s->locating=FALSE;
    charmap_search_title(cl,o,"");
}

static VOID charmap_nav_select(Class *cl,Object *o,BOOL linewise,BOOL blockwise,BOOL semanticwise)
{
    struct charmapcondata *data=INST_DATA(cl,o);struct charmap_search_ui *s=data->search;
    if(!s->nav.selecting){s->nav.anchor=s->nav.cursor;s->nav.selecting=TRUE;}
    else if(s->nav.linewise==linewise && s->nav.blockwise==blockwise && s->nav.semanticwise==semanticwise)s->nav.selecting=FALSE;
    s->nav.linewise=linewise;s->nav.blockwise=blockwise;s->nav.semanticwise=semanticwise;s->nav.match_ready=FALSE;s->nav.match_phase=0;
    s->nav_dirty=TRUE;s->found=s->locating=FALSE;charmap_search_title(cl,o,"");
}

static ULONG charmap_binding_action(Class *cl,Object *o,const struct InputEvent *event)
{
    struct charmapcondata *data=INST_DATA(cl,o);struct charmap_search_ui *s=data->search;
    /* Legacy profiles excluded query editing from navigation. Version 19 makes
     * Navigation and Search independent, matching combined mode conditions. */
    BOOL navigation=s && s->active && s->nav_mode &&
        (!s->nav_query || ICU(o)->bindings.bp_Version>=19);
    ULONG context=navigation?CONSOLENG_BIND_NAVONLY:0;
    if(s && s->active && (!s->nav_mode || s->nav_query))context|=CONSOLENG_BIND_SEARCHMODE;
    if(ICU(o)->keyboardAlternate)context|=CONSOLENG_BIND_ALTSCREEN;
    if(ICU(o)->conFlags&CF_APPLICATION_CURSOR)context|=CONSOLENG_BIND_APPCURSOR;
    if(ICU(o)->conFlags&CF_APPLICATION_KEYPAD)context|=CONSOLENG_BIND_APPKEYPAD;
    return con_binding_action_in_context(&ICU(o)->bindings,event,ConsoleDevice,context);
}

/* Shared by the built-in navigation keys and configurable single-shot actions. */
static VOID charmap_nav_action(Class *cl,Object *o,ULONG action)
{
    struct charmapcondata *data=INST_DATA(cl,o);struct charmap_search_ui *s=data->search;
    if(!s || !s->active || !s->nav_mode || s->nav_dirty || s->locating ||
       s->nav_find_pending || s->nav_seed || s->nav_search || s->scanning)return;
    if(action>=CONSOLENG_BIND_NAVSELECTCHAR && action<=CONSOLENG_BIND_NAVSELECTSEMANTIC){
        charmap_nav_select(cl,o,action==CONSOLENG_BIND_NAVSELECTLINE,
            action==CONSOLENG_BIND_NAVSELECTBLOCK,action==CONSOLENG_BIND_NAVSELECTSEMANTIC);
    } else if(action==CONSOLENG_BIND_NAVOPENLINK){
        UBYTE url[1024];
        if(con_url_at((struct charmap_line *)s->nav.cursor.line,s->nav.cursor.column,url,sizeof(url)) &&
           !con_open_url(url))DisplayBeep(WINDOW(o)->WScreen);
    } else if(action==CONSOLENG_BIND_NAVCENTER){
        ULONG height=(ULONG)CHAR_YMAX(o)+1,middle=height>1?height/2-1:0;
        ULONG view=s->nav.cursor.row>middle?s->nav.cursor.row-middle:0;
        ULONG limit=charmap_history_limit(data);
        s->nav_view=TRUE;s->nav_view_row=view<limit?view:limit;
        s->nav_dirty=TRUE;s->found=FALSE;charmap_search_title(cl,o,"");
    }
}

/* Navigation shares search's cancellation and bounded row-location path. */
static VOID charmap_nav_key(Class *cl,Object *o,struct InputEvent *event)
{
    struct charmapcondata *data=INST_DATA(cl,o);struct charmap_search_ui *s=data->search;
    enum con_nav_motion motion;
    s->nav_copy_clear=FALSE;
    UWORD code=event->ie_Code&127;struct InputEvent plain=*event;UBYTE text[8];
    plain.ie_Qualifier=0;LONG n=MapRawKey(&plain,text,sizeof(text),NULL);
    BOOL shifted=!!(event->ie_Qualifier&(IEQUALIFIER_LSHIFT|IEQUALIFIER_RSHIFT));
    ULONG action=charmap_binding_action(cl,o,event);
    BOOL toggle=action==CONSOLENG_BIND_NAVIGATE || (!action && code==0x40 &&
        shifted && (event->ie_Qualifier&IEQUALIFIER_CONTROL) &&
        !(event->ie_Qualifier&(IEQUALIFIER_LALT|IEQUALIFIER_RALT|IEQUALIFIER_LCOMMAND|IEQUALIFIER_RCOMMAND)));
    if(s->nav_find_pending){
        if(code==RAWKEY_ESCAPE){s->nav_find_pending=FALSE;charmap_search_title(cl,o,"");return;}
        if(n==1 && text[0]=='c' && (event->ie_Qualifier&IEQUALIFIER_CONTROL)){
            charmap_search_close(cl,o,TRUE);return;
        }
        if(!(event->ie_Qualifier&(IEQUALIFIER_CONTROL|IEQUALIFIER_LCOMMAND|IEQUALIFIER_RCOMMAND))){
            UBYTE mapped[16];LONG count=MapRawKey(event,mapped,sizeof(mapped),NULL);
            if(count==1 && mapped[0]>=32 && mapped[0]!=127 && mapped[0]!=155){
                s->nav.find_char=mapped[0];s->nav_find_pending=FALSE;
                con_nav_move(&s->nav,CON_NAV_FIND);s->nav.reverse=s->nav.find_reverse;
                s->nav_view=FALSE;s->nav_dirty=TRUE;s->found=s->locating=FALSE;
                charmap_search_title(cl,o,"");
            }
        }
        return;
    }
    if(toggle && (event->ie_Qualifier&IEQUALIFIER_REPEAT))return;
    s->nav_notice=NULL;
    if(s->nav_seed){
        s->nav_seed=FALSE;memset(&s->seed,0,sizeof(s->seed));s->nav_dirty=TRUE;
    }
    /* A new navigation action supersedes an in-flight repeat search. */
    if(s->nav_search){
        s->nav_anchor=FALSE;memset(&s->seed_anchor,0,sizeof(s->seed_anchor));
        s->revision++;s->nav_search=s->scanning=s->locating=FALSE;s->seek=NULL;
        memset(&s->origin_search,0,sizeof(s->origin_search));
        con_search_select_cancel(&s->selection);con_regex_select_cancel(&s->regex_selection);
    }
    if(action>=CONSOLENG_BIND_NAVLEFT && action<=CONSOLENG_BIND_NAVOCCUPIED){
        static const enum con_nav_motion motions[]={CON_NAV_LEFT,CON_NAV_RIGHT,CON_NAV_UP,CON_NAV_DOWN,CON_NAV_FIRST,CON_NAV_LAST,CON_NAV_TOP,CON_NAV_BOTTOM,CON_NAV_WORD_NEXT,CON_NAV_WORD_END,CON_NAV_WORD_PREV,CON_NAV_SEMANTIC_NEXT,CON_NAV_SEMANTIC_END,CON_NAV_SEMANTIC_PREV,CON_NAV_PARAGRAPH_UP,CON_NAV_PARAGRAPH_DOWN,CON_NAV_BRACKET,CON_NAV_OCCUPIED};
        motion=motions[action-CONSOLENG_BIND_NAVLEFT];
        goto apply_motion;
    }
    if(action>=CONSOLENG_BIND_NAVVIEWTOP && action<=CONSOLENG_BIND_NAVVIEWBOTTOM){
        ULONG height=(ULONG)CHAR_YMAX(o)+1;
        ULONG offset=action==CONSOLENG_BIND_NAVVIEWTOP?0:action==CONSOLENG_BIND_NAVVIEWBOTTOM?height-1:(height>1?height/2-1:0);
        s->nav.target_row=data->scrollback_pos+offset;motion=CON_NAV_VIEW_ROW;
        goto apply_motion;
    }
    if(action>=CONSOLENG_BIND_NAVPAGEUP && action<=CONSOLENG_BIND_NAVLINEDOWN){
        ULONG height=(ULONG)CHAR_YMAX(o)+1;
        BOOL line=action>=CONSOLENG_BIND_NAVLINEUP;
        ULONG amount=line?1:action>=CONSOLENG_BIND_NAVHALFUP?height/2:height;
        BOOL up=action==CONSOLENG_BIND_NAVPAGEUP || action==CONSOLENG_BIND_NAVHALFUP || action==CONSOLENG_BIND_NAVLINEUP;
        charmap_nav_scroll(cl,o,amount,up,!line);return;
    }
    if(action>=CONSOLENG_BIND_NAVFINDFORWARD && action<=CONSOLENG_BIND_NAVTILLBACKWARD)goto start_inline_find;
    if(action==CONSOLENG_BIND_NAVFINDREPEAT || action==CONSOLENG_BIND_NAVFINDREVERSE)goto repeat_inline_find;
    if(action==CONSOLENG_BIND_NAVSEARCHFORWARD || action==CONSOLENG_BIND_NAVSEARCHBACKWARD)goto start_nav_query;
    if(action==CONSOLENG_BIND_NAVSEEDFORWARD || action==CONSOLENG_BIND_NAVSEEDBACKWARD)goto seed_nav_query;
    if(action==CONSOLENG_BIND_NAVSEARCHNEXT || action==CONSOLENG_BIND_NAVSEARCHPREVIOUS)goto repeat_nav_query;
    if(toggle || (n==1 && text[0]=='i' && !(event->ie_Qualifier&(IEQUALIFIER_CONTROL|IEQUALIFIER_LSHIFT|IEQUALIFIER_RSHIFT|IEQUALIFIER_LALT|IEQUALIFIER_RALT|IEQUALIFIER_LCOMMAND|IEQUALIFIER_RCOMMAND))) ||
       (n==1 && text[0]=='c' && (event->ie_Qualifier&IEQUALIFIER_CONTROL) && !(event->ie_Qualifier&(IEQUALIFIER_LSHIFT|IEQUALIFIER_RSHIFT|IEQUALIFIER_LALT|IEQUALIFIER_RALT|IEQUALIFIER_LCOMMAND|IEQUALIFIER_RCOMMAND)))){
        /* Ctrl+C leaves history visible. Insert/toggle returns to the saved
         * live view without walking retained rows or changing app coordinates. */
        BOOL restored=(toggle || text[0]=='i') && charmap_restore_live(data);
        charmap_search_close(cl,o,TRUE);
        if(restored){
            charmapcon_adj_prop(cl,o);
            if(!CON_IS_OBSCURED(o))Console_RenderCursor(o);
        }
        return;
    }
    if(code==RAWKEY_ESCAPE){
        s->nav.selecting=FALSE;s->nav_dirty=TRUE;s->found=FALSE;
        charmapcon_clear_selection(cl,o);charmap_search_title(cl,o,"");return;
    }
    BOOL copy=action==CONSOLENG_BIND_COPY || (n==1&&text[0]=='y' && !shifted &&
        !(event->ie_Qualifier&(IEQUALIFIER_CONTROL|IEQUALIFIER_LALT|IEQUALIFIER_RALT|IEQUALIFIER_LCOMMAND|IEQUALIFIER_RCOMMAND)));
    if(copy){
        if(s->nav.selecting){
            charmap_nav_copy_cancel(s);
            s->copy_nav=s->nav;s->nav_copy=TRUE;s->nav_copy_clear=TRUE;
            /* Small ready selections still publish without waiting for redraw. */
            enum con_copy_result result=con_nav_step(&s->copy_nav,0)?charmap_nav_copy_step(cl,o,64):CON_COPY_MORE;
            if(result!=CON_COPY_MORE)charmap_nav_copy_finish(cl,o,result);
            charmap_search_title(cl,o,"");
        }
        return;
    }
    if(n==1 && text[0]=='v' && (event->ie_Qualifier&(IEQUALIFIER_LALT|IEQUALIFIER_RALT)) &&
       !(event->ie_Qualifier&(IEQUALIFIER_CONTROL|IEQUALIFIER_LSHIFT|IEQUALIFIER_RSHIFT|IEQUALIFIER_LCOMMAND|IEQUALIFIER_RCOMMAND))){
        if(!(event->ie_Qualifier&IEQUALIFIER_REPEAT))charmap_nav_select(cl,o,FALSE,FALSE,TRUE);
        return;
    }
    if(n==1 && (event->ie_Qualifier&IEQUALIFIER_CONTROL) && !shifted &&
       !(event->ie_Qualifier&(IEQUALIFIER_LALT|IEQUALIFIER_RALT|IEQUALIFIER_LCOMMAND|IEQUALIFIER_RCOMMAND))){
        UBYTE cp=text[0];ULONG height=(ULONG)CHAR_YMAX(o)+1;
        if(cp=='v'){
            if(!(event->ie_Qualifier&IEQUALIFIER_REPEAT))charmap_nav_select(cl,o,FALSE,TRUE,FALSE);
            return;
        }
        if(cp=='b'||cp=='f'||cp=='u'||cp=='d'||cp=='y'||cp=='e'){
            ULONG amount=(cp=='y'||cp=='e')?1:(cp=='u'||cp=='d')?height/2:height;
            charmap_nav_scroll(cl,o,amount,cp=='b'||cp=='u'||cp=='y',cp!='y'&&cp!='e');return;
        }
    }
    if(event->ie_Qualifier&(IEQUALIFIER_CONTROL|IEQUALIFIER_LALT|IEQUALIFIER_RALT|
        IEQUALIFIER_LCOMMAND|IEQUALIFIER_RCOMMAND))return;
    if((code==RAWKEY_RETURN || code==RAWKEY_KP_ENTER) && !shifted){
        if(!(event->ie_Qualifier&IEQUALIFIER_REPEAT))charmap_nav_action(cl,o,CONSOLENG_BIND_NAVOPENLINK);
        return;
    }
    if(n==1 && text[0]=='z' && !shifted){
        charmap_nav_action(cl,o,CONSOLENG_BIND_NAVCENTER);return;
    }
    if(n==1 && (text[0]=='f'||text[0]=='t')){
start_inline_find:
        if(event->ie_Qualifier&IEQUALIFIER_REPEAT)return;
        s->nav_find_pending=TRUE;s->nav.find_char=0;
        s->nav.find_reverse=action>=CONSOLENG_BIND_NAVFINDFORWARD ? (action==CONSOLENG_BIND_NAVFINDBACKWARD || action==CONSOLENG_BIND_NAVTILLBACKWARD) : shifted;
        s->nav.find_short=action>=CONSOLENG_BIND_NAVFINDFORWARD ? action>=CONSOLENG_BIND_NAVTILLFORWARD : text[0]=='t';
        charmap_search_title(cl,o,"");return;
    }
    if(n==1 && !shifted && (text[0]==';'||text[0]==',')){
repeat_inline_find:
        if(s->nav.find_char){
            BOOL reverse=action>=CONSOLENG_BIND_NAVFINDFORWARD ? action==CONSOLENG_BIND_NAVFINDREVERSE : text[0]==',';
            con_nav_move(&s->nav,CON_NAV_FIND);s->nav.reverse=s->nav.find_reverse!=reverse;
            s->nav_view=FALSE;s->nav_dirty=TRUE;s->found=s->locating=FALSE;
            charmap_search_title(cl,o,"");
        }
        return;
    }
    if(n==1 && shifted && (text[0]=='8'||text[0]=='3')){
seed_nav_query:
        s->revision++;s->nav_seed=TRUE;s->seed_reverse=action>=CONSOLENG_BIND_NAVSEARCHFORWARD ? action==CONSOLENG_BIND_NAVSEEDBACKWARD : text[0]=='3';
        s->scanning=s->locating=s->found=FALSE;s->seek=NULL;
        con_seed_begin(&s->seed,&s->nav,data->top_of_scrollback);
        charmap_search_title(cl,o,"");return;
    }
    if(n==1 && text[0]=='/'){
start_nav_query:
        s->nav_origin=s->nav;s->nav_origin.pending=FALSE;s->nav_origin.match_ready=FALSE;s->nav_origin_view=data->top_of_window;s->nav_origin_view_row=data->scrollback_pos;
        s->nav_query=TRUE;s->nav_confirm=s->nav_query_miss=FALSE;
        s->length=0;s->history_at=0;s->reverse_search=action>=CONSOLENG_BIND_NAVSEARCHFORWARD ? action==CONSOLENG_BIND_NAVSEARCHBACKWARD : shifted;s->regex_mode=TRUE;
        charmap_search_start(cl,o,s->reverse_search,FALSE);return;
    }
    if(n==1 && text[0]=='n'){
repeat_nav_query:
        if(s->length)charmap_search_start(cl,o,s->reverse_search!=(action>=CONSOLENG_BIND_NAVSEARCHFORWARD ? action==CONSOLENG_BIND_NAVSEARCHPREVIOUS : shifted),TRUE);
        return;
    }
    if(n==1 && shifted && (text[0]=='h' || text[0]=='m' || text[0]=='l')){
        ULONG height=(ULONG)CHAR_YMAX(o)+1;
        ULONG offset=text[0]=='h'?0:text[0]=='l'?height-1:(height>1?height/2-1:0);
        s->nav.target_row=data->scrollback_pos+offset;motion=CON_NAV_VIEW_ROW;
    }
    else if(code==RAWKEY_LEFT || (n==1&&text[0]=='h'))motion=CON_NAV_LEFT;
    else if(code==RAWKEY_RIGHT || (n==1&&text[0]=='l'))motion=CON_NAV_RIGHT;
    else if(code==RAWKEY_UP || (n==1&&text[0]=='k'))motion=CON_NAV_UP;
    else if(code==RAWKEY_DOWN || (n==1&&text[0]=='j'))motion=CON_NAV_DOWN;
    else if(n==1&&text[0]=='0'&&!shifted)motion=CON_NAV_FIRST;
    else if(n==1&&text[0]=='4'&&shifted)motion=CON_NAV_LAST;
    else if(n==1&&text[0]=='g')motion=shifted?CON_NAV_BOTTOM:CON_NAV_TOP;
    else if(n==1&&text[0]=='w')motion=shifted?CON_NAV_WORD_NEXT:CON_NAV_SEMANTIC_NEXT;
    else if(n==1&&text[0]=='e')motion=shifted?CON_NAV_WORD_END:CON_NAV_SEMANTIC_END;
    else if(n==1&&text[0]=='b')motion=shifted?CON_NAV_WORD_PREV:CON_NAV_SEMANTIC_PREV;
    else if(n==1&&text[0]=='['&&shifted)motion=CON_NAV_PARAGRAPH_UP;
    else if(n==1&&text[0]==']'&&shifted)motion=CON_NAV_PARAGRAPH_DOWN;
    else if(n==1&&text[0]=='5'&&shifted)motion=CON_NAV_BRACKET;
    else if(n==1&&text[0]=='6'&&shifted)motion=CON_NAV_OCCUPIED;
    else if(n==1&&text[0]=='v'&&!(event->ie_Qualifier&IEQUALIFIER_REPEAT)){
        charmap_nav_select(cl,o,shifted,FALSE,FALSE);return;
    }else return;
apply_motion:
    s->nav_view=FALSE;con_nav_move(&s->nav,motion);
    /* Single-cell actions must not overwrite each other between timer ticks.
     * They take exactly one bounded step; redraw and selection expansion stay
     * on the timer like longer motions. */
    if(motion<=CON_NAV_DOWN)con_nav_step(&s->nav,1);
    s->nav_dirty=TRUE;s->found=s->locating=FALSE;
}

static BOOL charmap_search_key(Class *cl, Object *o, struct InputEvent *event)
{
    struct charmapcondata *data = INST_DATA(cl, o);
    struct charmap_search_ui *s = data->search;
    if (s && s->active && WINDOW(o)->Title != (UBYTE *)s->title)
        charmap_search_close(cl, o, TRUE);
    UWORD code = event->ie_Code & 0x7f;
    UBYTE mask = 1U << (code & 7);
    BOOL up = !!(event->ie_Code & IECODE_UP_PREFIX);
    if (s && (s->swallowed[code >> 3] & mask))
    {
        if (up) {
            s->swallowed[code >> 3] &= ~mask;
            return TRUE;
        }
        if (event->ie_Qualifier & IEQUALIFIER_REPEAT) {
            if (!s->active) return TRUE;
        } else {
            /* Focus loss or cancellation may discard the release. A new
             * physical press must not remain swallowed indefinitely. */
            s->swallowed[code >> 3] &= ~mask;
        }
    }
    if(s && s->active && !up &&
       charmap_binding_action(cl,o,event)==CONSOLENG_BIND_CLEARHISTORY){
        if(!(event->ie_Qualifier&IEQUALIFIER_REPEAT))charmap_history_clear_action(cl,o);
        s->swallowed[code>>3]|=mask;
        return TRUE;
    }
    if(s && s->active && !up) {
        ULONG action = charmap_binding_action(cl,o,event);
        if(action>=CONSOLENG_BIND_NAVLEFT && action<=CONSOLENG_BIND_NAVSEARCHPREVIOUS &&
           (!s->nav_mode || s->nav_query)) {
            s->swallowed[code>>3]|=mask;return TRUE;
        }
        if(action>=CONSOLENG_BIND_NAVCENTER && action<=CONSOLENG_BIND_NAVSELECTSEMANTIC) {
            if(!(event->ie_Qualifier&IEQUALIFIER_REPEAT))charmap_nav_action(cl,o,action);
            s->swallowed[code>>3]|=mask;
            return TRUE;
        }
        if(action==CONSOLENG_BIND_CLEARSELECTION) {
            if(!(event->ie_Qualifier&IEQUALIFIER_REPEAT))charmap_selection_clear_action(cl,o);
            s->swallowed[code>>3]|=mask;
            return TRUE;
        }
        if(action >= CONSOLENG_BIND_FONTLARGER && action <= CONSOLENG_BIND_FONTRESET) {
            if(!(event->ie_Qualifier&IEQUALIFIER_REPEAT)) {
                IPTR zoom = action;
                Console_DoCommand(o,C_ZOOM_FONT,1,&zoom);
            }
            s->swallowed[code>>3]|=mask;
            return TRUE;
        }
    }
    if (!s || !s->active)
    {
        ULONG action = charmap_binding_action(cl,o,event);
        if(!action && code==0x40 && (event->ie_Qualifier&IEQUALIFIER_CONTROL) &&
           (event->ie_Qualifier&(IEQUALIFIER_LSHIFT|IEQUALIFIER_RSHIFT)) &&
           !(event->ie_Qualifier&(IEQUALIFIER_LALT|IEQUALIFIER_RALT|IEQUALIFIER_LCOMMAND|IEQUALIFIER_RCOMMAND)))
            action=CONSOLENG_BIND_NAVIGATE;
        if (up || (action != CONSOLENG_BIND_SEARCH &&
            action != CONSOLENG_BIND_SEARCH_BACKWARD && action != CONSOLENG_BIND_NAVIGATE)) return FALSE;
        if (!s)
        {
            s = AllocMem(sizeof(*s), MEMF_PUBLIC | MEMF_CLEAR);
            if (!s) return FALSE;
            data->search = s;
        }
        s->active = TRUE;
        s->reverse_search = action == CONSOLENG_BIND_SEARCH_BACKWARD;
        s->length = 0;
        s->history_at = 0;
        s->saved_title = WINDOW(o)->Title;
        charmapcon_clear_selection(cl, o);
        s->nav_mode=action==CONSOLENG_BIND_NAVIGATE;
        if(s->nav_mode){
            /* The application cursor is relative to the live screen, even
             * while retained history is displayed. Only walk/allocate within
             * the visible grid; never traverse the history to enter mode. */
            ULONG live=data->unrendered?data->saved_scrollback_pos:data->scrollback_pos;
            ULONG offset=0,column=0;
            if(live>=data->scrollback_pos &&
               live-data->scrollback_pos<=(ULONG)CHAR_YMAX(o) &&
               (ULONG)YCP<=(ULONG)CHAR_YMAX(o)-(live-data->scrollback_pos)){
                offset=live-data->scrollback_pos+(ULONG)YCP;
                column=(ULONG)XCP;
            }
            struct charmap_line *entry=charmapcon_find_line(cl,o,offset);
            if(!entry){charmap_search_close(cl,o,FALSE);s->swallowed[code>>3]|=mask;return TRUE;}
            con_nav_begin(&s->nav,entry,data->scrollback_pos+offset,column,CHAR_XMAX(o)+1);
            con_nav_set_delimiters(&s->nav,ICU(o)->semanticPrefs.sp_Chars,ICU(o)->semanticPrefs.sp_Count);
            s->nav_dirty=TRUE;s->found=s->scanning=s->locating=FALSE;
            charmap_search_title(cl,o,"");
        }else charmap_search_start(cl, o, s->reverse_search, FALSE);
    }
    else if (!up && s->nav_mode && !s->nav_query)charmap_nav_key(cl,o,event);
    else if (!up)
    {
        struct InputEvent plain = *event;
        UBYTE binding[8];
        plain.ie_Qualifier = 0;
        LONG n = MapRawKey(&plain, binding, sizeof(binding), NULL);
        ULONG action = charmap_binding_action(cl,o,event);
        BOOL paste = action == CONSOLENG_BIND_PASTE;
        /* Preserve the established Ctrl+Shift+V query-editing alias while
         * Right-Amiga+V is configured. Removing/remapping that binding also
         * removes its alias; explicit assignments always take precedence. */
        if (!action && n == 1 && (binding[0] == 'v' || binding[0] == 'V') &&
            (event->ie_Qualifier & IEQUALIFIER_CONTROL) &&
            (event->ie_Qualifier & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)) &&
            !(event->ie_Qualifier & (IEQUALIFIER_LALT | IEQUALIFIER_RALT |
                IEQUALIFIER_LCOMMAND | IEQUALIFIER_RCOMMAND)))
        {
            for (ULONG i=0; i<ICU(o)->bindings.bp_Count; i++) {
                const struct ConsolengKeyBinding *b=&ICU(o)->bindings.bp_Entries[i];
                if (b->kb_Key == 'v' && b->kb_Modifiers == CONSOLENG_BIND_RAMIGA &&
                    b->kb_Action == CONSOLENG_BIND_PASTE) { paste=TRUE; break; }
            }
        }
        if (paste)
        {
            s->revision++;
            s->paste_status = con_query_clipboard_request(ConsoleDevice, o, s->revision)
                ? "pasting" : "clipboard busy";
            charmap_search_title(cl, o, s->paste_status);
        }
        else if (!action && n == 1 && (binding[0] == 'r' || binding[0] == 'R') &&
            (event->ie_Qualifier & IEQUALIFIER_CONTROL) &&
            !(event->ie_Qualifier & (IEQUALIFIER_LALT | IEQUALIFIER_RALT |
                IEQUALIFIER_LCOMMAND | IEQUALIFIER_RCOMMAND | IEQUALIFIER_REPEAT)))
        {
            s->regex_mode = !s->regex_mode;
            charmap_search_start(cl, o, s->reverse_search, FALSE);
        }
        else if ((event->ie_Qualifier & IEQUALIFIER_CONTROL) &&
            !(event->ie_Qualifier & (IEQUALIFIER_LALT | IEQUALIFIER_RALT |
                IEQUALIFIER_LCOMMAND | IEQUALIFIER_RCOMMAND)) &&
            (code == RAWKEY_BACKSPACE ||
             (n == 1 && (binding[0] == 'w' || binding[0] == 'W' ||
                         binding[0] == 'u' || binding[0] == 'U'))))
        {
            if (code != RAWKEY_BACKSPACE && (binding[0] == 'u' || binding[0] == 'U'))
                s->length = 0;
            else charmap_search_delete_word(s);
            s->history_at = 0;
            charmap_search_start(cl, o, s->reverse_search, FALSE);
        }
        else if ((code == RAWKEY_UP || code == RAWKEY_DOWN) &&
            !(event->ie_Qualifier & (IEQUALIFIER_CONTROL | IEQUALIFIER_LSHIFT |
                IEQUALIFIER_RSHIFT | IEQUALIFIER_LALT | IEQUALIFIER_RALT |
                IEQUALIFIER_LCOMMAND | IEQUALIFIER_RCOMMAND)))
        {
            if (charmap_search_recall(s, code == RAWKEY_UP))
                charmap_search_start(cl, o, s->reverse_search, FALSE);
        }
        else if (code == RAWKEY_ESCAPE || (s->nav_query && n==1 && binding[0]=='c' && (event->ie_Qualifier&IEQUALIFIER_CONTROL))) {
            if(s->nav_query)charmap_nav_query_finish(cl,o,TRUE);else charmap_search_accept(cl,o);
        }
        else if (code == RAWKEY_RETURN || code == RAWKEY_KP_ENTER) {
            if(s->nav_query){
                if(s->scanning||s->locating)s->nav_confirm=TRUE;
                else charmap_nav_query_finish(cl,o,FALSE);
            }else charmap_search_start(cl, o,
                s->reverse_search != !!(event->ie_Qualifier & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)), TRUE);
        }
        else if (code == RAWKEY_BACKSPACE)
        {
            if (s->length) s->length--;
            s->history_at = 0;
            charmap_search_start(cl, o, s->reverse_search, FALSE);
        }
        else if (!(event->ie_Qualifier & (IEQUALIFIER_CONTROL | IEQUALIFIER_LCOMMAND | IEQUALIFIER_RCOMMAND)))
        {
            UBYTE mapped[16];
            LONG n = MapRawKey(event, mapped, sizeof(mapped), NULL);
            if (n == 1 && mapped[0] >= 32 && mapped[0] != 127 && mapped[0] != 155 && s->length < CON_SEARCH_MAX)
            {
                s->query[s->length++] = mapped[0];
                s->history_at = 0;
                charmap_search_start(cl, o, s->reverse_search, FALSE);
            }
        }
    }
    if (!up) s->swallowed[code >> 3] |= mask;
    return TRUE;
}

/* Encoded byte length of a cell range: UTF-8 in CF_UTF8 units, one
   Latin-1 byte per cell otherwise (higher codepoints become '?'). */
static ULONG enc_cells_len(const ULONG *cells, ULONG n, BOOL utf8)
{
    ULONG i, size = 0;
    UBYTE tmp[4];

    for (i = 0; i < n; i++)
    {
        if (cells[i] == CELL_WIDE_CONT)
            continue;
        size += utf8 ? con_utf8_encode(tmp, cells[i]) : 1;
    }

    return size;
}

static char *enc_cells(char *dst, const ULONG *cells, ULONG n, BOOL utf8)
{
    ULONG i;

    for (i = 0; i < n; i++)
    {
        if (cells[i] == CELL_WIDE_CONT)
            continue;
        if (utf8)
            dst += con_utf8_encode((UBYTE *) dst, cells[i]);
        else
            *dst++ = cells[i] < 0x100 ? (char)cells[i] : '?';
    }
    return dst;
}

/* A wrap spacer is layout, not text. Only trim it while its exact boundary
 * survives; explicit trailing spaces remain part of the copied line. */
static ULONG charmap_copy_columns(const struct charmap_line *line)
{
    ULONG end = line->size;
    while (end && line->text && line->text[end - 1] == ' ' &&
        line->flags && (line->flags[end - 1] == CHARMAP_ERASED)) end--;
    return end - (line->wrapColumns == end && line->wrapColumns &&
        line->wrapPadding && line->text && line->text[end - 1] == ' ');
}

static VOID charmap_selection_span(struct charmap_line *cur,
    struct charmap_line *first, struct charmap_line *last,
    ULONG minx, ULONG maxx, ULONG *start, ULONG *end, BOOL block)
{
    ULONG limit = charmap_copy_columns(cur);
    *start = cur == first ? minx : 0;
    *end = cur == last ? maxx : limit;
    if (first == last && *start > *end)
    {
        ULONG tmp = *start; *start = *end; *end = tmp;
    }
    *start = MIN(*start, limit);
    *end = MIN(*end, limit);
    if(block){
        *start=MIN(MIN(minx,maxx),limit);*end=MIN(MAX(minx,maxx),limit);
        charmap_block_span(cur,start,end);
        while(*end>*start && cur->text && charmap_search_space(cur->text[*end-1]))(*end)--;
    }
}

static ULONG charmapcon_calc_selection_size(struct charmap_line *first,
    struct charmap_line *last, ULONG minx, ULONG maxx, BOOL utf8, BOOL exact_end, BOOL block)
{
    struct charmap_line *cur = first;
    ULONG size = 0;
    while (cur)
    {
        ULONG start, end;
        charmap_selection_span(cur, first, last, minx, maxx, &start, &end, block);
        if (cur->text && end > start)
            size += enc_cells_len(cur->text + start, end - start, utf8);
        if (block ? cur != last : (!cur->wrapColumns && (cur != last || (!exact_end && (first == last ? MAX(minx, maxx) : maxx) >= cur->size))))
            size += 1;
        if (cur == last) break;
        cur = cur->next;
    }
    return size;
}

static char *charmapcon_get_selection(ULONG size,
    struct charmap_line *first, struct charmap_line *last,
    ULONG minx, ULONG maxx, BOOL utf8, BOOL exact_end, BOOL block)
{
    char *buf = AllocMem(size, MEMF_ANY), *bufpos = buf;
    struct charmap_line *cur = first;
    if (!buf) return NULL;
    while (cur)
    {
        ULONG start, end;
        charmap_selection_span(cur, first, last, minx, maxx, &start, &end, block);
        if (cur->text && end > start)
            bufpos = enc_cells(bufpos, cur->text + start, end - start, utf8);
        if (block ? cur != last : (!cur->wrapColumns && (cur != last || (!exact_end && (first == last ? MAX(minx, maxx) : maxx) >= cur->size))))
            *bufpos++ = '\r';
        if (cur == last) break;
        cur = cur->next;
    }
    return buf;
}

struct ConClipData
{
    ULONG flags;                /* always zero? */
    ULONG size;                 /* does not include NUL termination */
    APTR buffer;                /* NUL-terminated string! */
};

/* FIXME: Belongs in snipmapcon - here temporarily until refactored out
   selection code */
static VOID charmapcon_copy(Class *cl, Object *o, Msg copymsg)
{
    struct charmapcondata *data = INST_DATA(cl, o);
    char *buf;

    struct charmap_line *first, *last;
    ULONG minx, maxx, size;

    if(data->prepared_copy){
        buf=(char *)data->prepared_copy;size=data->prepared_copy_size;
        data->prepared_copy=NULL;data->prepared_copy_size=0;
    }else{
        /* Create a string from the contents of the scrollback buffer */
        if (data->select_y_min < data->select_y_max)
        {
            first = data->select_line_min;
            last = data->select_line_max;
            minx = data->select_x_min;
            maxx = data->select_x_max;
        }
        else
        {
            first = data->select_line_max;
            last = data->select_line_min;
            minx = data->select_x_max;
            maxx = data->select_x_min;
        }

        size = charmapcon_calc_selection_size(first, last, minx, maxx,
            CON_IS_UTF8(o) ? TRUE : FALSE, data->selection_exact_end, data->selection_block);
        if (!size) return;
        buf = charmapcon_get_selection(size, first, last, minx, maxx,
            CON_IS_UTF8(o) ? TRUE : FALSE, data->selection_exact_end, data->selection_block);
    }
    D(bug("%d bytes copied\n"));

    /* No selection/allocation failure must not destroy the previous snip. */
    if (!buf) return;

    if (IsListEmpty(&ConsoleDevice->sniphooks))
    {
        LONG queued = con_copy_clipboard_request(ConsoleDevice, (UBYTE *)buf, size);
        if (queued)
        {
            FreeMem(buf, size);
            return;
        }
    }

    ObtainSemaphore(&ConsoleDevice->copyBufferLock);
    FreeMem((APTR) ConsoleDevice->copyBuffer,
        ConsoleDevice->copyBufferSize);

    ConsoleDevice->copyBuffer = buf;
    if (ConsoleDevice->copyBuffer)
        ConsoleDevice->copyBufferSize = size;
    else
        ConsoleDevice->copyBufferSize = 0;

    if (!IsListEmpty(&ConsoleDevice->sniphooks))
    {
        /* OS2-3.x compatible conclip format */
        struct Hook *conhook;
        struct ConClipData ccd;

        D(bug("AOS conclip\n"));
        if (ConsoleDevice->copyBufferSize)
        {
            ccd.flags = 0;
            ccd.size = ConsoleDevice->copyBufferSize;
            /* must be NUL-terminated */
            ccd.buffer = AllocVec(ccd.size + 1, MEMF_CLEAR);
            if (ccd.buffer)
            {
                CopyMem(ConsoleDevice->copyBuffer, ccd.buffer, ccd.size);
                ForeachNode(&ConsoleDevice->sniphooks, conhook)
                {
                    D(bug("Calling AOS conclip hook %p\n", conhook));
                    CALLHOOKPKT(conhook, NULL, &ccd);
                }
                FreeVec(ccd.buffer);
            }
        }
    }
    ReleaseSemaphore(&ConsoleDevice->copyBufferLock);
}


/**********************************
**  CharMapCon::NewWindowSize()  **
**********************************/
/* Select retained text, including history, without selecting unused grid rows.
 * Selection Y coordinates stay relative to the current scrollback viewport. */
static BOOL charmapcon_select_all(Class *cl, Object *o)
{
    struct charmapcondata *data = INST_DATA(cl, o);
    struct charmap_line *last = NULL;
    ULONG row = 0, lastrow = 0;
    if (CON_IS_OBSCURED(o)) return FALSE;
    charmap_search_close(cl, o, TRUE);
    charmapcon_clear_selection(cl, o);
    for (struct charmap_line *line = data->top_of_scrollback; line;
        line = line->next, row++)
    {
        if (charmap_copy_columns(line)) { last = line; lastrow = row; }
    }
    if (!last) return TRUE;
    data->select_line_min = data->top_of_scrollback;
    data->select_line_max = last;
    data->select_x_min = 0;
    data->select_x_max = last->size;
    data->select_y_min = -(LONG)data->scrollback_pos;
    data->select_y_max = (LONG)lastrow - (LONG)data->scrollback_pos;
    data->active_selection = data->ignore_drag = FALSE;
    charmapcon_refresh_lines(cl, o, CHAR_YMIN(o), CHAR_YMAX(o));
    return TRUE;
}

static struct charmap_line *charmap_row_at(struct charmap_line *line, ULONG row)
{
    while (line && row--) line = line->next;
    return line;
}

static BOOL charmap_reflow_primary(Class *cl, Object *o,
    ULONG oldx, ULONG oldy, BOOL pending)
{
    struct charmapcondata *data = INST_DATA(cl, o);
    struct charmap_line *cursor, *saved, *tail, *extra = NULL, *extra_tail = NULL;
    struct charmap_line *head = NULL, *oldhead = data->top_of_scrollback;
    ULONG rows = 0, width = CHAR_XMAX(o) + 1, top, available = 0;
    ULONG needed = oldy + 1;
    BOOL ok;
    if (!data->top_of_window || data->alternate || CHAR_XMAX(o) < 0 || CHAR_YMAX(o) < 0)
        return FALSE;
    if (ICU(o)->savedCursor && ICU(o)->savedY >= needed)
        needed = ICU(o)->savedY + 1;
    /* A cursor can address blank rows which have never needed storage.
       Build that temporary suffix separately, and undo its attachment before
       publishing anything, including when the repacker runs out of memory. */
    for (tail = data->top_of_window; ; tail = tail->next)
    {
        available++;
        if (!tail->next) break;
    }
    while (available < needed)
    {
        struct charmap_line *line = charmap_newline(NULL, extra_tail);
        if (!line) { charmap_dispose_lines(extra); return FALSE; }
        if (!extra) extra = line;
        extra_tail = line;
        available++;
    }
    if (extra) { tail->next = extra; extra->prev = tail; }
    cursor = charmap_row_at(data->top_of_window, oldy);
    saved = ICU(o)->savedCursor ?
        charmap_row_at(data->top_of_window, ICU(o)->savedY) : NULL;
    struct charmap_anchor a[5] = {
        {cursor, oldx + !!pending, TRUE, NULL, 0, 0},
        {saved, ICU(o)->savedX + !!ICU(o)->savedWrapPending, TRUE, NULL, 0, 0},
        {data->top_of_window, 0, FALSE, NULL, 0, 0},
        {data->select_line_min, data->select_x_min, FALSE, NULL, 0, 0},
        {data->select_line_max, data->select_x_max, FALSE, NULL, 0, 0}
    };
    ok = charmap_reflow(ConsoleDevice, oldhead, width, a, 5, &head, &rows);
    if (extra)
    {
        tail->next = NULL; extra->prev = NULL;
        charmap_dispose_lines(extra);
    }
    if (!ok) return FALSE;
    /* Reflow may synthesize default-role blanks for a cursor beyond stored
     * text. Resolve those roles instead of drawing their placeholder pen 0. */
    UBYTE pens[CHARMAP_COLORPENS], identity[256];
    for (ULONG i = 0; i < 256; i++) identity[i] = i;
    charmapcon_colorpens(o, pens);
    charmap_remap_colors(head, identity, pens);
    top = a[2].newRow;
    if (a[0].newRow < top) top = a[0].newRow;
    if (a[0].newRow > top + CHAR_YMAX(o)) top = a[0].newRow - CHAR_YMAX(o);
    data->top_of_scrollback = head;
    data->top_of_window = charmap_row_at(head, top);
    data->scrollback_size = rows; data->scrollback_pos = top;
    data->saved_top_of_window = data->top_of_window;
    data->saved_scrollback_pos = top;
    XCP = XCCP = MIN(a[0].newColumn, width - 1);
    YCP = YCCP = a[0].newRow - top;
    ICU(o)->conFlags &= ~CF_WRAP_PENDING;
    if (a[0].newColumn == width) ICU(o)->conFlags |= CF_WRAP_PENDING;
    if (saved)
    {
        ICU(o)->savedWrapPending = a[1].newColumn == width &&
            a[1].newRow >= top && a[1].newRow <= top + CHAR_YMAX(o);
        ICU(o)->savedX = MIN(a[1].newColumn, width - 1);
        ICU(o)->savedY = MIN(MAX((LONG)a[1].newRow - (LONG)top, 0), CHAR_YMAX(o));
    }
    data->select_line_min = a[3].newLine ? a[3].newLine : data->top_of_window;
    data->select_line_max = a[4].newLine ? a[4].newLine : data->top_of_window;
    data->select_x_min = a[3].newLine ? a[3].newColumn : 0;
    data->select_x_max = a[4].newLine ? a[4].newColumn : 0;
    data->select_y_min = a[3].newLine ? (LONG)a[3].newRow - (LONG)top : 0;
    data->select_y_max = a[4].newLine ? (LONG)a[4].newRow - (LONG)top : 0;
    charmap_dispose_lines(oldhead);
    charmap_scroll_up(cl, o, 0); /* enforce the existing history budget */
    /* Trimming changes absolute offsets; the live snapshot must agree. */
    data->saved_top_of_window = data->top_of_window;
    data->saved_scrollback_pos = data->scrollback_pos;
    return TRUE;
}

static VOID charmapcon_newwindowsize(Class *cl, Object *o,
    struct P_Console_NewWindowSize *msg)
{
    struct charmapcondata *data = INST_DATA(cl, o);

    charmap_search_close(cl, o, FALSE);

    /* A resize returns to the live screen, like new output. Applying the
       live cursor's clamp to a historical viewport would discard its position. */
    BOOL restored = charmap_restore_live(data);
    WORD old_ycp = YCP;
    WORD old_xcp = XCP, old_width = CHAR_XMAX(o) + 1;
    BOOL pending = !!(ICU(o)->conFlags & CF_WRAP_PENDING);
    BOOL reflowed;

    DoSuperMethodA(cl, o, (Msg) msg);
    charmapcon_update_text_width(cl, o);
    D(bug("CharMapCon::NewWindowSize(o=%p) x=%d, y=%d, ymax=%d\n",
            o, XCP, YCP, CHAR_YMAX(o)));

    // Is console empty? Unlikely, but anyway.
    if (!data->top_of_window)
        return;

    reflowed = old_width != CHAR_XMAX(o) + 1 &&
        charmap_reflow_primary(cl, o, old_xcp, old_ycp, pending);

    // Scroll up if new window size has forced the cursor up
    if (!reflowed && old_ycp > CHAR_YMAX(o))
    {
        charmap_scroll_up(cl, o, old_ycp - CHAR_YMAX(o));
    }

    if (!CON_IS_OBSCURED(o))
    {
        charmapcon_refresh(cl, o, 0);
        charmapcon_adj_prop(cl, o);
        if (restored)
            Console_RenderCursor(o);
    }
}

static VOID charmapcon_handlemouse(Class *cl, Object *o,
    struct P_Console_HandleGadgets *msg)
{
    struct charmapcondata *data = INST_DATA(cl, o);
    struct Window *w = CU(o)->cu_Window;
    struct InputEvent *e = msg->Event;

    /* We have the following states:
     * - No active selection and no mouse button => ignore
     * - No active selection and left mouse button => Start selection
     * - Active selection and no mouse button => End selection
     * - Active selection and left mouse button => Update selection
     */

    LONG x, y;

    /*
     * Take window-relative mouse position.
     * The original code here failed when the screen was moved away from (0, 0)
     *  position. x and y were unaware of shifted display (ie_X and ie_Y are
     * raw physical coordinates, and w->LeftEdge and w->TopEdge are screen-
     * relative), and this caused lockups here.
     * TODO: Verify this intuition's behavior with AmigaOS3.1 using screentest
     * program. (sonic)
     *
     x = e->ie_X - w->LeftEdge;
     y = e->ie_Y - w->TopEdge;
     */
    x = w->MouseX;
    y = w->MouseY;

    /* An explicit chord opens a retained URL without stealing ordinary
     * selection or the application's Control-click mouse reporting. */
    if (e->ie_Code == IECODE_LBUTTON &&
        (e->ie_Qualifier & IEQUALIFIER_CONTROL) &&
        (e->ie_Qualifier & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)) &&
        x >= GFX_XMIN(o) && x <= GFX_XMAX(o) &&
        y >= GFX_YMIN(o) && y <= GFX_YMAX(o))
    {
        UBYTE url[1024];
        struct charmap_line *line = charmapcon_find_line(cl,o,(y-GFX_YMIN(o))/YRSIZE);
        if (con_url_at(line,(x-GFX_XMIN(o))/XRSIZE,url,sizeof(url)))
        {
            data->active_selection = FALSE;
            data->ignore_drag = TRUE;
            if (!con_open_url(url)) DisplayBeep(w->WScreen);
            return;
        }
    }

    /* Active selection */

    if (!(e->ie_Qualifier & IEQUALIFIER_LEFTBUTTON))
    {
        /* End selection */
        data->active_selection = 0;
        data->ignore_drag = 0;
        return;
    }

    if (!data->active_selection)
    {
        if (e->ie_Qualifier & IEQUALIFIER_LEFTBUTTON && !data->ignore_drag)
        {
            /* Inside the console area? */
            if (x >= GFX_XMIN(o) && x <= GFX_XMAX(o) &&
                y >= GFX_YMIN(o) && y <= GFX_YMAX(o))
            {
                D(bug("activated selection with x: %ld, y: %ld,"
                    " xmin: %ld, ymin: %Ld, xmax: %ld, ymax: %ld\n",
                    x, y, GFX_XMIN(o), GFX_YMIN(o), GFX_XMAX(o),
                    GFX_YMAX(o)));

                /* We need to clear these lines
                 */
                LONG old_min_y =
                    MIN(data->select_y_min, data->select_y_max);
                LONG old_max_y =
                    MAX(data->select_y_min, data->select_y_max);
                BOOL clear_old_selection =
                    data->select_x_min != data->select_x_max ||
                    data->select_y_min != data->select_y_max;

                /* Yes, so start selection */
                data->active_selection = 1;
                data->selection_exact_end = FALSE;
                data->selection_block = FALSE;
                data->ignore_drag = 0;
                data->select_x_min = (x - GFX_XMIN(o)) / XRSIZE;
                data->select_y_min = (y - GFX_YMIN(o)) / YRSIZE;
                data->select_line_min =
                    charmapcon_find_line(cl, o, data->select_y_min);

                data->select_x_max = data->select_x_min;
                data->select_y_max = data->select_y_min;

                /* Clear a previous non-empty selection.  Equal endpoints
                 * select no cells, so refreshing that line only redraws its
                 * contents and needlessly removes/restores the cursor. */
                /* FIXME: Determine exactly which lines are affected, as
                   follows:

                   - If max_y has increased, render from old_max_y to new
                   - If min_y has decreased, refresh from new min y to old
                   - If both have changed, refresh in two batches.
                   - If there's been a reversal of the relative positions of
                   y_min/y_max, refresh the entire range.
                 */
                if (clear_old_selection)
                    charmapcon_refresh_lines(cl, o, old_min_y, old_max_y);

            }
            else
                data->ignore_drag = 1;
        }
        return;
    }

    /* Update selection. */
    if (x >= GFX_XMIN(o) && x <= GFX_XMAX(o) &&
        y >= GFX_YMIN(o) && y <= GFX_YMAX(o))
    {

        LONG xmax, ymax;
        xmax = data->select_x_max;
        ymax = data->select_y_max;
        data->select_x_max = (x - GFX_XMIN(o)) / XRSIZE;
        data->select_y_max = (y - GFX_YMIN(o)) / YRSIZE;
        data->select_line_max =
            charmapcon_find_line(cl, o, data->select_y_max);
        /* FIXME: More intelligent refresh */
        if (xmax != data->select_x_max || ymax != data->select_y_max)
        {
            charmapcon_refresh_lines(cl, o, MIN(ymax,
                    MIN(data->select_y_min, data->select_y_max)), MAX(ymax,
                    MAX(data->select_y_min, data->select_y_max)));
        }
    }
    else
    {
        /* FIXME: Outside the console area, we need to scroll the window
           and just update the selection based on that */
    }

}

/* Clamp once and redraw once, independent of wheel distance. */
static VOID charmap_wheel_action(Class *cl, Object *o, BOOL down)
{
    struct charmapcondata *data=INST_DATA(cl,o);
    ULONG lines=ICU(o)->wheelLines;
    if(!lines || data->alternate || CON_IS_OBSCURED(o))return;
    charmap_search_close(cl,o,TRUE);
    ULONG limit=charmap_history_limit(data),pos=data->scrollback_pos;
    if(pos>limit)pos=limit;
    ULONG next=down?(lines>limit-pos?limit:pos+lines):(lines>pos?0:pos-lines);
    if(next!=data->scrollback_pos){
        charmapcon_scroll_to(cl,o,next);
        charmapcon_adj_prop(cl,o);
    }
}

static VOID charmapcon_handlegadgets(Class *cl, Object *o,
    struct P_Console_HandleGadgets *msg)
{
    struct InputEvent *e = msg->Event;

    if(e->ie_Class==IECLASS_GADGETDOWN &&
       ((IPTR)e->ie_EventAddress==1 || (IPTR)e->ie_EventAddress==2)){
        charmap_wheel_action(cl,o,(IPTR)e->ie_EventAddress==2);return;
    }
    if (e->ie_Class == IECLASS_RAWMOUSE || e->ie_Class == IECLASS_GADGETDOWN)
        charmap_search_close(cl, o, TRUE);

    if (e->ie_Class == IECLASS_ACTIVEWINDOW ||
        e->ie_Class == IECLASS_INACTIVEWINDOW)
    {
        DoSuperMethodA(cl, o, (Msg)msg);
        return;
    }

    /* Timer events also drive held scrollbar gadgets below. */
    if (e->ie_Class == IECLASS_TIMER)
    {
        charmap_search_tick(cl, o);
        DoSuperMethodA(cl, o, (Msg)msg);
    }

    if (e->ie_Class == IECLASS_RAWMOUSE)
    {
        charmapcon_handlemouse(cl, o, msg);
        return;
    }

    if (e->ie_Class == IECLASS_RAWKEY)
    {
        charmapcon_clear_selection(cl, o);
        DoSuperMethodA(cl, o, (Msg)msg);
        return;
    }

    struct charmapcondata *data = INST_DATA(cl, o);

    if (e->ie_Class == IECLASS_GADGETUP)
    {
        data->activeGad = 0;
        return;
    }

    if (e->ie_Class == IECLASS_GADGETDOWN)
    {
        data->activeGad = e->ie_EventAddress;
    }

    if (data->activeGad == (APTR) &(data->prop->scroller))
    {
        ULONG hidden = charmap_history_limit(data);
        ULONG pos =
            (((struct PropInfo *)((struct Gadget *)&(data->prop->
                        scroller))->SpecialInfo)->VertPot * hidden +
            (MAXPOT / 2)) / MAXPOT;

        if (pos != data->scrollback_pos)
            charmapcon_scroll_to(cl, o, pos);
    }
    else if (data->activeGad == (APTR) &(data->prop->down))
    {
        if (data->scrollback_pos < charmap_history_limit(data))
        {
            charmap_scroll_up(cl, o, 1);
            charmapcon_refresh(cl, o, 0);
            charmapcon_adj_prop(cl, o);
        }
    }
    else if (data->activeGad == (APTR) &(data->prop->up))
    {
        if (data->top_of_window != data->top_of_scrollback)
        {
            charmap_scroll_down(cl, o, 1);
            charmapcon_refresh(cl, o, 0);
            charmapcon_adj_prop(cl, o);
        }
    }
}



/* Read-only: painting the cursor must never allocate or extend history. */
static IPTR charmap_cursor_span(Class *cl, Object *o)
{
    struct charmapcondata *data = INST_DATA(cl, o);
    struct charmap_line *line = data->top_of_window;
    LONG row = YCP, column = XCP;
    if (row < 0 || row > CHAR_YMAX(o) || column < 0 || column > CHAR_XMAX(o)) return 1;
    while (line && row--) line = line->next;
    if (!line || (ULONG)column >= line->size) return 1;
    if (line->text[column] == CELL_WIDE_CONT && column > 0 &&
        con_cp_width(line->text[column - 1]) == 2)
        return 3;
    if (column < CHAR_XMAX(o) && (ULONG)column + 1 < line->size &&
        con_cp_width(line->text[column]) == 2 &&
        line->text[column + 1] == CELL_WIDE_CONT)
        return 2;
    return 1;
}

AROS_UFH3S(IPTR, dispatch_charmapconclass,
    AROS_UFHA(Class *, cl, A0),
    AROS_UFHA(Object *, o, A2), AROS_UFHA(Msg, msg, A1))
{
    AROS_USERFUNC_INIT

    IPTR retval = 0UL;

    switch (msg->MethodID)
    {
    case OM_NEW:
        retval = (IPTR) charmapcon_new(cl, o, (struct opSet *)msg);
        break;

    case OM_DISPOSE:
        charmapcon_dispose(cl, o, msg);
        break;

    case M_Console_DoCommand:
        charmapcon_docommand(cl, o, (struct P_Console_DoCommand *)msg);
        break;

    case M_Console_ClearCell:
        // FIXME: scroll down to end here if it's not there already.
        charmapcon_clearcell(cl, o, (struct P_Console_ClearCell *)msg);
        break;

    case M_Console_NewWindowSize:
        charmapcon_newwindowsize(cl, o,
            (struct P_Console_NewWindowSize *)msg);
        break;

    case M_Console_CursorSpan:
        retval = charmap_cursor_span(cl, o);
        break;

    case M_Console_QueryPaste:
        charmap_search_paste(cl, o, (struct P_Console_QueryPaste *)msg);
        break;

    case M_Console_FilterInput:
        retval = charmap_search_key(cl, o, ((struct P_Console_FilterInput *)msg)->Event);
        break;

    case M_Console_HandleGadgets:
        //D(bug("CharMapCon::HandleGadgets\n"));
        charmapcon_handlegadgets(cl, o,
            (struct P_Console_HandleGadgets *)msg);
        break;

        /* FIXME: Belongs in snimapcon - here temporarily until refactored out
           selection code */
    case M_Console_HistoryAction:
        retval = charmap_history_action(cl, o, ((struct P_Console_HistoryAction *)msg)->Action);
        break;

    case M_Console_SelectAll:
        retval = charmapcon_select_all(cl, o);
        break;

    case M_Console_Copy:
        charmapcon_copy(cl, o, msg);
        break;

    default:
        retval = DoSuperMethodA(cl, o, msg);
        break;
    }

    return retval;

    AROS_USERFUNC_EXIT
}

#undef ConsoleDevice

Class *makeCharMapConClass(struct ConsoleBase *ConsoleDevice)
{

    Class *cl;

    cl = MakeClass(NULL, NULL, STDCONCLASSPTR,
        sizeof(struct charmapcondata), 0UL);
    if (cl)
    {
        cl->cl_Dispatcher.h_Entry = (APTR) dispatch_charmapconclass;
        cl->cl_Dispatcher.h_SubEntry = NULL;

        cl->cl_UserData = (IPTR) ConsoleDevice;

        return (cl);
    }
    return (NULL);
}
