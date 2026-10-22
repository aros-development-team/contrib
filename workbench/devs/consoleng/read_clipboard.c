/* Copyright (C) 2026, The AROS Development Team. All rights reserved. */
#include <proto/exec.h>
#include <dos/conclip.h>
#include <string.h>
#include "console_gcc.h"

extern const STRPTR CONCLIP_PORTNAME;

/* Protected by consoleTaskLock. Requests own their message/reply port until
 * completion; a unit pointer is only an identity until its generation passes. */
struct osc_read_job {
    struct ConClipTextMessage request;
    struct MsgPort reply;
    struct osc_read_job *next;
    Object *unit;
    UQUAD generation;
    UBYTE target;
    BOOL st, valid;
};
static struct osc_read_job *osc_read_jobs;

VOID con_osc_read_cancel(Object *unit)
{
    for (struct osc_read_job *j=osc_read_jobs; j; j=j->next)
        if (j->unit == unit) j->valid = FALSE;
}

BOOL con_osc_read_request(struct ConsoleBase *base, Object *unit, UBYTE target, BOOL st)
{
    if (!(ICU(unit)->oscCopyDisabled & 2)) return FALSE;
    ULONG count = 0;
    for (struct osc_read_job *j=osc_read_jobs; j; j=j->next)
        if (++count >= 16) return FALSE;
    struct osc_read_job *j=AllocMem(sizeof(*j),MEMF_PUBLIC|MEMF_CLEAR);
    if (!j) return FALSE;
    j->unit=unit; j->generation=ICU(unit)->inputGeneration;
    j->target=target; j->st=st; j->valid=TRUE;
    j->reply.mp_Node.ln_Type=NT_MSGPORT;
    j->reply.mp_Flags=PA_SIGNAL;
    j->reply.mp_SigBit=SIGBREAKB_CTRL_D;
    j->reply.mp_SigTask=base->consoleTask;
    NewList(&j->reply.mp_MsgList);
    j->request.msg.mn_Node.ln_Type=NT_MESSAGE;
    j->request.msg.mn_ReplyPort=&j->reply;
    j->request.msg.mn_Length=sizeof(j->request);
    j->request.code=CONCLIP_READ_TEXT;
    j->request.version=CONCLIP_TEXT_VERSION;
    j->next=osc_read_jobs; osc_read_jobs=j;
    Forbid();
    struct MsgPort *port=target=='c'?FindPort(CONCLIP_PORTNAME):NULL;
    if (port) PutMsg(port,&j->request.msg);
    Permit();
    if (!port) {
        ObtainSemaphore(&base->copyBufferLock);
        ULONG length=base->copyBufferSize;
        if (length <= CONCLIP_WRITE_MAX && (!length || base->copyBuffer)) {
            UBYTE *bytes=length?AllocMem(length,MEMF_PUBLIC):NULL;
            if (!length || bytes) {
                if (length) memcpy(bytes,base->copyBuffer,length);
                j->request.data=bytes;
                j->request.length=j->request.capacity=length;
                j->request.status=CONCLIP_TEXT_OK;
            }
        }
        ReleaseSemaphore(&base->copyBufferLock);
        ReplyMsg(&j->request.msg);
    }
    return TRUE;
}

static VOID osc_read_deliver(struct ConsoleBase *base, struct osc_read_job *j)
{
    ULONG length=j->request.length;
    if (j->request.status!=CONCLIP_TEXT_OK || length>CONCLIP_WRITE_MAX ||
        length>j->request.capacity || (length && !j->request.data) ||
        !con_osc_clipboard_utf8(j->request.data,length)) return;
    ULONG size=7+4*((length+2)/3)+(j->st?2:1);
    UBYTE *reply=AllocMem(size,MEMF_PUBLIC);
    if (!reply) return;
    static const UBYTE alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    memcpy(reply,"\x1b]52;c;",7); reply[5]=j->target;
    ULONG out=7;
    for (ULONG i=0;i<length;i+=3) {
        ULONG remaining=length-i;
        ULONG value=(ULONG)j->request.data[i]<<16;
        if (remaining>1) value|=(ULONG)j->request.data[i+1]<<8;
        if (remaining>2) value|=j->request.data[i+2];
        reply[out++]=alphabet[(value>>18)&63];
        reply[out++]=alphabet[(value>>12)&63];
        reply[out++]=remaining>1?alphabet[(value>>6)&63]:'=';
        reply[out++]=remaining>2?alphabet[value&63]:'=';
    }
    if (j->st) { reply[out++]=27; reply[out++]='\\'; }
    else reply[out++]=7;
    con_inject(base,CU(j->unit),reply,out);
    FreeMem(reply,size);
}

VOID con_osc_read_poll(struct ConsoleBase *base,
    BOOL (*valid_unit)(Object *, UQUAD, struct ConsoleBase *))
{
    struct osc_read_job **link=&osc_read_jobs;
    while (*link) {
        struct osc_read_job *j=*link;
        if (!GetMsg(&j->reply)) { link=&j->next; continue; }
        *link=j->next;
        if (j->valid && valid_unit(j->unit,j->generation,base) &&
            (ICU(j->unit)->oscCopyDisabled & 2)) osc_read_deliver(base,j);
        if (j->request.status==CONCLIP_TEXT_OK && j->request.data)
            FreeMem(j->request.data,j->request.capacity);
        FreeMem(j,sizeof(*j));
    }
}
