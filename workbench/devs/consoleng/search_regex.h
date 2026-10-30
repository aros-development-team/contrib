/* Copyright (C) 2026, The AROS Development Team. All rights reserved. */
#ifndef CONSOLENG_SEARCH_REGEX_H
#define CONSOLENG_SEARCH_REGEX_H
#include "search.h"
#define CON_REGEX_STATES (CON_SEARCH_MAX * 2 + 2)
#define CON_REGEX_COMPILE_WORK 8192
struct con_regex_node { UWORD op, out, other, first, count, flags; ULONG cp; };
struct con_regex_range { ULONG low, high; };
struct con_regex_thread { UWORD pc; UQUAD ordinal; struct con_search_position start; };
/* Fixed storage, borrowed retained rows; cancel before any row mutation.
 * No recursive execution or allocations. Compilation depth is limited to 32.
 * Matches use leftmost-longest, nonoverlapping semantics; Unicode simple folding.
 * ^/$ are logical-line anchors; \A/\z anchor the entire input. Dot excludes
 * newline unless scoped/global s mode enables it. Unsupported syntax is
 * rejected rather than silently interpreted. Empty matches advance one token. */
struct con_regex {
    struct con_regex_node nodes[CON_REGEX_STATES];
    struct con_regex_range ranges[CON_SEARCH_MAX];
    UWORD nodes_used, ranges_used, start;
    BOOL fold, valid, done, reverse, prefer_first, line_boundaries;
    struct con_search_stream stream, before, best_resume;
    struct con_search_token token;
    struct con_regex_thread seeds[CON_REGEX_STATES], leaves[CON_REGEX_STATES];
    UWORD stack[CON_REGEX_STATES], stack_count, seed_count, seed_at, leaf_count, leaf_at;
    UBYTE seen[CON_REGEX_STATES];
    struct con_regex_thread current;
    struct con_search_match best;
    struct con_search_position boundary;
    UQUAD ordinal, best_start, best_end;
    ULONG previous, best_previous;
    UWORD phase;
    BOOL eof, best_eof, injected, have_best, live, line_end;
};
/* Raw-query Uppercase property, matching the pinned Alacritty smartcase policy. */
BOOL con_regex_smart_fold(const ULONG *query, ULONG length);
BOOL con_regex_begin(struct con_regex *regex, const struct charmap_line *first,
    const ULONG *query, ULONG length, BOOL unicode_fold);
/* Reverse compilation and retained-token traversal; the low-level contract
 * remains greedy leftmost-longest in scan order. Results have forward spans. */
BOOL con_regex_direction_begin(struct con_regex *regex, const struct charmap_line *first,
    const ULONG *query, ULONG length, BOOL unicode_fold, BOOL reverse);
/* Console search policy: ordered alternatives for nonnullable expressions;
 * nullable expressions keep longest matching, as in the pinned reference.
 * Hard row breaks terminate input; soft wraps remain continuous. */
BOOL con_regex_ui_begin(struct con_regex *regex, const struct charmap_line *first,
    const ULONG *query, ULONG length, BOOL unicode_fold, BOOL reverse);
VOID con_regex_cancel(struct con_regex *regex);
/* One work unit per stream step, epsilon operation or consuming-state test.
 * Zero budget is inert. Non-FOUND results leave match unchanged. */
enum con_search_result con_regex_step(struct con_regex *regex, ULONG budget,
    struct con_search_match *match);
struct con_regex_selection {
    struct con_regex scan;
    struct con_search_match anchor, first, last;
    BOOL reverse, anchored, seen_anchor, have_match, finished;
};
BOOL con_regex_select_begin(struct con_regex_selection *selection,
    const struct charmap_line *first, const ULONG *query, ULONG length,
    BOOL unicode_fold, BOOL reverse, const struct con_search_match *anchor);
VOID con_regex_select_cancel(struct con_regex_selection *selection);
enum con_search_result con_regex_select_step(struct con_regex_selection *selection,
    ULONG budget, struct con_search_match *result);
#endif
