/*
    Copyright (C) 2010-2026, The AROS Development Team. All rights reserved.

    Desc: Code for CONU_CHARMAP console units.
*/

#include <proto/exec.h>
#include <proto/utility.h>

#include <string.h>

#include "console_gcc.h"
#include "charmap.h"

struct charmap_line *charmap_dispose_line(struct charmap_line *line)
{
    struct charmap_line *next = NULL;
    if (line)
    {
        next = line->next;
        /* Unlink before freeing: history trimming must not leave a backward
           link into released storage when the user scrolls toward the top. */
        if (next) next->prev = line->prev;
        if (line->prev) line->prev->next = next;
        if (line->links) {
            con_link_fill(line->links,line->capacity,NULL);
            FreeMem(line->links,line->capacity*sizeof(*line->links));
        }
        if (line->underlineColors) FreeMem(line->underlineColors, line->capacity * sizeof(ULONG));
        if (line->capacity)
        {
            if (line->text)
                FreeMem(line->text, line->capacity * sizeof(ULONG));
            if (line->fgpen)
                FreeMem(line->fgpen, line->capacity);
            if (line->bgpen)
                FreeMem(line->bgpen, line->capacity);
            if (line->roles) FreeMem(line->roles, line->capacity * sizeof(UQUAD));
            if (line->flags)    /* was leaked before the UCS-4 rework */
                FreeMem(line->flags, line->capacity);
        }
        FreeMem(line, sizeof(struct charmap_line));
    }
    return next;
}

VOID charmap_dispose_lines(struct charmap_line *line)
{
    while ((line = charmap_dispose_line(line)));
}

struct charmap_line *charmap_newline(struct charmap_line *next,
    struct charmap_line *prev)
{
    struct charmap_line *newline =
        (struct charmap_line *)AllocMem(sizeof(struct charmap_line),
        MEMF_ANY);
    if (!newline) return NULL;
    newline->next = next;
    newline->prev = prev;
    if (next)
        next->prev = newline;
    if (prev)
        prev->next = newline;
    newline->text = 0;
    newline->fgpen = 0;
    newline->bgpen = 0;
    newline->flags = 0;
    newline->roles = 0;
    newline->underlineColors = NULL;
    newline->links = NULL;
    newline->wrapColumns = 0;
    newline->wrapPadding = 0;
    newline->size = 0;
    newline->capacity = 0;
    return newline;
}


