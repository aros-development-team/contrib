/*
 * Amiga Generic Set - set of libraries and includes to ease sw development for all Amiga platforms
 * Copyright (C) 2001-2011 Tomasz Wiszkowski Tomasz.Wiszkowski at gmail.com.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "LibC.h"
#include <Startup/Startup.h>
#include <proto/exec.h>
#include <exec/memory.h>
#include <Generic/Types.h>

// REMEMBER:
// the resulting stuff is used by
// free and realloc!
// you may want to change these if you modify anything here.

#if defined(AROS_WORSTALIGN)
#define MALLOC_ALIGN AROS_WORSTALIGN
#else
#define MALLOC_ALIGN 8
#endif

void* malloc(size_t lSize)
{
    struct FPMemPrivate *priv = NULL;
    void *raw = NULL;
    void *pMem = NULL;
    size_t mallocSize = 0;
    size_t prefix_bytes = 0;

    if (lSize == 0)
        return NULL;

    ADB(kprintf("[fryingpan] %s: requested size = %zu\n", __func__, lSize));

    if (__InternalMemPool == NULL)
        return NULL;

    ADB(kprintf("[fryingpan] %s: internal pool @ %p\n", __func__, __InternalMemPool));

#ifndef MEMDEBUG
    /* Non-debug path: prefix is size of FPMemPrivate */
    /* The returned pointer is aligned to MALLOC_ALIGN, so the prefix must be a
     * whole number of MALLOC_ALIGN units.  A bare sizeof(struct FPMemPrivate)
     * leaves the block short of the request whenever aligning the user pointer
     * consumes more than that.
     */
    prefix_bytes = (sizeof(struct FPMemPrivate) + (MALLOC_ALIGN - 1)) & ~(size_t)(MALLOC_ALIGN - 1);

    /* Overflow detection */
    if (lSize + prefix_bytes < lSize) {
        return NULL; /* overflow */
    }
    if (lSize + prefix_bytes + (MALLOC_ALIGN - 1) < lSize + prefix_bytes) {
        return NULL; /* overflow */
    }

    /* One alignment unit of slack: the pool may return a block that is not
     * MALLOC_ALIGN aligned, and aligning the user pointer then costs more.
     */
    mallocSize = (lSize + prefix_bytes + MALLOC_ALIGN + (MALLOC_ALIGN - 1)) & ~(IPTR)(MALLOC_ALIGN - 1);

    ADB(kprintf("[fryingpan] %s: adjusted size = %zu\n", __func__, mallocSize));

    for (;;) {
#ifndef __amigaos4__
        ObtainSemaphore(&__InternalSemaphore);
        raw = (void*)AllocPooled(__InternalMemPool, mallocSize);
        ReleaseSemaphore(&__InternalSemaphore);
#else
        IExec->ObtainSemaphore(&__InternalSemaphore);
        raw = (void*)IExec->AllocPooled(__InternalMemPool, mallocSize);
        IExec->ReleaseSemaphore(&__InternalSemaphore);
#endif
        if (raw == NULL) {
            /* ask user to retry; if they cancel, give up */
            _error("Unable to allocate %zu bytes of memory!!!\nClick 'OK' to retry", lSize);
            /* else loop and try again */
            continue;
        }

        /* success: compute aligned returned pointer (byte arithmetic) */
        {
            IPTR base = (IPTR)raw;
            IPTR addr = base + prefix_bytes; /* leave room for priv */
            addr = (addr + (MALLOC_ALIGN - 1)) & ~(IPTR)(MALLOC_ALIGN - 1);

            priv = (struct FPMemPrivate*)(addr - sizeof(struct FPMemPrivate));
            pMem = (void*)addr;

            ADB(kprintf("[fryingpan] %s: alloc @ %p (raw %p)\n", __func__, pMem, raw));

            /* Record the pool this block really came from.  Every binary
             * links its own copy of this allocator with its own pool, so a
             * block freed by another binary's free() must not be returned to
             * that binary's pool.
             */
            priv->allocpool = __InternalMemPool;
            priv->allocwanted = lSize;
            priv->allocsize = mallocSize;
            priv->allocraw  = raw;
        }

        break; /* done */
    }

#else
    /* MEMDEBUG path: reserve space for canaries + metadata before returned pointer */
    prefix_bytes = MEMDEBUG_PREFIX_SLOTS * sizeof(IPTR);
#if !defined(__AROS__) || __WORDSIZE==32
    prefix_bytes += sizeof(IPTR);
