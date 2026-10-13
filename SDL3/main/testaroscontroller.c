/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Desc: testaroscontroller - console exerciser for the AROS SDL3 joystick
          backend (controller.hidd devices and lowlevel.library ports).
          Lists the joysticks, opens each as a gamepad when possible, prints
          the mapping SDL derived, then polls for the given time reporting
          hotplug and input events. With SERIAL the output is mirrored to
          the debug console so a QEMU run can be checked from its log.

    Usage: testaroscontroller [SERIAL] [TIME=<seconds>] [RUMBLE] [NOHIDD]
*/

#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <exec/types.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <aros/debug.h>

#include <stdio.h>
#include <string.h>

static BOOL g_serial = FALSE;

static void out(const char *fmt, ...)
{
    char buf[1024];
    va_list ap;

    va_start(ap, fmt);
    SDL_vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    fputs(buf, stdout);
    fflush(stdout);
    if (g_serial) {
        kprintf("%s", buf);
    }
}

static void describe(SDL_JoystickID id)
{
    SDL_GUID guid = SDL_GetJoystickGUIDForID(id);
    char g[33];
    char *mapping;

    SDL_GUIDToString(guid, g, sizeof(g));
    out("  joystick %u '%s' guid %s vendor %04x product %04x path '%s' gamepad %s\n",
        (unsigned)id, SDL_GetJoystickNameForID(id), g,
        SDL_GetJoystickVendorForID(id), SDL_GetJoystickProductForID(id),
        SDL_GetJoystickPathForID(id) ? SDL_GetJoystickPathForID(id) : "",
        SDL_IsGamepad(id) ? "yes" : "no");
    mapping = SDL_GetGamepadMappingForID(id);
    if (mapping) {
        out("    mapping: %s\n", mapping);
        SDL_free(mapping);
    }
}

int main(int argc, char *argv[])
{
    IPTR args[4] = { 0, 0, 0, 0 };
    struct RDArgs *rda = ReadArgs("SERIAL/S,TIME/N,RUMBLE/S,NOHIDD/S", args, NULL);
    int seconds = 10;
    Uint64 deadline;
    int rc = RETURN_OK;

    if (!rda) {
        PrintFault(IoErr(), "testaroscontroller");
        return RETURN_FAIL;
    }
    g_serial = args[0] ? TRUE : FALSE;
    if (args[1]) {
        seconds = *(LONG *)args[1];
    }
    if (args[3]) {
        SDL_SetHint("SDL_JOYSTICK_AROS_HIDD", "0");
    }

    SDL_SetMainReady();
    if (!SDL_Init(SDL_INIT_JOYSTICK | SDL_INIT_GAMEPAD | SDL_INIT_EVENTS)) {
        out("SDL_Init failed: %s\n", SDL_GetError());
        FreeArgs(rda);
        return RETURN_FAIL;
    }

    {
        int count = 0;
        SDL_JoystickID *ids = SDL_GetJoysticks(&count);
        int i;

        out("testaroscontroller: %d joystick(s) at start\n", count);
        for (i = 0; ids && i < count; i++) {
            describe(ids[i]);
            if (SDL_IsGamepad(ids[i])) {
                SDL_OpenGamepad(ids[i]);
            } else {
                SDL_OpenJoystick(ids[i]);
            }
        }
        SDL_free(ids);
    }

    deadline = SDL_GetTicks() + (Uint64)seconds * 1000;
    while (SDL_GetTicks() < deadline) {
        SDL_Event ev;

        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
            case SDL_EVENT_JOYSTICK_ADDED:
                out("event: joystick %u added\n", (unsigned)ev.jdevice.which);
                describe(ev.jdevice.which);
                if (!SDL_IsGamepad(ev.jdevice.which)) {
                    SDL_OpenJoystick(ev.jdevice.which);
                }
                break;
            case SDL_EVENT_JOYSTICK_REMOVED:
                out("event: joystick %u removed\n", (unsigned)ev.jdevice.which);
                break;
            case SDL_EVENT_GAMEPAD_ADDED:
            {
                SDL_Gamepad *gp = SDL_OpenGamepad(ev.gdevice.which);
                out("event: gamepad %u added%s\n", (unsigned)ev.gdevice.which, gp ? "" : " (open failed)");
                if (gp && args[2]) {
                    out("  rumble: %s\n", SDL_RumbleGamepad(gp, 0x4000, 0x8000, 500) ? "ok" : SDL_GetError());
                }
                break;
            }
            case SDL_EVENT_GAMEPAD_REMOVED:
                out("event: gamepad %u removed\n", (unsigned)ev.gdevice.which);
                break;
            case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
            case SDL_EVENT_GAMEPAD_BUTTON_UP:
                out("event: gamepad %u button %s %s\n", (unsigned)ev.gbutton.which,
                    SDL_GetGamepadStringForButton((SDL_GamepadButton)ev.gbutton.button), ev.gbutton.down ? "down" : "up");
                break;
            case SDL_EVENT_GAMEPAD_AXIS_MOTION:
                out("event: gamepad %u axis %s %d\n", (unsigned)ev.gaxis.which,
                    SDL_GetGamepadStringForAxis((SDL_GamepadAxis)ev.gaxis.axis), (int)ev.gaxis.value);
                break;
            case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
            case SDL_EVENT_JOYSTICK_BUTTON_UP:
                out("event: joystick %u button %d %s\n", (unsigned)ev.jbutton.which, (int)ev.jbutton.button,
                    ev.jbutton.down ? "down" : "up");
                break;
            case SDL_EVENT_JOYSTICK_AXIS_MOTION:
                out("event: joystick %u axis %d %d\n", (unsigned)ev.jaxis.which, (int)ev.jaxis.axis, (int)ev.jaxis.value);
                break;
            case SDL_EVENT_JOYSTICK_HAT_MOTION:
                out("event: joystick %u hat %d %d\n", (unsigned)ev.jhat.which, (int)ev.jhat.hat, (int)ev.jhat.value);
                break;
            case SDL_EVENT_JOYSTICK_BATTERY_UPDATED:
                out("event: joystick %u battery state %d percent %d\n", (unsigned)ev.jbattery.which,
                    (int)ev.jbattery.state, ev.jbattery.percent);
                break;
            default:
                break;
            }
        }
        SDL_Delay(10);
    }

    {
        int count = 0;
        SDL_JoystickID *ids = SDL_GetJoysticks(&count);
        out("testaroscontroller: %d joystick(s) at end\n", count);
        SDL_free(ids);
    }

    SDL_Quit();
    FreeArgs(rda);
    return rc;
}
