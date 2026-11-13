/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.
*/

#include <aros/debug.h>
#include <proto/console.h>
#include <proto/exec.h>

#include "console_gcc.h"

AROS_LH1(void, AddConSnipHook,
    AROS_LHA(struct Hook *, hook, A0),
    struct ConsoleBase *, ConsoleDevice, 11, Consoleng)
{
    AROS_LIBFUNC_INIT

    ObtainSemaphore(&ConsoleDevice->copyBufferLock);
    AddTail((struct List *)&ConsoleDevice->sniphooks, (struct Node *)hook);
    ReleaseSemaphore(&ConsoleDevice->copyBufferLock);

    AROS_LIBFUNC_EXIT
}
