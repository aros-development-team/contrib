/* Copyright (C) 2026, The AROS Development Team. All rights reserved.
 * Cursor-relative selection over the existing literal/regex match streams.
 * No pointer ordering, allocations or unbounded retained-row traversal. */
#ifndef CONSOLENG_SEARCH_ORIGIN_H
#define CONSOLENG_SEARCH_ORIGIN_H
struct con_search_origin {
    struct con_search_match pending, first, last, before;
    const struct charmap_line *line;
    ULONG row, origin_row, origin_column, first_row, last_row, before_row, result_row;
    BOOL regex, reverse, inclusive, have, have_before, locating, finished;
};
static inline BOOL con_search_origin_begin(struct con_search_origin *s,
    struct con_search *literal, struct con_regex *regex,
    const struct charmap_line *head, const ULONG *query, ULONG length,
    BOOL use_regex, BOOL reverse, BOOL inclusive, ULONG row, ULONG column)
{
    memset(s,0,sizeof(*s));
    s->line=head;s->origin_row=row;s->origin_column=column;
    s->regex=use_regex;s->reverse=reverse;s->inclusive=inclusive;
    BOOL ok=use_regex?con_regex_ui_begin(regex,head,query,length,con_regex_smart_fold(query,length),reverse):
        con_search_begin(literal,head,0,query,length,TRUE);
    s->finished=!ok;return ok;
}
/* One engine operation, row transition or match classification per work unit.
 * Regex matches follow their scan direction; literal matches ascend. */
static inline enum con_search_result con_search_origin_step(
    struct con_search_origin *s,struct con_search *literal,struct con_regex *regex,
    ULONG budget,struct con_search_match *result)
{
    if(s->finished)return CON_SEARCH_DONE;
    while(budget--){
        if(s->locating){
            if(s->line!=s->pending.first.line){
                if(!s->line){s->finished=TRUE;return CON_SEARCH_DONE;}
                if(s->regex&&s->reverse&&s->have){
                    s->line=s->line->prev;if(s->row)s->row--;
                }else{s->line=s->line->next;s->row++;}
                continue;
            }
            s->locating=FALSE;
            BOOL equal=s->row==s->origin_row && s->pending.first.column==s->origin_column;
            BOOL earlier=s->row<s->origin_row ||
                (s->row==s->origin_row && s->pending.first.column<s->origin_column);
            if(!s->have){s->first=s->pending;s->first_row=s->row;}
            s->last=s->pending;s->last_row=s->row;s->have=TRUE;
            if(s->reverse&&s->regex){
                if(earlier || (equal&&s->inclusive)){
                    *result=s->pending;s->result_row=s->row;s->finished=TRUE;return CON_SEARCH_FOUND;
                }
            }else if(s->reverse){
                if(earlier || (equal&&s->inclusive)){
                    s->before=s->pending;s->before_row=s->row;s->have_before=TRUE;
                }else if(s->have_before){
                    *result=s->before;s->result_row=s->before_row;s->finished=TRUE;return CON_SEARCH_FOUND;
                }
            }else if(!earlier && (!equal||s->inclusive)){
                *result=s->pending;s->result_row=s->row;s->finished=TRUE;return CON_SEARCH_FOUND;
            }
            continue;
        }
        enum con_search_result status=s->regex?
            con_regex_step(regex,1,&s->pending):con_search_step(literal,1,&s->pending);
        if(status==CON_SEARCH_FOUND){
            if(!s->regex || s->pending.first.line!=s->pending.end.line ||
                s->pending.first.column!=s->pending.end.column)s->locating=TRUE;
        }
        else if(status==CON_SEARCH_DONE){
            s->finished=TRUE;
            if(!s->have)return CON_SEARCH_DONE;
            *result=s->reverse&&!s->regex?(s->have_before?s->before:s->last):s->first;
            s->result_row=s->reverse&&!s->regex?(s->have_before?s->before_row:s->last_row):s->first_row;
            return CON_SEARCH_FOUND;
        }
    }
    return CON_SEARCH_MORE;
}
#endif
