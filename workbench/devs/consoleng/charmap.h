/*
    Copyright (C) 1995-2020, The AROS Development Team. All rights reserved.
*/

#ifndef CHARMAP_H
#define CHARMAP_H

#include <exec/types.h>
#include "hyperlink.h"

/* Reserved combination: single and double underline are mutually exclusive.
 * Compare exactly and strip before rendering; never a public rendition. */
#define CHARMAP_ERASED 0x21

// FIXME:
// This wastes a lot of memory - most characters are likely to share the
// same fg/bg/flags. One option would be to optimize away the fgpen/bgpen/
// flags buffers if they're the same for every character in the line
struct charmap_line
{
    // FIXME: Replace with a MinNode?
    struct charmap_line *next;
    struct charmap_line *prev;

    ULONG size;                 /* active columns (cells), NOT bytes */
    ULONG capacity;             /* allocated columns (cells) */
    ULONG *text;                /* one UCS-4 codepoint per cell; 0 = end */
    BYTE *fgpen;
    BYTE *bgpen;
    BYTE *flags;
    ULONG wrapColumns;          /* automatic continuation boundary; zero = hard line */
    UBYTE wrapPadding;          /* synthetic spacer before a wide glyph */
    struct con_link **links;    /* optional owning references per cell */
    ULONG *underlineColors; /* optional color key + 1 per cell; zero = text color */
    UQUAD *roles; /* low 16 bits: dynamic roles; high 48 bits: original RGB colors */
};

VOID charmap_dispose_lines(struct charmap_line *line);
struct charmap_line *charmap_dispose_line(struct charmap_line *line);
struct charmap_line *charmap_newline(struct charmap_line *next,
    struct charmap_line *prev);
VOID charmap_resize(struct ConsoleBase *ConsoleDevice, struct charmap_line *line, ULONG newsize);

BOOL charmap_underline_colors(struct charmap_line *line, ULONG start, ULONG count, ULONG color);

BOOL charmap_links(struct charmap_line *line, ULONG start, ULONG count, struct con_link *link);

BOOL charmap_edit_lines(struct ConsoleBase *base, struct charmap_line *first,
    struct charmap_line *last, ULONG count, BOOL insert, ULONG width,
    BYTE fg, BYTE bg, UQUAD roles);

BOOL charmap_append_blanks(struct ConsoleBase *base, struct charmap_line *after,
    ULONG count, ULONG width, BYTE fg, BYTE bg, UQUAD roles);

#define CHARMAP_REFLOW_ANCHORS 8
struct charmap_anchor
{
    struct charmap_line *line;
    ULONG column;
    BOOL extend;
    struct charmap_line *newLine;
    ULONG newColumn, newRow;
};
BOOL charmap_reflow(struct ConsoleBase *base, struct charmap_line *head,
    ULONG width, struct charmap_anchor *anchors, ULONG nanchors,
    struct charmap_line **newhead, ULONG *newrows);

#endif
