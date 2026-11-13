/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc: Console.device
*/

/*****************************************************************************/

#define SDEBUG 0
#define DEBUG 0
#include <aros/debug.h>

#include <proto/exec.h>
#include <proto/console.h>
#include <proto/intuition.h>
#include <proto/keymap.h>
#include <proto/graphics.h>
#include <proto/diskfont.h>
#include <graphics/text.h>

#include <exec/resident.h>
#include <exec/errors.h>
#include <exec/memory.h>
#include <exec/initializers.h>
#include <devices/inputevent.h>
#include <devices/conunit.h>
#include <devices/newstyle.h>
#include <dos/consoleinfo.h>

/* conunit.h pulls the PUBLIC <devices/console.h>, which is the stock
 * console.device's header and deliberately not modified by this device (see
 * the note in mmakefile.src). CD_ASKTITLE is this device's own additional
 * command, so take it from the private header rather than by patching the
 * public one out from under console.device. */
#include "include/consoleng.h"
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <intuition/classusr.h>
#include <graphics/rastport.h>
#include <aros/libcall.h>
#include <aros/symbolsets.h>
#include <graphics/rastport.h>

#include "console_gcc.h"
#include "clipboard_prefs.h"
#include "semantic_prefs.h"
#include "consoleif.h"

#include LC_LIBDEFS_FILE

#include <string.h>
/*****************************************************************************/

#define NEWSTYLE_DEVICE 1

/*****************************************************************************/


/*****************************************************************************/

#if NEWSTYLE_DEVICE

static const UWORD SupportedCommands[] = {
    CMD_READ,
    CMD_WRITE,
    NSCMD_DEVICEQUERY,
    CD_GETGEOMETRY,
    CD_GETCURSORPREFS,
    CD_SETCURSORPREFS,
    CD_GETDEFAULTCURSORPREFS,
    CD_SETDEFAULTCURSORPREFS,
    CD_GETHISTORYPREFS,
    CD_SETHISTORYPREFS,
    CD_GETDEFAULTHISTORYPREFS,
    CD_SETDEFAULTHISTORYPREFS,
    CD_GETDEFAULTPREFS,
    CD_SETDEFAULTPREFS,
    CD_GETFONTPREFS, CD_SETFONTPREFS, CD_VALIDATEFONTPREFS,
    CD_GETDEFAULTPROFILE, CD_SETDEFAULTPROFILE,
    CD_GETCOLORPREFS, CD_SETCOLORPREFS, CD_GETTHEMEPROFILE, CD_SETTHEMEPROFILE,
    CD_GETPASTEMODE,
    CD_GETEMOJIPREFS, CD_SETEMOJIPREFS,
    CD_GETDEFAULTEMOJIPREFS, CD_SETDEFAULTEMOJIPREFS,
    CD_GETRENDERPROFILE, CD_SETRENDERPROFILE,
    CD_GETBELLPREFS, CD_SETBELLPREFS, CD_GETDEFAULTBELLPREFS, CD_SETDEFAULTBELLPREFS,
    CD_GETSESSIONPROFILE, CD_SETSESSIONPROFILE,
    CD_GETCLIPBOARDPREFS, CD_SETCLIPBOARDPREFS, CD_GETDEFAULTCLIPBOARDPREFS, CD_SETDEFAULTCLIPBOARDPREFS,
    CD_GETTERMINALPROFILE, CD_SETTERMINALPROFILE,
    CD_GETCURSORCOLORPREFS, CD_SETCURSORCOLORPREFS, CD_GETDEFAULTCURSORCOLORPREFS, CD_SETDEFAULTCURSORCOLORPREFS,
    CD_GETAPPEARANCEPROFILE, CD_SETAPPEARANCEPROFILE,
    CD_GETINTERACTIONPROFILE, CD_SETINTERACTIONPROFILE,
    CD_GETINPUTPROFILE, CD_SETINPUTPROFILE,
    CD_GETSELECTIONPROFILE, CD_SETSELECTIONPROFILE,
    CD_GETSEMANTICPREFS, CD_SETSEMANTICPREFS,
    CD_GETDEFAULTSEMANTICPREFS, CD_SETDEFAULTSEMANTICPREFS,
    CD_GETSCROLLPREFS,
    CD_SETSCROLLPREFS,
    CD_GETDEFAULTSCROLLPREFS,
    CD_SETDEFAULTSCROLLPREFS,
    CD_GETBINDINGPREFS, CD_SETBINDINGPREFS,
    CD_GETDEFAULTBINDINGPREFS, CD_SETDEFAULTBINDINGPREFS,
    0
};

#endif

/*****************************************************************************/

static BOOL ConsolengValidColorPrefs(const struct ConsolengColorPrefs *p)
{
    ULONG i;
    if (p->cp_Size != sizeof(*p) || p->cp_Version != 1 || (p->cp_Mask & ~CONSOLENG_COLOR_MASK)) return FALSE;
    for (i=0;i<CONSOLENG_COLOR_COUNT;i++) if (p->cp_RGB[i] > 0xffffffUL) return FALSE;
    return TRUE;
}

static BOOL ConsolengValidFontPrefs(const struct ConsolengFontPrefs *p)
{
    ULONG i;
    if (p->fp_Size != sizeof(*p) || p->fp_Version != 1 ||
        p->fp_Pixels > 65535 || !memchr(p->fp_Name, 0, sizeof(p->fp_Name))) return FALSE;
    if (!p->fp_Name[0]) return p->fp_Pixels == 0;
    if (!p->fp_Pixels) return FALSE;
    for (i = 0; p->fp_Name[i]; i++)
        if ((UBYTE)p->fp_Name[i] < 32 || (UBYTE)p->fp_Name[i] == 127) return FALSE;
    return TRUE;
}

static BOOL ConsolengFontAvailable(const struct ConsolengFontPrefs *p)
{
    struct Library *GfxBase = TaggedOpenLibrary(TAGGEDOPEN_GRAPHICS);
    struct Library *DiskfontBase;
    struct TextFont *font;
    struct TextAttr ta;
    char name[CONSOLENG_FONT_NAME_SIZE + 8];
    BOOL result = FALSE;
    if (!p->fp_Name[0]) return TRUE;
    if (!GfxBase) return FALSE;
    DiskfontBase = OpenLibrary("diskfont.library", 0);
    if (!DiskfontBase) return FALSE;
    strcpy(name, p->fp_Name);
    ULONG len = strlen(name);
    if (len < 5 || strnicmp(name + len - 5, ".font", 5)) strcat(name, ".font");
    ta = (struct TextAttr){(STRPTR)name, p->fp_Pixels, FS_NORMAL, 0};
    font = OpenDiskFont(&ta);
    if (font)
    {
        result = !(font->tf_Flags & FPF_PROPORTIONAL) && font->tf_YSize == p->fp_Pixels;
        CloseFont(font);
    }
    CloseLibrary(DiskfontBase);
    return result;
}

static BOOL ConsolengValidCursorPrefs(const struct ConsolengCursorPrefs *prefs)
{
    return prefs->cp_Size == sizeof(*prefs) &&
        prefs->cp_Version == CONSOLENG_CURSOR_PREFS_VERSION &&
        prefs->cp_Style <= 6 &&
        prefs->cp_BlinkIntervalMS >= CONSOLENG_BLINK_MIN_MS &&
        prefs->cp_BlinkIntervalMS <= CONSOLENG_BLINK_MAX_MS &&
        (prefs->cp_IdleTimeoutMS == 0 ||
         (prefs->cp_IdleTimeoutMS >= 1000 &&
          prefs->cp_IdleTimeoutMS <= CONSOLENG_TIMEOUT_MAX_MS));
}

static int GM_UNIQUENAME(Init) (LIBBASETYPEPTR ConsoleDevice)
{
    ConsoleDevice->cb_UtilityBase =
        TaggedOpenLibrary(TAGGEDOPEN_UTILITY);
    if (!ConsoleDevice->cb_UtilityBase)
        return FALSE;

    ConsoleDevice->cb_IntuitionBase =
        TaggedOpenLibrary(TAGGEDOPEN_INTUITION);
    if (!ConsoleDevice->cb_IntuitionBase)
        return FALSE;

    ConsoleDevice->cb_KeymapBase =
        TaggedOpenLibrary(TAGGEDOPEN_KEYMAP);
    if (!ConsoleDevice->cb_KeymapBase)
    {
        CloseLibrary(ConsoleDevice->cb_IntuitionBase);
        return FALSE;
    }

    ConsoleDevice->cursorDefaults = (struct ConsolengCursorPrefs) {
        sizeof(struct ConsolengCursorPrefs), CONSOLENG_CURSOR_PREFS_VERSION,
        0, 500, 0
    };
    ConsoleDevice->historyDefaults = (struct ConsolengHistoryPrefs) {
        sizeof(struct ConsolengHistoryPrefs), CONSOLENG_HISTORY_PREFS_VERSION, 1000
    };
    ConsoleDevice->bellDefaults = (struct ConsolengBellPrefs){sizeof(struct ConsolengBellPrefs),1,1};
    con_bindings_defaults(&ConsoleDevice->bindingDefaults);
    ConsoleDevice->semanticDefaults=con_semantic_defaults();
    ConsoleDevice->wheelLinesDefault=1; /* Preserve existing native wheel speed. */
    ConsoleDevice->clipboardDefaults = (struct ConsolengClipboardPrefs){sizeof(struct ConsolengClipboardPrefs),1,1};
    ConsoleDevice->cursorColorDefaults = (struct ConsolengCursorColorPrefs){sizeof(struct ConsolengCursorColorPrefs),1,0,0x12a4e8};
    ConsoleDevice->emojiDefaults = (struct ConsolengEmojiPrefs){sizeof(struct ConsolengEmojiPrefs),1,1};
    ConsoleDevice->colorDefaults = (struct ConsolengColorPrefs){sizeof(struct ConsolengColorPrefs),1,0,{0}};
    ConsoleDevice->fontDefaults = (struct ConsolengFontPrefs){sizeof(struct ConsolengFontPrefs),1,0,{0}};
    ConsoleDevice->nextInputGeneration = 0;
    NEWLIST(&ConsoleDevice->unitList);
    NEWLIST(&ConsoleDevice->sniphooks);
    InitSemaphore(&ConsoleDevice->unitListLock);
    InitSemaphore(&ConsoleDevice->consoleTaskLock);
    InitSemaphore(&ConsoleDevice->copyBufferLock);
    InitSemaphore(&ConsoleDevice->glyphLock);
    NEWLIST(&ConsoleDevice->ownWindows);
    NEWLIST(&ConsoleDevice->fontWindows);
    InitSemaphore(&ConsoleDevice->fontWindowLock);

    ConsoleDevice->copyBuffer = 0;
    ConsoleDevice->copyBufferSize = 0;

    /* Create the console classes */
    CONSOLECLASSPTR = makeConsoleClass(ConsoleDevice);
    STDCONCLASSPTR = makeStdConClass(ConsoleDevice);
    CHARMAPCLASSPTR = makeCharMapConClass(ConsoleDevice);
    SNIPMAPCLASSPTR = makeSnipMapConClass(ConsoleDevice);

    if (!CONSOLECLASSPTR || !STDCONCLASSPTR || !CHARMAPCLASSPTR
        || !SNIPMAPCLASSPTR)
        Alert(AT_DeadEnd | AN_ConsoleDev | AG_NoMemory);

    /* Create the console.device task. */
    ConsoleDevice->consoleTask =
        NewCreateTask(TASKTAG_NAME, "consoleng.device", TASKTAG_PRI,
        COTASK_PRIORITY, TASKTAG_STACKSIZE, COTASK_STACKSIZE,
        TASKTAG_TASKMSGPORT, &ConsoleDevice->commandPort, TASKTAG_PC,
        consoleTaskEntry, TASKTAG_ARG1, ConsoleDevice, TAG_DONE);

    return ConsoleDevice->consoleTask ? TRUE : FALSE;
}

