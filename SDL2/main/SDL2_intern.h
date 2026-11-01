#ifndef	_SDL2_INTERN_H
#define	_SDL2_INTERN_H

#include <exec/libraries.h>
#include <devices/timer.h>

/* No library bases here: SDL2.library holds one open of each for its own
 * lifetime, in the globals set up by init_libs() in SDL2_library.c. The
 * per-opener copies that used to live here were only written by SDL2LIB_Open()
 * and only released for the LAST closer, so every other opener's handles
 * leaked. */
struct SDL2Base
{
    struct Library          _lib;
	struct SDL2Base      	*Parent;
    struct SDL2Base      	*Root;
};

#endif /* _SDL2_INTERN_H */
