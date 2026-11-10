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

#include <libraries/gadtools.h>
#include <proto/intuition.h>

#include <devices/timer.h>
#include <exec/execbase.h>
#include <exec/resident.h>
#include <proto/exec.h>
#include <proto/gadtools.h>

#include <stddef.h>
#include <stdlib.h>

#include LC_LIBDEFS_FILE

#include "SDL2_intern.h"

extern void SDL_Quit(void);

/* Per-client SDL state.  Allocated and released by the SDL sources
   (src/aros/SDL_arosstate.c); this file owns the base it hangs off and so
   provides the lookup the SDL sources use.  genmodule installs the base of
   the opener whose call is being served in a task storage slot on entry to
   every library function, which is what makes __aros_getbase_SDL2Base()
   name the right client here. */
extern APTR SDL_AROS_StateAlloc(void);
extern void SDL_AROS_StateDestroy(APTR state);

/* Generated into SDL2_start.c by genmodule for a peropenerbase module. */
extern struct SDL2Base *__aros_getbase_SDL2Base(void);

APTR SDL_AROS_GetStatePtr(void)
{
	struct SDL2Base *base = (struct SDL2Base *)__aros_getbase_SDL2Base();

	return base ? base->State : NULL;
}

struct SDL2Base   		*GlobalBase = NULL;

struct DosLibrary    	*DOSBase = NULL;
struct IntuitionBase	*IntuitionBase = NULL;
struct GfxBase       	*GfxBase = NULL;
struct Library       	*UtilityBase = NULL;
struct Library       	*CyberGfxBase = NULL;
struct Library       	*KeymapBase = NULL;
struct Library       	*WorkbenchBase = NULL;
struct Library       	*IconBase = NULL;
struct Library       	*CxBase = NULL;
struct Library       	*TimerBase = NULL;
struct Library       	*IFFParseBase = NULL;
struct Library       	*OpenURLBase = NULL;
struct Library       	*GadToolsBase = NULL;
struct Library 			*OOPBase = NULL;

struct timerequest   	GlobalTimeReq;

/**********************************************************************
	init_system
**********************************************************************/

static void init_system(LIBBASETYPEPTR LIBBASE)
{
    D(bug("[SDL2] %s(0x%p)\n", __func__, LIBBASE));
	// Detect platform/chipset feature availability
}

/**********************************************************************
	init_libs
**********************************************************************/

/* Every one of these is process-independent on AROS: OpenLibrary() hands the
   same base to every opener.  So the module holds exactly one open of each
   for its own lifetime rather than one per client - the globals below can
   only ever hold one handle, so a per-opener open leaked every handle but the
   last, and closing them from the close path took them away from the clients
   still running. */
static void close_libs(void)
{
    D(bug("[SDL2] %s()\n", __func__));

	if (GlobalTimeReq.tr_node.io_Device)
	{
		CloseDevice(&GlobalTimeReq.tr_node);
		GlobalTimeReq.tr_node.io_Device = NULL;
		TimerBase = NULL;
	}

	/* CloseLibrary(NULL) is a no-op, so this doubles as the unwind path for a
	   partly completed init_libs(). */
	CloseLibrary(OpenURLBase);						OpenURLBase = NULL;
	CloseLibrary(GadToolsBase);						GadToolsBase = NULL;
	CloseLibrary(IFFParseBase);						IFFParseBase = NULL;
	CloseLibrary(CxBase);							CxBase = NULL;
	CloseLibrary(IconBase);							IconBase = NULL;
	CloseLibrary(WorkbenchBase);					WorkbenchBase = NULL;
	CloseLibrary(KeymapBase);						KeymapBase = NULL;
	CloseLibrary(CyberGfxBase);						CyberGfxBase = NULL;
	CloseLibrary(OOPBase);							OOPBase = NULL;
	CloseLibrary(UtilityBase);						UtilityBase = NULL;
	CloseLibrary((struct Library *)IntuitionBase);	IntuitionBase = NULL;
	CloseLibrary((struct Library *)DOSBase);		DOSBase = NULL;
	CloseLibrary((struct Library *)GfxBase);		GfxBase = NULL;
}

