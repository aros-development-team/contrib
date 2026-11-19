/* Copyright (C) 2026, The AROS Development Team. All rights reserved. */
#include <proto/exec.h>
#include <string.h>
#include "console_gcc.h"
#include "charmap.h"

struct reflow_builder
{
    struct ConsoleBase *base;
    struct charmap_line *head, *tail;
    ULONG width, rows;
};

static BOOL reflow_row(struct reflow_builder *b, BOOL continuation)
{
    struct charmap_line *line = charmap_newline(NULL, b->tail);
    if (!line) return FALSE;
    if (b->tail && continuation) b->tail->wrapColumns = b->width;
    if (!b->head) b->head = line;
    b->tail = line;
    b->rows++;
    return TRUE;
}

static BOOL reflow_cell(struct reflow_builder *b, ULONG cp,
    BYTE fg, BYTE bg, BYTE flags, UQUAD roles, ULONG underlineColor, struct con_link *link)
{
    struct charmap_line *line = b->tail;
    ULONG x = line->size;
    if (!line->capacity)
    {
        charmap_resize(b->base, line, b->width);
        if (line->size != b->width) return FALSE;
        line->size = 0;
    }
    line->text[x] = cp;
    line->fgpen[x] = fg; line->bgpen[x] = bg;
    line->flags[x] = flags; line->roles[x] = roles;
    line->size++;
    return charmap_underline_colors(line,x,1,underlineColor) && charmap_links(line,x,1,link);
}

static VOID reflow_map(struct charmap_anchor *a, ULONG n,
    struct charmap_line *old, ULONG column, struct reflow_builder *b,
    ULONG outcolumn)
{
    ULONG i;
    for (i = 0; i < n; i++)
        if (a[i].line == old && a[i].column == column)
        {
            a[i].newLine = b->tail;
            a[i].newColumn = outcolumn;
            a[i].newRow = b->rows - 1;
        }
}

/* Build privately; the caller swaps ownership only after TRUE. Anchors are
 * bounded, caller-owned cursor/selection positions, never pointers into cells.
 * A one-column viewport retains a wide codepoint in one cell; widening restores
 * its continuation cell rather than losing the character. */
BOOL charmap_reflow(struct ConsoleBase *base, struct charmap_line *head,
    ULONG width, struct charmap_anchor *anchors, ULONG nanchors,
    struct charmap_line **newhead, ULONG *newrows)
{
    struct reflow_builder b = {base, NULL, NULL, width, 0};
    struct charmap_anchor mapped[CHARMAP_REFLOW_ANCHORS];
    struct charmap_line *old;
    ULONG i;
    BOOL continuation = FALSE;
    if (!width || width > 0x7fffffffUL / sizeof(ULONG) ||
        nanchors > CHARMAP_REFLOW_ANCHORS) return FALSE;
    if (nanchors) memcpy(mapped, anchors, nanchors * sizeof(*mapped));
    for (i = 0; i < nanchors; i++) mapped[i].newLine = NULL;

