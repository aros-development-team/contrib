/* Copyright (C) 2026, The AROS Development Team. All rights reserved. */
#include <string.h>
struct ConsoleBase;
#include "charmap.h"
#include "search.h"

/* Same private retained-cell marker as CELL_WIDE_CONT in console_gcc.h. */
#define SEARCH_WIDE_CONT 0xfffffffeUL

static ULONG search_fold(ULONG cp, BOOL fold)
{
    return fold && cp >= 'A' && cp <= 'Z' ? cp + ('a' - 'A') : cp;
}

VOID con_search_cancel(struct con_search *search)
{
    memset(search, 0, sizeof(*search));
}

BOOL con_search_begin(struct con_search *search, const struct charmap_line *first,
    ULONG column, const ULONG *query, ULONG length, BOOL ascii_fold)
{
    con_search_cancel(search);
    if (!query || !length || length > CON_SEARCH_MAX) return FALSE;
    for (ULONG i = 0; i < length; i++)
    {
        ULONG cp = query[i];
        if (!cp || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff))
        {
            con_search_cancel(search);
            return FALSE;
        }
        search->query[i] = search_fold(cp, ascii_fold);
    }
    for (ULONG i = 1, prefix = 0; i < length; i++)
    {
        while (prefix && search->query[i] != search->query[prefix])
            prefix = search->prefix[prefix - 1];
        if (search->query[i] == search->query[prefix]) prefix++;
        search->prefix[i] = prefix;
    }
    search->length = length;
    search->ascii_fold = ascii_fold;
    con_search_stream_begin(&search->stream, first, column);
    return TRUE;
}

VOID con_search_stream_begin(struct con_search_stream *stream,
    const struct charmap_line *first, ULONG column)
{
    memset(stream, 0, sizeof(*stream));
    stream->cursor.line = first;
    stream->cursor.column = column;
}

VOID con_search_stream_reverse_begin(struct con_search_stream *stream,
    const struct charmap_line *first)
{
    con_search_stream_begin(stream, first, 0);
    stream->head = first;
    stream->reverse = stream->seeking = TRUE;
}

static enum con_search_stream_result search_reverse_step(
    struct con_search_stream *stream, struct con_search_token *token)
{
    const struct charmap_line *line = stream->cursor.line;
    if (!line) return CON_STREAM_DONE;
    if (stream->seeking)
    {
        if (line->next) stream->cursor.line = line->next;
        else stream->seeking = FALSE;
        return CON_STREAM_MORE;
    }
    if (!stream->limit_ready)
    {
        stream->limit = line->size;
        if (line->wrapColumns)
        {
            ULONG boundary = line->wrapPadding < line->wrapColumns ?
                line->wrapColumns - line->wrapPadding : 0;
            if (boundary < stream->limit) stream->limit = boundary;
        }
        stream->limit_ready = TRUE;
        stream->trimming = !line->wrapColumns && line->flags;
        stream->cursor.column = stream->limit;
    }
    if (stream->trimming)
    {
        ULONG last = stream->limit;
        if (last && line->text[last - 1] == ' ' &&
            line->flags[last - 1] == CHARMAP_ERASED &&
            (!line->links || !line->links[last - 1]))
        {
            stream->cursor.column = --stream->limit;
            return CON_STREAM_MORE;
        }
        stream->trimming = FALSE;
    }
    if (stream->separator)
    {
        stream->separator = FALSE;
        if (!line->wrapColumns)
        {
            token->cp = '\n';
            token->first = (struct con_search_position){line, stream->limit};
            token->end = (struct con_search_position){line->next, 0};
            return CON_STREAM_TOKEN;
        }
    }
    if (!stream->cursor.column)
    {
        stream->cursor.line = line == stream->head ? NULL : line->prev;
        stream->limit_ready = FALSE;
        stream->separator = TRUE;
        return stream->cursor.line ? CON_STREAM_MORE : CON_STREAM_DONE;
    }
    ULONG column = --stream->cursor.column;
    ULONG cp = line->text[column];
    if (cp == SEARCH_WIDE_CONT) return CON_STREAM_MORE;
    token->cp = cp ? cp : ' ';
    token->first = (struct con_search_position){line, column};
    token->end = (struct con_search_position){line, column + 1};
    if (column + 1 < stream->limit &&
        line->text[column + 1] == SEARCH_WIDE_CONT)
        token->end.column++;
    return CON_STREAM_TOKEN;
}