#endif
    /* Overflow detection without SIZE_MAX */
    if (lSize + prefix_bytes < lSize) {
        return NULL;
    }
    if (lSize + prefix_bytes + (MALLOC_ALIGN - 1) < lSize + prefix_bytes) {
        return NULL;
    }

    mallocSize = (lSize + prefix_bytes + (MALLOC_ALIGN - 1)) & ~(IPTR)(MALLOC_ALIGN - 1);

    ADB(kprintf("[fryingpan] %s: adjusted size = %zu\n", __func__, mallocSize));

    for (;;) {
#ifndef __amigaos4__
        ObtainSemaphore(&__InternalSemaphore);
        raw = (void*)AllocPooled(__InternalMemPool, mallocSize);
        ReleaseSemaphore(&__InternalSemaphore);
#else
        IExec->ObtainSemaphore(&__InternalSemaphore);
        raw = (void*)IExec->AllocPooled(__InternalMemPool, mallocSize);
        IExec->ReleaseSemaphore(&__InternalSemaphore);
#endif
        if (raw == NULL) {
            _error("Unable to allocate %zu bytes of memory!!!\nClick 'OK' to retry", lSize);
            continue;
        }

        /* success: set up debug layout */
        {
            IPTR *pMem2;
            uint32 i;

            IPTR base = (IPTR)raw;
            IPTR addr = base + prefix_bytes;
            addr = (addr + (MALLOC_ALIGN - 1)) & ~(IPTR)(MALLOC_ALIGN - 1);

            pMem = (void*)addr; /* user pointer */

            /* Fill canaries: pMem[-1] .. pMem[-MEMDEBUG_CANARY_SLOTS] */
            for (i = 1; i <= (uint32)MEMDEBUG_CANARY_SLOTS; ++i) {
                ((IPTR*)pMem)[-(int)i] = (IPTR)0xC0DEDBAD;
            }

            /* metadata slots after canaries: */
            ((IPTR*)pMem)[-(MEMDEBUG_CANARY_SLOTS + 1)] = 0;
            ((IPTR*)pMem)[-(MEMDEBUG_CANARY_SLOTS + 2)] = (IPTR)mallocSize;
            ((IPTR*)pMem)[-(MEMDEBUG_CANARY_SLOTS + 3)] = (IPTR)raw;

            /* Mirror canaries at block end */
            pMem2 = (IPTR *)((IPTR)raw + mallocSize);
            for (i = 1; i <= (uint32)MEMDEBUG_CANARY_SLOTS; ++i) {
                pMem2[-(int)i] = (IPTR)0xC0DEDBAD;
            }

            ADB(kprintf("[fryingpan] %s: alloc @ %p (raw %p)\n", __func__, pMem, raw));
        }

        break; /* done */
    }

#endif /* MEMDEBUG */

    return pMem;
}

void* malloc_pooled(void* pPool, size_t lSize)
{
    struct FPMemPrivate *priv;
    IPTR *raw = NULL;
    IPTR *pMem = NULL;

    if (lSize == 0)
        return NULL;

    ADB(kprintf("[fryingpan] %s: requested size = %u\n", __func__, lSize));

    // Reserve space for metadata + max alignment overhead
    if (__InternalMemPool != NULL)
    {
        ADB(kprintf("[fryingpan] %s: internal pool @ 0x%p\n", __func__, __InternalMemPool));

#ifndef MEMDEBUG
        // Round up requested size to alignment
        size_t mallocSize = (lSize +  sizeof(struct FPMemPrivate) + (MALLOC_ALIGN - 1)) & ~(MALLOC_ALIGN - 1);

#ifndef __amigaos4__
        raw = (IPTR *)AllocPooled(pPool, mallocSize);
#else
        raw = (IPTR *)IExec->AllocPooled(pPool, mallocSize);
#endif

        if (raw != NULL)
        {
            // Leave room for metadata and MALLOC_ALIGN the pointer
            IPTR addr = (IPTR)raw + sizeof(struct FPMemPrivate);
            addr = (addr + (MALLOC_ALIGN - 1)) & ~(MALLOC_ALIGN - 1);
            priv = (struct FPMemPrivate *)addr - sizeof(struct FPMemPrivate);
            pMem = (IPTR *)addr;

            ADB(kprintf("[fryingpan] %s: alloc @ 0x%p (raw 0x%p)\n", __func__, pMem, raw));

            // Store metadata just before aligned address
            priv->allocpool = pPool;
            priv->allocwanted = lSize;
            priv->allocsize = mallocSize;
            priv->allocraw = raw;
        }
#else
        size_t mallocSize = (lSize +  (sizeof(IPTR) << 4) +  (sizeof(IPTR) * 3) + (MALLOC_ALIGN - 1)) & ~(MALLOC_ALIGN - 1);

#ifndef __amigaos4__
        raw = (IPTR*)AllocPooled(pPool, mallocSize);
#else
        raw = (IPTR*)IExec->AllocPooled(pPool, mallocSize);
#endif
        if (raw != NULL)
        {
            IPTR *pMem2;
            uint32 i;

            IPTR addr = (IPTR)(raw + (sizeof(IPTR) << 5) +  (sizeof(IPTR) * 3));
            addr = (addr + (MALLOC_ALIGN - 1)) & ~(MALLOC_ALIGN - 1);
            pMem = (IPTR *)addr;

            pMem[-17] = (IPTR)pPool;
            pMem[-18] = mallocSize;
            pMem[-19] = (IPTR)raw;

            pMem2   = (IPTR *)((IPTR)raw + mallocSize);
            for (i=1; i<=(16); i++)
            {
                    pMem[-i]    = 0xC0DEDBAD;
                    pMem2[-i]   = 0xC0DEDBAD;
            }
        }      
#endif
   }

   if (pMem == NULL)
   {
        _error("Unable to allocate %ld bytes of memory!!!\nClick 'OK' to retry", lSize);
        return malloc(lSize);
   }

   return pMem;
}
