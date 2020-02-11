
#ifndef DTCONVERT_DTCONVERT_H
#define DTCONVERT_DTCONVERT_H 1
/*
**
**  $VER: DTConvert.h 1.8 (20.3.98)
**  datatypes.library/Examples/DTConvert
**
**  Converts file into another format using datatypes
**
**  project header
**
**  Written 1996-1998 by Roland 'Gizzy' Mainz
**
*/

/* amiga includes */
#include <exec/types.h>
#include <exec/memory.h>
#include <exec/libraries.h>
#include <dos/dos.h>
#include <dos/rdargs.h>
#include <dos/dosasl.h>
#include <dos/dosextens.h>
#include <dos/dostags.h>
#include <graphics/gfxbase.h>
#include <intuition/intuitionbase.h>
#include <intuition/classes.h>  /* must be $Id: classes.h,v 40.0 94/02/15 17:46:35 davidj Exp Locker: davidj $ */
#include <intuition/classusr.h>
#include <intuition/cghooks.h>
#include <intuition/icclass.h>
#include <intuition/gadgetclass.h>
#include <datatypes/datatypes.h>
#include <datatypes/datatypesclass.h>
#include <datatypes/textclass.h>

#if 0
#include <datatypes/documentclass.h>
#endif

#include <datatypes/soundclass.h>

#if 0
#include <datatypes/instrumentclass.h>
#endif

#if 0
#include <datatypes/musicclass.h>
#endif

#include <datatypes/pictureclass.h>
#include <datatypes/animationclass.h>
#include <libraries/gadtools.h>
#include <libraries/asl.h>
#include <workbench/icon.h>
#include <workbench/startup.h>
#include <workbench/workbench.h>

#if !defined(__AROS__)
/* amiga prototypes */
#include <clib/exec_protos.h>
#include <clib/utility_protos.h>
#include <clib/dos_protos.h>
#include <clib/graphics_protos.h>
#include <clib/intuition_protos.h>
#include <clib/gadtools_protos.h>
#include <clib/datatypes_protos.h>
#include <clib/dtclass_protos.h>
#include <clib/icon_protos.h>
#include <clib/wb_protos.h>
#include <clib/asl_protos.h>

/* amiga pragmas */
#include <pragmas/exec_pragmas.h>
#include <pragmas/utility_pragmas.h>
#include <pragmas/dos_pragmas.h>
#include <pragmas/graphics_pragmas.h>
#include <pragmas/intuition_pragmas.h>
#include <pragmas/gadtools_pragmas.h>
#include <pragmas/datatypes_pragmas.h>
#include <pragmas/dtclass_pragmas.h>
#include <pragmas/icon_pragmas.h>
#include <pragmas/wb_pragmas.h>
#include <pragmas/asl_pragmas.h>
#include <pragmas/alib_pragmas.h> /* tagcall stubs */
#else
#include <aros/debug.h>

#include <proto/exec.h>
#include <proto/utility.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/gadtools.h>
#include <proto/datatypes.h>
#include <proto/dtclass.h>
#include <proto/icon.h>
#include <proto/wb.h>
#include <proto/asl.h>

#include <clib/alib_protos.h>

#define __stdargs
#define DISPATCHERFLAGS
#define REGA0 register
#define REGA1 register
#define REGA2 register
#endif

/* ansi includes */
#include <string.h>

/*****************************************************************************/
#if !defined(__AROS__)
#ifndef PARAMETERS_STACK
#define PARAMETERS_STACK 1
#define  CLIB_ALIB_PROTOS_H
__stdargs void  NewList( struct List *list );
__stdargs ULONG DoMethodA( Object *obj, Msg message );
__stdargs ULONG DoMethod( Object *obj, unsigned long MethodID, ... );
__stdargs ULONG DoSuperMethodA( struct IClass *cl, Object *obj, Msg message );
__stdargs ULONG DoSuperMethod( struct IClass *cl, Object *obj, unsigned long MethodID, ... );
__stdargs ULONG CoerceMethodA( struct IClass *cl, Object *obj, Msg message );
__stdargs ULONG CoerceMethod( struct IClass *cl, Object *obj, unsigned long MethodID, ... );
__stdargs ULONG SetSuperAttrs( struct IClass *cl, Object *obj, unsigned long Tag1, ... );
#endif /* !PARAMETERS_STACK */

/*****************************************************************************/

/* SASC specific defines */
#define DISPATCHERFLAGS __saveds __asm
#define REGARGS __asm
#define REGA0 register __a0
#define REGA1 register __a1
#define REGA2 register __a2
#endif

/* misc macros */
#define V( x )      ((VOID *)(x))
#define G( o )      ((struct Gadget *)(o))
#define EXTG( o )   ((struct ExtGadget *)(o))

#define XTAG( expr, tagid ) ((Tag)((expr)?(tagid):(TAG_IGNORE)))

#define Strlen( s ) ((s)?(strlen( (s) )):(0))

/*****************************************************************************/
/* misc defines */
#ifndef NAME
#define NAME "DTConvert"
#endif /* !NAME */

/*****************************************************************************/
/* shared libraries */
#if !defined(__AROS__)
extern struct Library       *SysBase;
#endif
extern struct UtilityBase   *UtilityBase;
extern struct DosLibrary    *DOSBase;
extern struct GfxBase       *GfxBase;
extern struct IntuitionBase *IntuitionBase;
extern struct Library       *GadToolsBase;
extern struct Library       *DataTypesBase;
extern struct Library       *AslBase;
extern struct Library       *IconBase;
extern struct Library       *WorkbenchBase;

/*****************************************************************************/
/* global prototypes */

#include "DTConvert_iprotos.h"

/*****************************************************************************/

#endif /* !DTCONVERT_DTCONVERT_H */