enum con_search_stream_result con_search_stream_step(
    struct con_search_stream *stream, struct con_search_token *token)
{
    if (stream->reverse) return search_reverse_step(stream, token);
    if (!stream->cursor.line) return CON_STREAM_DONE;
    const struct charmap_line *line = stream->cursor.line;
    struct con_search_position first = stream->cursor, end;
    ULONG cp;
    if (!stream->limit_ready)
    {
        stream->limit = line->size;
        if (line->wrapColumns)
        {
            ULONG boundary = line->wrapPadding < line->wrapColumns ?
                line->wrapColumns - line->wrapPadding : 0;
            if (boundary < stream->limit) stream->limit = boundary;
        }
        stream->limit_ready = TRUE;
        stream->trimming = !line->wrapColumns && line->flags;
    }
    /* Erased padding is drawable background, not a hard line's text.
     * Inspect at most one trailing erased cell per budget unit; retain
     * explicitly printed spaces, linked blanks and soft-wrap spaces. */
    if (stream->trimming)
    {
        ULONG last = stream->limit;
        if (last && line->text[last - 1] == ' ' &&
            (line->flags[last - 1] == CHARMAP_ERASED) &&
            (!line->links || !line->links[last - 1]))
        {
            stream->limit--;
            return CON_STREAM_MORE;
        }
        stream->trimming = FALSE;
    }
    ULONG limit = stream->limit;
    if (first.column >= limit)
    {
        stream->cursor.line = line->next;
        stream->cursor.column = 0;
        stream->limit_ready = FALSE;
        if (!line->next || line->wrapColumns) return stream->cursor.line ? CON_STREAM_MORE : CON_STREAM_DONE;
        first.column = limit;
        end = stream->cursor;
        cp = '\n';
    }
    else
    {
        cp = line->text[first.column];
        stream->cursor.column++;
        if (cp == SEARCH_WIDE_CONT) return CON_STREAM_MORE;
        if (!cp) cp = ' ';
        end = stream->cursor;
        if (end.column < limit && line->text[end.column] == SEARCH_WIDE_CONT)
            end.column++;
    }
    token->cp = cp; token->first = first; token->end = end;
    return CON_STREAM_TOKEN;
}

enum con_search_result con_search_step(struct con_search *search, ULONG budget,
    struct con_search_match *match)
{
    if (!search->length || !search->stream.cursor.line) return CON_SEARCH_DONE;
    while (budget-- && search->stream.cursor.line)
    {
        struct con_search_token token;
        enum con_search_stream_result status = con_search_stream_step(&search->stream, &token);
        if (status == CON_STREAM_DONE) return CON_SEARCH_DONE;
        if (status != CON_STREAM_TOKEN) continue;
        ULONG cp = token.cp;
        struct con_search_position first = token.first, end = token.end;
        cp = search_fold(cp, search->ascii_fold);
        search->recent[search->next] = first;
        search->next = (search->next + 1) % search->length;
        while (search->matched && cp != search->query[search->matched])
            search->matched = search->prefix[search->matched - 1];
        if (cp == search->query[search->matched]) search->matched++;
        if (search->matched == search->length)
        {
            match->first = search->recent[search->next];
            match->end = end;
            search->matched = search->prefix[search->matched - 1];
            return CON_SEARCH_FOUND;
        }
    }
    return search->stream.cursor.line ? CON_SEARCH_MORE : CON_SEARCH_DONE;
}


VOID con_search_select_cancel(struct con_search_selection *selection)
{
    memset(selection, 0, sizeof(*selection));
}

