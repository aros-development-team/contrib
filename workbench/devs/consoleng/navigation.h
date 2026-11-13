/* Copyright (C) 2026, The AROS Development Team. All rights reserved.
 * Borrowed retained rows: cancel before mutation/reflow/disposal. */
#ifndef CONSOLENG_NAVIGATION_H
#define CONSOLENG_NAVIGATION_H
struct con_nav_cursor {
    const struct charmap_line *line;
    ULONG row, column;
};
enum con_nav_motion { CON_NAV_LEFT, CON_NAV_RIGHT, CON_NAV_UP, CON_NAV_DOWN,
    CON_NAV_FIRST, CON_NAV_LAST, CON_NAV_TOP, CON_NAV_BOTTOM,
    CON_NAV_WORD_NEXT, CON_NAV_WORD_END, CON_NAV_WORD_PREV,
    CON_NAV_SEMANTIC_NEXT, CON_NAV_SEMANTIC_END, CON_NAV_SEMANTIC_PREV,
    CON_NAV_PARAGRAPH_UP, CON_NAV_PARAGRAPH_DOWN, CON_NAV_BRACKET, CON_NAV_VIEW_ROW, CON_NAV_ROW, CON_NAV_OCCUPIED, CON_NAV_FIND };
struct con_nav {
    struct con_nav_cursor cursor, anchor;
    ULONG width, target_row;
    enum con_nav_motion motion;
    BOOL selecting, pending, linewise, blockwise, semanticwise, match_ready;
    ULONG match_phase;
    struct con_nav_cursor selection_first, selection_last;
    ULONG word_phase, word_kind, scan_column, bracket_char, bracket_peer, depth;
    BOOL row_text, reverse;
    struct con_nav_cursor origin, candidate;
    BOOL candidate_valid;
    ULONG find_char;
    BOOL find_reverse, find_short;
    BOOL semantic_custom;
    ULONG semantic_count, semantic_chars[64];
};
static inline ULONG con_nav_columns(const struct con_nav *n)
{
    ULONG size=n->cursor.line?n->cursor.line->size:0;
    if(n->cursor.line && n->cursor.line->wrapColumns){
        ULONG boundary=n->cursor.line->wrapPadding<n->cursor.line->wrapColumns?
            n->cursor.line->wrapColumns-n->cursor.line->wrapPadding:0;
        if(size>boundary)size=boundary;
    }
    return size<n->width?size:n->width;
}
static inline VOID con_nav_text_clamp(struct con_nav *n)
{
    ULONG size=con_nav_columns(n);
    if(n->cursor.column>=size)n->cursor.column=size?size-1:0;
    /* A continuation belongs to its preceding wide-cell leader. */
    if(n->cursor.column && n->cursor.line && n->cursor.line->text &&
       n->cursor.line->text[n->cursor.column]==0xfffffffeUL)n->cursor.column--;
}
/* Cursor positions cover the visible grid, including unallocated blank cells.
 * A synthetic wide-wrap spacer is not a navigable cell. */
