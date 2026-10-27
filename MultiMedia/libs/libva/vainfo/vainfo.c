/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    vainfo: the VA-API driver and decode profiles of a screen's display.
*/

#include <proto/exec.h>
#include <proto/intuition.h>

#include <stdio.h>
#include <stdlib.h>

#include <va/va.h>
#include <va/va_str.h>
#include <va/va_aros.h>

int main(void)
{
    struct Screen *screen;
    VADisplay dpy;
    VAProfile *profiles;
    VAStatus st;
    int major, minor, num, i, rc = 0;

    screen = LockPubScreen(NULL);
    if (!screen)
    {
        printf("vainfo: no public screen\n");
        return 20;
    }

    dpy = vaGetDisplayAROS(screen);
    if (!dpy)
    {
        printf("vainfo: vaGetDisplayAROS failed\n");
        UnlockPubScreen(NULL, screen);
        return 20;
    }

    st = vaInitialize(dpy, &major, &minor);
    if (st != VA_STATUS_SUCCESS)
    {
        printf("vainfo: vaInitialize failed: %s\n", vaErrorStr(st));
        UnlockPubScreen(NULL, screen);
        return 20;
    }

    printf("vainfo: VA-API version %d.%d\n", major, minor);
    printf("vainfo: Driver version: %s\n", vaQueryVendorString(dpy));

    profiles = malloc(vaMaxNumProfiles(dpy) * sizeof(VAProfile));
    if (profiles && vaQueryConfigProfiles(dpy, profiles, &num) == VA_STATUS_SUCCESS)
    {
        printf("vainfo: Supported profile and entrypoints\n");
        if (num == 0)
            printf("      (none: the display has no hardware decoder)\n");
        for (i = 0; i < num; i++)
        {
            VAEntrypoint eps[8];
            int neps = 0, j;

            if (vaQueryConfigEntrypoints(dpy, profiles[i], eps, &neps) != VA_STATUS_SUCCESS)
                continue;
            for (j = 0; j < neps; j++)
                printf("      %-32s: %s\n", vaProfileStr(profiles[i]), vaEntrypointStr(eps[j]));
        }
    }
    else
        rc = 10;
    free(profiles);

    vaTerminate(dpy);
    UnlockPubScreen(NULL, screen);
    return rc;
}
