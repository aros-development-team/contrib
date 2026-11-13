/* Copyright (C) 2026, The AROS Development Team. All rights reserved. */
#include <proto/exec.h>
#include <string.h>
#include "hyperlink.h"

static BOOL link_string(const UBYTE *s, ULONG length)
{
    if (!s && length) return FALSE;
    for (ULONG i=0;i<length;i++)
        if (s[i]<32 || s[i]==127) return FALSE;
    return TRUE;
}

BOOL con_link_retain(struct con_link *link)
{
    if (!link) return TRUE;
    if (link->references == (ULONG)~0UL) return FALSE;
    link->references++;
    return TRUE;
}

struct con_link *con_link_create(struct con_link_pool *pool,
    const UBYTE *uri, ULONG uriLength, const UBYTE *id, ULONG idLength)
{
    if (!pool || !uriLength || uriLength>CON_LINK_URI_MAX ||
        idLength>CON_LINK_ID_MAX || !link_string(uri,uriLength) ||
        !link_string(id,idLength)) return NULL;
    /* Named identical links share identity. Unnamed openings remain distinct
     * even when they point at the same URI (separate labels/highlight groups). */
    if (idLength)
        for (struct con_link *p=pool->first;p;p=p->next)
            if (p->uriLength==uriLength && p->idLength==idLength &&
                !memcmp(p->data,uri,uriLength) &&
                !memcmp(p->data+uriLength+1,id,idLength))
                return con_link_retain(p) ? p : NULL;
    ULONG bytes=sizeof(struct con_link)+uriLength+idLength+1;
    if (pool->records>=CON_LINK_RECORD_MAX || bytes>CON_LINK_BYTES_MAX ||
        pool->bytes>CON_LINK_BYTES_MAX-bytes) return NULL;
    struct con_link *p=AllocMem(bytes,MEMF_ANY);
    if (!p) return NULL;
    p->pool=pool;p->references=1;p->allocation=bytes;
    p->uriLength=uriLength;p->idLength=idLength;
    memcpy(p->data,uri,uriLength);p->data[uriLength]=0;
    if(idLength)memcpy(p->data+uriLength+1,id,idLength);
    p->data[uriLength+1+idLength]=0;
    p->next=pool->first;pool->first=p;pool->records++;pool->bytes+=bytes;
    return p;
}

VOID con_link_release(struct con_link *link)
{
    if (!link || --link->references) return;
    struct con_link_pool *pool=link->pool;
    struct con_link **p=&pool->first;
    while (*p!=link) p=&(*p)->next;
    *p=link->next;pool->records--;pool->bytes-=link->allocation;
    FreeMem(link,link->allocation);
}

/* Cell arrays are zero-initialized owning references, not borrowed pointers.
 * Acquire before release so filling/copying from the final live reference is
 * safe. A failed retain leaves every original cell and count unchanged. */
BOOL con_link_fill(struct con_link **cells, ULONG count, struct con_link *link)
{
    if ((!cells && count) || (link && count>(ULONG)~0UL-link->references)) return FALSE;
    if (link) link->references+=count;
    for (ULONG i=0;i<count;i++) {
        con_link_release(cells[i]);cells[i]=link;
    }
    return TRUE;
}

BOOL con_link_copy(struct con_link **destination, struct con_link **source, ULONG count)
{
    if ((!destination || !source) && count) return FALSE;
    if (count>(ULONG)~0UL/sizeof(*source)) return FALSE;
    ULONG i;
    for(i=0;i<count;i++)
        if(!con_link_retain(source[i])) {
            while(i) con_link_release(source[--i]);
            return FALSE;
        }
    for(i=0;i<count;i++)con_link_release(destination[i]);
    if(count)memmove(destination,source,count*sizeof(*source));
    return TRUE;
}

BOOL con_link_open(struct con_link_pool *pool, struct con_link **current,
    const UBYTE *payload, ULONG length)
{
    struct con_link *next = NULL;
    const UBYTE *id = NULL;
    ULONG separator = 0, idLength = 0;
    BOOL valid = FALSE, foundId = FALSE;
    if (!current || !pool) return FALSE;
    if (payload && length <= 2045 && link_string(payload, length))
    {
        while (separator < length && payload[separator] != ';') separator++;
        if (separator < length)
        {
            ULONG start = 0;
            while (start < separator)
            {
                ULONG end = start;
                while (end < separator && payload[end] != ':') end++;
                if (!foundId && end - start >= 3 &&
                    !memcmp(payload + start, "id=", 3))
                {
                    id = payload + start + 3;
                    idLength = end - start - 3;
                    foundId = TRUE;
                }
                start = end + 1;
            }
            ULONG uriLength = length - separator - 1;
            if (!uriLength) valid = TRUE;
            else
            {
                next = con_link_create(pool, payload + separator + 1,
                    uriLength, id, idLength);
                valid = next != NULL;
            }
        }
    }
    con_link_release(*current);
    *current = next;
    return valid;
}
