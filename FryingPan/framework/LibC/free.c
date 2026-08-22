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

#include <Startup/Startup.h>
#include "LibC.h"
#include <proto/exec.h>
#include <Generic/Types.h>

void free(void* pMem)
{
    struct FPMemPrivate *priv;
    void* pPool;
    size_t lSize;    /* use size_t to match stored allocsize */

    if (NULL == pMem)
        return;

    ADB(kprintf("[fryingpan] %s: allocation @ 0x%p\n", __func__, pMem);)

#ifndef MEMDEBUG
    /* subtract bytes from the byte-address, then cast to the struct pointer */
    priv = (struct FPMemPrivate *)((IPTR)pMem - (IPTR)sizeof(struct FPMemPrivate));
    pPool = priv->allocpool;
    lSize = priv->allocsize;
    pMem  = priv->allocraw;

    ADB(kprintf("[fryingpan] %s: pool @ 0x%p, raw = 0x%p (%u bytes)\n", __func__, pPool, pMem, lSize);)

#else
    IPTR *pMem1 = (IPTR *)pMem;
    /* indices computed from MEMDEBUG_CANARY_SLOTS to avoid magic numbers */
    const int META_POOL_INDEX = -(MEMDEBUG_CANARY_SLOTS + 1);   /* -17 */
    const int META_SIZE_INDEX = -(MEMDEBUG_CANARY_SLOTS + 2);   /* -18 */
    const int META_RAW_INDEX  = -(MEMDEBUG_CANARY_SLOTS + 3);   /* -19 */

    pPool = ((void**)pMem)[META_POOL_INDEX];
    lSize = ((IPTR *)pMem)[META_SIZE_INDEX];
    pMem  = (void*)((IPTR *)pMem)[META_RAW_INDEX];

    /* verify canaries */
    {
        IPTR *pMem2 = (IPTR *)((IPTR)pMem + (IPTR)lSize);
        uint32 i;

        for (i = 1; i <= (uint32)MEMDEBUG_CANARY_SLOTS; ++i)
        {
            if ((pMem1[-(int)i] != (IPTR)0xC0DEDBAD) ||
                (pMem2[-(int)i] != (IPTR)0xC0DEDBAD))
            {
                _error("Freeing corrupted memory block!\nSize: %zu bytes\nMEMORY WILL NOT BE FREED!\nPLEASE REPORT!", lSize - (MEMDEBUG_CANARY_SLOTS * sizeof(IPTR)) - (IPTR)sizeof(void *));
                return;
            }
        }
    }
#endif

    /* Now free using either the block's pool or the internal pool */
    if (NULL != pPool) {
#ifndef __amigaos4__
        ObtainSemaphore(&__InternalSemaphore);
        FreePooled(pPool, pMem, (IPTR)lSize);
        ReleaseSemaphore(&__InternalSemaphore);
#else
        IExec->ObtainSemaphore(&__InternalSemaphore);
        IExec->FreePooled(pPool, pMem, (IPTR)lSize);
        IExec->ReleaseSemaphore(&__InternalSemaphore);
#endif
    } else {
#ifndef __amigaos4__
        ObtainSemaphore(&__InternalSemaphore);
        FreePooled(__InternalMemPool, pMem, (IPTR)lSize);
        ReleaseSemaphore(&__InternalSemaphore);
#else
        IExec->ObtainSemaphore(&__InternalSemaphore);
        IExec->FreePooled(__InternalMemPool, pMem, (IPTR)lSize);
        IExec->ReleaseSemaphore(&__InternalSemaphore);
#endif
    }
}
