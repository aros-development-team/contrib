#ifndef _VA_AROS_H_
#define _VA_AROS_H_

/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    VA-API on AROS: displays are screens. Decoding is done by the hardware
    decoder of the screen's display driver (gfx.hidd decode interface);
    decoded surfaces are video bitmaps of that display and are shown on a
    video plane over a window, or not at all: there is no colour
    conversion fallback, an application without a plane uses its own
    path (vaGetImage / vaDeriveImage and its software renderer).
*/

#include <va/va.h>

struct Screen;
struct Window;

#ifdef __cplusplus
extern "C" {
#endif

/* VADriverContext.display_type of an AROS display */
#define VA_DISPLAY_AROS     0xA0

/*
 * The VA display of a screen: its display driver decodes and its video
 * planes show the surfaces. Release it with vaTerminate().
 */
VADisplay vaGetDisplayAROS(struct Screen *screen);

/*
 * As vaGetDisplayAROS() for the public screen of that name (NULL: the
 * default public screen). The screen stays locked (LockPubScreen) until
 * vaTerminate(). For code that does not include Amiga headers.
 */
VADisplay vaGetDisplayAROSPubScreen(const char *name);

/*
 * Show a surface in a window (of the display's screen) on a video plane.
 * src is the part of the surface (NULL: all of it), dst the rectangle
 * relative to the window's inner area (NULL: all of it); a different size
 * scales when the plane can. The first call attaches the window, later
 * calls for the same window flip to the new surface at the next vertical
 * blank and follow window moves and resizes; the visible part tracks the
 * window's layer.
 *
 * Returns VA_STATUS_ERROR_OPERATION_FAILED when no plane shows the
 * surface (none free, or the format cannot be scanned out): the
 * application then presents frames itself.
 */
VAStatus vaPutSurfaceAROS(VADisplay dpy, VASurfaceID surface, struct Window *window,
                          const VARectangle *src, const VARectangle *dst,
                          unsigned int flags);

/* Stop showing surfaces in the window (before closing it). */
VAStatus vaReleaseWindowAROS(VADisplay dpy, struct Window *window);

#ifdef __cplusplus
}
#endif

#endif /* _VA_AROS_H_ */
