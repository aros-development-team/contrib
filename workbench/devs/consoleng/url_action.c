/* Copyright (C) 2026, The AROS Development Team. All rights reserved. */
#include "console_gcc.h"
#include "charmap.h"
#include <string.h>
#define __DOS_NOLIBBASE__
#include <proto/dos.h>
#include <proto/exec.h>
#include <dos/dostags.h>

#define CON_URL_MAX 1023

static BOOL url_character(ULONG cp)
{
    return cp > 32 && cp != 127 && cp != '"' && cp != '\'' &&
        cp != '<' && cp != '>' && cp != '`';
}

static BOOL url_scheme(const UBYTE *url)
{
    const char *schemes[] = {"https://", "http://", "ftp://", "mailto:"};
    for (ULONG i=0;i<4;i++)
    {
        ULONG n=0;
        while (schemes[i][n] && url[n])
        {
            UBYTE c=url[n]; if(c>='A'&&c<='Z')c+='a'-'A';
            if(c!=(UBYTE)schemes[i][n])break;
            n++;
        }
        if (!schemes[i][n] && url[n]) return TRUE;
    }
    return FALSE;
}

/* Explicit labels may contain spaces and need not display their URI. Metadata
 * takes precedence over automatic detection, including unsupported schemes. */
static struct con_link *url_explicit(struct charmap_line *line, ULONG column)
{
    if (!line || !line->links || column>=line->size ||
        (line->wrapColumns && column>=line->wrapColumns-line->wrapPadding)) return NULL;
    return line->links[column];
}

/* Read one logical token across automatic wraps, never across hard newlines.
 * A bounded token avoids scanning arbitrary scrollback on a mouse event. */
static BOOL url_scan(struct charmap_line *line, ULONG column, UBYTE *url, ULONG capacity,
    LONG *first, LONG *last)
{
    if (!line || !line->text || column>=line->size || !url || capacity<2 ||
        !url_character(line->text[column]) ||
        (line->wrapColumns && column>=line->wrapColumns-line->wrapPadding)) return FALSE;
    struct charmap_line *start=line; ULONG col=column, count=0;
    for (;;)
    {
        struct charmap_line *prev=start; ULONG x=col;
        if (x) x--;
        else if (start->prev && start->prev->wrapColumns)
        {
            prev=start->prev;
            x=prev->wrapColumns-prev->wrapPadding;
            if (!x || x>prev->size) break;
            x--;
        }
        else break;
        if (!prev->text || !url_character(prev->text[x]) || url_explicit(prev,x)) break;
        if (++count>CON_URL_MAX) return FALSE;
        start=prev;col=x;
    }
    ULONG n=0, cells=0, clicked=0;
    while (start && start->text)
    {
        ULONG end=start->wrapColumns ? start->wrapColumns-start->wrapPadding : start->size;
        if (end>start->size)end=start->size;
        if (col>=end)
        {
            if (!start->wrapColumns) break;
            start=start->next;col=0;continue;
        }
        if (url_explicit(start,col)) break;
        if (start==line && col==column) clicked=cells;
        ULONG cp=start->text[col++]; if(!url_character(cp))break;
        cells++;
        if (cp==CELL_WIDE_CONT)
        {
            /* Continuations are physical cells, not extra URI characters.
             * Never accept an orphan continuation as part of a link. */
            if (col<2 || con_cp_width(start->text[col-2])!=2) return FALSE;
            continue;
        }
        UBYTE encoded[4];ULONG bytes=con_utf8_encode(encoded,cp);
        if(!bytes || n+bytes>CON_URL_MAX || n+bytes>=capacity)return FALSE;
        memcpy(url+n,encoded,bytes);n+=bytes;
    }
    url[n]=0;
    ULONG skip=0;while(url[skip]=='('||url[skip]=='['||url[skip]=='{')skip++;
    if(skip){memmove(url,url+skip,n-skip+1);n-=skip;}
    ULONG beforeTrim=n;
    while(n)
    {
        UBYTE last=url[n-1];
        if(last=='.'||last==','||last==';'||last=='!') {url[--n]=0;continue;}
        UBYTE open=last==')'?'(':last==']'?'[':last=='}'?'{':0;
        if(!open)break;
        LONG balance=0;for(ULONG i=0;i<n;i++){if(url[i]==open)balance++;if(url[i]==last)balance--;}
        if(balance>=0)break;
        url[--n]=0;
    }
    *first=(LONG)skip-(LONG)clicked;
    *last=(LONG)cells-(LONG)(beforeTrim-n)-(LONG)clicked;
    return url_scheme(url);
}

/* Return a single display run, sharing precisely the click detector's token
 * and punctuation boundaries. Never store decoration in application flags. */
static BOOL url_run_segment(struct charmap_line *line, ULONG column, ULONG *end)
{
    UBYTE url[CON_URL_MAX+1]; LONG first=0,last=0;
    *end=column+1;
    if (!line || !line->text || column>=line->size) return FALSE;
    ULONG limit=line->size;
    if (line->wrapColumns && line->wrapColumns<=limit)
        limit=line->wrapColumns-line->wrapPadding;
    if (column>=limit) { *end=line->size; return FALSE; }
    struct con_link *link=url_explicit(line,column);
    if (link) {
        while (*end<limit && url_explicit(line,*end)==link) (*end)++;
        return url_scheme(link->data);
    }
    if (url_scan(line,column,url,sizeof(url),&first,&last))
    {
        LONG boundary=(LONG)column+(first>0 ? first : last);
        if (boundary>(LONG)column)
        {
            *end=boundary>(LONG)limit ? limit : (ULONG)boundary;
            return first<=0;
        }
    }
    /* Skip the rest of this non-link token (or whitespace run). */
    BOOL token=url_character(line->text[column]);
    while (*end<limit && !url_explicit(line,*end) && url_character(line->text[*end])==token) (*end)++;
    return FALSE;
}