static int init_libs(LIBBASETYPEPTR LIBBASE)
{
    D(bug("[SDL2] %s(0x%p)\n", __func__, LIBBASE));

	/* Required: the SDL sources call into these without checking. */
	if (!(GfxBase       = (APTR)OpenLibrary("graphics.library", 39)))	goto fail;
	if (!(DOSBase       = (APTR)OpenLibrary("dos.library", 36)))			goto fail;
	if (!(IntuitionBase = (APTR)OpenLibrary("intuition.library", 39)))	goto fail;
	if (!(UtilityBase   = OpenLibrary("utility.library", 36)))			goto fail;
	if (!(OOPBase       = OpenLibrary("oop.library", 0)))				goto fail;
	if (!(CyberGfxBase  = OpenLibrary("cybergraphics.library", 40)))		goto fail;
	if (!(KeymapBase    = OpenLibrary("keymap.library", 36)))			goto fail;
	if (!(WorkbenchBase = OpenLibrary("workbench.library", 0)))			goto fail;
	if (!(IconBase      = OpenLibrary("icon.library", 0)))				goto fail;
	if (!(CxBase        = OpenLibrary("commodities.library", 37)))		goto fail;
	if (!(IFFParseBase  = OpenLibrary("iffparse.library", 0)))			goto fail;
	if (!(GadToolsBase  = OpenLibrary("gadtools.library", 0)))			goto fail;

	/* Optional: SDL_sysurl.c checks for NULL and answers SDL_Unsupported(),
	   and openurl.library is not part of a base AROS install. */
	OpenURLBase = OpenLibrary("openurl.library", 0);

	if (OpenDevice("timer.device", UNIT_MICROHZ, &GlobalTimeReq.tr_node, 0) != 0)
		goto fail;

	TimerBase = (struct Library *)GlobalTimeReq.tr_node.io_Device;

	init_system(LIBBASE);

	return 1;

fail:
	/* An init failure means the expunge hook never runs to catch what was
	   already opened. */
	close_libs();

	return 0;
}

/**********************************************************************
	SDL2LIB_Init
**********************************************************************/

static int SDL2LIB_Init(LIBBASETYPEPTR LIBBASE)
{

	GlobalBase = LIBBASE;
	LIBBASE->Parent    = NULL;

    D(bug("[SDL2] %s(0x%p)\n", __func__, LIBBASE));

	if (init_libs(LIBBASE) == 0)
	{
		return FALSE;
	}

    return TRUE;
}

/**********************************************************************
	DeleteLib
**********************************************************************/

static BOOL DeleteLib(LIBBASETYPEPTR LIBBASE)
{
    D(bug("[SDL2] %s(0x%p)\n", __func__, LIBBASE));

	if (LIBBASE->_lib.lib_OpenCnt == 0)
	{
		close_libs();

		return TRUE;
	}

	return FALSE;
}

/**********************************************************************
	SDL2LIB_Expunge
**********************************************************************/

static int SDL2LIB_Expunge(LIBBASETYPEPTR LIBBASE)
{
    D(bug("[SDL2] %s(0x%p)\n", __func__, LIBBASE));

	if (LIBBASE->_lib.lib_Flags & LIBF_DELEXP)
		return FALSE;

	return DeleteLib(LIBBASE);
}

/**********************************************************************
	LIB_Close
*********************************************************************/

static void SDL2LIB_Close(LIBBASETYPEPTR LIBBASE)
{
    D(bug("[SDL2] %s(0x%p)\n", __func__, LIBBASE));

	/* Do NOT call SDL_Quit() here.  SDL's state belongs to the application,
	   which has already called SDL_Quit() itself; doing it again from the
	   close path runs the teardown twice over freed objects.  Nor is there
	   anything per-opener to release any more - init_libs() owns every
	   library base for the module's lifetime - so all that is left is this
	   client's state block.  genmodule's CloseLib counts the opener out and
	   expunges if it was the last. */
	SDL_AROS_StateDestroy(LIBBASE->State);
	LIBBASE->State = NULL;

	return;
}

/**********************************************************************
	SDL2LIB_Open
**********************************************************************/

static int SDL2LIB_Open(LIBBASETYPEPTR LIBBASE)
{
    D(bug("[SDL2] %s(0x%p)\n", __func__, LIBBASE));

	/* All this opener has of its own is its SDL state.  The base was copied
	   from the root base, whose State is NULL, so every opener allocates one
	   here.  Nothing else: init_libs() already holds one open of every
	   library the SDL sources use, for as long as the module is resident.

	   Two of the bases that used to be taken here are gone entirely:
	   locale.library was never referenced by the SDL sources, and
	   muimaster.library is opened locally by AROS_ShowMessageBox(), which
	   shadowed the global. */
	if ((LIBBASE->State = SDL_AROS_StateAlloc()) == NULL)
		return FALSE;

	return TRUE;
}

int SDL_LoadObject(void)
{
    D(bug("[SDL2] %s()\n", __func__));
	return 0;
}

int SDL_LoadFunction(void)
{
    D(bug("[SDL2] %s()\n", __func__));
	return 0;
}

int SDL_UnloadObject(void)
{
    D(bug("[SDL2] %s()\n", __func__));
	return 0;
}

ADD2INITLIB(SDL2LIB_Init, 0);
ADD2OPENLIB(SDL2LIB_Open, 0);
ADD2CLOSELIB(SDL2LIB_Close, 0);
ADD2EXPUNGELIB(SDL2LIB_Expunge, 0);
