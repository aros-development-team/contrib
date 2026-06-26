/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: Variadic public functions for the SDL3 link library that are not
          provided by the locally-linked SDL_log.c / SDL_string.c / SDL_error.c
          objects.

    A va_list value cannot be passed across the AROS library jump table, so
    every variadic public function (and the va_list-consuming "V" variant it
    forwards to) must execute inside the application rather than via an LVO
    stub. SDL_Log(), the SDL_*printf() family and SDL_SetError() are supplied
    by the SDL_log.c, SDL_string.c and SDL_error.c link-library objects. The
    two below are not, so they are provided here. They format locally
    (SDL_vasprintf is local, from SDL_string.c) and then hand the finished
    string to a non-variadic,
    LVO-exported entry point - so no va_list ever crosses the jump table.
*/

#include "SDL_internal.h"

#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_stdinc.h>

#include <stdarg.h>

size_t SDL_IOprintf(SDL_IOStream *context, const char *fmt, ...)
{
    char *str = NULL;
    int len;
    size_t result;
    va_list ap;

    va_start(ap, fmt);
    len = SDL_vasprintf(&str, fmt, ap);
    va_end(ap);

    if (len < 0) {
        return 0;
    }

    result = SDL_WriteIO(context, str, (size_t)len);
    SDL_free(str);
    return result;
}

bool SDL_RenderDebugTextFormat(SDL_Renderer *renderer, float x, float y, const char *fmt, ...)
{
    char *str = NULL;
    va_list ap;
    bool result;

    va_start(ap, fmt);
    if (SDL_vasprintf(&str, fmt, ap) < 0) {
        va_end(ap);
        return false;
    }
    va_end(ap);

    result = SDL_RenderDebugText(renderer, x, y, str);
    SDL_free(str);
    return result;
}
