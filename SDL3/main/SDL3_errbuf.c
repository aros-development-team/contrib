/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: Per-thread error buffer accessor for the SDL3 link library.

    SDL3 keeps SDL_GetErrBuf() in src/thread/SDL_thread.c, which also pulls in
    the whole thread backend (SDL_SYS_CreateThread, the TLS internals, ...).
    The link library only needs SDL_GetErrBuf() so that the locally-linked
    SDL_error.c / SDL_log.c objects (which must stay in the application to keep
    their va_list handling out of the library jump-table) have somewhere to
    store the error text. This is a self-contained copy of that accessor that
    relies solely on the exported (LVO) TLS and memory entry points.
*/

#include "SDL_internal.h"

#include "SDL_error_c.h"

#include <SDL3/SDL_thread.h>
#include <SDL3/SDL_stdinc.h>

static SDL_error *SDL_GetStaticErrBuf(void)
{
    static SDL_error SDL_global_error;
    static char SDL_global_error_str1[128];
    static char SDL_global_error_str2[128];
    SDL_global_error.info[0].str = SDL_global_error_str1;
    SDL_global_error.info[0].len = sizeof(SDL_global_error_str1);
    SDL_global_error.info[1].str = SDL_global_error_str2;
    SDL_global_error.info[1].len = sizeof(SDL_global_error_str2);
    return &SDL_global_error;
}

static void SDLCALL SDL_FreeErrBuf(void *data)
{
    SDL_error *errbuf = (SDL_error *)data;

    if (errbuf->info[0].str) {
        errbuf->free_func(errbuf->info[0].str);
    }
    if (errbuf->info[1].str) {
        errbuf->free_func(errbuf->info[1].str);
    }
    errbuf->free_func(errbuf);
}

SDL_error *SDL_GetErrBuf(bool create)
{
    static SDL_TLSID tls_errbuf;
    SDL_error *errbuf;

    errbuf = (SDL_error *)SDL_GetTLS(&tls_errbuf);
    if (!errbuf) {
        SDL_realloc_func realloc_func;
        SDL_free_func free_func;

        if (!create) {
            return NULL;
        }

        // Use the original allocators so the buffer survives an app-level
        // SDL_SetMemoryFunctions() call.
        SDL_GetOriginalMemoryFunctions(NULL, NULL, &realloc_func, &free_func);

        errbuf = (SDL_error *)realloc_func(NULL, sizeof(*errbuf));
        if (!errbuf) {
            return SDL_GetStaticErrBuf();
        }
        SDL_zerop(errbuf);
        errbuf->realloc_func = realloc_func;
        errbuf->free_func = free_func;
        SDL_SetTLS(&tls_errbuf, errbuf, SDL_FreeErrBuf);
    }
    return errbuf;
}