/*****************************************************************************/

static int GM_UNIQUENAME(Expunge) (LIBBASETYPEPTR ConsoleDevice)
{
    /* Drop the outline engine this device opened for Unicode glyphs. */
    ConsolengGlyphUnbind(ConsoleDevice);
    CloseLibrary(ConsoleDevice->cb_IntuitionBase);
    CloseLibrary(ConsoleDevice->cb_KeymapBase);
    return TRUE;
}

/*****************************************************************************/

/*
 * Open a window for an opener that did not supply one.
 *
 * WHY THIS EXISTS. console.device is the odd one out among AROS devices: it
 * takes a Window through io_Data at OpenDevice() time, and consoleclass.c's
 * New() returns NULL without one. Everything that wants a console therefore
 * has to be a window-owning program.
 *
 * That is exactly what kept this device unreachable. rom/filesys/
 * console_handler already has a generic "device mode" - it takes the device
 * NAME from the mount's FileSysStartupMsg (con_handler.c:473) and states in
 * its own comment that "any device answering those can stand in for
 * console.device" - but it opens that device with io_Data = NULL, because a
 * plain device takes a unit and flags rather than a window. So the seam for
 * mounting a second console already existed and this device could not use it.
 *
 * The alternative was to teach rom/filesys/console_handler to carry a console
 * device name down its window path. That handler is load-bearing for the
 * Shell, every CLI tool and Startup-Sequence, and terminills' standing call on
 * this work is to leave the stock path alone - so the missing capability goes
 * HERE, in the new device, which is also where "consoleng handles everything
 * internally" points. A mount needs no code at all now: give it
 * Device = consoleng.device and Unit = 3 (CONU_SNIPMAP, what CON: uses).
 *
 * A caller that DOES pass a window keeps owning it; only what we opened
 * ourselves is tracked and closed again.
 *
 * THE FONT. A fresh window inherits the screen's font, and on a stock
 * install that is a bitmap font (ttcourier/topaz): no .otag, no outline
 * engine, so every codepoint above Latin-1 renders as the glyph path's
 * reason digit ('2' = CONSOLENG_GLYPH_NOOTAG). The blocks, box lines and
 * arrows a TUI is drawn with are exactly what this device exists for, so
 * the window it opens for itself gets DejaVu Sans Mono - the monospace face
 * the tree ships for this coverage - at the screen font's size. The unit's
 * cell grid is derived from the RastPort font at New(), so this has to
 * happen before the unit object is created. A caller-supplied window keeps
 * whatever font its owner chose; OSC 50 can still switch either later.
 */
#define CONSOLENG_OWNWIN_FONT   "DejaVuSansMono.font"
#define CONSOLENG_OWNWIN_MINYSIZE 12

static struct TextFont *ConsolengOpenOwnWindowFont(LIBBASETYPEPTR ConsoleDevice,
    struct Screen *scr, struct Window *win)
{
    struct Library *GfxBase = TaggedOpenLibrary(TAGGEDOPEN_GRAPHICS);
    struct Library *DiskfontBase = OpenLibrary("diskfont.library", 0);
    struct TextFont *tf = NULL;

    if (GfxBase && DiskfontBase)
    {
        struct TextAttr ta;

        ta.ta_Name  = CONSOLENG_OWNWIN_FONT;
        ta.ta_YSize = scr->Font ? scr->Font->ta_YSize : 0;
        if (ta.ta_YSize < CONSOLENG_OWNWIN_MINYSIZE)
            ta.ta_YSize = CONSOLENG_OWNWIN_MINYSIZE;
        ta.ta_Style = FS_NORMAL;
        ta.ta_Flags = 0;

        tf = OpenDiskFont(&ta);
        if (tf && (tf->tf_Flags & FPF_PROPORTIONAL))
        {
            /* the cell grid needs a fixed pitch; keep the screen font */
            CloseFont(tf);
            tf = NULL;
        }
        if (tf)
            SetFont(win->RPort, tf);
    }
    if (DiskfontBase)
        CloseLibrary(DiskfontBase);

    return tf;
}

static struct Window *ConsolengOpenOwnWindow(LIBBASETYPEPTR ConsoleDevice,
    struct TextFont **fontp)
{
    struct Screen *scr;
    struct Window *win = NULL;

    *fontp = NULL;

    /* Lock the default public screen rather than assuming one: a console can
       legitimately be opened before Wanderer, and LockPubScreen(NULL) failing
       is a real answer ("no screen yet"), not an error to paper over. */
    scr = LockPubScreen(NULL);
    if (!scr)
        return NULL;

    {
        WORD w = scr->Width  - 100;
        WORD h = scr->Height - 100;
        struct TagItem wintags[] =
        {
            { WA_Left,          40                        },
            { WA_Top,           40                        },
            { WA_Width,         0                         },
            { WA_Height,        0                         },
            { WA_Title,         (IPTR)"AROS Console"       },
            { WA_PubScreen,     (IPTR)scr                 },
            { WA_CloseGadget,   TRUE                      },
            { WA_DragBar,       TRUE                      },
            { WA_DepthGadget,   TRUE                      },
            { WA_SizeGadget,    TRUE                      },
            /* Intuition defaults omitted limits to the initial size,
               which makes an otherwise resizable window immovable. */
            { WA_MinWidth,      200                       },
            { WA_MinHeight,     120                       },
            { WA_MaxWidth,      (IPTR)scr->Width           },
            { WA_MaxHeight,     (IPTR)scr->Height          },
            { WA_SmartRefresh,  TRUE                      },
            { WA_Activate,      TRUE                      },
            { WA_SimpleRefresh, FALSE                     },
            { TAG_DONE,         0                         }
        };

        if (w < 200) w = 200;
        if (h < 120) h = 120;

        wintags[2].ti_Data = (IPTR)w;
        wintags[3].ti_Data = (IPTR)h;

        /* OpenWindowTagList, NOT OpenWindowTags. The amiga.lib VARARGS stubs
           resolve IntuitionBase from a GLOBAL that is NULL in a
           __INTUITION_NOLIBBASE__ module like this one; the A-forms take the
           base from the macro in console_gcc.h. Getting this wrong faults at
           a small negative offset off a NULL base. */
        win = OpenWindowTagList(NULL, wintags);
    }

    if (win)
        *fontp = ConsolengOpenOwnWindowFont(ConsoleDevice, scr, win);

    UnlockPubScreen(NULL, scr);

    return win;
}

static int GM_UNIQUENAME(Open)
    (LIBBASETYPEPTR ConsoleDevice,
    struct IOStdReq *ioreq, ULONG unitnum, ULONG flags)
{
    BOOL success = FALSE;
    struct Window *ownwin = NULL;
    struct TextFont *ownfont = NULL;

    /* Keep compiler happy */
    flags = 0;

    EnterFunc(bug("OpenConsole()\n"));

