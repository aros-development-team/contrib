/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Multi-client check for SDL3.library.

    Run two instances at the same time. Each one reports values that are
    derived from state SDL keeps per client. If that state is shared between
    openers, the two reports disagree in ways this program names explicitly.

    Usage: sdl3mc <tag> <iterations>
*/

#include <SDL3/SDL.h>

#include <proto/dos.h>
#include <proto/exec.h>
#include <aros/debug.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HINTNAME "SDL_MULTICLIENT_PROBE"

static void report(const char *tag, const char *what, const char *fmt, ...)
{
    va_list ap;
    char buf[256];

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    /* bug() so both clients land interleaved in the one debug log the
       bootstrap writes, with no shell redirection to get right. */
    bug("[MC3:%s] %-22s %s\n", tag, what, buf);
}

int main(int argc, char **argv)
{
    const char *tag = (argc > 1) ? argv[1] : "client";
    int iters = (argc > 2) ? atoi(argv[2]) : 6;
    SDL_Window *win;
    SDL_WindowID myid = 0;
    const char *hint;
    int i, rc = 0;

    report(tag, "start", "task 0x%p", FindTask(NULL));

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        report(tag, "SDL_Init", "FAILED: %s", SDL_GetError());
        return 20;
    }
    report(tag, "SDL_Init", "ok");

    /* 1. Subsystem refcounts.  Only this client asked for events+joystick, so
          only this client may see them as initialised. */
    if (strcmp(tag, "A") == 0) {
        SDL_InitSubSystem(SDL_INIT_JOYSTICK);
        report(tag, "init JOYSTICK", "yes");
    }
    report(tag, "WasInit(JOYSTICK)", "%s",
           SDL_WasInit(SDL_INIT_JOYSTICK) ? "SET" : "clear");

    /* 2. Hints.  In SDL3 these live in a property set, so this also checks
          that the client's hint properties are its own. */
    SDL_SetHint(HINTNAME, tag);
    hint = SDL_GetHint(HINTNAME);
    report(tag, "hint readback", "'%s' %s", hint ? hint : "(null)",
           (hint && strcmp(hint, tag) == 0) ? "ok" : "*** MISMATCH ***");

    /* 3. SDL_GetGlobalProperties() is per process. */
    {
        SDL_PropertiesID g = SDL_GetGlobalProperties();
        SDL_SetStringProperty(g, "mc.owner", tag);
        const char *owner = SDL_GetStringProperty(g, "mc.owner", "(unset)");
        report(tag, "global property", "'%s' %s", owner,
               (strcmp(owner, tag) == 0) ? "ok" : "*** SHARED global props ***");
    }

    /* 4. The window list hangs off the video device, so each client must see
          exactly its own one window.  (Unlike SDL2, SDL3 hands out object ids
          from one global counter, so the id value itself says nothing about
          whether the device is shared - the list length does.) */
    win = SDL_CreateWindow(tag, 160, 120, SDL_WINDOW_HIDDEN);
    if (!win) {
        report(tag, "SDL_CreateWindow", "FAILED: %s", SDL_GetError());
        rc = 20;
    } else {
        int count = -1;
        SDL_Window * const *list = SDL_GetWindows(&count);

        myid = SDL_GetWindowID(win);
        report(tag, "window id", "%u", (unsigned)myid);
        report(tag, "windows visible", "%d %s", count,
               (count == 1 && list && list[0] == win)
                   ? "ok" : "*** video device is shared ***");
    }

    /* 5. Registered user-event ranges are per client. */
    {
        Uint32 ev = SDL_RegisterEvents(1);
        report(tag, "first user event", "%u %s", (unsigned)ev,
               (ev == SDL_EVENT_USER) ? "ok" : "*** SHARED user event range ***");

        /* 6. The event queue is per client. */
        {
            SDL_Event push, got;
            int seen_mine = 0, seen_other = 0;

            SDL_zero(push);
            push.type = ev;
            push.user.code = (tag[0] == 'A') ? 0xAAAA : 0xBBBB;
            SDL_PushEvent(&push);

            Delay(25);

            while (SDL_PollEvent(&got)) {
                if (got.type != ev)
                    continue;
                if (got.user.code == ((tag[0] == 'A') ? 0xAAAA : 0xBBBB))
                    seen_mine++;
                else
                    seen_other++;
            }
            report(tag, "queue mine/other", "%d/%d %s", seen_mine, seen_other,
                   (seen_mine == 1 && seen_other == 0)
                       ? "ok" : "*** SHARED event queue ***");
        }
    }

    /* 7. Event enable/disable is per client. */
    if (strcmp(tag, "A") == 0) {
        SDL_SetEventEnabled(SDL_EVENT_MOUSE_MOTION, false);
        report(tag, "disable MOUSEMOTION", "yes");
    }
    Delay(50);
    {
        bool on = SDL_EventEnabled(SDL_EVENT_MOUSE_MOTION);
        bool want = (strcmp(tag, "A") != 0);
        report(tag, "MOUSEMOTION state", "%s %s", on ? "enabled" : "ignored",
               (on == want) ? "ok" : "*** SHARED disable set ***");
    }

    /* Hold the state live so the other client overlaps with us, re-checking
       as we go: a late SDL_Quit() or hint write by the other client shows up
       here rather than after we have finished. */
    for (i = 0; i < iters; i++) {
        Delay(50);
        hint = SDL_GetHint(HINTNAME);
        if (!hint || strcmp(hint, tag) != 0)
            report(tag, "hint drifted", "at %d: '%s' *** SHARED ***", i,
                   hint ? hint : "(null)");
        if (strcmp(tag, "A") != 0 && SDL_WasInit(SDL_INIT_JOYSTICK))
            report(tag, "JOYSTICK appeared", "at %d *** SHARED REFCOUNTS ***", i);
        if (win) {
            int count = -1;
            SDL_GetWindows(&count);
            if (SDL_GetWindowID(win) != myid)
                report(tag, "window id changed", "at %d *** SHARED ***", i);
            if (count != 1)
                report(tag, "window count changed", "at %d: %d *** SHARED ***", i, count);
        }
    }

    if (win)
        SDL_DestroyWindow(win);
    if (strcmp(tag, "A") == 0)
        SDL_QuitSubSystem(SDL_INIT_JOYSTICK);

    SDL_Quit();
    report(tag, "done", "rc=%d", rc);

    return rc;
}
