/* Copyright (C) 2026, The AROS Development Team. All rights reserved.
 * First forward match whose exclusive end follows the navigation cursor.
 * Retained row location and regex work share one bounded step budget. */
#ifndef CONSOLENG_SEARCH_ANCHOR_H
#define CONSOLENG_SEARCH_ANCHOR_H
struct con_search_anchor {
    struct con_search_match pending, first;
    const struct charmap_line *line;
    ULONG row, match_row, first_row, first_end_row, result_row, result_end_row;
    ULONG origin_row, origin_column, phase;
    BOOL have, finished;
};
static inline VOID con_search_anchor_begin(struct con_search_anchor *s,
    const struct charmap_line *head,ULONG row,ULONG column)
{
    memset(s,0,sizeof(*s));s->line=head;s->origin_row=row;s->origin_column=column;
}
static inline enum con_search_result con_search_anchor_step(
    struct con_search_anchor *s,struct con_regex *regex,ULONG budget,
    struct con_search_match *result)
{
    if(s->finished)return CON_SEARCH_DONE;
    while(budget--){
        if(s->phase){
            const struct charmap_line *target=s->phase==1?s->pending.first.line:s->pending.end.line;
            if(s->line!=target){
                if(!s->line){s->finished=TRUE;return CON_SEARCH_DONE;}
                s->line=s->line->next;s->row++;continue;
            }
            if(s->phase==1){s->match_row=s->row;s->phase=2;continue;}
            s->phase=0;
            if(!s->have){
                s->first=s->pending;s->first_row=s->match_row;s->first_end_row=s->row;s->have=TRUE;
            }
            if(s->row>s->origin_row ||
               (s->row==s->origin_row && s->pending.end.column>s->origin_column)){
                *result=s->pending;s->result_row=s->match_row;s->result_end_row=s->row;
                s->finished=TRUE;return CON_SEARCH_FOUND;
            }
            continue;
        }
        enum con_search_result status=con_regex_step(regex,1,&s->pending);
        if(status==CON_SEARCH_FOUND){
            if(s->pending.first.line!=s->pending.end.line ||
                s->pending.first.column!=s->pending.end.column)s->phase=1;
        }
        else if(status==CON_SEARCH_DONE){
            s->finished=TRUE;
            if(!s->have)return CON_SEARCH_DONE;
            *result=s->first;s->result_row=s->first_row;s->result_end_row=s->first_end_row;
            return CON_SEARCH_FOUND;
        }
    }
    return CON_SEARCH_MORE;
}
#endif
