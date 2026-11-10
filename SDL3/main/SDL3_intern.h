#ifndef	_SDL3_INTERN_H
#define	_SDL3_INTERN_H

#include <exec/libraries.h>
#include <devices/timer.h>

struct SDL3Base
{
    struct Library          _lib;
	struct SDL3Base      	*Parent;
    struct SDL3Base      	*Root;

	/* This opener's SDL state (struct SDL3State, private to the SDL sources -
	   see src/aros/SDL_arosbase.h in the port).  SDL3.library is a
	   peropenerbase module, so every client gets its own block here instead
	   of sharing the file-scope statics SDL would otherwise use. */
	APTR                    State;
};

#endif /* _SDL3_INTERN_H */