VOID charmap_resize(struct ConsoleBase *ConsoleDevice, struct charmap_line *line, ULONG newsize)
{
    ULONG *oldtext = line->text;
    BYTE *oldfgpen = line->fgpen;
    BYTE *oldbgpen = line->bgpen;
    BYTE *oldflags = line->flags;
    UQUAD *oldroles = line->roles;
    ULONG *oldUnderlineColors = line->underlineColors;
    struct con_link **oldlinks = line->links;
    ULONG oldsize = line->size;
    ULONG oldcapacity = line->capacity ? line->capacity : line->size;

    if (newsize < oldsize)
    {
        line->wrapColumns = 0;
        line->wrapPadding = 0;
    }

    if (newsize && newsize <= line->capacity)
    {
        if (newsize > oldsize)
        {
            ULONG extra = newsize - oldsize;

            SetMem(line->text + oldsize, 0, extra * sizeof(ULONG));
            SetMem(line->fgpen + oldsize, 0, extra);
            SetMem(line->bgpen + oldsize, 0, extra);
            SetMem(line->flags + oldsize, 0, extra);
            SetMem(line->roles + oldsize, 0, extra * sizeof(UQUAD));
            if (oldUnderlineColors) SetMem(oldUnderlineColors + oldsize, 0, extra * sizeof(ULONG));
        }
        else if (newsize < oldsize)
        {
            ULONG removed = oldsize - newsize;
            if (oldlinks) con_link_fill(oldlinks+newsize,removed,NULL);

            SetMem(line->text + newsize, 0, removed * sizeof(ULONG));
            SetMem(line->fgpen + newsize, 0, removed);
            SetMem(line->bgpen + newsize, 0, removed);
            SetMem(line->flags + newsize, 0, removed);
            SetMem(line->roles + newsize, 0, removed * sizeof(UQUAD));
            if (oldUnderlineColors) SetMem(oldUnderlineColors + newsize, 0, removed * sizeof(ULONG));
        }
        line->size = newsize;
        return;
    }

    if (newsize)
    {
        ULONG growth = line->capacity / 2;
        ULONG newcapacity;
        ULONG copycells;
        ULONG *newtext;
        BYTE *newfgpen;
        BYTE *newbgpen;
        BYTE *newflags;
        UQUAD *newroles;
        ULONG *newUnderlineColors = NULL;
        struct con_link **newlinks = NULL;

        if (growth < 8)
            growth = 8;
        newcapacity = line->capacity + growth;
        if (newcapacity < newsize || newcapacity < line->capacity)
            newcapacity = newsize;

        if (newcapacity > (ULONG)~0UL / sizeof(UQUAD))
            return;

        newtext = (ULONG *)AllocMem(newcapacity * sizeof(ULONG), MEMF_ANY);
        newfgpen = (BYTE *)AllocMem(newcapacity, MEMF_ANY);
        newbgpen = (BYTE *)AllocMem(newcapacity, MEMF_ANY);
        newflags = (BYTE *)AllocMem(newcapacity, MEMF_ANY);
        newroles = (UQUAD *)AllocMem(newcapacity * sizeof(UQUAD), MEMF_ANY);

        if (oldUnderlineColors) newUnderlineColors = AllocMem(newcapacity * sizeof(ULONG), MEMF_ANY);
        if(oldlinks) newlinks=AllocMem(newcapacity*sizeof(*newlinks),MEMF_ANY);
        if (!newtext || !newfgpen || !newbgpen || !newflags || !newroles || (oldlinks && !newlinks) || (oldUnderlineColors && !newUnderlineColors))
        {
            if (newUnderlineColors) FreeMem(newUnderlineColors, newcapacity * sizeof(ULONG));
            if (newlinks) FreeMem(newlinks,newcapacity*sizeof(*newlinks));
            if (newroles) FreeMem(newroles, newcapacity * sizeof(UQUAD));
            if (newtext)
                FreeMem(newtext, newcapacity * sizeof(ULONG));
            if (newfgpen)
                FreeMem(newfgpen, newcapacity);
            if (newbgpen)
                FreeMem(newbgpen, newcapacity);
            if (newflags)
                FreeMem(newflags, newcapacity);
            return;
        }

        SetMem(newtext, 0, newcapacity * sizeof(ULONG));
        SetMem(newfgpen, 0, newcapacity);
        SetMem(newbgpen, 0, newcapacity);
        SetMem(newflags, 0, newcapacity);
        SetMem(newroles, 0, newcapacity * sizeof(UQUAD));

        copycells = oldsize < newsize ? oldsize : newsize;
        if (oldtext && copycells)
            memcpy(newtext, oldtext, copycells * sizeof(ULONG));
        if (oldfgpen && copycells)
            memcpy(newfgpen, oldfgpen, copycells);
        if (oldbgpen && copycells)
            memcpy(newbgpen, oldbgpen, copycells);
        if (oldflags && copycells)
            memcpy(newflags, oldflags, copycells);

        if (oldroles && copycells) memcpy(newroles, oldroles, copycells * sizeof(UQUAD));
        /* Transfer existing ownership only after all allocations succeeded. */
        if (newlinks) {
            SetMem(newlinks,0,newcapacity*sizeof(*newlinks));
            memcpy(newlinks,oldlinks,copycells*sizeof(*newlinks));
            SetMem(oldlinks,0,copycells*sizeof(*oldlinks));
        }
        if (newUnderlineColors) {
            SetMem(newUnderlineColors, 0, newcapacity * sizeof(ULONG));
            if (copycells) memcpy(newUnderlineColors, oldUnderlineColors, copycells * sizeof(ULONG));
        }
        line->underlineColors = newUnderlineColors;
        line->links = newlinks;
        line->roles = newroles;
        line->text = newtext;
        line->fgpen = newfgpen;
        line->bgpen = newbgpen;
        line->flags = newflags;
        line->size = newsize;
        line->capacity = newcapacity;
    }
    else
    {
        line->text = 0;
        line->fgpen = 0;
        line->bgpen = 0;
        line->flags = 0;
        line->roles = 0;
        line->underlineColors = NULL;
        line->links = NULL;
        line->size = 0;
        line->capacity = 0;
    }

    if (oldlinks) {
        con_link_fill(oldlinks,oldcapacity,NULL);
        FreeMem(oldlinks,oldcapacity*sizeof(*oldlinks));
    }
    if (oldUnderlineColors) FreeMem(oldUnderlineColors, oldcapacity * sizeof(ULONG));
    if (oldroles) FreeMem(oldroles, oldcapacity * sizeof(UQUAD));
    if (oldtext)
        FreeMem(oldtext, oldcapacity * sizeof(ULONG));
    if (oldfgpen)
        FreeMem(oldfgpen, oldcapacity);
    if (oldbgpen)
        FreeMem(oldbgpen, oldcapacity);
    if (oldflags)
        FreeMem(oldflags, oldcapacity);
}

/* Move cell ownership without changing line-node identities or history links. */
static VOID charmap_swap_cells(struct charmap_line *a, struct charmap_line *b)
{
    struct charmap_line old = *a;
    struct charmap_line *an = a->next, *ap = a->prev;
    *a = *b; a->next = an; a->prev = ap;
    an = b->next; ap = b->prev;
    *b = old; b->next = an; b->prev = ap;
}

/* Allocate all replacement blanks before moving any retained cell. The caller
 * supplies an existing inclusive row range and a count bounded to its height. */
