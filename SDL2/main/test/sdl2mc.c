/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Multi-client check for SDL2.library.

    Run two instances at the same time. Each one reports values that are
    derived from state SDL keeps per client. If that state is shared between
    openers, the two reports disagree in ways this program names explicitly.

    Usage: sdl2mc <tag> <seconds>
*/

#include <SDL2/SDL.h>

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
    bug("[MC:%s] %-22s %s\n", tag, what, buf);
}

int main(int argc, char **argv)
{
    const char *tag = (argc > 1) ? argv[1] : "client";
    int seconds = (argc > 2) ? atoi(argv[2]) : 6;
    SDL_Window *win;
    const char *hint;
    int i, rc = 0;

    /* Stagger so both clients are certain to overlap inside SDL_Init(). */
    report(tag, "start", "pid task 0x%p", FindTask(NULL));

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        report(tag, "SDL_Init", "FAILED: %s", SDL_GetError());
        return 20;
    }
    report(tag, "SDL_Init", "ok");

    /* 1. Subsystem refcounts. Only this client asked for TIMER, so only this
          client may see it as initialised. */
    if (strcmp(tag, "A") == 0) {
        SDL_InitSubSystem(SDL_INIT_TIMER);
        report(tag, "init TIMER", "yes");
    }
    report(tag, "WasInit(TIMER)", "%s",
           SDL_WasInit(SDL_INIT_TIMER) ? "SET" : "clear");

    /* 2. Hints. Each client writes its own tag and must read back its own. */
    SDL_SetHint(HINTNAME, tag);
    hint = SDL_GetHint(HINTNAME);
    report(tag, "hint readback", "'%s' %s", hint ? hint : "(null)",
           (hint && strcmp(hint, tag) == 0) ? "ok" : "*** MISMATCH ***");

    /* 3. Window ids come from the video device's next_object_id counter, so
          the first window each client creates must be id 1 for both. */
    win = SDL_CreateWindow(tag, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                           160, 120, SDL_WINDOW_HIDDEN);
    if (!win) {
        report(tag, "SDL_CreateWindow", "FAILED: %s", SDL_GetError());
        rc = 20;
    } else {
        Uint32 id = SDL_GetWindowID(win);
        report(tag, "first window id", "%u %s", (unsigned)id,
               (id == 1) ? "ok" : "*** NOT 1: video device is shared ***");

        /* 4. A window id this client never created must not resolve. */
        report(tag, "foreign id 2", "%s",
               SDL_GetWindowFromID(2) ? "*** RESOLVED: shared window list ***"
                                      : "unresolved ok");
    }

    /* 5. Registered user-event ranges are per client, so each client's first
          SDL_RegisterEvents() must return the same value. */
    {
        Uint32 ev = SDL_RegisterEvents(1);
        report(tag, "first user event", "%u %s", (unsigned)ev,
               (ev == SDL_USEREVENT) ? "ok" : "*** SHARED user event range ***");

        /* 6. The event queue is per client: push one with our own tag and
              make sure nothing of the other client's turns up. */
        {
            SDL_Event push, got;
            int seen_mine = 0, seen_other = 0;

            SDL_memset(&push, 0, sizeof(push));
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

    /* 7. Event enable/disable is per client.  A turns mouse motion off; B
          must still have it on. */
    if (strcmp(tag, "A") == 0) {
        SDL_EventState(SDL_MOUSEMOTION, SDL_IGNORE);
        report(tag, "disable MOUSEMOTION", "yes");
    }
    Delay(50);
    report(tag, "MOUSEMOTION state", "%s %s",
           SDL_EventState(SDL_MOUSEMOTION, SDL_QUERY) == SDL_ENABLE
               ? "enabled" : "ignored",
           (strcmp(tag, "A") == 0)
               ? (SDL_EventState(SDL_MOUSEMOTION, SDL_QUERY) == SDL_IGNORE ? "ok" : "*** not ours ***")
               : (SDL_EventState(SDL_MOUSEMOTION, SDL_QUERY) == SDL_ENABLE ? "ok" : "*** SHARED disable set ***"));

    /* Hold the state live so the other client overlaps with us, re-checking
       as we go: a late SDL_Quit() or hint write by the other client shows up
       here rather than after we have finished. */
    for (i = 0; i < seconds; i++) {
        Delay(50);
        hint = SDL_GetHint(HINTNAME);
        if (!hint || strcmp(hint, tag) != 0)
            report(tag, "hint drifted", "at %ds: '%s' *** SHARED ***", i,
                   hint ? hint : "(null)");
        if (strcmp(tag, "A") != 0 && SDL_WasInit(SDL_INIT_TIMER))
            report(tag, "TIMER appeared", "at %ds *** SHARED REFCOUNTS ***", i);
        if (win && SDL_GetWindowID(win) != 1)
            report(tag, "window id changed", "at %ds *** SHARED ***", i);
    }

    if (win)
        SDL_DestroyWindow(win);
    if (strcmp(tag, "A") == 0)
        SDL_QuitSubSystem(SDL_INIT_TIMER);

    SDL_Quit();
    report(tag, "done", "rc=%d", rc);

    return rc;
}