    if (((LONG) unitnum) == CONU_LIBRARY)
        /* unitnum is ULONG while CONU_LIBRARY is -1 :-(   */
    {
        D(bug("Opening CONU_LIBRARY unit\n"));
        ioreq->io_Device = (struct Device *)ConsoleDevice;

        /* Set io_Unit so that CloseDevice knows this is a CONU_LIBRARY unit.
         * WB1.3 Setmap sets io_Unit to CONU_LIBRARY (-1) before closing
         * console.device */
        ioreq->io_Unit = (struct Unit *)CONU_LIBRARY;
        success = TRUE;
    }
    else
    {
        Class *classptr = NULL; /* Keep compiler happy */
#ifndef __mc68000
        /* AOS programs don't always initialize mn_Length. */
        if (ioreq->io_Message.mn_Length < sizeof(struct IOStdReq))
        {
            D(bug("console.device/open: IORequest structure passed to"
                " OpenDevice is too small!\n"));
            goto open_fail;
        }
#endif
        struct TagItem conunit_tags[] = {
            {A_Console_Window, 0},
            {TAG_DONE, 0}
        };

        /* Init tags */

        /* No window supplied: open one ourselves. This is the plain-device
           open the con-handler's device mode performs (io_Data = NULL), and
           without it every such open fails in consoleclass.c's New(). */
        if (ioreq->io_Data == NULL)
        {
            ownwin = ConsolengOpenOwnWindow(ConsoleDevice, &ownfont);
            if (!ownwin)
            {
                D(bug("consoleng/open: no window supplied and none could be"
                    " opened\n"));
                goto open_fail;
            }
        }

        conunit_tags[0].ti_Data =
            (IPTR)(ownwin ? ownwin : (struct Window *)ioreq->io_Data);

        /* Select class of which to create console object */
        switch (unitnum)
        {
        case CONU_STANDARD:
            D(bug("Opening CONU_STANDARD console\n"));
            classptr = STDCONCLASSPTR;
            break;

        case CONU_CHARMAP:
            classptr = CHARMAPCLASSPTR;
            break;

        case CONU_SNIPMAP:
            classptr = SNIPMAPCLASSPTR;
            break;

        default:
            goto open_fail;
        }

        /* Create console object */
        ioreq->io_Unit =
            (struct Unit *)NewObjectA(classptr, NULL, conunit_tags);
        if (ioreq->io_Unit)
        {
            struct opAddTail add_msg;
            success = TRUE;

            /* Add the newly created unit to console's list of units */
            ObtainSemaphore(&ConsoleDevice->unitListLock);

            /* The unit is not published yet. Serialize defaults with the
               preferences request and apply the entire snapshot before adding
               it to the console task's unit list. Existing units are untouched. */
            {
                if (ConsoleDevice->fontDefaults.fp_Name[0])
                {
                    IPTR fontArgs[2] = {(IPTR)ConsoleDevice->fontDefaults.fp_Name,
                        ConsoleDevice->fontDefaults.fp_Pixels};
                    Console_DoCommand((Object *)ioreq->io_Unit,C_SET_FONT,2,fontArgs);
                    /* If an installed font disappears, retain the valid
                       caller/window baseline rather than fail the console. */
                }
                ICU(ioreq->io_Unit)->bindings = ConsoleDevice->bindingDefaults;
                ICU(ioreq->io_Unit)->wheelLines = ConsoleDevice->wheelLinesDefault;
                ICU(ioreq->io_Unit)->semanticPrefs = ConsoleDevice->semanticDefaults;
                ICU(ioreq->io_Unit)->emojiDisabled = !ConsoleDevice->emojiDefaults.ep_Enabled;
                ICU(ioreq->io_Unit)->bellDisabled = !ConsoleDevice->bellDefaults.bp_Enabled;
                ICU(ioreq->io_Unit)->oscCopyDisabled = con_clipboard_policy(&ConsoleDevice->clipboardDefaults);
                IPTR colorArgs[2] = {(IPTR)&ConsoleDevice->colorDefaults,FALSE};
                Console_DoCommand((Object *)ioreq->io_Unit,C_SET_COLOR_PREFS,2,colorArgs);
                IPTR args[1] = { (IPTR)&ConsoleDevice->cursorDefaults };
                Console_DoCommand((Object *)ioreq->io_Unit,
                    C_SET_CURSOR_PREFS, 1, args);
                IPTR cursorColorArgs[2] = {(IPTR)&ConsoleDevice->cursorColorDefaults,FALSE};
                Console_DoCommand((Object *)ioreq->io_Unit,C_SET_CURSOR_COLOR_PREFS,2,cursorColorArgs);
                IPTR historyArgs[2] = { (IPTR)&ConsoleDevice->historyDefaults, FALSE };
                Console_DoCommand((Object *)ioreq->io_Unit,
                    C_SET_HISTORY_PREFS, 2, historyArgs);
            }

            /* A queued event may outlive its unit and allocator address. */
            if (++ConsoleDevice->nextInputGeneration == 0)
                ++ConsoleDevice->nextInputGeneration;
            ICU(ioreq->io_Unit)->inputGeneration = ConsoleDevice->nextInputGeneration;

            add_msg.MethodID = OM_ADDTAIL;
            add_msg.opat_List = (struct List *)&ConsoleDevice->unitList;
            DoMethodA((Object *) ioreq->io_Unit, (Msg) &add_msg);

            /* Remember a window we opened ourselves, so Close() can close it
               again. Tracked per UNIT, not per device, because several units
               can be open at once and only some of them self-opened. If the
               bookkeeping cannot be allocated, close the window now rather
               than leak it - the unit still works with a caller's window and
               this path only ever applies to one we would have owned. */
            if (ownwin)
            {
                struct ConsolengOwnWin *ow =
                    AllocMem(sizeof(struct ConsolengOwnWin),
                             MEMF_CLEAR | MEMF_PUBLIC);

                if (ow)
                {
                    ow->unit = ioreq->io_Unit;
                    ow->win  = ownwin;
                    ow->font = ownfont;
                    AddTail((struct List *)&ConsoleDevice->ownWindows,
                            (struct Node *)&ow->node);
                    ownwin = NULL;      /* handed over to the list */
                    ownfont = NULL;
                }
            }

            ReleaseSemaphore(&ConsoleDevice->unitListLock);
        } /* if (console unit created) */
    } /* if (not CONU_LIBRARY) */

    if (!success)
        goto open_fail;

    return TRUE;

  open_fail:

    /* Still set means the unit was never created, or its bookkeeping failed,
       so nothing else can be holding this window. */
    if (ownwin)
        CloseWindow(ownwin);
    ConsolengCloseFont(ConsoleDevice, ownfont);

    ioreq->io_Error = IOERR_OPENFAIL;

    return FALSE;
}

/*****************************************************************************/

static int GM_UNIQUENAME(Close)
    (LIBBASETYPEPTR ConsoleDevice, struct IORequest *ioreq)
{
    if (ioreq->io_Unit && ioreq->io_Unit != (struct Unit *)CONU_LIBRARY)
    {
        ULONG mid = OM_REMOVE;
        struct ConsolengOwnWin *ow;
        struct Window *ownwin = NULL;
        struct TextFont *ownfont = NULL;

        /* Match the console task's task-lock -> list-lock order. Input
         * dispatch must finish using the unit before it can be destroyed. */
        ObtainSemaphore(&ConsoleDevice->consoleTaskLock);
        /* Remove the console from the console list. */
        ObtainSemaphore(&ConsoleDevice->unitListLock);
        DoMethodA((Object *) ioreq->io_Unit, (Msg) &mid);

        /* Unlink any window this device opened for THIS unit. A window the
           caller supplied is not on this list and is left alone. */
        ForeachNode(&ConsoleDevice->ownWindows, ow)
        {
            if (ow->unit == ioreq->io_Unit)
            {
                Object *state = (Object *)ConsoleDevice->unitList.mlh_Head;
                Object *sibling;
                /* The original unit is already unlinked. Keep the device's
                   window and baseline font alive for any remaining borrower. */
                while ((sibling = NextObject(&state)))
                    if (CU(sibling)->cu_Window == ow->win) break;
                if (sibling)
                {
                    ow->unit = (struct Unit *)sibling;
                    break;
                }
                Remove((struct Node *)&ow->node);
                ownwin = ow->win;
                ownfont = ow->font;
                FreeMem(ow, sizeof(struct ConsolengOwnWin));
                break;
            }
        }

        ReleaseSemaphore(&ConsoleDevice->unitListLock);

        DisposeObject((Object *) ioreq->io_Unit);

        /* Only after the unit is gone: it renders into this window, so
           closing the window first would leave it drawing into freed
           layers. */
        if (ownwin)
            CloseWindow(ownwin);
        /* ...and the font only once nothing renders with it any more. */
        ConsolengCloseFont(ConsoleDevice, ownfont);
        ReleaseSemaphore(&ConsoleDevice->consoleTaskLock);
    }

    return TRUE;
}

/*****************************************************************************/

ADD2INITLIB(GM_UNIQUENAME(Init), 0)
ADD2EXPUNGELIB(GM_UNIQUENAME(Expunge), 0)
ADD2OPENDEV(GM_UNIQUENAME(Open), 0) ADD2CLOSEDEV(GM_UNIQUENAME(Close), 0)
/*****************************************************************************/
    AROS_LH1(void, beginio,
    AROS_LHA(struct IOStdReq *, ioreq, A1),
    struct ConsoleBase *, ConsoleDevice, 5, Consoleng)
{
    AROS_LIBFUNC_INIT

    LONG error = 0;
    BOOL done_quick = TRUE;

    /* WaitIO will look into this */
    ioreq->io_Message.mn_Node.ln_Type = NT_MESSAGE;

    EnterFunc(bug("BeginIO(ioreq=%p)\n", ioreq));

