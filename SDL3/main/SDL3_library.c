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

#include "SDL3_intern.h"

extern void SDL_Quit(void);

struct SDL3Base   		*GlobalBase = NULL;

struct DosLibrary    	*DOSBase = NULL;
struct IntuitionBase	*IntuitionBase = NULL;
struct GfxBase       	*GfxBase = NULL;
struct Library       	*UtilityBase = NULL;
struct Library       	*CyberGfxBase = NULL;
struct Library       	*KeymapBase = NULL;
struct Library       	*WorkbenchBase = NULL;
struct Library       	*IconBase = NULL;
struct Library       	*MUIMasterBase = NULL;
struct Library       	*CxBase = NULL;
struct Library       	*TimerBase = NULL;
struct Library       	*LocaleBase = NULL;
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
    D(bug("[SDL3] %s(0x%p)\n", __func__, LIBBASE));
	// Detect platform/chipset feature availability
}

/**********************************************************************
	init_libs
**********************************************************************/

static int init_libs(LIBBASETYPEPTR LIBBASE)
{
    D(bug("[SDL3] %s(0x%p)\n", __func__, LIBBASE));

	if ((GfxBase = LIBBASE->MyGfxBase = (APTR)OpenLibrary("graphics.library", 39)) != NULL)
	if ((DOSBase = LIBBASE->MyDOSBase = (APTR)OpenLibrary("dos.library", 36)) != NULL)
	if ((IntuitionBase = LIBBASE->MyIntuiBase = (APTR)OpenLibrary("intuition.library", 39)) != NULL)
	if ((UtilityBase = OpenLibrary("utility.library", 36)) != NULL)
	if ((OOPBase = OpenLibrary("oop.library", 0)) != NULL)
	if (OpenDevice("timer.device", UNIT_MICROHZ, &GlobalTimeReq.tr_node, 0) == 0)
	{
		TimerBase = (struct Library *)GlobalTimeReq.tr_node.io_Device;

		init_system(LIBBASE);

		return 1;
	}

	return 0;
}

/**********************************************************************
	SDL3LIB_Init
**********************************************************************/

static int SDL3LIB_Init(LIBBASETYPEPTR LIBBASE)
{

	GlobalBase = LIBBASE;
	LIBBASE->Parent    = NULL;

    D(bug("[SDL3] %s(0x%p)\n", __func__, LIBBASE));

	InitSemaphore(&LIBBASE->Semaphore);

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
    D(bug("[SDL3] %s(0x%p)\n", __func__, LIBBASE));

	if (LIBBASE->_lib.lib_OpenCnt == 0)
	{
		CloseDevice(&GlobalTimeReq.tr_node);
		CloseLibrary(OOPBase);
		CloseLibrary(UtilityBase);
		CloseLibrary((struct Library *)LIBBASE->MyIntuiBase);
		CloseLibrary((struct Library *)LIBBASE->MyDOSBase);
		CloseLibrary((struct Library *)LIBBASE->MyGfxBase);

		return TRUE;
	}

	return FALSE;
}

/**********************************************************************
	UserLibClose
**********************************************************************/

static void UserLibClose(LIBBASETYPEPTR LIBBASE, struct ExecBase *SysBase)
{
    D(bug("[SDL3] %s(0x%p)\n", __func__, LIBBASE));

	CloseLibrary(OpenURLBase);
	CloseLibrary(GadToolsBase);
	CloseLibrary(IFFParseBase);
	CloseLibrary(LocaleBase);

	OpenURLBase = NULL;
	GadToolsBase = NULL;
	IFFParseBase = NULL;
	LocaleBase = NULL;

	CloseLibrary(LIBBASE->MyCxBase);
	CloseLibrary(LIBBASE->MyMUIMasterBase);
	CloseLibrary(LIBBASE->MyIconBase);
	CloseLibrary(LIBBASE->MyWorkbenchBase);
	CloseLibrary(LIBBASE->MyKeymapBase);
	CloseLibrary(LIBBASE->MyCyberGfxBase);

	CxBase           = LIBBASE->MyCxBase           = NULL;
    MUIMasterBase    = LIBBASE->MyMUIMasterBase    = NULL;
	IconBase         = LIBBASE->MyIconBase         = NULL;
	WorkbenchBase    = LIBBASE->MyWorkbenchBase    = NULL;
	KeymapBase       = LIBBASE->MyKeymapBase       = NULL;
	CyberGfxBase     = LIBBASE->MyCyberGfxBase     = NULL;
}

