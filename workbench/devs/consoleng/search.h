/* Copyright (C) 2026, The AROS Development Team. All rights reserved. */
#ifndef CONSOLENG_SEARCH_H
#define CONSOLENG_SEARCH_H
#include <exec/types.h>

#define CON_SEARCH_MAX 256
#define CON_SEARCH_PASTE_BYTES (4 * CON_SEARCH_MAX)
struct charmap_line;
struct con_search_position {
    const struct charmap_line *line;
    ULONG column;
};
struct con_search_match {
    struct con_search_position first, end; /* end is exclusive */
};
/* One bounded physical-cell or row-transition operation per call. */
struct con_search_stream {
    struct con_search_position cursor;
    ULONG limit;
    BOOL limit_ready, trimming;
    const struct charmap_line *head;
    BOOL reverse, seeking, separator;
};
struct con_search_token {
    ULONG cp;
    struct con_search_position first, end;
};
enum con_search_stream_result { CON_STREAM_DONE, CON_STREAM_MORE, CON_STREAM_TOKEN };
VOID con_search_stream_begin(struct con_search_stream *stream,
    const struct charmap_line *first, ULONG column);
/* Discover the tail incrementally, then walk borrowed doubly linked rows.
 * Tokens keep forward-oriented first/exclusive-end positions. */
VOID con_search_stream_reverse_begin(struct con_search_stream *stream,
    const struct charmap_line *first);
/* Non-TOKEN results leave the caller's token unchanged; no allocation. */
enum con_search_stream_result con_search_stream_step(
    struct con_search_stream *stream, struct con_search_token *token);

struct con_search {
    ULONG query[CON_SEARCH_MAX];
    UWORD prefix[CON_SEARCH_MAX];
    struct con_search_position recent[CON_SEARCH_MAX];
    struct con_search_stream stream;
    UWORD length, matched, next;
    BOOL ascii_fold;
};
enum con_search_result { CON_SEARCH_DONE, CON_SEARCH_MORE, CON_SEARCH_FOUND };

/* Caller owns the state; no allocation. All line pointers are borrowed.
 * Cancel before any retained-row mutation/disposal/reflow, then restart from
 * the new history. Calls run in the owning console task. */
BOOL con_search_begin(struct con_search *search, const struct charmap_line *first,
    ULONG column, const ULONG *query, ULONG length, BOOL ascii_fold);
VOID con_search_cancel(struct con_search *search);
/* Budget counts physical cells and row transitions, including ignored wide
 * continuations/empty rows. Zero budget makes no progress. Match is unchanged
 * unless FOUND. Hard boundaries are '\n'; soft boundaries contribute no token. */
enum con_search_result con_search_step(struct con_search *search, ULONG budget,
    struct con_search_match *match);
/* Append a bounded clipboard payload atomically. UTF-8 must encode Unicode
 * scalars canonically; legacy mode maps bytes to Latin-1. CRLF/CR become LF,
 * tabs become spaces, other C0/C1 controls are discarded. Invalid encoding,
 * oversized input or query overflow leaves both query and length unchanged.
 * Existing query must already contain valid scalars. Empty input is allowed. */
BOOL con_search_append(ULONG *query, ULONG *length, const UBYTE *bytes,
    ULONG size, BOOL utf8);

/* One next/previous selection over a stable history. The anchor is the exact
 * previously selected match for this same query; NULL starts at first/last.
 * A missing anchor falls back to first/last. Selection wraps, including back
 * to the same match when it is the only one. No pointer ordering is used. */
struct con_search_selection {
    struct con_search scan;
    struct con_search_match anchor, first, last;
    BOOL reverse, anchored, seen_anchor, have_match, finished;
};
BOOL con_search_select_begin(struct con_search_selection *selection,
    const struct charmap_line *first, const ULONG *query, ULONG length,
    BOOL ascii_fold, BOOL reverse, const struct con_search_match *anchor);
VOID con_search_select_cancel(struct con_search_selection *selection);
/* MORE leaves result untouched. FOUND returns one final selection, then
 * subsequent calls return DONE. DONE without FOUND means no matches.
 * Budget bounds physical cells plus transitions, even when matches are dense. */
enum con_search_result con_search_select_step(
    struct con_search_selection *selection, ULONG budget,
    struct con_search_match *result);
#endif
