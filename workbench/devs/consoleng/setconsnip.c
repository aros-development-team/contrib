/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.
*/

#include <aros/debug.h>
#include <proto/console.h>

#include <string.h>

#include "console_gcc.h"

AROS_LH1(LONG, SetConSnip,
    AROS_LHA(APTR, data, A0),
    struct ConsoleBase *, ConsoleDevice, 10, Consoleng)
{
    AROS_LIBFUNC_INIT

    /* Allocate before publishing: failure must preserve the previous snip.
     * A NULL/empty input still clears it and returns zero, as before. */
    ULONG size = data ? strlen(data) : 0;
    APTR replacement = size ? AllocMem(size, MEMF_PUBLIC) : NULL;
    if (size && !replacement)
        return 0;
    if (size)
        CopyMem(data, replacement, size);

    ObtainSemaphore(&ConsoleDevice->copyBufferLock);
    FreeMem((APTR) ConsoleDevice->copyBuffer, ConsoleDevice->copyBufferSize);
    ConsoleDevice->copyBuffer = replacement;
    ConsoleDevice->copyBufferSize = size;
    ReleaseSemaphore(&ConsoleDevice->copyBufferLock);

    return size != 0;

    AROS_LIBFUNC_EXIT
}
