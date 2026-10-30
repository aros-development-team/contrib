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

#define DEBUG 0
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

/* SDL_malloc() is served from an exec pool owned by this library
 * (src/stdlib/aros/SDL_arosmem.c). */
extern void AROS_MemQuit(void);

/* Library bases used by the SDL sources, which reach them as plain externs so
 * that the proto/ inline calls resolve.
 *
 * Every one of these is process-independent on AROS: OpenLibrary() hands the
 * same base pointer to every opener. So SDL2.library holds exactly ONE open of
 * each for its own lifetime, taken in init_libs() and released in close_libs()
 * at expunge.
 *
 * They used to be (re)opened from SDL2LIB_Open() per opener, which leaked:
 * SDL2LIB_Close() only ran the teardown for the LAST closer, so every earlier
 * opener's handles were dropped on the floor - and the four bases with no
 * per-opener slot (OpenURL, GadTools, IFFParse, Locale) could only ever have
 * the last opener's handle closed, because that is all the global held.
 */
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

static void close_libs(void)
{
    D(bug("[SDL2] %s()\n", __func__));

	if (GlobalTimeReq.tr_node.io_Device) {
		CloseDevice(&GlobalTimeReq.tr_node);
		GlobalTimeReq.tr_node.io_Device = NULL;
		TimerBase = NULL;
	}

	/* CloseLibrary(NULL) is a no-op, so this doubles as the unwind path for
	 * a partially completed init_libs(). */
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

	/* Required: the SDL sources call into all of these without checking, so
	 * failing to get one means the library genuinely cannot work. */
	if (!(GfxBase       = (APTR)OpenLibrary("graphics.library", 39)))		goto fail;
	if (!(DOSBase       = (APTR)OpenLibrary("dos.library", 36)))			goto fail;
	if (!(IntuitionBase = (APTR)OpenLibrary("intuition.library", 39)))		goto fail;
	if (!(UtilityBase   = OpenLibrary("utility.library", 36)))				goto fail;
	if (!(OOPBase       = OpenLibrary("oop.library", 0)))					goto fail;
	if (!(CyberGfxBase  = OpenLibrary("cybergraphics.library", 40)))		goto fail;
	if (!(KeymapBase    = OpenLibrary("keymap.library", 36)))				goto fail;
	if (!(WorkbenchBase = OpenLibrary("workbench.library", 0)))				goto fail;
	if (!(IconBase      = OpenLibrary("icon.library", 0)))					goto fail;
	if (!(CxBase        = OpenLibrary("commodities.library", 37)))			goto fail;
	if (!(IFFParseBase  = OpenLibrary("iffparse.library", 0)))				goto fail;
	if (!(GadToolsBase  = OpenLibrary("gadtools.library", 0)))				goto fail;

	/* Optional: SDL_sysurl.c checks for NULL and answers SDL_Unsupported(),
	 * and openurl.library is not part of a base AROS install. */
	OpenURLBase = OpenLibrary("openurl.library", 0);

	if (OpenDevice("timer.device", UNIT_MICROHZ, &GlobalTimeReq.tr_node, 0) != 0)
		goto fail;

	TimerBase = (struct Library *)GlobalTimeReq.tr_node.io_Device;

	init_system(LIBBASE);

	return 1;

fail:
	/* The old &&-chain returned without closing what it had already opened,
	 * and an init failure means the expunge hook never runs to catch it. */
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

		/* DeletePool() releases every block at once, so it must come last,
		 * once nothing can allocate again - and only here, at expunge,
		 * never at close. */
		AROS_MemQuit();

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

	/* Nothing to do. Every library base is owned by init_libs()/close_libs()
	 * for the lifetime of SDL2.library, so an opener has nothing of its own
	 * to release.
	 *
	 * In particular, do NOT call SDL_Quit() here: SDL's global state is owned
	 * by the application, which already calls SDL_Quit() itself. Tearing it
	 * down a second time from the library close path double-frees SDL objects
	 * (freed Intuition ports etc.).
	 */
}

/**********************************************************************
	SDL2LIB_Open
**********************************************************************/

static int SDL2LIB_Open(LIBBASETYPEPTR LIBBASE)
{
    D(bug("[SDL2] %s(0x%p)\n", __func__, LIBBASE));

	/* Nothing to open per opener - init_libs() already holds one open of
	 * every library the SDL sources use, for as long as SDL2.library is
	 * resident. See the comment on the base globals at the top of this file
	 * for why opening them here instead was wrong.
	 *
	 * Two of the bases this used to take are gone entirely: locale.library
	 * was never referenced by the SDL sources at all, and muimaster.library
	 * is opened locally by AROS_ShowMessageBox(), which shadowed the global.
	 */
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
