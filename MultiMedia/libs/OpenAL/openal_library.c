/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: openal.library lifetime.
*/

#include <exec/libraries.h>
#include <exec/memory.h>
#include <aros/symbolsets.h>
#include <proto/exec.h>

#include <stddef.h>
#include <string.h>

#include LC_LIBDEFS_FILE

/*
 * Memory. The C library's malloc() takes memory from the calling program's
 * own pool, which is deleted when that program exits. OpenAL keeps lists
 * shared by every program using the library (device names, HRTF data), and
 * a program may create or resize them and exit while another still uses
 * them. So the library allocates from a pool of its own instead: these
 * definitions take the place of the C library's for the code linked into
 * openal.library. The pool lives as long as the library - it is created
 * when the library is loaded, before OpenAL's constructors allocate, and
 * deleted when it is expunged, after OpenAL's destructors have released
 * what they own.
 */
struct OpenALMemHeader
{
    size_t size;
    size_t pad;     /* keeps the memory handed out 16-byte aligned */
};

static APTR openal_pool;

void *malloc(size_t size)
{
    struct OpenALMemHeader *hdr;

    if (!openal_pool || size > (~(size_t)0) - sizeof(*hdr))
        return NULL;
    if (size == 0)
        size = 1;

    hdr = AllocPooled(openal_pool, size + sizeof(*hdr));
    if (!hdr)
        return NULL;
    hdr->size = size;

    return hdr + 1;
}

void free(void *mem)
{
    if (mem && openal_pool)
    {
        struct OpenALMemHeader *hdr = (struct OpenALMemHeader *)mem - 1;

        FreePooled(openal_pool, hdr, hdr->size + sizeof(*hdr));
    }
}

void *calloc(size_t count, size_t size)
{
    void *mem;

    if (size && count > (~(size_t)0) / size)
        return NULL;
    if ((mem = malloc(count * size)))
        memset(mem, 0, count * size);

    return mem;
}

void *realloc(void *mem, size_t size)
{
    struct OpenALMemHeader *hdr;
    void *newmem;

    if (!mem)
        return malloc(size);
    if (size == 0)
    {
        free(mem);
        return NULL;
    }

    hdr = (struct OpenALMemHeader *)mem - 1;
    if ((newmem = malloc(size)))
    {
        memcpy(newmem, mem, (hdr->size < size) ? hdr->size : size);
        free(mem);
    }

    return newmem;
}

char *strdup(const char *str)
{
    size_t len = strlen(str) + 1;
    char *copy;

    if ((copy = malloc(len)))
        memcpy(copy, str, len);

    return copy;
}

static int OpenAL_CreatePool(void)
{
    openal_pool = CreatePool(MEMF_ANY | MEMF_SEM_PROTECTED, 65536, 4096);

    return openal_pool != NULL;
}

static void OpenAL_FreePool(void)
{
    if (openal_pool)
    {
        DeletePool(openal_pool);
        openal_pool = NULL;
    }
}

/* Library load (before the constructors) and expunge (after the destructors) */
ADD2INIT(OpenAL_CreatePool, 0);
ADD2EXIT(OpenAL_FreePool, 0);

/*
 * OpenAL keeps global state - its configuration, the log stream, the
 * device and HRTF lists - and the C runtime bases it was linked with are
 * opened when the library is loaded, on behalf of the program that loaded
 * it. None of that is valid once that program has exited. When the last
 * opener closes the library, have it expunged, so the next program loads
 * it afresh and OpenAL is initialised in that program's context.
 */
static int OpenAL_CloseLib(LIBBASETYPEPTR LIBBASE)
{
    if (((struct Library *)LIBBASE)->lib_OpenCnt == 0)
        ((struct Library *)LIBBASE)->lib_Flags |= LIBF_DELEXP;

    return TRUE;
}

ADD2CLOSELIB(OpenAL_CloseLib, 0);