BOOL charmap_edit_lines(struct ConsoleBase *base, struct charmap_line *first,
    struct charmap_line *last, ULONG count, BOOL insert, ULONG width,
    BYTE fg, BYTE bg, UQUAD roles)
{
    struct charmap_line *blank = NULL, *tail = NULL, *line, *source;
    ULONG n, x;
    for (n = 0; n < count; n++)
    {
        line = charmap_newline(NULL, tail);
        if (!line) goto fail;
        if (!blank) blank = line;
        tail = line;
        charmap_resize(base, line, width);
        if (line->size != width) goto fail;
        for (x = 0; x < width; x++)
        {
            line->text[x] = ' '; line->fgpen[x] = fg; line->bgpen[x] = bg;
            line->flags[x] = CHARMAP_ERASED; if (line->roles) line->roles[x] = roles;
        }
    }
    source = insert ? last : first;
    for (n = 0; n < count; n++) source = insert ? source->prev : source->next;
    line = insert ? last : first;
    while (source && (insert ? source != first->prev : source != last->next))
    {
        charmap_swap_cells(line, source);
        line = insert ? line->prev : line->next;
        source = insert ? source->prev : source->next;
    }
    for (tail = blank; tail; tail = tail->next)
    {
        charmap_swap_cells(line, tail);
        line = insert ? line->prev : line->next;
    }
    /* Keep moved paragraphs reflowable; only sever links across a splice
     * or into newly blank rows. Node identities and outside rows stay put. */
    if (first->prev) { first->prev->wrapColumns = 0; first->prev->wrapPadding = 0; }
    line = last;
    if (!insert) for (n = 0; n < count; n++) line = line->prev;
    if (line) { line->wrapColumns = 0; line->wrapPadding = 0; }
    charmap_dispose_lines(blank);
    return TRUE;
fail:
    charmap_dispose_lines(blank);
    return FALSE;
}

/* Insert fully initialized rows after the visible bottom, before any retained
 * off-screen tail. Allocate first: failure leaves the original chain untouched.
 * Preserve the preceding wrap marker for implicit output continuation. */
BOOL charmap_append_blanks(struct ConsoleBase *base, struct charmap_line *after,
    ULONG count, ULONG width, BYTE fg, BYTE bg, UQUAD roles)
{
    struct charmap_line *head = NULL, *tail = NULL, *line;
    ULONG n, x;
    if (!after || !count) return FALSE;
    for (n = 0; n < count; n++)
    {
        line = charmap_newline(NULL, tail);
        if (!line) goto fail;
        if (!head) head = line;
        tail = line;
        charmap_resize(base, line, width);
        if (line->size != width) goto fail;
        for (x = 0; x < width; x++)
        {
            line->text[x] = ' '; line->fgpen[x] = fg; line->bgpen[x] = bg;
            line->flags[x] = CHARMAP_ERASED; if (line->roles) line->roles[x] = roles;
        }
    }
    tail->next = after->next;
    if (tail->next) tail->next->prev = tail;
    head->prev = after;
    after->next = head;
    return TRUE;
fail:
    charmap_dispose_lines(head);
    return FALSE;
}

/* Allocate metadata only for lines that actually contain explicit links. */
BOOL charmap_links(struct charmap_line *line, ULONG start, ULONG count, struct con_link *link)
{
    if (!line || start>line->size || count>line->size-start) return FALSE;
    if (!count) return TRUE;
    if (!line->links) {
        if (!link) return TRUE;
        if (!line->capacity || line->capacity>(ULONG)~0UL/sizeof(*line->links)) return FALSE;
        struct con_link **cells=AllocMem(line->capacity*sizeof(*cells),MEMF_ANY);
        if (!cells) return FALSE;
        memset(cells,0,line->capacity*sizeof(*cells));
        if (!con_link_fill(cells+start,count,link)) {
            FreeMem(cells,line->capacity*sizeof(*cells));return FALSE;
        }
        line->links=cells;return TRUE;
    }
    return con_link_fill(line->links+start,count,link);
}

/* Allocate only for an explicit underline color. Failure leaves every cell
 * and the old allocation untouched, so callers can keep updates atomic. */
BOOL charmap_underline_colors(struct charmap_line *line, ULONG start,
    ULONG count, ULONG color)
{
    ULONG x;
    if (!line || start > line->size || count > line->size - start) return FALSE;
    if (!count) return TRUE;
    if (!line->underlineColors)
    {
        ULONG *cells;
        if (!color) return TRUE;
        if (!line->capacity || line->capacity > (ULONG)~0UL / sizeof(ULONG)) return FALSE;
        cells = AllocMem(line->capacity * sizeof(ULONG), MEMF_ANY);
        if (!cells) return FALSE;
        memset(cells, 0, line->capacity * sizeof(ULONG));
        line->underlineColors = cells;
    }
    for (x = start; x < start + count; x++) line->underlineColors[x] = color;
    return TRUE;
}