static inline ULONG con_nav_grid_columns(const struct con_nav *n)
{
    const struct charmap_line *line=n->cursor.line;
    ULONG width=n->width;
    if(line && line->wrapColumns && line->wrapColumns<=width)
        width=line->wrapPadding<line->wrapColumns?line->wrapColumns-line->wrapPadding:0;
    return width;
}
static inline VOID con_nav_clamp(struct con_nav *n)
{
    ULONG width=con_nav_grid_columns(n);
    if(n->cursor.column>=width)n->cursor.column=width?width-1:0;
    const struct charmap_line *line=n->cursor.line;
    if(n->cursor.column && line && line->text && n->cursor.column<line->size &&
       line->text[n->cursor.column]==0xfffffffeUL)n->cursor.column--;
}
static inline ULONG con_nav_grid_advance(struct con_nav *n,BOOL reverse)
{
    const struct charmap_line *line=n->cursor.line;if(!line)return 0;
    if(reverse){
        if(n->cursor.column){n->cursor.column--;con_nav_clamp(n);return 1;}
        if(!line->prev || !n->cursor.row)return 0;
        n->cursor.line=line->prev;n->cursor.row--;n->cursor.column=n->width;
        con_nav_clamp(n);return n->cursor.line->wrapColumns?1:2;
    }
    ULONG next=n->cursor.column+1;
    if(line->text && next<line->size && line->text[next]==0xfffffffeUL)next++;
    if(next<con_nav_grid_columns(n)){n->cursor.column=next;return 1;}
    if(!line->next)return 0;
    n->cursor.line=line->next;n->cursor.row++;n->cursor.column=0;
    return line->wrapColumns?1:2;
}
static inline VOID con_nav_begin(struct con_nav *n,const struct charmap_line *line,
    ULONG row,ULONG column,ULONG width)
{
    memset(n,0,sizeof(*n));n->cursor=(struct con_nav_cursor){line,row,column};
    n->width=width;con_nav_clamp(n);
}
static inline BOOL con_nav_semantic_delimiter(ULONG cp)
{
    return !cp || cp=='\t' || cp==0x2502 ||
        (cp<128 && strchr(",`|:\"' ()[]{}<>",(char)cp));
}
static inline BOOL con_nav_set_delimiters(struct con_nav *n,const ULONG *chars,ULONG count)
{
    if(count>64 || (count && !chars))return FALSE;
    n->semantic_custom=TRUE;n->semantic_count=count;
    for(ULONG i=0;i<count;i++)n->semantic_chars[i]=chars[i];
    return TRUE;
}
static inline BOOL con_nav_delimiter(const struct con_nav *n,ULONG cp)
{
    if(!cp)return TRUE; /* Unallocated cells never become searchable words. */
    if(!n->semantic_custom)return con_nav_semantic_delimiter(cp);
    for(ULONG i=0;i<n->semantic_count;i++)if(n->semantic_chars[i]==cp)return TRUE;
    return FALSE;
}
static inline ULONG con_nav_kind(const struct con_nav *n)
{
    const struct charmap_line *line=n->cursor.line;ULONG cp=0;
    if(line && line->text && n->cursor.column<line->size)cp=line->text[n->cursor.column];
    if(!cp || cp==' ' || cp=='\t')return 0;
    if(n->motion>=CON_NAV_SEMANTIC_NEXT && con_nav_delimiter(n,cp))return 2;
    return 1;
}
/* Return 2 for a hard row boundary, 1 for a cell/soft-wrap move, 0 at EOF. */
static inline ULONG con_nav_advance(struct con_nav *n,BOOL reverse)
{
    const struct charmap_line *line=n->cursor.line;if(!line)return 0;
    if(reverse){
        if(n->cursor.column){n->cursor.column--;con_nav_text_clamp(n);return 1;}
        if(!line->prev || !n->cursor.row)return 0;
        n->cursor.line=line->prev;n->cursor.row--;n->cursor.column=n->width;
        con_nav_text_clamp(n);return n->cursor.line->wrapColumns?1:2;
    }
    if(n->cursor.column+1<con_nav_columns(n)){
        n->cursor.column++;
        if(line->text && line->text[n->cursor.column]==0xfffffffeUL)n->cursor.column++;
        if(n->cursor.column<con_nav_columns(n))return 1;
    }
    if(!line->next){con_nav_text_clamp(n);return 0;}
    n->cursor.line=line->next;n->cursor.row++;n->cursor.column=0;
    return line->wrapColumns?1:2;
}
static inline VOID con_nav_word_step(struct con_nav *n)
{
    BOOL next=n->motion==CON_NAV_WORD_NEXT || n->motion==CON_NAV_SEMANTIC_NEXT;
    BOOL reverse=n->motion==CON_NAV_WORD_PREV || n->motion==CON_NAV_SEMANTIC_PREV;
    ULONG kind=con_nav_kind(n);
    if(next){
        if(!n->word_phase){
            if(!kind){n->word_phase=1;return;}
            ULONG boundary=con_nav_advance(n,FALSE);
            if(!boundary){n->pending=FALSE;return;}
            if(boundary==2 || kind==2 || con_nav_kind(n)!=kind)n->word_phase=1;
        }else if(kind)n->pending=FALSE;
        else if(!con_nav_advance(n,FALSE))n->pending=FALSE;
        return;
    }
    if(!n->word_phase){
        if(!con_nav_advance(n,reverse))n->pending=FALSE;
        n->word_phase=1;return;
    }
    if(n->word_phase==1){
        if(!kind){if(!con_nav_advance(n,reverse))n->pending=FALSE;return;}
        n->word_phase=2;n->word_kind=kind;
    }
    if(kind==2){n->pending=FALSE;return;}
    struct con_nav probe=*n;ULONG boundary=con_nav_advance(&probe,reverse);
    if(boundary!=1 || con_nav_kind(&probe)!=n->word_kind)n->pending=FALSE;
    else n->cursor=probe.cursor;
}
static inline VOID con_nav_paragraph_step(struct con_nav *n)
{
    ULONG size=con_nav_columns(n);const struct charmap_line *line=n->cursor.line;
    if(n->scan_column<size){
        ULONG cp=line->text?line->text[n->scan_column]:0;n->scan_column++;
        if(cp && cp!=' ' && cp!='\t' && cp!=0xfffffffeUL){n->row_text=TRUE;n->scan_column=size;}
        return;
    }
    if(!n->row_text && n->word_phase){n->pending=FALSE;return;}
    if(n->row_text)n->word_phase=1;
    BOOL reverse=n->motion==CON_NAV_PARAGRAPH_UP;
    const struct charmap_line *next=reverse?line->prev:line->next;
    if(!next || (reverse&&!n->cursor.row)){n->pending=FALSE;return;}
    n->cursor.line=next;n->cursor.row+=reverse?(ULONG)-1:1;n->cursor.column=0;
    n->scan_column=0;n->row_text=FALSE;
}
static inline VOID con_nav_bracket_step(struct con_nav *n)
{
    const struct charmap_line *line=n->cursor.line;
    if(!n->word_phase){
        ULONG cp=line->text && n->cursor.column<line->size?line->text[n->cursor.column]:0;
        const char *pairs="()[]{}<>";unsigned i=0;while(i<8 && cp!=(ULONG)pairs[i])i++;
        if(i==8){n->pending=FALSE;return;}
        n->bracket_char=cp;n->bracket_peer=pairs[i^1];n->reverse=!!(i&1);
        n->depth=0;n->word_phase=1;return;
    }
    if(!con_nav_advance(n,n->reverse)){n->cursor=n->origin;n->pending=FALSE;return;}
    line=n->cursor.line;ULONG cp=line->text && n->cursor.column<line->size?line->text[n->cursor.column]:0;
    if(cp==n->bracket_char){
        if(n->depth==(ULONG)~(ULONG)0){n->cursor=n->origin;n->pending=FALSE;return;}
        n->depth++;
    }else if(cp==n->bracket_peer){
        if(!n->depth)n->pending=FALSE;else n->depth--;
    }
}
/* Seek one retained row or inspect one cell per budget unit. */
static inline VOID con_nav_view_step(struct con_nav *n)
{
    const struct charmap_line *line=n->cursor.line;
    if(!n->word_phase){
        if(n->cursor.row<n->target_row && line->next){n->cursor.line=line->next;n->cursor.row++;return;}
        if(n->cursor.row>n->target_row && line->prev){n->cursor.line=line->prev;n->cursor.row--;return;}
        if(n->motion==CON_NAV_ROW){con_nav_clamp(n);n->pending=FALSE;return;}
        n->cursor.column=0;n->word_phase=1;
    }
    ULONG size=con_nav_columns(n);
    if(n->scan_column<size){
        ULONG cp=line->text?line->text[n->scan_column]:0;
        if(cp && cp!=' ' && cp!='\t' && cp!=0xfffffffeUL){
            n->cursor.column=n->scan_column;n->pending=FALSE;return;
        }
        n->scan_column++;return;
    }
    n->pending=FALSE;
}
/* Inline find stays within a logical line; failed searches restore the cursor. */
static inline VOID con_nav_find_step(struct con_nav *n)
{
    struct con_nav_cursor before=n->cursor;
    if(con_nav_grid_advance(n,n->reverse)!=1){n->cursor=n->origin;n->pending=FALSE;return;}
    const struct charmap_line *line=n->cursor.line;
    ULONG cp=line->text && n->cursor.column<line->size?line->text[n->cursor.column]:0;
    if(!cp)cp=' ';
    if(cp!=n->find_char)return;
    if(n->find_short){
        if(!n->reverse)n->cursor=before;
        else {
            /* Reference stop-short offsets one grid cell. A wide continuation
             * belongs to its leader, not to the following character. */
            const struct charmap_line *matched=n->cursor.line;
            if(matched->text && n->cursor.column+1<matched->size &&
               matched->text[n->cursor.column+1]==0xfffffffeUL)con_nav_clamp(n);
            else con_nav_grid_advance(n,FALSE);
        }
    }
    n->pending=FALSE;
}
/* Endpoint motions inspect one stored cell or cross one row per step. */
static inline BOOL con_nav_occupied_cell(const struct charmap_line *line,ULONG column)
{
    ULONG cp=line && line->text && column<line->size?line->text[column]:0;
    return cp && cp!=' ' && cp!='\t';
}
static inline VOID con_nav_last_step(struct con_nav *n)
{
    if(!n->word_phase){n->scan_column=con_nav_columns(n);n->word_phase=1;}
    if(n->word_phase==2){
        if(n->cursor.line->wrapColumns && n->cursor.line->next){
            n->cursor.line=n->cursor.line->next;n->cursor.row++;return;
        }
        n->scan_column=con_nav_columns(n);n->word_phase=3;return;
    }
    if(n->scan_column){
        n->scan_column--;
        if(!con_nav_occupied_cell(n->cursor.line,n->scan_column))return;
        n->row_text=TRUE;
    }
    ULONG occupied=n->row_text?n->scan_column:0;
    ULONG current=n->origin.column;
    const struct charmap_line *line=n->origin.line;
    if(line && line->text && current+1<line->size && line->text[current+1]==0xfffffffeUL)current++;
    if(n->word_phase==3){
        n->cursor.column=n->row_text?occupied:current;
    }else if(current<occupied){
        n->cursor.column=occupied;
    }else if(n->cursor.line->wrapColumns && n->cursor.line->next &&
             current+1>=con_nav_grid_columns(n)){
        n->word_phase=2;n->row_text=FALSE;return;
    }else n->cursor.column=n->width?n->width-1:0;
    con_nav_clamp(n);n->pending=FALSE;
}
static inline VOID con_nav_occupied_step(struct con_nav *n)
{
    if(!n->word_phase){n->word_phase=1;n->scan_column=0;n->candidate_valid=FALSE;}
    if(n->word_phase==2){
        const struct charmap_line *prev=n->cursor.line->prev;
        if(prev && n->cursor.row && prev->wrapColumns){
            n->cursor.line=prev;n->cursor.row--;n->scan_column=0;n->word_phase=3;return;
        }
        if(n->candidate_valid){n->cursor=n->candidate;n->pending=FALSE;return;}
        n->cursor=n->origin;n->scan_column=0;n->word_phase=4;return;
    }
    ULONG size=con_nav_columns(n);
    BOOL occupied=n->scan_column<size && con_nav_occupied_cell(n->cursor.line,n->scan_column);
    if(n->scan_column<size && !occupied){n->scan_column++;return;}
    ULONG column=occupied?n->scan_column:(n->width?n->width-1:0);
    if(n->word_phase==1){
        if(n->origin.column!=column){n->cursor.column=column;con_nav_clamp(n);n->pending=FALSE;return;}
        n->word_phase=2;return;
    }
    if(n->word_phase==3){
        if(occupied){n->candidate=n->cursor;n->candidate.column=column;n->candidate_valid=TRUE;}
        n->word_phase=2;return;
    }
    if(occupied || !n->cursor.line->wrapColumns || !n->cursor.line->next){
        n->cursor.column=column;con_nav_clamp(n);n->pending=FALSE;return;
    }
    n->cursor.line=n->cursor.line->next;n->cursor.row++;n->scan_column=0;
}
/* Expand line selections to complete logical lines without an unbounded walk. */
static inline VOID con_nav_line_selection_step(struct con_nav *n)
{
    if(!n->match_phase){
        n->selection_first=n->anchor;n->selection_last=n->cursor;
        if(n->selection_first.row>n->selection_last.row){
            struct con_nav_cursor tmp=n->selection_first;n->selection_first=n->selection_last;n->selection_last=tmp;
        }
        n->match_phase=1;return;
    }
    if(n->match_phase==1){
        const struct charmap_line *line=n->selection_first.line;
        if(line && line->prev && n->selection_first.row && line->prev->wrapColumns){
            n->selection_first.line=line->prev;n->selection_first.row--;return;
        }
        n->match_phase=2;return;
    }
    const struct charmap_line *line=n->selection_last.line;
    if(line && line->wrapColumns && line->next){
        n->selection_last.line=line->next;n->selection_last.row++;return;
    }
    n->match_ready=TRUE;
}
/* Semantic endpoints scan neighboring cells, not the starting delimiter.
 * A single-point bracket match takes precedence, as in the reference. */
