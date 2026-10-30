/* Copyright (C) 2026, The AROS Development Team. All rights reserved. */
#include <proto/exec.h>
#include <proto/intuition.h>
#include <dos/dos.h>
#include <intuition/sghooks.h>
#include <string.h>
#include "console_gcc.h"
#include "consoleif.h"
#include "search.h"

extern const STRPTR CONCLIP_PORTNAME;

/* Compatible with ConClip's string-gadget protocol, without its DOS-handler
 * injection. The job owns every pointer ConClip sees until it replies. */
struct query_clipboard_job {
    struct { struct Message msg; struct SGWork *sgw; WORD code; } request;
    struct SGWork work;
    struct StringInfo string;
    struct MsgPort reply;
    struct query_clipboard_job *next;
    Object *unit;                 /* opaque identity, never dereferenced here */
    UQUAD generation, revision;
    BOOL utf8, valid;
    /* One overflow byte plus the terminator: ConClip truncates to MaxChars-1.
     * Without the sentinel, an oversized clipboard looks exactly at the limit. */
    UBYTE bytes[CON_SEARCH_PASTE_BYTES + 2];
};
/* Accessed only by the single console task. Unanswered jobs stay bounded even
 * if ConClip stops responding. No unit/window lifetime is retained. */
static struct query_clipboard_job *query_clipboard_jobs;

BOOL con_query_clipboard_request(struct ConsoleBase *ConsoleDevice,
    Object *unit, UQUAD revision)
{
    ULONG count = 0;
    for (struct query_clipboard_job *p = query_clipboard_jobs; p; p = p->next)
        if (++count >= 16) return FALSE;
    struct query_clipboard_job *job = AllocMem(sizeof(*job), MEMF_PUBLIC | MEMF_CLEAR);
    if (!job) return FALSE;
    job->unit = unit;
    job->generation = ICU(unit)->inputGeneration;
    job->revision = revision;
    job->utf8 = CON_IS_UTF8(unit);
    job->valid = TRUE;
    job->reply.mp_Node.ln_Type = NT_MSGPORT;
    job->reply.mp_Flags = PA_SIGNAL;
    job->reply.mp_SigBit = SIGBREAKB_CTRL_D;
    job->reply.mp_SigTask = ConsoleDevice->consoleTask;
    NewList(&job->reply.mp_MsgList);
    job->request.msg.mn_Node.ln_Type = NT_MESSAGE;
    job->request.msg.mn_ReplyPort = &job->reply;
    job->request.msg.mn_Length = sizeof(job->request);
    job->request.sgw = &job->work;
    job->request.code = 'V';
    job->work.WorkBuffer = (STRPTR)job->bytes;
    job->work.StringInfo = &job->string;
    job->string.MaxChars = sizeof(job->bytes);
    job->next = query_clipboard_jobs;
    query_clipboard_jobs = job;
    Forbid();
    struct MsgPort *port = FindPort(CONCLIP_PORTNAME);
    if (port) PutMsg(port, &job->request.msg);
    Permit();
    if (!port)
    {
        ObtainSemaphore(&ConsoleDevice->copyBufferLock);
        ULONG length = ConsoleDevice->copyBufferSize;
        job->valid = length <= CON_SEARCH_PASTE_BYTES &&
            (!length || ConsoleDevice->copyBuffer);
        if (job->valid)
        {
            if (length) memcpy(job->bytes, ConsoleDevice->copyBuffer, length);
            job->work.BufferPos = length;
        }
        ReleaseSemaphore(&ConsoleDevice->copyBufferLock);
        ReplyMsg(&job->request.msg);
    }
    return TRUE;
}

VOID con_query_clipboard_poll(struct ConsoleBase *ConsoleDevice,
    BOOL (*valid_unit)(Object *, UQUAD, struct ConsoleBase *))
{
    struct query_clipboard_job **link = &query_clipboard_jobs;
    while (*link)
    {
        struct query_clipboard_job *job = *link;
        if (!GetMsg(&job->reply)) { link = &job->next; continue; }
        *link = job->next;
        if (valid_unit(job->unit, job->generation, ConsoleDevice))
        {
            LONG length = job->work.BufferPos;
            BOOL valid = job->valid && length >= 0 && length <= CON_SEARCH_PASTE_BYTES;
            Console_QueryPaste(job->unit, job->revision, job->bytes,
                valid ? length : 0, job->utf8, valid);
        }
        FreeMem(job, sizeof(*job));
    }
}