    switch (ioreq->io_Command)
    {
#if NEWSTYLE_DEVICE
    case NSCMD_DEVICEQUERY:
        if (ioreq->io_Length < ((LONG) OFFSET(NSDeviceQueryResult,
                    SupportedCommands)) + sizeof(UWORD *))
        {
            ioreq->io_Error = IOERR_BADLENGTH;
        }
        else
        {
            struct NSDeviceQueryResult *d;

            d = (struct NSDeviceQueryResult *)ioreq->io_Data;

            d->DevQueryFormat = 0;
            d->SizeAvailable = sizeof(struct NSDeviceQueryResult);
            d->DeviceType = NSDEVTYPE_CONSOLE;
            d->DeviceSubType = 0;
            d->SupportedCommands = (UWORD *) SupportedCommands;

            ioreq->io_Actual = sizeof(struct NSDeviceQueryResult);
        }
        break;
#endif

    case CMD_WRITE:
        {
            ULONG towrite;
            ioreq->io_Actual = 0;
            if (!ioreq->io_Unit || ioreq->io_Unit == (struct Unit *)CONU_LIBRARY ||
                (ioreq->io_Length && !ioreq->io_Data))
            {
                error = IOERR_BADADDRESS;
                break;
            }
            if (ioreq->io_Length != (ULONG)-1 && ioreq->io_Length > 0x7fffffffUL)
            {
                error = IOERR_BADLENGTH;
                break;
            }
            D(bug("CMD_WRITE %p,%d\n", ioreq->io_Data, ioreq->io_Length));
#if DEBUG
            {
                char *str;
                int i;
                str = ioreq->io_Data;
                for (i = 0; i < ioreq->io_Length; i++)
                {
                    kprintf("%c", *str++);
                }
                kprintf("\n");
            }
#endif
            if (ioreq->io_Length == -1)
            {
                towrite = strlen((STRPTR) ioreq->io_Data);
            }
            else
            {
                towrite = ioreq->io_Length;
            }

            /* Parsing, synchronized-frame state and rendering share the
             * unit with refresh/input/timer processing in the console task.
             * Keep one write atomic with respect to those paths. */
            ObtainSemaphore(&ConsoleDevice->consoleTaskLock);
            ioreq->io_Actual =
                writeToConsole((struct ConUnit *)ioreq->io_Unit,
                ioreq->io_Data, towrite, ConsoleDevice, &error);
            ReleaseSemaphore(&ConsoleDevice->consoleTaskLock);

            break;
        }

    case CMD_READ:
        D(bug("CMD_READ %p,%d\n", ioreq->io_Data, ioreq->io_Length));
        ioreq->io_Actual = 0;
        if (!ioreq->io_Unit || ioreq->io_Unit == (struct Unit *)CONU_LIBRARY ||
            (ioreq->io_Length && !ioreq->io_Data))
            error = IOERR_BADADDRESS;
        else if (ioreq->io_Length)
        {
            ioreq->io_Error = 0;
            done_quick = FALSE;
        }
        /* A valid zero-length read completes now without consuming input. */
        break;

    case CD_GETSEMANTICPREFS:
    case CD_SETSEMANTICPREFS:
    case CD_GETDEFAULTSEMANTICPREFS:
    case CD_SETDEFAULTSEMANTICPREFS:
        ioreq->io_Actual = 0;
        {
            BOOL defaults = ioreq->io_Command == CD_GETDEFAULTSEMANTICPREFS ||
                ioreq->io_Command == CD_SETDEFAULTSEMANTICPREFS;
            BOOL set = ioreq->io_Command == CD_SETSEMANTICPREFS ||
                ioreq->io_Command == CD_SETDEFAULTSEMANTICPREFS;
            struct ConsolengSemanticPrefs prefs;
            if (!ioreq->io_Data || !ioreq->io_Unit ||
                (defaults != (ioreq->io_Unit == (struct Unit *)CONU_LIBRARY)))
                error = IOERR_BADADDRESS;
            else if (ioreq->io_Length < sizeof(prefs)) error = IOERR_BADLENGTH;
            else {
                if (set) {
                    CopyMem(ioreq->io_Data, &prefs, sizeof(prefs));
                    if (!con_semantic_prefs_valid(&prefs)) error = CONSOLENG_ERR_BADPREFS;
                }
                if (!error) {
                    struct SignalSemaphore *lock = defaults ?
                        &ConsoleDevice->unitListLock : &ConsoleDevice->consoleTaskLock;
                    ObtainSemaphore(lock);
                    struct ConsolengSemanticPrefs *target = defaults ?
                        &ConsoleDevice->semanticDefaults : &ICU(ioreq->io_Unit)->semanticPrefs;
                    if (set) *target = prefs;
                    else prefs = *target;
                    ReleaseSemaphore(lock);
                    if (!set) CopyMem(&prefs, ioreq->io_Data, sizeof(prefs));
                    ioreq->io_Actual = sizeof(prefs);
                }
            }
        }
        break;

    case CD_GETSCROLLPREFS:
    case CD_SETSCROLLPREFS:
    case CD_GETDEFAULTSCROLLPREFS:
    case CD_SETDEFAULTSCROLLPREFS:
        ioreq->io_Actual = 0;
        {
            BOOL defaults = ioreq->io_Command == CD_GETDEFAULTSCROLLPREFS ||
                ioreq->io_Command == CD_SETDEFAULTSCROLLPREFS;
            BOOL set = ioreq->io_Command == CD_SETSCROLLPREFS ||
                ioreq->io_Command == CD_SETDEFAULTSCROLLPREFS;
            struct ConsolengScrollPrefs prefs;
            if (!ioreq->io_Data || !ioreq->io_Unit ||
                (defaults != (ioreq->io_Unit == (struct Unit *)CONU_LIBRARY)))
                error = IOERR_BADADDRESS;
            else if (ioreq->io_Length < sizeof(prefs)) error = IOERR_BADLENGTH;
            else {
                if (set) {
                    CopyMem(ioreq->io_Data, &prefs, sizeof(prefs));
                    if (prefs.sp_Size!=sizeof(prefs) || prefs.sp_Version!=1 || prefs.sp_Lines>CONSOLENG_SCROLL_MAX_LINES) error = CONSOLENG_ERR_BADPREFS;
                }
                if (!error) {
                    struct SignalSemaphore *lock = defaults ?
                        &ConsoleDevice->unitListLock : &ConsoleDevice->consoleTaskLock;
                    ObtainSemaphore(lock);
                    ULONG *target = defaults ?
                        &ConsoleDevice->wheelLinesDefault : &ICU(ioreq->io_Unit)->wheelLines;
                    if (set) *target = prefs.sp_Lines;
                    else prefs = (struct ConsolengScrollPrefs){sizeof(prefs),1,*target};
                    ReleaseSemaphore(lock);
                    if (!set) CopyMem(&prefs, ioreq->io_Data, sizeof(prefs));
                    ioreq->io_Actual = sizeof(prefs);
                }
            }
        }
        break;

    case CD_GETBINDINGPREFS:
    case CD_SETBINDINGPREFS:
    case CD_GETDEFAULTBINDINGPREFS:
    case CD_SETDEFAULTBINDINGPREFS:
        ioreq->io_Actual = 0;
        {
            BOOL defaults = ioreq->io_Command == CD_GETDEFAULTBINDINGPREFS ||
                ioreq->io_Command == CD_SETDEFAULTBINDINGPREFS;
            BOOL set = ioreq->io_Command == CD_SETBINDINGPREFS ||
                ioreq->io_Command == CD_SETDEFAULTBINDINGPREFS;
            struct ConsolengBindingPrefs prefs;
            if (!ioreq->io_Data || !ioreq->io_Unit ||
                (defaults != (ioreq->io_Unit == (struct Unit *)CONU_LIBRARY)))
                error = IOERR_BADADDRESS;
            else if (ioreq->io_Length < sizeof(prefs)) error = IOERR_BADLENGTH;
            else {
                if (set) {
                    CopyMem(ioreq->io_Data, &prefs, sizeof(prefs));
                    if (!con_bindings_valid(&prefs)) error = CONSOLENG_ERR_BADPREFS;
                }
                if (!error) {
                    struct SignalSemaphore *lock = defaults ?
                        &ConsoleDevice->unitListLock : &ConsoleDevice->consoleTaskLock;
                    ObtainSemaphore(lock);
                    struct ConsolengBindingPrefs *target = defaults ?
                        &ConsoleDevice->bindingDefaults : &ICU(ioreq->io_Unit)->bindings;
                    if (set) *target = prefs; else prefs = *target;
                    ReleaseSemaphore(lock);
                    if (!set) CopyMem(&prefs, ioreq->io_Data, sizeof(prefs));
                    ioreq->io_Actual = sizeof(prefs);
                }
            }
        }
        break;

    case CD_GETCURSORCOLORPREFS:
    case CD_SETCURSORCOLORPREFS:
    case CD_GETDEFAULTCURSORCOLORPREFS:
    case CD_SETDEFAULTCURSORCOLORPREFS:
        ioreq->io_Actual = 0;
        {
            BOOL defaults = ioreq->io_Command == CD_GETDEFAULTCURSORCOLORPREFS ||
                ioreq->io_Command == CD_SETDEFAULTCURSORCOLORPREFS;
            BOOL set = ioreq->io_Command == CD_SETCURSORCOLORPREFS ||
                ioreq->io_Command == CD_SETDEFAULTCURSORCOLORPREFS;
            struct ConsolengCursorColorPrefs prefs;
            if (!ioreq->io_Data || !ioreq->io_Unit ||
                (defaults != (ioreq->io_Unit == (struct Unit *)CONU_LIBRARY)))
                error = IOERR_BADADDRESS;
            else if (ioreq->io_Length < sizeof(prefs)) error = IOERR_BADLENGTH;
            else
            {
                if (set)
                {
                    CopyMem(ioreq->io_Data, &prefs, sizeof(prefs));
                    if (prefs.ccp_Size != sizeof(prefs) || prefs.ccp_Version != 1 ||
                        prefs.ccp_Enabled > 1 || prefs.ccp_RGB > 0xffffffUL) error = CONSOLENG_ERR_BADPREFS;
                }
                if (!error)
                {
                    if (defaults)
                    {
                        ObtainSemaphore(&ConsoleDevice->unitListLock);
                        if (set) ConsoleDevice->cursorColorDefaults = prefs;
                        else prefs = ConsoleDevice->cursorColorDefaults;
                        ReleaseSemaphore(&ConsoleDevice->unitListLock);
                    }
                    else
                    {
                        IPTR args[2] = {(IPTR)&prefs,FALSE};
                        ObtainSemaphore(&ConsoleDevice->consoleTaskLock);
                        Console_DoCommand((Object *)ioreq->io_Unit,
                            set ? C_SET_CURSOR_COLOR_PREFS : C_GET_CURSOR_COLOR_PREFS, 2, args);
                        ReleaseSemaphore(&ConsoleDevice->consoleTaskLock);
                        if (!args[1]) error = IOERR_NOCMD;
                    }
                    if (!error)
                    {
                        if (!set) CopyMem(&prefs, ioreq->io_Data, sizeof(prefs));
                        ioreq->io_Actual = sizeof(prefs);
                    }
                }
            }
        }
        break;

    case CD_GETCLIPBOARDPREFS:
    case CD_SETCLIPBOARDPREFS:
    case CD_GETDEFAULTCLIPBOARDPREFS:
    case CD_SETDEFAULTCLIPBOARDPREFS:
        ioreq->io_Actual = 0;
        {
            BOOL defaults = ioreq->io_Command == CD_GETDEFAULTCLIPBOARDPREFS ||
                ioreq->io_Command == CD_SETDEFAULTCLIPBOARDPREFS;
            BOOL set = ioreq->io_Command == CD_SETCLIPBOARDPREFS ||
                ioreq->io_Command == CD_SETDEFAULTCLIPBOARDPREFS;
            struct ConsolengClipboardPrefs prefs;
            if (!ioreq->io_Data || !ioreq->io_Unit ||
                (defaults != (ioreq->io_Unit == (struct Unit *)CONU_LIBRARY)))
                error = IOERR_BADADDRESS;
            else if (ioreq->io_Length < sizeof(prefs)) error = IOERR_BADLENGTH;
            else
            {
                if (set)
                {
                    CopyMem(ioreq->io_Data, &prefs, sizeof(prefs));
                    if (!con_clipboard_prefs_valid(&prefs)) error = CONSOLENG_ERR_BADPREFS;
                }
                if (!error)
                {
                    if (defaults)
                    {
                        ObtainSemaphore(&ConsoleDevice->unitListLock);
                        if (set) ConsoleDevice->clipboardDefaults = prefs;
                        else prefs = ConsoleDevice->clipboardDefaults;
                        ReleaseSemaphore(&ConsoleDevice->unitListLock);
                    }
                    else
                    {
                        IPTR args[2] = {(IPTR)&prefs,FALSE};
                        ObtainSemaphore(&ConsoleDevice->consoleTaskLock);
                        Console_DoCommand((Object *)ioreq->io_Unit,
                            set ? C_SET_CLIPBOARD_PREFS : C_GET_CLIPBOARD_PREFS, 2, args);
                        ReleaseSemaphore(&ConsoleDevice->consoleTaskLock);
                        if (!args[1]) error = IOERR_NOCMD;
                    }
                    if (!error)
                    {
                        if (!set) CopyMem(&prefs, ioreq->io_Data, sizeof(prefs));
                        ioreq->io_Actual = sizeof(prefs);
                    }
                }
            }
        }
        break;

    case CD_GETINTERACTIONPROFILE:
    case CD_SETINTERACTIONPROFILE:
    case CD_GETSELECTIONPROFILE:
    case CD_SETSELECTIONPROFILE:
    case CD_GETINPUTPROFILE:
    case CD_SETINPUTPROFILE:
    {
        BOOL semantic = ioreq->io_Command == CD_GETSELECTIONPROFILE || ioreq->io_Command == CD_SETSELECTIONPROFILE;
        BOOL extended = semantic || ioreq->io_Command == CD_GETINPUTPROFILE || ioreq->io_Command == CD_SETINPUTPROFILE;
        ULONG size = semantic ? sizeof(struct ConsolengSelectionProfile) : extended ? sizeof(struct ConsolengInputProfile) : sizeof(struct ConsolengInteractionProfile);
        ioreq->io_Actual = 0;
        if (!ioreq->io_Data || ioreq->io_Unit != (struct Unit *)CONU_LIBRARY) error=IOERR_BADADDRESS;
        else if (ioreq->io_Length < size) error=IOERR_BADLENGTH;
        else
        {
            struct ConsolengInteractionProfile full;
            struct ConsolengInputProfile input;
            struct ConsolengSelectionProfile selection;
            struct ConsolengAppearanceProfile p;
            BOOL set = ioreq->io_Command == CD_SETSELECTIONPROFILE || ioreq->io_Command == CD_SETINTERACTIONPROFILE || ioreq->io_Command == CD_SETINPUTPROFILE;
            if (set)
            {
                if (extended) {
                    if(semantic) {
                        CopyMem(ioreq->io_Data,&selection,sizeof(selection));
                        if(selection.sp_Size!=sizeof(selection)||selection.sp_Version!=1||
                           !con_semantic_prefs_valid(&selection.sp_Semantic))error=CONSOLENG_ERR_BADPREFS;
                        input=selection.sp_Input;
                    } else CopyMem(ioreq->io_Data,&input,sizeof(input));
                    full=input.ip_Interaction;
                    if(input.ip_Size!=sizeof(input)||input.ip_Version!=1||
                       input.ip_Scroll.sp_Size!=sizeof(input.ip_Scroll)||input.ip_Scroll.sp_Version!=1||
                       input.ip_Scroll.sp_Lines>CONSOLENG_SCROLL_MAX_LINES)error=CONSOLENG_ERR_BADPREFS;
                } else CopyMem(ioreq->io_Data,&full,sizeof(full));
                p=full.ip_Appearance;
                if (full.ip_Size!=sizeof(full) || full.ip_Version!=1 ||
                    !con_bindings_valid(&full.ip_Bindings)) error=CONSOLENG_ERR_BADPREFS;
                if (p.rp_Size!=sizeof(p) || p.rp_Version!=1 ||
                    p.rp_Theme.tp_Size!=sizeof(p.rp_Theme) || p.rp_Theme.tp_Version!=1 ||
                    !ConsolengValidCursorPrefs(&p.rp_Theme.tp_Cursor) ||
                    p.rp_Theme.tp_History.hp_Size!=sizeof(p.rp_Theme.tp_History) ||
                    p.rp_Theme.tp_History.hp_Version!=1 || p.rp_Theme.tp_History.hp_Lines>CONSOLENG_HISTORY_MAX_LINES ||
                    !ConsolengValidFontPrefs(&p.rp_Theme.tp_Font) || !ConsolengFontAvailable(&p.rp_Theme.tp_Font) ||
                    !ConsolengValidColorPrefs(&p.rp_Theme.tp_Colors) ||
                    p.rp_Emoji.ep_Size!=sizeof(p.rp_Emoji) || p.rp_Emoji.ep_Version!=1 ||
                    p.rp_Emoji.ep_Enabled>1 || p.rp_Bell.bp_Size!=sizeof(p.rp_Bell) ||
                    p.rp_Bell.bp_Version!=1 || p.rp_Bell.bp_Enabled>1 ||
                    !con_clipboard_prefs_valid(&p.rp_Clipboard) ||
                    p.rp_CursorColor.ccp_Size!=sizeof(p.rp_CursorColor) || p.rp_CursorColor.ccp_Version!=1 ||
                    p.rp_CursorColor.ccp_Enabled>1 || p.rp_CursorColor.ccp_RGB>0xffffffUL) error=CONSOLENG_ERR_BADPREFS;
            }
            if (!error)
            {
                ObtainSemaphore(&ConsoleDevice->unitListLock);
                if (set)
                {
                    if(semantic)ConsoleDevice->semanticDefaults=selection.sp_Semantic;
                    if(extended)ConsoleDevice->wheelLinesDefault=input.ip_Scroll.sp_Lines;
                    ConsoleDevice->bindingDefaults=full.ip_Bindings;
                    ConsoleDevice->cursorDefaults=p.rp_Theme.tp_Cursor;
                    ConsoleDevice->historyDefaults=p.rp_Theme.tp_History;
                    ConsoleDevice->fontDefaults=p.rp_Theme.tp_Font;
                    ConsoleDevice->colorDefaults=p.rp_Theme.tp_Colors;
                    ConsoleDevice->emojiDefaults=p.rp_Emoji;
                    ConsoleDevice->bellDefaults=p.rp_Bell;
                    ConsoleDevice->clipboardDefaults=p.rp_Clipboard;
                    ConsoleDevice->cursorColorDefaults=p.rp_CursorColor;
                }
                else p=(struct ConsolengAppearanceProfile){sizeof(p),1,
                    {sizeof(p.rp_Theme),1,ConsoleDevice->cursorDefaults,ConsoleDevice->historyDefaults,
                     ConsoleDevice->fontDefaults,ConsoleDevice->colorDefaults},ConsoleDevice->emojiDefaults,ConsoleDevice->bellDefaults,ConsoleDevice->clipboardDefaults,ConsoleDevice->cursorColorDefaults};
                if (!set) full=(struct ConsolengInteractionProfile){sizeof(full),1,p,ConsoleDevice->bindingDefaults};
                if(!set && extended)input=(struct ConsolengInputProfile){sizeof(input),1,full,
                    {sizeof(input.ip_Scroll),1,ConsoleDevice->wheelLinesDefault}};
                if(!set && semantic)selection=(struct ConsolengSelectionProfile){sizeof(selection),1,input,ConsoleDevice->semanticDefaults};
                ReleaseSemaphore(&ConsoleDevice->unitListLock);
                if (!set) {
                    if(semantic)CopyMem(&selection,ioreq->io_Data,sizeof(selection));
                    else if(extended)CopyMem(&input,ioreq->io_Data,sizeof(input));
                    else CopyMem(&full,ioreq->io_Data,sizeof(full));
                }
                ioreq->io_Actual=size;
            }
        }
        break;
    }

    case CD_GETAPPEARANCEPROFILE:
    case CD_SETAPPEARANCEPROFILE:
        ioreq->io_Actual = 0;
        if (!ioreq->io_Data || ioreq->io_Unit != (struct Unit *)CONU_LIBRARY) error=IOERR_BADADDRESS;
        else if (ioreq->io_Length < sizeof(struct ConsolengAppearanceProfile)) error=IOERR_BADLENGTH;
        else
        {
            struct ConsolengAppearanceProfile p;
            BOOL set = ioreq->io_Command == CD_SETAPPEARANCEPROFILE;
            if (set)
            {
                CopyMem(ioreq->io_Data,&p,sizeof(p));
                if (p.rp_Size!=sizeof(p) || p.rp_Version!=1 ||
                    p.rp_Theme.tp_Size!=sizeof(p.rp_Theme) || p.rp_Theme.tp_Version!=1 ||
                    !ConsolengValidCursorPrefs(&p.rp_Theme.tp_Cursor) ||
                    p.rp_Theme.tp_History.hp_Size!=sizeof(p.rp_Theme.tp_History) ||
                    p.rp_Theme.tp_History.hp_Version!=1 || p.rp_Theme.tp_History.hp_Lines>CONSOLENG_HISTORY_MAX_LINES ||
                    !ConsolengValidFontPrefs(&p.rp_Theme.tp_Font) || !ConsolengFontAvailable(&p.rp_Theme.tp_Font) ||
                    !ConsolengValidColorPrefs(&p.rp_Theme.tp_Colors) ||
                    p.rp_Emoji.ep_Size!=sizeof(p.rp_Emoji) || p.rp_Emoji.ep_Version!=1 ||
                    p.rp_Emoji.ep_Enabled>1 || p.rp_Bell.bp_Size!=sizeof(p.rp_Bell) ||
                    p.rp_Bell.bp_Version!=1 || p.rp_Bell.bp_Enabled>1 ||
                    !con_clipboard_prefs_valid(&p.rp_Clipboard) ||
                    p.rp_CursorColor.ccp_Size!=sizeof(p.rp_CursorColor) || p.rp_CursorColor.ccp_Version!=1 ||
                    p.rp_CursorColor.ccp_Enabled>1 || p.rp_CursorColor.ccp_RGB>0xffffffUL) error=CONSOLENG_ERR_BADPREFS;
            }
            if (!error)
            {
                ObtainSemaphore(&ConsoleDevice->unitListLock);
                if (set)
                {
                    ConsoleDevice->cursorDefaults=p.rp_Theme.tp_Cursor;
                    ConsoleDevice->historyDefaults=p.rp_Theme.tp_History;
                    ConsoleDevice->fontDefaults=p.rp_Theme.tp_Font;
                    ConsoleDevice->colorDefaults=p.rp_Theme.tp_Colors;
                    ConsoleDevice->emojiDefaults=p.rp_Emoji;
                    ConsoleDevice->bellDefaults=p.rp_Bell;
                    ConsoleDevice->clipboardDefaults=p.rp_Clipboard;
                    ConsoleDevice->cursorColorDefaults=p.rp_CursorColor;
                }
                else p=(struct ConsolengAppearanceProfile){sizeof(p),1,
                    {sizeof(p.rp_Theme),1,ConsoleDevice->cursorDefaults,ConsoleDevice->historyDefaults,
                     ConsoleDevice->fontDefaults,ConsoleDevice->colorDefaults},ConsoleDevice->emojiDefaults,ConsoleDevice->bellDefaults,ConsoleDevice->clipboardDefaults,ConsoleDevice->cursorColorDefaults};
                ReleaseSemaphore(&ConsoleDevice->unitListLock);
                if (!set) CopyMem(&p,ioreq->io_Data,sizeof(p));
                ioreq->io_Actual=sizeof(p);
            }
        }
        break;

    case CD_GETTERMINALPROFILE:
    case CD_SETTERMINALPROFILE:
        ioreq->io_Actual = 0;
        if (!ioreq->io_Data || ioreq->io_Unit != (struct Unit *)CONU_LIBRARY) error=IOERR_BADADDRESS;
        else if (ioreq->io_Length < sizeof(struct ConsolengTerminalProfile)) error=IOERR_BADLENGTH;
        else
        {
            struct ConsolengTerminalProfile p;
            BOOL set = ioreq->io_Command == CD_SETTERMINALPROFILE;
            if (set)
            {
                CopyMem(ioreq->io_Data,&p,sizeof(p));
                if (p.rp_Size!=sizeof(p) || p.rp_Version!=1 ||
                    p.rp_Theme.tp_Size!=sizeof(p.rp_Theme) || p.rp_Theme.tp_Version!=1 ||
                    !ConsolengValidCursorPrefs(&p.rp_Theme.tp_Cursor) ||
                    p.rp_Theme.tp_History.hp_Size!=sizeof(p.rp_Theme.tp_History) ||
                    p.rp_Theme.tp_History.hp_Version!=1 || p.rp_Theme.tp_History.hp_Lines>CONSOLENG_HISTORY_MAX_LINES ||
                    !ConsolengValidFontPrefs(&p.rp_Theme.tp_Font) || !ConsolengFontAvailable(&p.rp_Theme.tp_Font) ||
                    !ConsolengValidColorPrefs(&p.rp_Theme.tp_Colors) ||
                    p.rp_Emoji.ep_Size!=sizeof(p.rp_Emoji) || p.rp_Emoji.ep_Version!=1 ||
                    p.rp_Emoji.ep_Enabled>1 || p.rp_Bell.bp_Size!=sizeof(p.rp_Bell) ||
                    p.rp_Bell.bp_Version!=1 || p.rp_Bell.bp_Enabled>1 ||
                    !con_clipboard_prefs_valid(&p.rp_Clipboard)) error=CONSOLENG_ERR_BADPREFS;
            }
            if (!error)
            {
                ObtainSemaphore(&ConsoleDevice->unitListLock);
                if (set)
                {
                    ConsoleDevice->cursorDefaults=p.rp_Theme.tp_Cursor;
                    ConsoleDevice->historyDefaults=p.rp_Theme.tp_History;
                    ConsoleDevice->fontDefaults=p.rp_Theme.tp_Font;
                    ConsoleDevice->colorDefaults=p.rp_Theme.tp_Colors;
                    ConsoleDevice->emojiDefaults=p.rp_Emoji;
                    ConsoleDevice->bellDefaults=p.rp_Bell;
                    ConsoleDevice->clipboardDefaults=p.rp_Clipboard;
                }
                else p=(struct ConsolengTerminalProfile){sizeof(p),1,
                    {sizeof(p.rp_Theme),1,ConsoleDevice->cursorDefaults,ConsoleDevice->historyDefaults,
                     ConsoleDevice->fontDefaults,ConsoleDevice->colorDefaults},ConsoleDevice->emojiDefaults,ConsoleDevice->bellDefaults,ConsoleDevice->clipboardDefaults};
                ReleaseSemaphore(&ConsoleDevice->unitListLock);
                if (!set) CopyMem(&p,ioreq->io_Data,sizeof(p));
                ioreq->io_Actual=sizeof(p);
            }
        }
        break;

    case CD_GETBELLPREFS:
    case CD_SETBELLPREFS:
    case CD_GETDEFAULTBELLPREFS:
    case CD_SETDEFAULTBELLPREFS:
        ioreq->io_Actual = 0;
        {
            BOOL defaults = ioreq->io_Command == CD_GETDEFAULTBELLPREFS ||
                ioreq->io_Command == CD_SETDEFAULTBELLPREFS;
            BOOL set = ioreq->io_Command == CD_SETBELLPREFS ||
                ioreq->io_Command == CD_SETDEFAULTBELLPREFS;
            struct ConsolengBellPrefs prefs;
            if (!ioreq->io_Data || !ioreq->io_Unit ||
                (defaults != (ioreq->io_Unit == (struct Unit *)CONU_LIBRARY)))
                error = IOERR_BADADDRESS;
            else if (ioreq->io_Length < sizeof(prefs)) error = IOERR_BADLENGTH;
            else
            {
                if (set)
                {
                    CopyMem(ioreq->io_Data, &prefs, sizeof(prefs));
                    if (prefs.bp_Size != sizeof(prefs) || prefs.bp_Version != 1 ||
                        prefs.bp_Enabled > 1) error = CONSOLENG_ERR_BADPREFS;
                }
                if (!error)
                {
                    if (defaults)
                    {
                        ObtainSemaphore(&ConsoleDevice->unitListLock);
                        if (set) ConsoleDevice->bellDefaults = prefs;
                        else prefs = ConsoleDevice->bellDefaults;
                        ReleaseSemaphore(&ConsoleDevice->unitListLock);
                    }
                    else
                    {
                        IPTR args[2] = {(IPTR)&prefs,FALSE};
                        ObtainSemaphore(&ConsoleDevice->consoleTaskLock);
                        Console_DoCommand((Object *)ioreq->io_Unit,
                            set ? C_SET_BELL_PREFS : C_GET_BELL_PREFS, 2, args);
                        ReleaseSemaphore(&ConsoleDevice->consoleTaskLock);
                        if (!args[1]) error = IOERR_NOCMD;
                    }
                    if (!error)
                    {
                        if (!set) CopyMem(&prefs, ioreq->io_Data, sizeof(prefs));
                        ioreq->io_Actual = sizeof(prefs);
                    }
                }
            }
        }
        break;

    case CD_GETEMOJIPREFS:
    case CD_SETEMOJIPREFS:
    case CD_GETDEFAULTEMOJIPREFS:
    case CD_SETDEFAULTEMOJIPREFS:
        ioreq->io_Actual = 0;
        {
            BOOL defaults = ioreq->io_Command == CD_GETDEFAULTEMOJIPREFS ||
                ioreq->io_Command == CD_SETDEFAULTEMOJIPREFS;
            BOOL set = ioreq->io_Command == CD_SETEMOJIPREFS ||
                ioreq->io_Command == CD_SETDEFAULTEMOJIPREFS;
            struct ConsolengEmojiPrefs prefs;
            if (!ioreq->io_Data || !ioreq->io_Unit ||
                (defaults != (ioreq->io_Unit == (struct Unit *)CONU_LIBRARY)))
                error = IOERR_BADADDRESS;
            else if (ioreq->io_Length < sizeof(prefs)) error = IOERR_BADLENGTH;
            else
            {
                if (set)
                {
                    CopyMem(ioreq->io_Data, &prefs, sizeof(prefs));
                    if (prefs.ep_Size != sizeof(prefs) || prefs.ep_Version != 1 ||
                        prefs.ep_Enabled > 1) error = CONSOLENG_ERR_BADPREFS;
                }
                if (!error)
                {
                    if (defaults)
                    {
                        ObtainSemaphore(&ConsoleDevice->unitListLock);
                        if (set) ConsoleDevice->emojiDefaults = prefs;
                        else prefs = ConsoleDevice->emojiDefaults;
                        ReleaseSemaphore(&ConsoleDevice->unitListLock);
                    }
                    else
                    {
                        IPTR args[2] = {(IPTR)&prefs,FALSE};
                        ObtainSemaphore(&ConsoleDevice->consoleTaskLock);
                        Console_DoCommand((Object *)ioreq->io_Unit,
                            set ? C_SET_EMOJI_PREFS : C_GET_EMOJI_PREFS, 2, args);
                        ReleaseSemaphore(&ConsoleDevice->consoleTaskLock);
                        if (!args[1]) error = IOERR_NOCMD;
                    }
                    if (!error)
                    {
                        if (!set) CopyMem(&prefs, ioreq->io_Data, sizeof(prefs));
                        ioreq->io_Actual = sizeof(prefs);
                    }
                }
            }
        }
        break;

    case CD_GETSESSIONPROFILE:
    case CD_SETSESSIONPROFILE:
        ioreq->io_Actual = 0;
        if (!ioreq->io_Data || ioreq->io_Unit != (struct Unit *)CONU_LIBRARY) error=IOERR_BADADDRESS;
        else if (ioreq->io_Length < sizeof(struct ConsolengSessionProfile)) error=IOERR_BADLENGTH;
        else
        {
            struct ConsolengSessionProfile p;
            BOOL set = ioreq->io_Command == CD_SETSESSIONPROFILE;
            if (set)
            {
                CopyMem(ioreq->io_Data,&p,sizeof(p));
                if (p.rp_Size!=sizeof(p) || p.rp_Version!=1 ||
                    p.rp_Theme.tp_Size!=sizeof(p.rp_Theme) || p.rp_Theme.tp_Version!=1 ||
                    !ConsolengValidCursorPrefs(&p.rp_Theme.tp_Cursor) ||
                    p.rp_Theme.tp_History.hp_Size!=sizeof(p.rp_Theme.tp_History) ||
                    p.rp_Theme.tp_History.hp_Version!=1 || p.rp_Theme.tp_History.hp_Lines>CONSOLENG_HISTORY_MAX_LINES ||
                    !ConsolengValidFontPrefs(&p.rp_Theme.tp_Font) || !ConsolengFontAvailable(&p.rp_Theme.tp_Font) ||
                    !ConsolengValidColorPrefs(&p.rp_Theme.tp_Colors) ||
                    p.rp_Emoji.ep_Size!=sizeof(p.rp_Emoji) || p.rp_Emoji.ep_Version!=1 ||
                    p.rp_Emoji.ep_Enabled>1 || p.rp_Bell.bp_Size!=sizeof(p.rp_Bell) ||
                    p.rp_Bell.bp_Version!=1 || p.rp_Bell.bp_Enabled>1) error=CONSOLENG_ERR_BADPREFS;
            }
            if (!error)
            {
                ObtainSemaphore(&ConsoleDevice->unitListLock);
                if (set)
                {
                    ConsoleDevice->cursorDefaults=p.rp_Theme.tp_Cursor;
                    ConsoleDevice->historyDefaults=p.rp_Theme.tp_History;
                    ConsoleDevice->fontDefaults=p.rp_Theme.tp_Font;
                    ConsoleDevice->colorDefaults=p.rp_Theme.tp_Colors;
                    ConsoleDevice->emojiDefaults=p.rp_Emoji;
                    ConsoleDevice->bellDefaults=p.rp_Bell;
                }
                else p=(struct ConsolengSessionProfile){sizeof(p),1,
                    {sizeof(p.rp_Theme),1,ConsoleDevice->cursorDefaults,ConsoleDevice->historyDefaults,
                     ConsoleDevice->fontDefaults,ConsoleDevice->colorDefaults},ConsoleDevice->emojiDefaults,ConsoleDevice->bellDefaults};
                ReleaseSemaphore(&ConsoleDevice->unitListLock);
                if (!set) CopyMem(&p,ioreq->io_Data,sizeof(p));
                ioreq->io_Actual=sizeof(p);
            }
        }
        break;

    case CD_GETRENDERPROFILE:
    case CD_SETRENDERPROFILE:
        ioreq->io_Actual = 0;
        if (!ioreq->io_Data || ioreq->io_Unit != (struct Unit *)CONU_LIBRARY) error=IOERR_BADADDRESS;
        else if (ioreq->io_Length < sizeof(struct ConsolengRenderProfile)) error=IOERR_BADLENGTH;
        else
        {
            struct ConsolengRenderProfile p;
            BOOL set = ioreq->io_Command == CD_SETRENDERPROFILE;
            if (set)
            {
                CopyMem(ioreq->io_Data,&p,sizeof(p));
                if (p.rp_Size!=sizeof(p) || p.rp_Version!=1 ||
                    p.rp_Theme.tp_Size!=sizeof(p.rp_Theme) || p.rp_Theme.tp_Version!=1 ||
                    !ConsolengValidCursorPrefs(&p.rp_Theme.tp_Cursor) ||
                    p.rp_Theme.tp_History.hp_Size!=sizeof(p.rp_Theme.tp_History) ||
                    p.rp_Theme.tp_History.hp_Version!=1 || p.rp_Theme.tp_History.hp_Lines>CONSOLENG_HISTORY_MAX_LINES ||
                    !ConsolengValidFontPrefs(&p.rp_Theme.tp_Font) || !ConsolengFontAvailable(&p.rp_Theme.tp_Font) ||
                    !ConsolengValidColorPrefs(&p.rp_Theme.tp_Colors) ||
                    p.rp_Emoji.ep_Size!=sizeof(p.rp_Emoji) || p.rp_Emoji.ep_Version!=1 ||
                    p.rp_Emoji.ep_Enabled>1) error=CONSOLENG_ERR_BADPREFS;
            }
            if (!error)
            {
                ObtainSemaphore(&ConsoleDevice->unitListLock);
                if (set)
                {
                    ConsoleDevice->cursorDefaults=p.rp_Theme.tp_Cursor;
                    ConsoleDevice->historyDefaults=p.rp_Theme.tp_History;
                    ConsoleDevice->fontDefaults=p.rp_Theme.tp_Font;
                    ConsoleDevice->colorDefaults=p.rp_Theme.tp_Colors;
                    ConsoleDevice->emojiDefaults=p.rp_Emoji;
                }
                else p=(struct ConsolengRenderProfile){sizeof(p),1,
                    {sizeof(p.rp_Theme),1,ConsoleDevice->cursorDefaults,ConsoleDevice->historyDefaults,
                     ConsoleDevice->fontDefaults,ConsoleDevice->colorDefaults},ConsoleDevice->emojiDefaults};
                ReleaseSemaphore(&ConsoleDevice->unitListLock);
                if (!set) CopyMem(&p,ioreq->io_Data,sizeof(p));
                ioreq->io_Actual=sizeof(p);
            }
        }
        break;

    case CD_GETPASTEMODE:
        ioreq->io_Actual = 0;
        if (!ioreq->io_Data || !ioreq->io_Unit || ioreq->io_Unit == (struct Unit *)CONU_LIBRARY) error=IOERR_BADADDRESS;
        else if (ioreq->io_Length < sizeof(ULONG)) error=IOERR_BADLENGTH;
        else {
            ULONG enabled;
            ObtainSemaphore(&ConsoleDevice->consoleTaskLock);
            enabled = !!(ICU(ioreq->io_Unit)->conFlags & CF_BRACKETED_PASTE);
            ReleaseSemaphore(&ConsoleDevice->consoleTaskLock);
            CopyMem(&enabled, ioreq->io_Data, sizeof(enabled));
            ioreq->io_Actual = sizeof(enabled);
        }
        break;
    case CD_GETCOLORPREFS:
    case CD_SETCOLORPREFS:
        ioreq->io_Actual = 0;
        if (!ioreq->io_Data || !ioreq->io_Unit || ioreq->io_Unit == (struct Unit *)CONU_LIBRARY) error=IOERR_BADADDRESS;
        else if (ioreq->io_Length < sizeof(struct ConsolengColorPrefs)) error=IOERR_BADLENGTH;
        else {
            struct ConsolengColorPrefs p;
            BOOL set=ioreq->io_Command==CD_SETCOLORPREFS;
            CopyMem(ioreq->io_Data,&p,sizeof(p));
            if (set && !ConsolengValidColorPrefs(&p)) error=CONSOLENG_ERR_BADPREFS;
            else {
                IPTR args[2]={(IPTR)&p,FALSE};
                Console_DoCommand((Object *)ioreq->io_Unit,set?C_SET_COLOR_PREFS:C_GET_COLOR_PREFS,2,args);
                if (!args[1]) error=CONSOLENG_ERR_BADPREFS;
                else {if (!set) CopyMem(&p,ioreq->io_Data,sizeof(p));ioreq->io_Actual=sizeof(p);}
            }
        }
        break;
    case CD_GETTHEMEPROFILE:
    case CD_SETTHEMEPROFILE:
        ioreq->io_Actual=0;
        if (!ioreq->io_Data || ioreq->io_Unit != (struct Unit *)CONU_LIBRARY) error=IOERR_BADADDRESS;
        else if (ioreq->io_Length < sizeof(struct ConsolengThemeProfile)) error=IOERR_BADLENGTH;
        else {
            struct ConsolengThemeProfile p;
            BOOL set=ioreq->io_Command==CD_SETTHEMEPROFILE;
            if (set) {
                CopyMem(ioreq->io_Data,&p,sizeof(p));
                if (p.tp_Size!=sizeof(p) || p.tp_Version!=1 || !ConsolengValidCursorPrefs(&p.tp_Cursor) ||
                    p.tp_History.hp_Size!=sizeof(p.tp_History) || p.tp_History.hp_Version!=1 || p.tp_History.hp_Lines>CONSOLENG_HISTORY_MAX_LINES ||
                    !ConsolengValidFontPrefs(&p.tp_Font) || !ConsolengFontAvailable(&p.tp_Font) || !ConsolengValidColorPrefs(&p.tp_Colors)) error=CONSOLENG_ERR_BADPREFS;
            }
            if (!error) {
                ObtainSemaphore(&ConsoleDevice->unitListLock);
                if (set) {
                    ConsoleDevice->cursorDefaults=p.tp_Cursor;ConsoleDevice->historyDefaults=p.tp_History;
                    ConsoleDevice->fontDefaults=p.tp_Font;ConsoleDevice->colorDefaults=p.tp_Colors;
                } else p=(struct ConsolengThemeProfile){sizeof(p),1,ConsoleDevice->cursorDefaults,ConsoleDevice->historyDefaults,ConsoleDevice->fontDefaults,ConsoleDevice->colorDefaults};
                ReleaseSemaphore(&ConsoleDevice->unitListLock);
                if (!set) CopyMem(&p,ioreq->io_Data,sizeof(p));
                ioreq->io_Actual=sizeof(p);
            }
        }
        break;

    case CD_GETFONTPREFS:
    case CD_SETFONTPREFS:
    case CD_VALIDATEFONTPREFS:
        ioreq->io_Actual = 0;
        if (!ioreq->io_Data || !ioreq->io_Unit ||
            ((ioreq->io_Command == CD_VALIDATEFONTPREFS) != (ioreq->io_Unit == (struct Unit *)CONU_LIBRARY)))
            error = IOERR_BADADDRESS;
        else if (ioreq->io_Length < sizeof(struct ConsolengFontPrefs)) error = IOERR_BADLENGTH;
        else
        {
            Object *unit = (Object *)ioreq->io_Unit;
            struct ConsolengFontPrefs prefs = {sizeof(prefs),1,0,{0}};
            if (ioreq->io_Command != CD_GETFONTPREFS)
            {
                CopyMem(ioreq->io_Data,&prefs,sizeof(prefs));
                if (!ConsolengValidFontPrefs(&prefs) || !ConsolengFontAvailable(&prefs))
                    error = CONSOLENG_ERR_BADPREFS;
                else if (ioreq->io_Command == CD_SETFONTPREFS)
                {
                    IPTR args[2] = {(IPTR)prefs.fp_Name,prefs.fp_Pixels};
                    Console_DoCommand(unit,prefs.fp_Name[0]?C_SET_FONT:C_RESET_FONT,2,args);
                    if (!ICU(unit)->fontSetOK) error = CONSOLENG_ERR_BADPREFS;
                }
            }
            else
            {
                struct TextFont *font = ICU(unit)->setFont;
                if (font)
                {
                    const char *name = (const char *)font->tf_Message.mn_Node.ln_Name;
                    if (!name || strlen(name) >= sizeof(prefs.fp_Name)) error = CONSOLENG_ERR_BADPREFS;
                    else {strcpy(prefs.fp_Name,name);prefs.fp_Pixels=font->tf_YSize;}
                }
                if (!error) CopyMem(&prefs,ioreq->io_Data,sizeof(prefs));
            }
            if (!error) ioreq->io_Actual = sizeof(prefs);
        }
        break;

    case CD_GETDEFAULTPROFILE:
    case CD_SETDEFAULTPROFILE:
        ioreq->io_Actual = 0;
        if (!ioreq->io_Data || ioreq->io_Unit != (struct Unit *)CONU_LIBRARY) error = IOERR_BADADDRESS;
        else if (ioreq->io_Length < sizeof(struct ConsolengDefaultProfile)) error = IOERR_BADLENGTH;
        else
        {
            struct ConsolengDefaultProfile p;
            BOOL set = ioreq->io_Command == CD_SETDEFAULTPROFILE;
            if (set)
            {
                CopyMem(ioreq->io_Data,&p,sizeof(p));
                if (p.dp_Size != sizeof(p) || p.dp_Version != 1 ||
                    !ConsolengValidCursorPrefs(&p.dp_Cursor) ||
                    p.dp_History.hp_Size != sizeof(p.dp_History) || p.dp_History.hp_Version != 1 ||
                    p.dp_History.hp_Lines > CONSOLENG_HISTORY_MAX_LINES ||
                    !ConsolengValidFontPrefs(&p.dp_Font) || !ConsolengFontAvailable(&p.dp_Font))
                    error = CONSOLENG_ERR_BADPREFS;
            }
            if (!error)
            {
                ObtainSemaphore(&ConsoleDevice->unitListLock);
                if (set)
                {
                    ConsoleDevice->cursorDefaults = p.dp_Cursor;
                    ConsoleDevice->historyDefaults = p.dp_History;
                    ConsoleDevice->fontDefaults = p.dp_Font;
                }
                else p = (struct ConsolengDefaultProfile){sizeof(p),1,
                    ConsoleDevice->cursorDefaults,ConsoleDevice->historyDefaults,ConsoleDevice->fontDefaults};
                ReleaseSemaphore(&ConsoleDevice->unitListLock);
                if (!set) CopyMem(&p,ioreq->io_Data,sizeof(p));
                ioreq->io_Actual = sizeof(p);
            }
        }
        break;

    case CD_GETDEFAULTPREFS:
    case CD_SETDEFAULTPREFS:
        ioreq->io_Actual = 0;
        if (!ioreq->io_Data || ioreq->io_Unit != (struct Unit *)CONU_LIBRARY)
            error = IOERR_BADADDRESS;
        else if (ioreq->io_Length < sizeof(struct ConsolengDefaultPrefs))
            error = IOERR_BADLENGTH;
        else
        {
            struct ConsolengDefaultPrefs prefs;
            BOOL set = ioreq->io_Command == CD_SETDEFAULTPREFS;
            if (set)
            {
                CopyMem(ioreq->io_Data, &prefs, sizeof(prefs));
                if (prefs.dp_Size != sizeof(prefs) || prefs.dp_Version != 1 ||
                    !ConsolengValidCursorPrefs(&prefs.dp_Cursor) ||
                    prefs.dp_History.hp_Size != sizeof(prefs.dp_History) ||
                    prefs.dp_History.hp_Version != CONSOLENG_HISTORY_PREFS_VERSION ||
                    prefs.dp_History.hp_Lines > CONSOLENG_HISTORY_MAX_LINES)
                    error = CONSOLENG_ERR_BADPREFS;
            }
            if (!error)
            {
                ObtainSemaphore(&ConsoleDevice->unitListLock);
                if (set)
                {
                    ConsoleDevice->cursorDefaults = prefs.dp_Cursor;
                    ConsoleDevice->historyDefaults = prefs.dp_History;
                }
                else prefs = (struct ConsolengDefaultPrefs) {
                    sizeof(prefs), 1, ConsoleDevice->cursorDefaults,
                    ConsoleDevice->historyDefaults
                };
                ReleaseSemaphore(&ConsoleDevice->unitListLock);
                if (!set) CopyMem(&prefs, ioreq->io_Data, sizeof(prefs));
                ioreq->io_Actual = sizeof(prefs);
            }
        }
        break;

    case CD_GETHISTORYPREFS:
    case CD_SETHISTORYPREFS:
    case CD_GETDEFAULTHISTORYPREFS:
    case CD_SETDEFAULTHISTORYPREFS:
        ioreq->io_Actual = 0;
        {
            BOOL defaults = ioreq->io_Command == CD_GETDEFAULTHISTORYPREFS ||
                ioreq->io_Command == CD_SETDEFAULTHISTORYPREFS;
            BOOL set = ioreq->io_Command == CD_SETHISTORYPREFS ||
                ioreq->io_Command == CD_SETDEFAULTHISTORYPREFS;
            struct ConsolengHistoryPrefs prefs;
            if (!ioreq->io_Data || !ioreq->io_Unit ||
                (defaults != (ioreq->io_Unit == (struct Unit *)CONU_LIBRARY)))
                error = IOERR_BADADDRESS;
            else if (ioreq->io_Length < sizeof(prefs))
                error = IOERR_BADLENGTH;
            else
            {
                if (set)
                {
                    CopyMem(ioreq->io_Data, &prefs, sizeof(prefs));
                    if (prefs.hp_Size != sizeof(prefs) ||
                        prefs.hp_Version != CONSOLENG_HISTORY_PREFS_VERSION ||
                        prefs.hp_Lines > CONSOLENG_HISTORY_MAX_LINES)
                        error = CONSOLENG_ERR_BADPREFS;
                }
                if (!error)
                {
                    if (defaults)
                    {
                        ObtainSemaphore(&ConsoleDevice->unitListLock);
                        if (set) ConsoleDevice->historyDefaults = prefs;
                        else prefs = ConsoleDevice->historyDefaults;
                        ReleaseSemaphore(&ConsoleDevice->unitListLock);
                    }
                    else
                    {
                        IPTR args[2] = { (IPTR)&prefs, FALSE };
                        ObtainSemaphore(&ConsoleDevice->consoleTaskLock);
                        Console_DoCommand((Object *)ioreq->io_Unit,
                            set ? C_SET_HISTORY_PREFS : C_GET_HISTORY_PREFS, 2, args);
                        ReleaseSemaphore(&ConsoleDevice->consoleTaskLock);
                        if (!args[1]) error = IOERR_NOCMD;
                    }
                    if (!error)
                    {
                        if (!set) CopyMem(&prefs, ioreq->io_Data, sizeof(prefs));
                        ioreq->io_Actual = sizeof(prefs);
                    }
                }
            }
        }
        break;

    case CD_GETDEFAULTCURSORPREFS:
    case CD_SETDEFAULTCURSORPREFS:
        ioreq->io_Actual = 0;
        if (!ioreq->io_Data ||
            ioreq->io_Unit != (struct Unit *)CONU_LIBRARY)
            error = IOERR_BADADDRESS;
        else if (ioreq->io_Length < sizeof(struct ConsolengCursorPrefs))
            error = IOERR_BADLENGTH;
        else
        {
            struct ConsolengCursorPrefs prefs;
            BOOL set = ioreq->io_Command == CD_SETDEFAULTCURSORPREFS;
            if (set)
            {
                CopyMem(ioreq->io_Data, &prefs, sizeof(prefs));
                if (!ConsolengValidCursorPrefs(&prefs))
                    error = CONSOLENG_ERR_BADPREFS;
            }
            if (!error)
            {
                ObtainSemaphore(&ConsoleDevice->unitListLock);
                if (set) ConsoleDevice->cursorDefaults = prefs;
                else prefs = ConsoleDevice->cursorDefaults;
                ReleaseSemaphore(&ConsoleDevice->unitListLock);
                if (!set) CopyMem(&prefs, ioreq->io_Data, sizeof(prefs));
                ioreq->io_Actual = sizeof(prefs);
            }
        }
        break;

    case CD_GETCURSORPREFS:
    case CD_SETCURSORPREFS:
        ioreq->io_Actual = 0;
        if (!ioreq->io_Data || !ioreq->io_Unit ||
            ioreq->io_Unit == (struct Unit *)CONU_LIBRARY)
            error = IOERR_BADADDRESS;
        else if (ioreq->io_Length < sizeof(struct ConsolengCursorPrefs))
            error = IOERR_BADLENGTH;
        else
        {
            struct ConsolengCursorPrefs prefs;
            BOOL set = ioreq->io_Command == CD_SETCURSORPREFS;
            IPTR args[1] = { (IPTR)&prefs };
            if (set)
            {
                CopyMem(ioreq->io_Data, &prefs, sizeof(prefs));
                if (!ConsolengValidCursorPrefs(&prefs))
                    error = CONSOLENG_ERR_BADPREFS;
            }
            if (!error)
            {
                ObtainSemaphore(&ConsoleDevice->consoleTaskLock);
                Console_DoCommand((Object *)ioreq->io_Unit,
                    set ? C_SET_CURSOR_PREFS : C_GET_CURSOR_PREFS, 1, args);
                ReleaseSemaphore(&ConsoleDevice->consoleTaskLock);
                if (!set) CopyMem(&prefs, ioreq->io_Data, sizeof(prefs));
                ioreq->io_Actual = sizeof(prefs);
            }
        }
        break;

    case CD_GETGEOMETRY:
        if (!ioreq->io_Data || !ioreq->io_Unit ||
            ioreq->io_Unit == (struct Unit *)CONU_LIBRARY)
            error = IOERR_BADADDRESS;
        else if (ioreq->io_Length < sizeof(struct ConsoleGeometry))
            error = IOERR_BADLENGTH;
        else
        {
            struct ConUnit *unit = (struct ConUnit *)ioreq->io_Unit;
            struct ConsoleGeometry geometry;

            /* The console task holds this lock while processing resize.
               The open IO unit keeps the object alive during this query. */
            ObtainSemaphore(&ConsoleDevice->consoleTaskLock);
            geometry.cg_Size = sizeof(geometry);
            geometry.cg_Version = CONSOLE_GEOMETRY_VERSION;
            geometry.cg_Columns = unit->cu_XMax + 1;
            geometry.cg_Rows = unit->cu_YMax + 1;
            geometry.cg_Width = geometry.cg_Columns * unit->cu_XRSize;
            geometry.cg_Height = geometry.cg_Rows * unit->cu_YRSize;
            if (unit->cu_XMax < 0 || unit->cu_YMax < 0 ||
                unit->cu_XRSize <= 0 || unit->cu_YRSize <= 0)
                error = IOERR_BADADDRESS;
            ReleaseSemaphore(&ConsoleDevice->consoleTaskLock);
            if (!error)
            {
                CopyMem(&geometry, ioreq->io_Data, sizeof(geometry));
                ioreq->io_Actual = sizeof(geometry);
            }
        }
        break;

    case CD_ASKKEYMAP:
        /* FIXME: Returns always default keymap */
        if (ioreq->io_Length < sizeof(struct KeyMap))
            error = IOERR_BADLENGTH;
        else
            CopyMem(AskKeyMapDefault(), ioreq->io_Data,
                sizeof(struct KeyMap));
        break;
    case CD_ASKTITLE:
        if (ioreq->io_Length < 1)
            error = IOERR_BADLENGTH;
        else
        {
            struct intConUnit *icu = (struct intConUnit *) ioreq->io_Unit;
            LONG n = 0;

            while (icu->unitTitle[n] && n < ioreq->io_Length - 1
                && n < CON_TITLE_SIZE - 1)
            {
                ((UBYTE *) ioreq->io_Data)[n] = icu->unitTitle[n];
                n++;
            }
            ((UBYTE *) ioreq->io_Data)[n] = '\0';
            ioreq->io_Actual = n;
        }
        break;
    case CD_SETKEYMAP:
        D(bug("CD_SETKEYMAP\n"));
        error = IOERR_NOCMD;
        break;
    case CD_ASKDEFAULTKEYMAP:
        if (ioreq->io_Length < sizeof(struct KeyMap))
            error = IOERR_BADLENGTH;
        else
            CopyMem(AskKeyMapDefault(), ioreq->io_Data,
                sizeof(struct KeyMap));
        break;
    case CD_SETDEFAULTKEYMAP:
        D(bug("CD_SETDEFAULTKEYMAP\n"));
        error = IOERR_NOCMD;
        break;

    default:
        D(bug("IOERR_NOCMD %d\n", ioreq->io_Command));
        error = IOERR_NOCMD;
        break;
    } /* switch (ioreq->io_Command) */

