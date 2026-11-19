/* Copyright (C) 2026, The AROS Development Team. All rights reserved. */
#include <proto/exec.h>
#include <proto/intuition.h>
#include <devices/timer.h>
#include <exec/errors.h>
#include <string.h>
#include "console_gcc.h"

#undef IntuitionBase

#define CON_SYNC_MAX (2UL * 1024 * 1024)
struct ConsolengSync {
    struct MsgPort reply;
    struct timerequest timer;
    UBYTE *bytes;
    ULONG length, capacity;
    BOOL active, replay, pending;
};

BOOL con_sync_active(struct ConUnit *unit)
{
    return ICU(unit)->sync && ICU(unit)->sync->active;
}
BOOL con_sync_buffering(struct ConUnit *unit)
{
    return con_sync_active(unit) && !ICU(unit)->sync->replay;
}
static VOID sync_cancel(struct ConsolengSync *s)
{
    if (!s->pending) return;
    if (!CheckIO((struct IORequest *)&s->timer)) AbortIO((struct IORequest *)&s->timer);
    WaitIO((struct IORequest *)&s->timer);
    s->pending = FALSE;
}
static VOID sync_arm(struct ConsolengSync *s)
{
    sync_cancel(s);
    s->timer.tr_node.io_Command = TR_ADDREQUEST;
    s->timer.tr_time.tv_secs = 0;
    s->timer.tr_time.tv_micro = 150000;
    SendIO((struct IORequest *)&s->timer);
    s->pending = TRUE;
}
static BOOL sync_flush(struct ConUnit *unit, struct ConsoleBase *base)
{
    struct ConsolengSync *s = ICU(unit)->sync;
    LONG error = 0;
    sync_cancel(s);
    s->replay = TRUE;
    ULONG used = con_parse_output(unit, (STRPTR)s->bytes, s->length, base, &error);
    s->replay = FALSE;
    if (used < s->length) {
        memmove(s->bytes, s->bytes + used, s->length - used);
        s->length -= used;
        sync_arm(s);
        return FALSE;
    }
    s->length = 0;
    s->active = FALSE;
    return TRUE;
}
VOID con_sync_set(struct ConUnit *unit, struct ConsoleBase *base, BOOL enabled)
{
    struct ConsolengSync *s = ICU(unit)->sync;
    if (s && s->replay) return;
    if (!enabled) {
        if (s && s->active) sync_flush(unit, base);
        return;
    }
    if (!s) {
        s = AllocMem(sizeof(*s), MEMF_PUBLIC | MEMF_CLEAR);
        if (!s) return;
        s->reply.mp_Node.ln_Type = NT_MSGPORT;
        s->reply.mp_Flags = PA_SIGNAL;
        s->reply.mp_SigBit = SIGBREAKB_CTRL_D;
        s->reply.mp_SigTask = base->consoleTask;
        NewList(&s->reply.mp_MsgList);
        s->timer.tr_node.io_Message.mn_ReplyPort = &s->reply;
        s->timer.tr_node.io_Message.mn_Length = sizeof(s->timer);
        if (OpenDevice((CONST_STRPTR)"timer.device", UNIT_MICROHZ,
                (struct IORequest *)&s->timer, 0)) {
            FreeMem(s, sizeof(*s));
            return;
        }
        ICU(unit)->sync = s;
    }
    s->active = TRUE;
    sync_arm(s);
}
VOID con_sync_dispose(struct ConUnit *unit)
{
    struct ConsolengSync *s = ICU(unit)->sync;
    if (!s) return;
    sync_cancel(s);
    CloseDevice((struct IORequest *)&s->timer);
    if (s->bytes) FreeMem(s->bytes, s->capacity);
    FreeMem(s, sizeof(*s));
    ICU(unit)->sync = NULL;
}
/* Caller holds consoleTaskLock. Close follows the same lock order. Timer
 * replies own no unit pointer and are cancelled before the unit is freed. */
VOID con_sync_poll(struct ConsoleBase *base)
{
    struct Library *IntuitionBase = base->cb_IntuitionBase;
    Object *o, *state;
    ObtainSemaphoreShared(&base->unitListLock);
    state = (Object *)base->unitList.mlh_Head;
    while ((o = NextObject(&state))) {
        struct ConsolengSync *s = ICU(o)->sync;
        if (s && s->active && s->pending && CheckIO((struct IORequest *)&s->timer))
            sync_flush(CU(o), base);
    }
    ReleaseSemaphore(&base->unitListLock);
}
static ULONG sync_feed(struct ConUnit *unit, const UBYTE *bytes, ULONG length,
    struct ConsoleBase *base, LONG *error)
{
    struct ConsolengSync *s = ICU(unit)->sync;
    ULONG used = 0;
    while (used < length && s->active) {
        if (s->length == CON_SYNC_MAX) {
            if (!sync_flush(unit, base)) *error = IOERR_ABORTED;
            break;
        }
        if (s->length == s->capacity) {
            ULONG capacity = s->capacity ? s->capacity * 2 : 4096;
            UBYTE *buffer = AllocMem(capacity, MEMF_PUBLIC);
            if (!buffer) {
                if (!sync_flush(unit, base)) *error = IOERR_ABORTED;
                break;
            }
            if (s->length) CopyMem(s->bytes, buffer, s->length);
            if (s->bytes) FreeMem(s->bytes, s->capacity);
            s->bytes = buffer;
            s->capacity = capacity;
        }
        s->bytes[s->length++] = bytes[used++];
        if (s->length >= 8) {
            const UBYTE *tail = s->bytes + s->length - 8;
            if (!memcmp(tail, "\x1b[?2026l", 8)) {
                if (!sync_flush(unit, base)) *error = IOERR_ABORTED;
                break;
            }
            if (!memcmp(tail, "\x1b[?2026h", 8)) sync_arm(s);
        }
    }
    return used;
}
ULONG writeToConsole(struct ConUnit *unit, STRPTR bytes, ULONG length,
    struct ConsoleBase *base, LONG *error)
{
    ULONG used = 0;
    *error = 0;
    if (length > 0x7fffffffUL - ICU(unit)->pendingCSILen) {
        *error = IOERR_BADLENGTH;
        return 0;
    }
    while (used < length && !*error) {
        ULONG n;
        if (con_sync_buffering(unit)) n = sync_feed(unit, (UBYTE *)bytes + used, length - used, base, error);
        else n = con_parse_output(unit, bytes + used, length - used, base, error);
        used += n;
        if (!n && !*error && con_sync_buffering(unit)) continue;
        if (!n && !*error && !con_sync_active(unit)) {
            /* A buffer limit or allocation failure ended synchronization.
             * Retry these unconsumed bytes through the ordinary parser. */
            n = con_parse_output(unit, bytes + used, length - used, base, error);
            used += n;
            if (!n) break;
        }
    }
    return used;
}