static inline VOID con_nav_semantic_selection_step(struct con_nav *n)
{
    if(!n->match_phase){
        n->selection_first=n->anchor;n->selection_last=n->cursor;
        if(n->selection_first.row>n->selection_last.row ||
           (n->selection_first.row==n->selection_last.row && n->selection_first.column>n->selection_last.column)){
            struct con_nav_cursor tmp=n->selection_first;n->selection_first=n->selection_last;n->selection_last=tmp;
        }
        n->word_phase=0;
        n->match_phase=n->anchor.row==n->cursor.row && n->anchor.column==n->cursor.column?1:3;return;
    }
    struct con_nav probe=*n;
    if(n->match_phase==1){
        probe.cursor=n->selection_first;probe.origin=n->cursor;probe.pending=TRUE;
        con_nav_bracket_step(&probe);
        n->selection_first=probe.cursor;n->word_phase=probe.word_phase;
        n->bracket_char=probe.bracket_char;n->bracket_peer=probe.bracket_peer;
        n->reverse=probe.reverse;n->depth=probe.depth;
        if(probe.pending)return;
        if(probe.cursor.row!=n->cursor.row || probe.cursor.column!=n->cursor.column){
            if(n->selection_first.row>n->selection_last.row ||
               (n->selection_first.row==n->selection_last.row && n->selection_first.column>n->selection_last.column)){
                struct con_nav_cursor tmp=n->selection_first;n->selection_first=n->selection_last;n->selection_last=tmp;
            }
            n->match_ready=TRUE;return;
        }
        n->match_phase=3;return;
    }
    BOOL left=n->match_phase==3;probe.cursor=left?n->selection_first:n->selection_last;
    ULONG moved=con_nav_grid_advance(&probe,left),cp=0;
    if(probe.cursor.line && probe.cursor.line->text && probe.cursor.column<probe.cursor.line->size)
        cp=probe.cursor.line->text[probe.cursor.column];
    if(moved==1 && !con_nav_delimiter(n,cp)){
        if(left)n->selection_first=probe.cursor;else n->selection_last=probe.cursor;
    }else if(left)n->match_phase=4;else n->match_ready=TRUE;
}
/* Each step consumes at most one linked-row transition. The caller supplies
 * a budget for top/bottom traversal; a zero budget cannot move the cursor. */
