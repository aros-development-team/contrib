/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2022 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

//#define DEBUG 1
#include <aros/debug.h>

#include <aros/atomic.h>
#include <aros/symbolsets.h>

#include <devices/timer.h>
#include <proto/exec.h>

#include <stddef.h>
#include <stdlib.h>

#include "SDL3_intern.h"

extern void 	SDL_Quit(void);

struct Library       *TimerBase = NULL;

struct timerequest   GlobalTimeReq;

static int init_sdl2()
{
	if (OpenDevice("timer.device", UNIT_MICROHZ, &GlobalTimeReq.tr_node, 0) == 0)
	{
		TimerBase = (struct Library *)GlobalTimeReq.tr_node.io_Device;

		return 1;
	}

	return 0;
}

static void exit_sdl2()
{
    CloseDevice((struct IORequest *)&GlobalTimeReq);
}


ADD2INIT(init_sdl2, 0);
ADD2EXIT(exit_sdl2, 0);

