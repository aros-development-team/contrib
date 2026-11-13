/* Copyright (C) 2026, The AROS Development Team. All rights reserved. */
#ifndef CONSOLENG_HYPERLINK_H
#define CONSOLENG_HYPERLINK_H
#include <exec/types.h>

/* Private immutable OSC8 metadata. Pools and cell arrays are owned by one
 * console task; no cross-task mutation is allowed. Release all cell/cursor
 * references before disposing their pool. Limits apply to live metadata. */
#define CON_LINK_URI_MAX 1023
#define CON_LINK_ID_MAX 255
#define CON_LINK_RECORD_MAX 128
#define CON_LINK_BYTES_MAX 65536
struct con_link;
struct con_link_pool {
    struct con_link *first;
    ULONG records, bytes;
};
struct con_link {
    struct con_link *next;
    struct con_link_pool *pool;
    ULONG references, allocation, uriLength, idLength;
    UBYTE data[1];             /* URI NUL, then optional ID NUL */
};
struct con_link *con_link_create(struct con_link_pool *pool,
    const UBYTE *uri, ULONG uriLength, const UBYTE *id, ULONG idLength);
BOOL con_link_retain(struct con_link *link);
VOID con_link_release(struct con_link *link);
BOOL con_link_fill(struct con_link **cells, ULONG count, struct con_link *link);
BOOL con_link_copy(struct con_link **destination, struct con_link **source, ULONG count);
/* Set current from the OSC8 payload after "8;". Malformed payloads and
 * allocation failures close the previous link rather than mislabel new text. */
BOOL con_link_open(struct con_link_pool *pool, struct con_link **current,
    const UBYTE *payload, ULONG length);
#endif