    if (!done_quick)
    {
        /* Mark IO request to be done non-quick */
        ioreq->io_Flags &= ~IOF_QUICK;
        /* All producers and consumers of this private queue share the
         * task lock, including AbortIO removing an unprocessed read. */
        ObtainSemaphore(&ConsoleDevice->consoleTaskLock);
        PutMsg(ConsoleDevice->commandPort, &ioreq->io_Message);
        ReleaseSemaphore(&ConsoleDevice->consoleTaskLock);
    }
    else
    {

        /* If the quick bit is not set but the IO request was done quick,
         * reply the message to tell we're through
         */
        ioreq->io_Error = error;
        if (!(ioreq->io_Flags & IOF_QUICK))
            ReplyMsg(&ioreq->io_Message);
    }

    ReturnVoid("BeginIO");

    AROS_LIBFUNC_EXIT
}

/*****************************************************************************/

AROS_LH1(LONG, abortio,
    AROS_LHA(struct IORequest *, ioreq, A1),
    struct ConsoleBase *, ConsoleDevice, 6, Consoleng)
{
    AROS_LIBFUNC_INIT

    LONG ret = -1;

    ObtainSemaphore(&ConsoleDevice->consoleTaskLock);

    /* A quick-completed request may still have NT_MESSAGE, and a request
     * already collected by WaitIO no longer belongs to this device. Verify
     * ownership rather than removing arbitrary links based on node type.
     * BeginIO and consoleTaskEntry use this same lock for the private queues. */
    if (!(ioreq->io_Flags & IOF_QUICK) &&
        ioreq->io_Message.mn_Node.ln_Type == NT_MESSAGE)
    {
        struct List *queues[2] = { &ConsoleDevice->commandPort->mp_MsgList,
            (struct List *)&ConsoleDevice->readRequests };
        struct Node *node;
        int q;
        for (q = 0; q < 2 && ret != 0; q++)
        {
            ForeachNode(queues[q], node)
            {
                if (node == &ioreq->io_Message.mn_Node)
                {
                    Remove(node);
                    ((struct IOStdReq *)ioreq)->io_Actual = 0;
                    ioreq->io_Error = IOERR_ABORTED;
                    ReplyMsg(&ioreq->io_Message);
                    ret = 0;
                    break;
                }
            }
        }
    }

    ReleaseSemaphore(&ConsoleDevice->consoleTaskLock);

    return ret;

    AROS_LIBFUNC_EXIT
}

/*****************************************************************************/
