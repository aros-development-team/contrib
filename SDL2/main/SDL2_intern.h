#ifndef	_SDL2_INTERN_H
#define	_SDL2_INTERN_H

#include <exec/libraries.h>
#include <devices/timer.h>

struct SDL2Base
{
    struct Library          _lib;
	struct SDL2Base      	*Parent;
    struct SDL2Base      	*Root;

	/* This opener's SDL state (struct SDL2State, private to the SDL sources -
	   see src/aros/SDL_arosbase.h in the port).  SDL2.library is a
	   peropenerbase module, so every client gets its own block here instead
	   of sharing the file-scope statics SDL would otherwise use. */
	APTR                    State;
};

#endif /* _SDL2_INTERN_H */
