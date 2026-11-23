/* Copyright (C) 2026, The AROS Development Team. All rights reserved. */
#include <proto/exec.h>
#include <utility/hooks.h>
#include <intuition/sghooks.h>
#include <dos/conclip.h>
#include <string.h>
#include "console_gcc.h"

/* OSC52 copy and opt-in asynchronous reads. Decode at most
 * 1 MiB, independently of the ordinary title/font OSC buffer. */
#define OSC_CLIPBOARD_MAX ((CONCLIP_WRITE_MAX + 2) / 3 * 4 + 256)

VOID con_osc_clipboard_reset(struct intConUnit *unit)
{
    if (unit->oscClipboard)
        FreeMem(unit->oscClipboard, unit->oscClipboardCapacity);
    unit->oscClipboard = NULL;
    unit->oscClipboardLength = unit->oscClipboardCapacity = 0;
    unit->oscClipboardInvalid = FALSE;
}

VOID con_osc_clipboard_byte(struct intConUnit *unit, UBYTE byte)
{
    if ((unit->oscCopyDisabled & 1) && !(unit->oscCopyDisabled & 2)) {
        unit->oscClipboardInvalid = TRUE; return;
    }
    if (unit->oscClipboardInvalid) return;
    if (unit->oscClipboardLength == OSC_CLIPBOARD_MAX ||
        ((unit->oscCopyDisabled & 1) && unit->oscClipboardLength >= 258))
    {
        unit->oscClipboardInvalid = TRUE;
        return;
    }
    if (unit->oscClipboardLength == unit->oscClipboardCapacity)
    {
        ULONG capacity = unit->oscClipboardCapacity ? unit->oscClipboardCapacity * 2 : 256;
        if (capacity > OSC_CLIPBOARD_MAX) capacity = OSC_CLIPBOARD_MAX;
        UBYTE *bytes = AllocMem(capacity, MEMF_PUBLIC);
        if (!bytes) { unit->oscClipboardInvalid = TRUE; return; }
        if (unit->oscClipboardLength)
            CopyMem(unit->oscClipboard, bytes, unit->oscClipboardLength);
        if (unit->oscClipboard) FreeMem(unit->oscClipboard, unit->oscClipboardCapacity);
        unit->oscClipboard = bytes;
        unit->oscClipboardCapacity = capacity;
    }
    unit->oscClipboard[unit->oscClipboardLength++] = byte;
}

static LONG osc_base64_digit(UBYTE byte)
{
    if (byte >= 'A' && byte <= 'Z') return byte - 'A';
    if (byte >= 'a' && byte <= 'z') return byte - 'a' + 26;
    if (byte >= '0' && byte <= '9') return byte - '0' + 52;
    if (byte == '+') return 62;
    if (byte == '/') return 63;
    return -1;
}

/* Standard padded Base64, with canonical trailing bits and no whitespace.
 * Decode in place only after parsing the target; callers publish after UTF-8
 * validation, so malformed suffixes never expose a partial clipboard value. */
static BOOL osc_clipboard_decode(UBYTE *bytes, ULONG length, ULONG *decoded, UBYTE *target)
{
    ULONG start = 0, output = 0;
    while (start < length && bytes[start] != ';') start++;
    if (start == length || start > 255) return FALSE;
    *target = start ? bytes[0] : 'c';
    if (*target != 'c' && *target != 'p' && *target != 's') return FALSE;
    start++;
    ULONG count = length - start;
    if (count % 4 || count / 4 * 3 > CONCLIP_WRITE_MAX + 2) return FALSE;
    for (ULONG i = start; i < length; i += 4)
    {
        LONG a = osc_base64_digit(bytes[i]), b = osc_base64_digit(bytes[i+1]);
        LONG c = osc_base64_digit(bytes[i+2]), d = osc_base64_digit(bytes[i+3]);
        BOOL last = i + 4 == length;
        if (a < 0 || b < 0) return FALSE;
        if (bytes[i+2] == '=')
        {
            if (!last || bytes[i+3] != '=' || (b & 15)) return FALSE;
            bytes[output++] = (a << 2) | (b >> 4);
        }
        else if (bytes[i+3] == '=')
        {
            if (!last || c < 0 || (c & 3)) return FALSE;
            bytes[output++] = (a << 2) | (b >> 4);
            bytes[output++] = (b << 4) | (c >> 2);
        }
        else
        {
            if (c < 0 || d < 0) return FALSE;
            bytes[output++] = (a << 2) | (b >> 4);
            bytes[output++] = (b << 4) | (c >> 2);
            bytes[output++] = (c << 6) | d;
        }
    }
    if (output > CONCLIP_WRITE_MAX) return FALSE;
    *decoded = output;
    return TRUE;
}

