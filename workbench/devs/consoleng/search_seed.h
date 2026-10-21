/* Copyright (C) 2026, The AROS Development Team. All rights reserved.
 * Bounded semantic/selection text collection for navigation search. */
#ifndef CONSOLENG_SEARCH_SEED_H
#define CONSOLENG_SEARCH_SEED_H
enum con_seed_result { CON_SEED_MORE, CON_SEED_DONE, CON_SEED_EMPTY, CON_SEED_LIMIT };
struct con_search_seed {
    struct con_nav nav;
    const struct charmap_line *head, *line;
    struct con_search_match span;
    struct con_nav_cursor start, end;
    ULONG query[CON_SEARCH_MAX], length, phase, first_row, last_row;
    ULONG row, limit, left, right, column;
    BOOL selected, wrapped, finished, failed;
};
static inline BOOL con_seed_space(ULONG cp)
{
    return (cp>=9&&cp<=13)||cp==0x20||cp==0x85||cp==0xa0||cp==0x1680||
        (cp>=0x2000&&cp<=0x200a)||cp==0x2028||cp==0x2029||cp==0x202f||cp==0x205f||cp==0x3000;
}
static inline VOID con_seed_begin(struct con_search_seed *s,
    const struct con_nav *nav,const struct charmap_line *head)
{
    memset(s,0,sizeof(*s));s->nav=*nav;s->nav.pending=FALSE;
    s->head=head;s->selected=nav->selecting;s->phase=s->selected?1:0;
}
/* The pinned seeded-search escape set deliberately retains '.' as a regex
 * wildcard. Keep this policy separate from ordinary query input. */
static inline BOOL con_seed_append(struct con_search_seed *s,ULONG cp)
{
    if(!cp)cp=' ';
    if(cp==0xfffffffeUL)return TRUE;
    if(cp>0x10ffff || (cp>=0xd800&&cp<=0xdfff)){s->failed=TRUE;return FALSE;}
    BOOL escape=cp=='$'||(cp>='('&&cp<='+')||cp=='?'||
        (cp>='['&&cp<='^')||(cp>='{'&&cp<='}');
    if(s->length+1+(ULONG)escape>CON_SEARCH_MAX){s->failed=TRUE;return FALSE;}
    if(escape)s->query[s->length++]='\\';
    s->query[s->length++]=cp;return TRUE;
}
static inline enum con_seed_result con_seed_step(struct con_search_seed *s,ULONG budget)
{
    if(s->failed)return CON_SEED_LIMIT;
    if(s->finished)return s->length?CON_SEED_DONE:CON_SEED_EMPTY;
    while(budget--){
        if(s->phase==0){
            const struct charmap_line *line=s->nav.cursor.line;
            ULONG cp=line&&line->text&&s->nav.cursor.column<line->size?
                line->text[s->nav.cursor.column]:0;
            if(line && !con_nav_delimiter(&s->nav,cp)){
                s->nav.selecting=s->nav.semanticwise=TRUE;
                s->nav.linewise=s->nav.blockwise=FALSE;s->nav.match_ready=FALSE;
                s->nav.anchor=s->nav.cursor;
                s->nav.selection_first=s->nav.selection_last=s->nav.cursor;
                s->nav.match_phase=3;s->phase=1;continue;
            }
            if(!con_nav_grid_advance(&s->nav,FALSE)){
                if(s->wrapped || !s->head){s->finished=TRUE;return CON_SEED_EMPTY;}
                s->wrapped=TRUE;s->nav.cursor=(struct con_nav_cursor){s->head,0,0};
            }
            continue;
        }
        if(s->phase==1){
            if(!con_nav_step(&s->nav,1))continue;
            con_nav_match(&s->nav,&s->span,&s->first_row,&s->last_row);
            s->start=(struct con_nav_cursor){s->span.first.line,s->first_row,s->span.first.column};
            s->end=(struct con_nav_cursor){s->span.end.line,s->last_row,s->span.end.column};
            s->line=s->span.first.line;s->row=s->first_row;s->phase=2;continue;
        }
        if(!s->line){s->finished=TRUE;return s->length?CON_SEED_DONE:CON_SEED_EMPTY;}
        if(s->phase==2){s->limit=s->line->size;s->phase=3;continue;}
        if(s->phase==3){
            if(s->limit && s->line->text && s->line->text[s->limit-1]==' ' &&
               s->line->flags && s->line->flags[s->limit-1]==CHARMAP_ERASED){s->limit--;continue;}
            if(s->line->wrapColumns==s->limit && s->line->wrapColumns &&
               s->line->wrapPadding && s->line->text && s->line->text[s->limit-1]==' ')s->limit--;
            s->left=s->line==s->span.first.line?s->span.first.column:0;
            s->right=s->line==s->span.end.line?s->span.end.column:s->limit;
            if(s->nav.blockwise){s->left=s->span.first.column;s->right=s->span.end.column;}
            if(s->left>s->limit)s->left=s->limit;
            if(s->right>s->limit)s->right=s->limit;
            if(s->nav.blockwise && s->line->text){
                if(s->left && s->left<s->line->size && s->line->text[s->left]==0xfffffffeUL)s->left--;
                if(s->right<s->line->size && s->line->text[s->right]==0xfffffffeUL)s->right++;
            }
            s->phase=4;continue;
        }
        if(s->phase==4){
            if(s->nav.blockwise && s->right>s->left && s->line->text &&
               con_seed_space(s->line->text[s->right-1])){s->right--;continue;}
            s->column=s->left;s->phase=5;continue;
        }
        if(s->phase==5){
            if(s->column<s->right){
                ULONG cp=s->line->text?s->line->text[s->column]:0;s->column++;
                if(!con_seed_append(s,cp))return CON_SEED_LIMIT;
                continue;
            }
            BOOL last=s->line==s->span.end.line;
            BOOL newline=s->nav.blockwise?!last:(!s->line->wrapColumns &&
                (!last || (s->nav.linewise && s->span.end.column>=s->line->size)));
            if(newline && !con_seed_append(s,'\n'))return CON_SEED_LIMIT;
            if(last){
                if(!s->length && s->selected){
                    /* Empty retained selection falls back to the semantic word. */
                    s->selected=FALSE;s->nav.selecting=FALSE;s->phase=0;continue;
                }
                s->finished=TRUE;return s->length?CON_SEED_DONE:CON_SEED_EMPTY;
            }
            s->line=s->line->next;s->row++;s->phase=2;
        }
    }
    return CON_SEED_MORE;
}
#endif