/**********************************************************************
	SDL3LIB_Expunge
**********************************************************************/

static int SDL3LIB_Expunge(LIBBASETYPEPTR LIBBASE)
{
    D(bug("[SDL3] %s(0x%p)\n", __func__, LIBBASE));

	if (LIBBASE->_lib.lib_Flags & LIBF_DELEXP)
		return FALSE;

	return DeleteLib(LIBBASE);
}

/**********************************************************************
	LIB_Close
*********************************************************************/

static void SDL3LIB_Close(LIBBASETYPEPTR LIBBASE)
{
    D(bug("[SDL3] %s(0x%p)\n", __func__, LIBBASE));

	ObtainSemaphore(&LIBBASE->Semaphore);

	SDL_Quit();

	LIBBASE->_lib.lib_OpenCnt--;
	if (LIBBASE->_lib.lib_OpenCnt == 0)
	{
		UserLibClose(LIBBASE, SysBase);
	}
	else
		LIBBASE->_lib.lib_Flags |= LIBF_DELEXP;

	ReleaseSemaphore(&LIBBASE->Semaphore);

	return;
}

/**********************************************************************
	SDL3LIB_Open
**********************************************************************/

static int SDL3LIB_Open(LIBBASETYPEPTR LIBBASE)
{
    D(bug("[SDL3] %s(0x%p)\n", __func__, LIBBASE));

	D(bug("[SDL3] %s: opening intuition.library\n", __func__));
	if ((IntuitionBase = LIBBASE->MyIntuiBase = (APTR)OpenLibrary("intuition.library", 39)) == NULL) return FALSE;
	D(bug("[SDL3] %s: opening cybergraphics.library\n", __func__));
	if ((CyberGfxBase = LIBBASE->MyCyberGfxBase = (APTR)OpenLibrary("cybergraphics.library", 40)) == NULL) return FALSE;
	D(bug("[SDL3] %s: opening keymap.library\n", __func__));
	if ((KeymapBase = LIBBASE->MyKeymapBase = (APTR)OpenLibrary("keymap.library", 36)) == NULL) return FALSE;
	D(bug("[SDL3] %s: opening workbench.library\n", __func__));
	if ((WorkbenchBase = LIBBASE->MyWorkbenchBase = (APTR)OpenLibrary("workbench.library", 0)) == NULL) return FALSE;
	D(bug("[SDL3] %s: opening icon.library\n", __func__));
	if ((IconBase = LIBBASE->MyIconBase = (APTR)OpenLibrary("icon.library", 0)) == NULL) return FALSE;
	D(bug("[SDL3] %s: opening muimaster.library\n", __func__));
	if ((MUIMasterBase = LIBBASE->MyMUIMasterBase = (APTR)OpenLibrary("muimaster.library", 19)) == NULL) return FALSE;
	D(bug("[SDL3] %s: opening commodities.library\n", __func__));
	if ((CxBase = LIBBASE->MyCxBase = (APTR)OpenLibrary("commodities.library", 37)) == NULL) return FALSE;
	D(bug("[SDL3] %s: opening locale.library\n", __func__));
	if ((LocaleBase = OpenLibrary("locale.library", 0)) == NULL) return FALSE;
	D(bug("[SDL3] %s: opening iffparse.library\n", __func__));
	if ((IFFParseBase = OpenLibrary("iffparse.library", 0)) == NULL) return FALSE;
	D(bug("[SDL3] %s: opening gadtools.library\n", __func__));
	if ((GadToolsBase = OpenLibrary("gadtools.library", 0)) == NULL) return FALSE;
	D(bug("[SDL3] %s: opening openurl.library\n", __func__));
	if ((OpenURLBase = OpenLibrary("openurl.library", 0)) == NULL) return FALSE;

	D(bug("[SDL3] %s: all libraries opened OK\n", __func__));
	return TRUE;
}

ADD2INITLIB(SDL3LIB_Init, 0);
ADD2OPENLIB(SDL3LIB_Open, 0);
ADD2CLOSELIB(SDL3LIB_Close, 0);
ADD2EXPUNGELIB(SDL3LIB_Expunge, 0);
