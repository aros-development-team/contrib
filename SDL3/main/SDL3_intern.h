#ifndef	_SDL3_INTERN_H
#define	_SDL3_INTERN_H

#include <exec/libraries.h>
#include <exec/semaphores.h>
#include <devices/timer.h>

struct SDL3Base
{
    struct Library          _lib;
	struct SDL3Base      	*Parent;
    struct SDL3Base      	*Root;
    //
	struct DosLibrary       *MyDOSBase;
	struct IntuitionBase    *MyIntuiBase;
	struct GfxBase          *MyGfxBase;
	struct Library          *MyCyberGfxBase;
	struct Library          *MyKeymapBase;
	struct Library          *MyWorkbenchBase;
	struct Library          *MyIconBase;
	struct Library          *MyMUIMasterBase;
	struct Library          *MyCxBase;

	// library management
	struct SignalSemaphore Semaphore;
};

#endif /* _SDL3_INTERN_H */
