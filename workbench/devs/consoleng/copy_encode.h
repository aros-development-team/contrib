/* Copyright (C) 2026, The AROS Development Team. All rights reserved.
 * Incremental selection measurement and encoding. Rows are borrowed until
 * completion or cancellation; the caller owns the output allocation. */
#ifndef CONSOLENG_COPY_ENCODE_H
#define CONSOLENG_COPY_ENCODE_H
enum con_copy_result { CON_COPY_MORE, CON_COPY_MEASURED, CON_COPY_DONE, CON_COPY_ERROR };
struct con_copy_encode {
    const struct charmap_line *first,*last,*line;
    ULONG minx,maxx,limit,left,right,column,phase,size,offset;
    UBYTE *buffer;
    BOOL utf8,exact,block,writing,finished,failed;
};
static inline BOOL con_copy_space(ULONG cp)
{
    return (cp>=9&&cp<=13)||cp==0x20||cp==0x85||cp==0xa0||cp==0x1680||
        (cp>=0x2000&&cp<=0x200a)||cp==0x2028||cp==0x2029||cp==0x202f||cp==0x205f||cp==0x3000;
}
static inline VOID con_copy_begin(struct con_copy_encode *s,const struct con_search_match *span,
    BOOL utf8,BOOL exact,BOOL block)
{
    memset(s,0,sizeof(*s));s->first=s->line=span->first.line;s->last=span->end.line;
    s->minx=span->first.column;s->maxx=span->end.column;s->utf8=utf8;s->exact=exact;s->block=block;
}
static inline BOOL con_copy_put(struct con_copy_encode *s,ULONG cp)
{
    if(cp==0xfffffffeUL)return TRUE;
    UBYTE bytes[4];ULONG count;
    if(s->utf8)count=con_utf8_encode(bytes,cp);
    else {bytes[0]=cp<0x100?cp:'?';count=1;}
    /* Reserve one byte for legacy consumers that append a terminator. */
    if(s->offset>~(ULONG)0-count-1 || (s->writing && (s->offset>s->size || count>s->size-s->offset))){
        s->failed=TRUE;return FALSE;
    }
    if(s->writing)memcpy(s->buffer+s->offset,bytes,count);
    s->offset+=count;return TRUE;
}
static inline BOOL con_copy_write_begin(struct con_copy_encode *s,UBYTE *buffer,ULONG size)
{
    if(!s->finished || s->failed || s->writing || size!=s->size || (size&&!buffer))return FALSE;
    s->buffer=buffer;s->writing=TRUE;s->finished=FALSE;s->line=s->first;s->phase=0;s->offset=0;return TRUE;
}
static inline enum con_copy_result con_copy_step(struct con_copy_encode *s,ULONG budget)
{
    if(s->failed)return CON_COPY_ERROR;
    if(s->finished)return s->writing?CON_COPY_DONE:CON_COPY_MEASURED;
    while(budget--){
        if(!s->line){s->failed=TRUE;return CON_COPY_ERROR;}
        if(s->phase==0){s->limit=s->line->size;s->phase=1;continue;}
        if(s->phase==1){
            if(s->limit && s->line->text && s->line->text[s->limit-1]==' ' &&
               s->line->flags && s->line->flags[s->limit-1]==CHARMAP_ERASED){s->limit--;continue;}
            if(s->line->wrapColumns==s->limit && s->line->wrapColumns &&
               s->line->wrapPadding && s->line->text && s->line->text[s->limit-1]==' ')s->limit--;
            s->left=s->line==s->first?s->minx:0;s->right=s->line==s->last?s->maxx:s->limit;
            if(s->first==s->last && s->left>s->right){ULONG tmp=s->left;s->left=s->right;s->right=tmp;}
            if(s->block){s->left=s->minx<s->maxx?s->minx:s->maxx;s->right=s->minx>s->maxx?s->minx:s->maxx;}
            if(s->left>s->limit)s->left=s->limit;
            if(s->right>s->limit)s->right=s->limit;
            if(s->block && s->line->text){
                if(s->left && s->left<s->line->size && s->line->text[s->left]==0xfffffffeUL)s->left--;
                if(s->right<s->line->size && s->line->text[s->right]==0xfffffffeUL)s->right++;
            }
            s->phase=2;continue;
        }
        if(s->phase==2){
            if(s->block && s->right>s->left && s->line->text && con_copy_space(s->line->text[s->right-1])){s->right--;continue;}
            s->column=s->left;s->phase=3;continue;
        }
        if(s->column<s->right && s->line->text){
            if(!con_copy_put(s,s->line->text[s->column++]))return CON_COPY_ERROR;
            continue;
        }
        BOOL last=s->line==s->last;
        ULONG end=s->first==s->last && s->minx>s->maxx?s->minx:s->maxx;
        BOOL newline=s->block?!last:(!s->line->wrapColumns && (!last || (!s->exact && end>=s->line->size)));
        if(newline&&!con_copy_put(s,'\r'))return CON_COPY_ERROR;
        if(last){
            s->finished=TRUE;
            if(!s->writing){s->size=s->offset;return CON_COPY_MEASURED;}
            if(s->offset!=s->size){s->failed=TRUE;return CON_COPY_ERROR;}
            return CON_COPY_DONE;
        }
        s->line=s->line->next;s->phase=0;
    }
    return CON_COPY_MORE;
}
#endif
