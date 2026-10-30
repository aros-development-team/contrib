/* Copyright (C) 2026, The AROS Development Team. All rights reserved. */
#include <proto/exec.h>
#include <intuition/sghooks.h>
#include <dos/conclip.h>
#include <string.h>
#include "console_gcc.h"

extern const STRPTR CONCLIP_PORTNAME;

/* Console-task-only queue. Never retain a unit/window or a caller buffer.
 * Large writes use the optional snapshot protocol; old providers get a
 * bounded signed-WORD legacy fallback only after UNSUPPORTED. */
struct copy_clipboard_job {
    struct ConClipTextMessage request;
    struct SGWork work;
    struct MsgPort reply;
    struct copy_clipboard_job *next;
    ULONG allocation;
    UBYTE bytes[];
};
static struct copy_clipboard_job *copy_clipboard_jobs;

/* 0 = no ConClip (caller may use local snip/hooks), 1 = queued, -1 = rejected.
 * Queuing does not report completion. Extended replies report write status;
 * legacy replies acknowledge ownership only. */
LONG con_copy_clipboard_request(struct ConsoleBase *ConsoleDevice,
    const UBYTE *bytes, ULONG length)
{
    Forbid();
    BOOL available = FindPort(CONCLIP_PORTNAME) != NULL;
    Permit();
    if (!available) return 0;
    if ((!bytes && length) || length > CONCLIP_WRITE_MAX) return -1;
    ULONG count = 0;
    for (struct copy_clipboard_job *p = copy_clipboard_jobs; p; p = p->next)
        if (++count >= 16) return -1;
    ULONG allocation = sizeof(struct copy_clipboard_job) + length;
    struct copy_clipboard_job *job = AllocMem(allocation, MEMF_PUBLIC | MEMF_CLEAR);
    if (!job) return -1;
    job->allocation = allocation;
    if (length) memcpy(job->bytes, bytes, length);
    job->work.WorkBuffer = (STRPTR)job->bytes;
    job->work.NumChars = length <= 32767 ? length : 0;
    job->work.Code = 'C';
    job->work.EditOp = EO_BIGCHANGE;
    job->reply.mp_Node.ln_Type = NT_MSGPORT;
    job->reply.mp_Flags = PA_SIGNAL;
    job->reply.mp_SigBit = SIGBREAKB_CTRL_D;
    job->reply.mp_SigTask = ConsoleDevice->consoleTask;
    NewList(&job->reply.mp_MsgList);
    job->request.msg.mn_Node.ln_Type = NT_MESSAGE;
    job->request.msg.mn_ReplyPort = &job->reply;
    job->request.msg.mn_Length = sizeof(job->request);
    job->request.code = CONCLIP_WRITE_TEXT;
    job->request.version = CONCLIP_TEXT_VERSION;
    job->request.data = job->bytes;
    job->request.length = job->request.capacity = length;
    Forbid();
    struct MsgPort *port = FindPort(CONCLIP_PORTNAME);
    if (port) PutMsg(port, &job->request.msg);
    Permit();
    if (!port) { FreeMem(job, allocation); return 0; }
    job->next = copy_clipboard_jobs;
    copy_clipboard_jobs = job;
    return 1;
}

VOID con_copy_clipboard_poll(VOID)
{
    struct copy_clipboard_job **link = &copy_clipboard_jobs;
    while (*link)
    {
        struct copy_clipboard_job *job = *link;
        if (!GetMsg(&job->reply)) { link = &job->next; continue; }
        if (job->request.code == CONCLIP_WRITE_TEXT &&
            job->request.status == CONCLIP_TEXT_UNSUPPORTED &&
            job->request.length > 0 && job->request.length <= 32767)
        {
            /* Old servers ignore unknown codes. Retry only representable text;
             * an explicit ERROR must not duplicate/overwrite a failed write. */
            job->request.code = 'C';
            job->request.sgw = &job->work;
            Forbid();
            struct MsgPort *port = FindPort(CONCLIP_PORTNAME);
            if (port) PutMsg(port, &job->request.msg);
            Permit();
            if (port) { link = &job->next; continue; }
        }
        *link = job->next;
        FreeMem(job, job->allocation);
    }
}