BOOL con_osc_clipboard_utf8(const UBYTE *bytes, ULONG length)
{
    ULONG i = 0;
    while (i < length)
    {
        ULONG cp; UBYTE n, b = bytes[i++];
        if (b < 0x80) continue;
        if (b >= 0xc2 && b <= 0xdf) { cp = b & 31; n = 1; }
        else if (b >= 0xe0 && b <= 0xef) { cp = b & 15; n = 2; }
        else if (b >= 0xf0 && b <= 0xf4) { cp = b & 7; n = 3; }
        else return FALSE;
        if (length - i < n) return FALSE;
        UBYTE count = n;
        while (n--) {
            b = bytes[i++]; if ((b & 0xc0) != 0x80) return FALSE;
            cp = (cp << 6) | (b & 63);
        }
        if ((count == 2 && cp < 0x800) || (count == 3 && cp < 0x10000) ||
            cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) return FALSE;
    }
    return TRUE;
}

VOID con_osc_clipboard_apply(struct ConsoleBase *base, struct intConUnit *unit)
{
    ULONG length; UBYTE target;
    if ((unit->oscCopyDisabled & 1) || unit->oscClipboardInvalid ||
        !osc_clipboard_decode(unit->oscClipboard, unit->oscClipboardLength, &length, &target) ||
        !con_osc_clipboard_utf8(unit->oscClipboard, length)) return;
    /* Native selection p/s maps to the device snip. c uses ConClip when
     * available; registered classic snip hooks retain their existing route. */
    if (target == 'c' && IsListEmpty(&base->sniphooks))
    {
        LONG result = con_copy_clipboard_request(base, unit->oscClipboard, length);
        if (result) return;
    }
    UBYTE *replacement = length ? AllocMem(length, MEMF_PUBLIC) : NULL;
    if (length && !replacement) return;
    if (length) CopyMem(unit->oscClipboard, replacement, length);
    ObtainSemaphore(&base->copyBufferLock);
    FreeMem((APTR)base->copyBuffer, base->copyBufferSize);
    base->copyBuffer = replacement;
    base->copyBufferSize = length;
    if (target == 'c' && !IsListEmpty(&base->sniphooks))
    {
        struct Hook *hook;
        struct { ULONG flags, size; APTR buffer; } data = {0, length, NULL};
        data.buffer = AllocVec(length + 1, MEMF_PUBLIC | MEMF_CLEAR);
        if (data.buffer)
        {
            if (length) CopyMem(replacement, data.buffer, length);
            ForeachNode(&base->sniphooks, hook) CALLHOOKPKT(hook, NULL, &data);
            FreeVec(data.buffer);
        }
    }
    ReleaseSemaphore(&base->copyBufferLock);
}

BOOL con_osc_clipboard_read(struct ConsoleBase *base, struct intConUnit *unit, BOOL st)
{
    if (unit->oscClipboardInvalid) return FALSE;
    ULONG split=0;
    while (split<unit->oscClipboardLength && unit->oscClipboard[split]!=';') split++;
    if (split>255 || split+2!=unit->oscClipboardLength ||
        unit->oscClipboard[split+1]!='?') return FALSE;
    UBYTE target=split?unit->oscClipboard[0]:'c';
    if ((target=='c'||target=='p'||target=='s') && (unit->oscCopyDisabled & 2))
        con_osc_read_request(base,(Object *)unit,target,st);
    return TRUE;
}