BOOL con_search_select_begin(struct con_search_selection *selection,
    const struct charmap_line *first, const ULONG *query, ULONG length,
    BOOL ascii_fold, BOOL reverse, const struct con_search_match *anchor)
{
    con_search_select_cancel(selection);
    if (!con_search_begin(&selection->scan, first, 0, query, length, ascii_fold))
        return FALSE;
    selection->reverse = reverse;
    selection->anchored = anchor != NULL;
    if (anchor) selection->anchor = *anchor;
    return TRUE;
}

static BOOL search_same_match(const struct con_search_match *a,
    const struct con_search_match *b)
{
    return a->first.line == b->first.line && a->first.column == b->first.column &&
        a->end.line == b->end.line && a->end.column == b->end.column;
}

enum con_search_result con_search_select_step(
    struct con_search_selection *selection, ULONG budget,
    struct con_search_match *result)
{
    if (selection->finished || !selection->scan.length) return CON_SEARCH_DONE;
    while (budget--)
    {
        struct con_search_match match;
        /* One engine work unit per iteration: a FOUND result never resets
         * the caller's budget. Dense/overlapping matches remain bounded. */
        enum con_search_result status = con_search_step(&selection->scan, 1, &match);
        if (status == CON_SEARCH_DONE)
        {
            selection->finished = TRUE;
            if (!selection->have_match) return CON_SEARCH_DONE;
            *result = selection->reverse ? selection->last : selection->first;
            return CON_SEARCH_FOUND;
        }
        if (status != CON_SEARCH_FOUND) continue;
        if (selection->anchored && search_same_match(&selection->anchor, &match))
        {
            selection->seen_anchor = TRUE;
            if (selection->reverse && selection->have_match)
            {
                *result = selection->last;
                selection->finished = TRUE;
                return CON_SEARCH_FOUND;
            }
        }
        else if (!selection->reverse &&
            (!selection->anchored || selection->seen_anchor))
        {
            *result = match;
            selection->finished = TRUE;
            return CON_SEARCH_FOUND;
        }
        if (!selection->have_match) selection->first = match;
        selection->last = match;
        selection->have_match = TRUE;
    }
    return CON_SEARCH_MORE;
}


BOOL con_search_append(ULONG *query, ULONG *length, const UBYTE *bytes,
    ULONG size, BOOL utf8)
{
    ULONG added[CON_SEARCH_MAX], count = 0, at = 0;
    BOOL previous_cr = FALSE;
    if (!query || !length || *length > CON_SEARCH_MAX ||
        size > CON_SEARCH_PASTE_BYTES || (!bytes && size)) return FALSE;
    while (at < size)
    {
        ULONG cp = bytes[at++];
        if (utf8 && cp >= 0x80)
        {
            ULONG extra, minimum;
            if (cp >= 0xc2 && cp <= 0xdf)
                { extra = 1; minimum = 0x80; cp &= 0x1f; }
            else if (cp >= 0xe0 && cp <= 0xef)
                { extra = 2; minimum = 0x800; cp &= 0x0f; }
            else if (cp >= 0xf0 && cp <= 0xf4)
                { extra = 3; minimum = 0x10000; cp &= 7; }
            else return FALSE;
            if (extra > size - at) return FALSE;
            while (extra--)
            {
                ULONG next = bytes[at++];
                if ((next & 0xc0) != 0x80) return FALSE;
                cp = (cp << 6) | (next & 0x3f);
            }
            if (cp < minimum || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff))
                return FALSE;
        }
        BOOL skip_lf = previous_cr && cp == '\n';
        previous_cr = cp == '\r';
        if (skip_lf) continue;
        if (cp == '\r') cp = '\n';
        if (cp == '\t') cp = ' ';
        if ((cp < 32 && cp != '\n') || (cp >= 127 && cp <= 159)) continue;
        if (count >= CON_SEARCH_MAX - *length) return FALSE;
        added[count++] = cp;
    }
    memcpy(query + *length, added, count * sizeof(*added));
    *length += count;
    return TRUE;
}