static inline BOOL con_nav_step(struct con_nav *n,ULONG budget)
{
    while(budget && n->cursor.line &&
          (n->pending || (n->selecting && (n->linewise || n->semanticwise) && !n->match_ready))){
        budget--;
        if(!n->pending){if(n->semanticwise)con_nav_semantic_selection_step(n);else con_nav_line_selection_step(n);continue;}
        const struct charmap_line *line=n->cursor.line;
        if(n->motion==CON_NAV_FIND){con_nav_find_step(n);continue;}
        if(n->motion==CON_NAV_LAST){con_nav_last_step(n);continue;}
        if(n->motion==CON_NAV_OCCUPIED){con_nav_occupied_step(n);continue;}
        if(n->motion==CON_NAV_VIEW_ROW || n->motion==CON_NAV_ROW){con_nav_view_step(n);continue;}
        if(n->motion==CON_NAV_BRACKET){con_nav_bracket_step(n);continue;}
        if(n->motion==CON_NAV_PARAGRAPH_UP || n->motion==CON_NAV_PARAGRAPH_DOWN){con_nav_paragraph_step(n);continue;}
        if(n->motion>=CON_NAV_WORD_NEXT){con_nav_word_step(n);continue;}
        switch(n->motion){
        case CON_NAV_LEFT:case CON_NAV_RIGHT: {
            struct con_nav probe=*n;
            if(con_nav_grid_advance(&probe,n->motion==CON_NAV_LEFT)==1)n->cursor=probe.cursor;
            n->pending=FALSE;break;
        }
        case CON_NAV_UP:case CON_NAV_TOP:
            if(line->prev && n->cursor.row){n->cursor.line=line->prev;n->cursor.row--;}
            else n->pending=FALSE;
            if(n->motion==CON_NAV_UP)n->pending=FALSE;
            break;
        case CON_NAV_DOWN:case CON_NAV_BOTTOM:
            if(line->next){n->cursor.line=line->next;n->cursor.row++;}
            else n->pending=FALSE;
            if(n->motion==CON_NAV_DOWN)n->pending=FALSE;
            break;
        case CON_NAV_FIRST:
            if(!n->cursor.column && line->prev && n->cursor.row && line->prev->wrapColumns){
                n->cursor.line=line->prev;n->cursor.row--;
            }else {n->cursor.column=0;n->pending=FALSE;}
            break;
        default:n->pending=FALSE;break;
        }
        con_nav_clamp(n);
    }
    if(!n->cursor.line)n->pending=FALSE;
    return !n->pending && (!n->selecting || (!n->linewise && !n->semanticwise) || n->match_ready || !n->cursor.line);
}
static inline VOID con_nav_move(struct con_nav *n,enum con_nav_motion motion)
{
    n->motion=motion;n->pending=TRUE;n->word_phase=0;
    n->match_ready=FALSE;n->match_phase=0;
    n->scan_column=0;n->row_text=FALSE;n->origin=n->cursor;
    if(motion==CON_NAV_PARAGRAPH_UP||motion==CON_NAV_PARAGRAPH_DOWN)n->cursor.column=0;
}
static inline ULONG con_nav_end(const struct con_nav_cursor *p,ULONG width)
{
    ULONG end=p->column+1;
    if(p->line && p->line->text && end<p->line->size &&
        p->line->text[end]==0xfffffffeUL)end++;
    return end<width?end:width;
}
static inline VOID con_nav_match(const struct con_nav *n,struct con_search_match *m,
    ULONG *first,ULONG *last)
{
    if(n->selecting && n->semanticwise && n->match_ready){
        m->first=(struct con_search_position){n->selection_first.line,n->selection_first.column};
        m->end=(struct con_search_position){n->selection_last.line,con_nav_end(&n->selection_last,n->width)};
        *first=n->selection_first.row;*last=n->selection_last.row;return;
    }
    if(n->selecting && n->linewise && n->match_ready){
        m->first=(struct con_search_position){n->selection_first.line,0};
        m->end=(struct con_search_position){n->selection_last.line,n->width};
        *first=n->selection_first.row;*last=n->selection_last.row;return;
    }
    struct con_nav_cursor a=n->selecting?n->anchor:n->cursor,b=n->cursor;
    if(a.row>b.row || (a.row==b.row && a.column>b.column)){
        struct con_nav_cursor tmp=a;a=b;b=tmp;
    }
    if(n->selecting && n->blockwise){
        ULONG left=a.column<b.column?a.column:b.column;
        ULONG ae=con_nav_end(&a,n->width),be=con_nav_end(&b,n->width);
        m->first=(struct con_search_position){a.line,left};
        m->end=(struct con_search_position){b.line,ae>be?ae:be};
        *first=a.row;*last=b.row;return;
    }
    m->first=(struct con_search_position){a.line,a.column};
    m->end=(struct con_search_position){b.line,con_nav_end(&b,n->width)};
    *first=a.row;*last=b.row;
}
#endif