    for (old = head; old; old = old->next)
    {
        ULONG end = old->size, x = 0;
        if (old->wrapColumns && end > old->wrapColumns) end = old->wrapColumns;
        if (old->wrapColumns && old->wrapPadding && end == old->size &&
            end && old->text && old->text[end - 1] == ' ') end--;
        ULONG stored = end; /* anchors may still need trimmed cells' pens */
        /* Default or uniformly erased trailing blanks are not paragraph content. Keeping a
         * full-width erased/inserted row here would manufacture extra rows
         * on every shrink. Explicitly printed color, style and soft-wrap space remain
         * significant; cursor anchors below can still extend an empty row. */
        if (!old->wrapColumns)
            while (end && old->text && old->text[end - 1] == ' ' &&
                (!old->links || !old->links[end-1]) &&
                (((!old->flags || (old->flags[stored - 1] != CHARMAP_ERASED)) &&
                  old->roles && old->roles[end - 1] == 0x0102 &&
                  (!old->flags || !old->flags[end - 1])) ||
                 (old->flags && old->flags[end - 1] == CHARMAP_ERASED &&
                  old->flags[stored - 1] == CHARMAP_ERASED &&
                  old->fgpen[end - 1] == old->fgpen[stored - 1] &&
                  old->bgpen[end - 1] == old->bgpen[stored - 1] &&
                  old->roles && old->roles[end - 1] == old->roles[stored - 1] &&
                  (!old->underlineColors || old->underlineColors[end - 1] == old->underlineColors[stored - 1])))) end--;
        BOOL default_tail = end < stored && !old->wrapColumns;
        if (!continuation && !reflow_row(&b, FALSE)) goto fail;
        /* Preserve cursor positions beyond the stored text on a hard row.
         * Selection endpoints are clamped instead of manufacturing text. */
        if (!old->wrapColumns)
            for (i = 0; i < nanchors; i++)
                if (mapped[i].line == old && mapped[i].extend &&
                    mapped[i].column > end) end = mapped[i].column;
        while (x < end)
        {
            ULONG cp = x < stored && old->text ? old->text[x] : ' ';
            ULONG consumed = 1, cells;
            BYTE fg = x < stored && old->fgpen ? old->fgpen[x] : 0;
            BYTE bg = x < stored && old->bgpen ? old->bgpen[x] : 0;
            BYTE flags = x < stored && old->flags ? old->flags[x] : 0;
            UQUAD roles = x < stored && old->roles ? old->roles[x] : 0x0102;
            ULONG underlineColor = x < stored && old->underlineColors ? old->underlineColors[x] : 0;
            struct con_link *link = x < stored && old->links ? old->links[x] : NULL;
            if (cp == CELL_WIDE_CONT)
            {
                reflow_map(mapped, nanchors, old, x++, &b, b.tail->size);
                continue;
            }
            cells = con_cp_width(cp) == 2 ? 2 : 1;
            if (cells == 2 && x + 1 < stored && old->text[x + 1] == CELL_WIDE_CONT)
                consumed = 2;
            if (cells > width) cells = width;
            if (b.tail->size + cells > width)
            {
                if (b.tail->size < width)
                {
                    if (!reflow_cell(&b, ' ', fg, bg, flags, roles, underlineColor, NULL)) goto fail;
                    b.tail->wrapPadding = 1;
                }
                if (!reflow_row(&b, TRUE)) goto fail;
            }
            ULONG start = b.tail->size;
            reflow_map(mapped, nanchors, old, x, &b, start);
            if (!reflow_cell(&b, cp, fg, bg, flags, roles, underlineColor, link)) goto fail;
            if (cells == 2 && !reflow_cell(&b, CELL_WIDE_CONT, fg, bg, flags, roles, underlineColor, link)) goto fail;
            if (consumed == 2)
                reflow_map(mapped, nanchors, old, x + 1, &b, start + cells - 1);
            x += consumed;
        }
        for (i = 0; i < nanchors; i++)
            if (mapped[i].line == old && !mapped[i].newLine)
            {
                mapped[i].newLine = b.tail;
                mapped[i].newColumn = b.tail->size;
                mapped[i].newRow = b.rows - 1;
            }
        /* Keep the trimmed area drawable in its original colors and roles,
         * even if the current SGR background changes before a later redraw.
         * These blanks trim again on the next reflow, so they add no rows. */
        if (default_tail)
            while (b.tail->size < width)
                if (!reflow_cell(&b, ' ', old->fgpen[stored - 1],
                        old->bgpen[stored - 1], old->flags ? old->flags[stored - 1] : 0,
                        old->roles ? old->roles[stored - 1] : 0x0102,
                        old->underlineColors ? old->underlineColors[stored - 1] : 0, NULL)) goto fail;
        continuation = old->wrapColumns && old->next;
    }
    for (i = 0; i < nanchors; i++)
        if (mapped[i].line && !mapped[i].newLine) goto fail;
    if (nanchors) memcpy(anchors, mapped, nanchors * sizeof(*mapped));
    *newhead = b.head; *newrows = b.rows;
    return TRUE;
fail:
    charmap_dispose_lines(b.head);
    return FALSE;
}