/* Non-link token boundaries have no visual meaning. Keep them in one
 * paint run so a following JAM2 span cannot erase a preceding glyph edge. */
BOOL con_url_run(struct charmap_line *line, ULONG column, ULONG *end)
{
    BOOL underline = url_run_segment(line, column, end);
    if (underline || !line || !line->text) return underline;
    while (*end < line->size)
    {
        ULONG next;
        if (url_run_segment(line, *end, &next) || next <= *end) break;
        *end = next;
    }
    return FALSE;
}

BOOL con_url_at(struct charmap_line *line, ULONG column, UBYTE *url, ULONG capacity)
{
    struct con_link *link=url_explicit(line,column);
    if (link) {
        if (!url || capacity<=link->uriLength || !url_scheme(link->data)) return FALSE;
        memcpy(url,link->data,link->uriLength+1);return TRUE;
    }
    LONG first=0,last=0;
    return url_scan(line,column,url,capacity,&first,&last) && first<=0 && last>0;
}

BOOL con_url_in_row(struct charmap_line *line)
{
    if (!line) return FALSE;
    for (ULONG col=0,end=0;col<line->size;col=end)
        if (con_url_run(line,col,&end)) return TRUE;
    return FALSE;
}

/* NP_Arguments is parsed by OpenURL's ReadArgs, not by a shell. Escape the
 * two ReadArgs quoting characters and reject controls; never truncate a URI. */
static BOOL url_arguments(const UBYTE *url, UBYTE *args, ULONG capacity)
{
    if(!url || !args || !url_scheme(url) || capacity<4)return FALSE;
    ULONG n=0,i=0;args[n++]='"';
    while(url[i])
    {
        UBYTE c=url[i++];
        if(i>CON_URL_MAX || c<32 || c==127)return FALSE;
        if(n+4>=capacity)return FALSE;
        if(c=='"'||c=='*')args[n++]='*';
        args[n++]=c;
    }
    args[n++]='"';args[n++]='\n';args[n]=0;return TRUE;
}

/* Console input runs in an Exec Task, not a DOS Process. Only the worker
 * performs file I/O. Its entry is safe because consoleng.conf is noexpunge;
 * it never retains a console unit/window pointer. */
struct url_launch { UBYTE args[CON_URL_MAX*2+4]; };

static LONG url_launch_worker(void)
{
    struct url_launch *job=FindTask(NULL)->tc_UserData;
    struct DosLibrary *DOSBase=(struct DosLibrary*)OpenLibrary((CONST_STRPTR)"dos.library",0);
    if (DOSBase)
    {
        BPTR segment=LoadSeg((CONST_STRPTR)"C:OpenURL");
        if (segment)
        {
            /* The command owns its segment and a copy of NP_Arguments. */
            if (!CreateNewProcTags(NP_Seglist,segment,NP_FreeSeglist,TRUE,
                NP_Name,(IPTR)"ConsoleNG OpenURL",NP_StackSize,65536,
                NP_Cli,TRUE,NP_CopyVars,FALSE,NP_Path,BNULL,
                NP_Arguments,(IPTR)job->args,TAG_DONE)) UnLoadSeg(segment);
        }
        CloseLibrary((struct Library*)DOSBase);
    }
    FreeVec(job);
    return 0;
}

BOOL con_open_url(const UBYTE *url)
{
    struct url_launch *job=AllocVec(sizeof(*job),MEMF_PUBLIC);
    if (!job) return FALSE;
    if (!url_arguments(url,job->args,sizeof(job->args)))
    { FreeVec(job); return FALSE; }
    struct DosLibrary *DOSBase=(struct DosLibrary*)OpenLibrary((CONST_STRPTR)"dos.library",0);
    BOOL started=FALSE;
    if (DOSBase)
    {
        /* Explicit null streams prevent CreateNewProc from opening NIL: in
         * the calling Exec Task. The worker loads a separate command process
         * whose default NIL: input supports native ReadArgs argument delivery. */
        started=CreateNewProcTags(NP_Entry,(IPTR)url_launch_worker,
            NP_Name,(IPTR)"ConsoleNG URL launcher",NP_StackSize,65536,
            NP_UserData,(IPTR)job,NP_CopyVars,FALSE,NP_Cli,FALSE,
            NP_Input,BNULL,NP_Output,BNULL,NP_Error,BNULL,
            NP_CloseInput,FALSE,NP_CloseOutput,FALSE,NP_CloseError,FALSE,
            NP_CurrentDir,BNULL,NP_HomeDir,BNULL,NP_Path,BNULL,TAG_DONE)!=NULL;
        CloseLibrary((struct Library*)DOSBase);
    }
    if (!started) FreeVec(job);
    return started;
}
