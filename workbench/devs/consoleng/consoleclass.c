/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc: Base class for console units
*/

#define SDEBUG 0
#define DEBUG 0
#include <aros/debug.h>

#include <proto/exec.h>
#include <exec/memory.h>
#include <proto/intuition.h>
#include <proto/utility.h>
#include <proto/graphics.h>
#include <proto/diskfont.h>
#include <proto/dos.h>
#include <dos/dostags.h>
#include <dos/dosextens.h>
#include <graphics/text.h>

#include <aros/asmcall.h>
#include <devices/conunit.h>
#include <intuition/classes.h>
#include <intuition/intuition.h>

#include <string.h>

#include "consoleif.h"
#include "console_gcc.h"

VOID normalizecoords(Object *o, WORD *x_ptr, WORD *y_ptr);

/*
   Base class for the classes handling standard, charmap and snipmap
   consoles. This is a whitebox base class (like GADGETCLASS), which means subclasses
   can have direct access to its instance data.
*/

struct consoledata
{
    struct intConUnit intunit;
};

#undef ConsoleDevice
#define ConsoleDevice ((struct ConsoleBase *)cl->cl_UserData)

/* Registry ownership is separate from the unit list: New() runs before
 * publication, and Dispose() after removal from that list. */
static struct ConsolengFontWindow *console_attach_font_window(
    struct ConsoleBase *device, struct Window *window)
{
    struct ConsolengFontWindow *entry, *found = NULL;
    ObtainSemaphore(&device->fontWindowLock);
    ForeachNode(&device->fontWindows, entry)
        if (entry->window == window) { found = entry; break; }
    if (!found)
    {
        found = AllocVec(sizeof(*found), MEMF_PUBLIC | MEMF_CLEAR);
        if (found)
        {
            found->window = window;
            found->original = window->RPort->Font;
            AddTail((struct List *)&device->fontWindows, (struct Node *)&found->node);
        }
    }
    if (found) found->users++;
    ReleaseSemaphore(&device->fontWindowLock);
    return found;
}

static VOID console_restore_font(Class *cl, Object *o)
{
    struct Library *GfxBase = TaggedOpenLibrary(TAGGEDOPEN_GRAPHICS);
    struct TextFont *font = ICU(o)->setFont;
    if (!font && ICU(o)->fontWindow) font = ICU(o)->fontWindow->original;
    if (GfxBase && font) SetFont(RASTPORT(o), font);
}

static VOID console_detach_font_window(struct ConsoleBase *device, Object *o)
{
    struct ConsolengFontWindow *entry = ICU(o)->fontWindow;
    struct Library *GfxBase = TaggedOpenLibrary(TAGGEDOPEN_GRAPHICS);
    if (!entry) return;
    ObtainSemaphore(&device->fontWindowLock);
    /* A hidden unit must not change the visible sibling's font. */
    if (GfxBase && RASTPORT(o)->Font == ICU(o)->setFont)
        SetFont(RASTPORT(o), entry->original);
    if (!--entry->users)
    {
        Remove((struct Node *)&entry->node);
        FreeVec(entry);
    }
    ICU(o)->fontWindow = NULL;
    ReleaseSemaphore(&device->fontWindowLock);
}

/* Titles are already converted to Intuition encoding when stored. */
struct ConsoleTitleEntry
{
    struct ConsoleTitleEntry *next;
    UBYTE title[CON_TITLE_SIZE];
};
#define CON_TITLE_STACK_MAX 4096
static void console_publish_title(Class *cl, Object *o)
{
    memcpy(ICU(o)->displayTitle, ICU(o)->unitTitle, CON_TITLE_SIZE);
    SetWindowTitles(CU(o)->cu_Window, (STRPTR)ICU(o)->displayTitle, (STRPTR)~0);
}

static void console_title_stack(Class *cl, Object *o, BYTE command)
{
    struct ConsoleTitleEntry *entry;
    if (command == C_PUSH_TITLE)
    {
        entry = AllocVec(sizeof(*entry), MEMF_PUBLIC);
        if (!entry) return;
        memcpy(entry->title, ICU(o)->unitTitle, CON_TITLE_SIZE);
        entry->next = ICU(o)->titleStack;
        ICU(o)->titleStack = entry;
        if (ICU(o)->titleDepth == CON_TITLE_STACK_MAX)
        {
            while (entry->next->next) entry = entry->next;
            FreeVec(entry->next);
            entry->next = NULL;
        }
        else ICU(o)->titleDepth++;
    }
    else if (command == C_POP_TITLE)
    {
        entry = ICU(o)->titleStack;
        if (!entry) return;
        ICU(o)->titleStack = entry->next;
        ICU(o)->titleDepth--;
        memcpy(ICU(o)->unitTitle, entry->title, CON_TITLE_SIZE);
        FreeVec(entry);
        if (!CON_IS_OBSCURED(o))
            console_publish_title(cl, o);
    }
    else
    {
        while ((entry = ICU(o)->titleStack))
        {
            ICU(o)->titleStack = entry->next;
            FreeVec(entry);
        }
        ICU(o)->titleDepth = 0;
    }
}

/*********************
**  Console::New()  **
*********************/
static Object *console_new(Class *cl, Object *o, struct opSet *msg)
{
    struct Window *win;
    EnterFunc(bug("Console::New()\n"));

    /* Get console window */
    win =
        (struct Window *)GetTagData(A_Console_Window, 0, msg->ops_AttrList);

    if (!win)
    {
        ReturnPtr("Console::New", Object *, NULL);
    }

    o = (Object *) DoSuperMethodA(cl, o, (Msg) msg);
    if (o)
    {
        struct ConUnit *unit;
        struct consoledata *data;
        struct RastPort *rp = win->RPort;
        WORD i;

        data = INST_DATA(cl, o);

        unit = (struct ConUnit *)data;

        SetMem(data, 0, sizeof(struct consoledata));
        for (i = 0; i < CONUNIT_PEN_MAX; i ++)
        {
            data->intunit.pens[i] = i;
        }

        /* Initialize the unit fields */
        unit->cu_Window = win;
        if (win->Title)
            strncpy((char *)ICU(o)->unitTitle, (const char *)win->Title, CON_TITLE_SIZE - 1);
        data->intunit.fontWindow = console_attach_font_window(ConsoleDevice, win);
        if (!data->intunit.fontWindow)
        {
            CoerceMethod(cl, o, OM_DISPOSE);
            return NULL;
        }
        console_restore_font(cl, o);

        /* For now one should use only non-proportional fonts */
        unit->cu_XRSize = rp->Font->tf_XSize;
        unit->cu_YRSize = rp->Font->tf_YSize;

        D(bug("cu_XRSize: %d, cu_YRSize: %d\n",
                unit->cu_XRSize, unit->cu_YRSize));

        /* Use whole window for console. */
        unit->cu_XMax =
            (win->Width - (win->BorderLeft +
                win->BorderRight)) / unit->cu_XRSize - 1;
        unit->cu_YMax =
            (win->Height - (win->BorderTop +
                win->BorderBottom)) / unit->cu_YRSize - 1;

        unit->cu_XROrigin =
            win->Flags & WFLG_GIMMEZEROZERO ? 0 : win->BorderLeft;
        unit->cu_YROrigin =
            win->Flags & WFLG_GIMMEZEROZERO ? 0 : win->BorderTop;

        D(bug("cu_XROrigin: %d, cu_YROrigin: %d\n",
                unit->cu_XROrigin, unit->cu_YROrigin));

        unit->cu_XRExtant =
            unit->cu_XROrigin + (unit->cu_XRSize * (unit->cu_XMax + 1) - 1);
        unit->cu_YRExtant =
            unit->cu_YROrigin + (unit->cu_YRSize * (unit->cu_YMax + 1) - 1);

        ICU(o)->fgColor = CONUNIT_PEN_DEFFG;
        ICU(o)->bgColor = CONUNIT_PEN_DEFBG;
        unit->cu_XCP = DEF_CHAR_XMIN;
        unit->cu_YCP = DEF_CHAR_YMIN;

        unit->cu_XCCP = DEF_CHAR_XMIN;
        unit->cu_YCCP = DEF_CHAR_YMIN;

        for (i = 0; i < MAXTABS - 1; i++)
        {
            unit->cu_TabStops[i] = i * 8;
        }
        unit->cu_TabStops[i] = (UWORD) - 1;

        /* consoleng exists to carry Unicode, and what reaches it is
           TERM=xterm-256color output (node/Ink, ncurses) that assumes a
           UTF-8 terminal and never sends ESC % G.  Default the unit to
           UTF-8; ESC % @ (or CSI >8l) still selects byte mode for a Latin-1
           producer, and an invalid byte decodes as Latin-1 anyway (see
           con_utf8_decode).  console.device keeps its byte-mode default. */
        ICU(o)->conFlags = CF_UTF8;
        ICU(o)->utf8StashLen = 0;
        ICU(o)->numStoredChars = 0;

        NEWLIST(&ICU(o)->pasteData);

        SET_MODE(o, PMB_ASM);   /* auto-scroll-mode ON */
        SET_MODE(o, PMB_AWM);   /* auto-wrap-mode ON */

        /* RKM: Default mode is SET */
        SET_MODE(o, M_LNM);     /* linefeed mode = linefeed+carriage return */
    }

    ReturnPtr("Console::New", Object *, o);
}

/**********************
**  Console::Left()  **
**********************/
static VOID console_left(Class *cl, Object *o, struct P_Console_Left *msg)
{
    WORD newx;

    EnterFunc(bug("Console::Left()\n"));

    newx = XCCP - msg->Num;

    if (CHECK_MODE(o, PMB_AWM))
    {
        WORD scrollcount = 0;

        while (newx < CHAR_XMIN(o))
        {
            newx += (CHAR_XMAX(o) + 1);
            scrollcount++;
        }

        XCP = XCCP = newx;

        Console_Up(o, scrollcount);
    }
    else
    {
        if (newx < CHAR_XMIN(o))
            newx = CHAR_XMIN(o);
    }

    XCP = XCCP = newx;          /* XCP always same as XCCP?? */

    D(bug("XCP=%d, XCCP=%d\n", XCP, XCCP));

    ReturnVoid("Console::Left");
}

/***********************
**  Console::Right()  **
***********************/
static VOID console_right(Class *cl, Object *o, struct P_Console_Right *msg)
{
    WORD newx;

    EnterFunc(bug("Console::Right()\n"));

    newx = XCCP + msg->Num;

    if (CHECK_MODE(o, PMB_AWM))
    {
        WORD scrollcount = 0;

        while (newx > CHAR_XMAX(o))
        {
            newx -= (CHAR_XMAX(o) + 1);
            scrollcount++;
        }

        XCP = XCCP = newx;

        Console_Down(o, scrollcount);
    }
    else
    {
        if (newx > CHAR_XMAX(o))
            newx = CHAR_XMAX(o);
    }

    XCP = XCCP = newx;          /* XCP always same as XCCP?? */

    D(bug("XCP=%d, XCCP=%d\n", XCP, XCCP));

    ReturnVoid("Console::Right");
}

/********************
**  Console::Up()  **
********************/
static VOID console_up(Class *cl, Object *o, struct P_Console_Up *msg)
{
    EnterFunc(bug("Console::Up(num=%d)\n", msg->Num));

    LONG limit = YCCP >= CON_SCROLL_TOP(o) ? CON_SCROLL_TOP(o) : 0;
    BOOL inRegion = YCCP >= CON_SCROLL_TOP(o) && YCCP <= CON_SCROLL_BOTTOM(o);
    LONG target = (LONG)YCCP - msg->Num;
    YCCP = MAX(target, limit);

    if (target < limit)
    {
        if (inRegion && CHECK_MODE(o, PMB_ASM))
        {
            IPTR scroll_param = limit - target;

            YCCP = YCP = limit;
            Console_DoCommand(o, C_SCROLL_DOWN, 1, &scroll_param);
        }
        else
        {
            YCCP = limit;
        }
    }
    YCP = YCCP;                 /* YCP always same as YCCP ?? */

    D(bug("New coords: char (%d, %d), gfx (%d, %d)\n",
            XCCP, YCCP, CP_X(o), CP_Y(o)));
    ReturnVoid("Console::Up");
}



/**********************
**  Console::Down()  **
**********************/
static VOID console_down(Class *cl, Object *o, struct P_Console_Down *msg)
{
    EnterFunc(bug("Console::Down(num=%d)\n", msg->Num));

    LONG limit = YCCP <= CON_SCROLL_BOTTOM(o) ? CON_SCROLL_BOTTOM(o) : CHAR_YMAX(o);
    BOOL inRegion = YCCP >= CON_SCROLL_TOP(o) && YCCP <= CON_SCROLL_BOTTOM(o);
    LONG target = (LONG)YCCP + msg->Num;
    YCCP = MIN(target, limit);

    if (target > limit)
    {
        if (inRegion && CHECK_MODE(o, PMB_ASM))
        {
            IPTR scroll_param = target - limit;

            YCCP = YCP = limit;
            Console_DoCommand(o, C_SCROLL_UP, 1, &scroll_param);
        }
        else
        {
            YCCP = limit;
        }
    }
    YCP = YCCP;                 /* YCP always same as YCCP ?? */

    D(bug("New coords: char (%d, %d), gfx (%d, %d)\n",
            XCCP, YCCP, CP_X(o), CP_Y(o)));
    ReturnVoid("Console::Down");
}

/***************************
**  Console::DoCommand()  **
***************************/
static VOID console_install_font(Class *cl, Object *o, struct TextFont *tf)
{
    struct Library *GfxBase = TaggedOpenLibrary(TAGGEDOPEN_GRAPHICS);
    struct TextFont *effective = tf ? tf : ICU(o)->fontWindow->original;
    struct RastPort *rp = RASTPORT(o);
    UBYTE oldpen = rp->FgPen;
    BOOL visible = !CON_IS_OBSCURED(o);
    struct TextFont *previous = ICU(o)->setFont;

    if (visible) Console_UnRenderCursor(o);
    ICU(o)->setFont = tf;
    if (visible)
        SetFont(rp, effective);
    else if (previous && rp->Font == previous && ICU(o)->fontWindow)
        SetFont(rp, ICU(o)->fontWindow->original);
    ConsolengCloseFont(ConsoleDevice, previous);

    /* Clear the old text grid, never the window's
       scroller or other gadgets. Subclass refresh paints
       any partial-cell margins after re-deriving it. */
    if (visible)
    {
        SetAPen(rp, CU(o)->cu_BgPen);
        RectFill(rp, CU(o)->cu_XROrigin, CU(o)->cu_YROrigin,
            CU(o)->cu_XRExtant, CU(o)->cu_YRExtant);
        SetAPen(rp, oldpen);
    }

    CU(o)->cu_XRSize = effective->tf_XSize;
    CU(o)->cu_YRSize = effective->tf_YSize;

    /* recompute grid + clamp cursor; the charmap
       subclasses refresh from the cell store */
    Console_NewWindowSize(o);

    if (visible) Console_RenderCursor(o);
    ICU(o)->fontSetOK = TRUE;
}

struct zoom_font_request {
    struct Message message;
    struct TextAttr attr;
    char name[CONSOLENG_FONT_NAME_SIZE+8];
    struct TextFont *font;
};

static LONG console_zoom_font_worker(void)
{
    struct Process *self = (struct Process *)FindTask(NULL);
    struct zoom_font_request *job = self->pr_Task.tc_UserData;
    struct Library *DiskfontBase = OpenLibrary("diskfont.library",0);
    if (DiskfontBase) {
        job->font = OpenDiskFont(&job->attr);
        CloseLibrary(DiskfontBase);
    }
    /* The parent may release the request immediately after this reply. */
    ReplyMsg(&job->message);
    return 0;
}

/* Diskfont may use DOS current-directory state. Never run it in the console's
 * Exec input task. The worker receives only a copied font request, not a unit. */
static struct TextFont *console_zoom_open_font(const struct TextAttr *attr)
{
    struct TextFont *font = NULL;
    struct zoom_font_request job = {0};
    if (strlen(attr->ta_Name) >= sizeof(job.name)) return NULL;
    job.attr = *attr;
    strcpy(job.name,attr->ta_Name);
    job.attr.ta_Name = job.name;
    struct DosLibrary *DOSBase = (struct DosLibrary *)OpenLibrary("dos.library",0);
    if (!DOSBase) return NULL;
    struct MsgPort *port = CreateMsgPort();
    if (port) {
        job.message.mn_ReplyPort = port;
        job.message.mn_Length = sizeof(job);
        if (CreateNewProcTags(NP_Entry,(IPTR)console_zoom_font_worker,
            NP_Name,(IPTR)"ConsoleNG font loader",NP_StackSize,65536,
            NP_UserData,(IPTR)&job,NP_CopyVars,FALSE,NP_Cli,FALSE,
            NP_Input,BNULL,NP_Output,BNULL,NP_Error,BNULL,
            NP_CloseInput,FALSE,NP_CloseOutput,FALSE,NP_CloseError,FALSE,
            NP_CurrentDir,BNULL,NP_HomeDir,BNULL,NP_Path,BNULL,TAG_DONE)) {
            while (!GetMsg(port)) WaitPort(port);
            font = job.font;
        }
        DeleteMsgPort(port);
    }
    CloseLibrary((struct Library *)DOSBase);
    return font;
}

/* Each unit retains a value snapshot, never a pointer to a closed zoom font.
 * A bitmap face lacking the exact requested size leaves the current font intact. */
static VOID console_zoom_font(Class *cl, Object *o, ULONG action)
{
    struct TextFont *current = ICU(o)->setFont;
    struct ConsolengFontPrefs before = {sizeof(before),1,0,{0}}, wanted;
    BOOL reset = action == CONSOLENG_BIND_FONTRESET;
    BOOL borrowed = !current;
    ICU(o)->fontSetOK = FALSE;
    if (CON_IS_OBSCURED(o) || !ICU(o)->fontWindow) return;
    if (!current) current = ICU(o)->fontWindow->original;
    if (!current) return;
    if (reset) {
        if (!ICU(o)->fontZoomActive) return;
        if (ICU(o)->fontZoomBorrowed) {
            console_install_font(cl,o,NULL);
            ICU(o)->fontZoomActive = FALSE;
            return;
        }
        wanted = ICU(o)->fontZoomBase;
    } else {
        if (action != CONSOLENG_BIND_FONTLARGER && action != CONSOLENG_BIND_FONTSMALLER) return;
        const char *name = (const char *)current->tf_Message.mn_Node.ln_Name;
        if (!name || !*name || strlen(name) >= sizeof(before.fp_Name)) return;
        strcpy(before.fp_Name,name);
        before.fp_Pixels = current->tf_YSize;
        wanted = before;
        if (action == CONSOLENG_BIND_FONTLARGER) {
            if (wanted.fp_Pixels >= 65535) return;
            wanted.fp_Pixels++;
        } else {
            if (wanted.fp_Pixels <= 1) return;
            wanted.fp_Pixels--;
        }
    }
    struct Library *GfxBase = TaggedOpenLibrary(TAGGEDOPEN_GRAPHICS);
    if (GfxBase) {
        char name[CONSOLENG_FONT_NAME_SIZE+8];
        strcpy(name,wanted.fp_Name);
        ULONG len = strlen(name);
        if (len < 5 || strnicmp(name+len-5,".font",5)) strcat(name,".font");
        struct TextAttr ta = {(STRPTR)name,wanted.fp_Pixels,FS_NORMAL,0};
        struct TextFont *font = console_zoom_open_font(&ta);
        if (font) {
            if ((font->tf_Flags & FPF_PROPORTIONAL) || !font->tf_XSize ||
                font->tf_YSize != wanted.fp_Pixels) CloseFont(font);
            else {
                console_install_font(cl,o,font);
                if (!reset && !ICU(o)->fontZoomActive) {
                    ICU(o)->fontZoomBase = before;
                    ICU(o)->fontZoomBorrowed = borrowed;
                    ICU(o)->fontZoomActive = TRUE;
                }
                if (reset) ICU(o)->fontZoomActive = FALSE;
            }
        }
    }
}

/* DECRPM:0 unsupported,1 set,2 reset. Report only implemented modes. */
static ULONG console_mode_state(Object *o, BOOL privateMode, ULONG mode)
{
    BOOL enabled;
    if (!privateMode)
    {
        if (mode == 4) enabled = !!(ICU(o)->conFlags & CF_INSERT_MODE);
        else if (mode == 20) enabled = !!CHECK_MODE(o, M_LNM);
        else return 0;
    }
    else switch (mode)
    {
    case 1: enabled = !!(ICU(o)->conFlags & CF_APPLICATION_CURSOR); break;
    case 6: enabled = !!(ICU(o)->conFlags & CF_ORIGIN_MODE); break;
    case 7: enabled = !!CHECK_MODE(o, PMB_AWM); break;
    case 1000: case 1002: case 1003: enabled = ICU(o)->mouseMode == mode; break;
    case 1004: enabled = !!(ICU(o)->conFlags & CF_FOCUS_REPORTING); break;
    case 1005: enabled = !!ICU(o)->mouseUTF8; break;
    case 1006: enabled = !!ICU(o)->mouseSGR; break;
    case 1007: enabled = !!(ICU(o)->conFlags & CF_ALTERNATE_SCROLL); break;
    case 2004: enabled = !!(ICU(o)->conFlags & CF_BRACKETED_PASTE); break;
    case 2031: enabled = !!(ICU(o)->conFlags & CF_COLOR_SCHEME_NOTIFY); break;
    default: return 0;
    }
    return enabled ? 1 : 2;
}

static VOID console_docommand(Class *cl, Object *o,
    struct P_Console_DoCommand *msg)
{
    EnterFunc(bug("Console::DoCommand(cmd=%d)\n", msg->Command));

    if (msg->Command == C_GET_MODE_STATE)
    {
        *(ULONG *)msg->Params[2] = console_mode_state(o, msg->Params[0], msg->Params[1]);
        return;
    }

    switch (msg->Command)
    {
    case C_SHIFT_IN:
        ICU(o)->activeCharset = 0;
        break;
    case C_SHIFT_OUT:
        ICU(o)->activeCharset = 1;
        break;

    case C_SET_LF_MODE:
        D(bug("Set LF mode ON\n"));
        /* LF==LF+CR */
        /* ICU(o)->conFlags |= CF_LF_MODE_ON ; */
        SET_MODE(o, M_LNM);
        break;

    case C_RESET_LF_MODE:
        /* LF==LF */
        D(bug("Set LF mode OFF\n"));
        /* ICU(o)->conFlags &= ~CF_LF_MODE_ON; */
        CLEAR_MODE(o, M_LNM);
        break;

    case C_SET_AUTOSCROLL_MODE:
        SET_MODE(o, PMB_ASM);
        break;

    case C_RESET_AUTOSCROLL_MODE:
        CLEAR_MODE(o, PMB_ASM);
        break;

    case C_SET_AUTOWRAP_MODE:
        SET_MODE(o, PMB_AWM);
        break;

    case C_RESET_AUTOWRAP_MODE:
        CLEAR_MODE(o, PMB_AWM);
        break;

    case C_RESET_FONT:
        ICU(o)->fontSetOK = FALSE;
        if (ICU(o)->fontWindow) {
            console_install_font(cl, o, NULL);
            ICU(o)->fontZoomActive = FALSE;
        }
        break;

    case C_ZOOM_FONT:
        console_zoom_font(cl,o,msg->Params[0]);
        break;

    case C_SET_FONT:
        ICU(o)->fontSetOK = FALSE;
        /* OSC 50: live font switch - open the font, install it on the
           window RastPort, re-derive the cell grid and let the class
           refresh from its backing store */
        {
            STRPTR name = (STRPTR) msg->Params[0];
            UWORD namelen = strlen(name);
            struct Library *GfxBase;
            struct Library *DiskfontBase;
            UBYTE fullname[CON_OSCBUF_SIZE + 8];

            GfxBase = TaggedOpenLibrary(TAGGEDOPEN_GRAPHICS);
            DiskfontBase = OpenLibrary("diskfont.library", 0);

            if (GfxBase && DiskfontBase)
            {
                struct TextAttr ta;
                struct TextFont *tf;

                strcpy(fullname, name);
                if (namelen < 5
                    || strnicmp(fullname + namelen - 5, ".font", 5))
                    strcpy(fullname + namelen, ".font");

                ta.ta_Name = fullname;
                ta.ta_YSize = msg->Params[1] ?
                    msg->Params[1] : CU(o)->cu_YRSize;
                ta.ta_Style = FS_NORMAL;
                ta.ta_Flags = 0;

                tf = OpenDiskFont(&ta);
                D(bug("[console] OSC50 font %s,%d -> %p\n", fullname,
                        ta.ta_YSize, tf));

                if (tf)
                {
                    if (tf->tf_Flags & FPF_PROPORTIONAL)
                    {
                        /* fixed cell grid requires fixed-width fonts */
                        CloseFont(tf);
                    }
                    else
                    {
                        console_install_font(cl, o, tf);
                        ICU(o)->fontZoomActive = FALSE;
                    }
                }
            }
            if (DiskfontBase)
                CloseLibrary(DiskfontBase);
        }
        break;

    case C_PUSH_TITLE:
    case C_POP_TITLE:
    case C_CLEAR_TITLE_STACK:
        console_title_stack(cl, o, msg->Command);
        break;

    case C_SET_TITLE:
        /* OSC 0/2: remember the title; the active (visible) unit also
           drives the window title, GNOME-terminal style */
        {
            const UBYTE *t = (const UBYTE *) msg->Params[0];
            WORD n = 0;

            /* Intuition draws titles in ISO-8859-1: decode a UTF-8 title
               and leave out what that cannot show (a status glyph, with
               the space that separated it from the text) */
            while (*t && n < CON_TITLE_SIZE - 1)
            {
                ULONG c = *t++;

                if (CON_IS_UTF8(o) && c >= 0x80)
                {
                    WORD more = (c >= 0xF0) ? 3 : (c >= 0xE0) ? 2 :
                        (c >= 0xC0) ? 1 : 0;

                    c &= (more == 3) ? 0x07 : (more == 2) ? 0x0F : 0x1F;
                    while (more-- && (*t & 0xC0) == 0x80)
                        c = (c << 6) | (*t++ & 0x3F);
                }
                if (c > 0xFF || c < 0x20)
                    continue;
                if (c == ' ' && n == 0)
                    continue;
                ICU(o)->unitTitle[n++] = (UBYTE) c;
            }
            ICU(o)->unitTitle[n] = '\0';

            if (!CON_IS_OBSCURED(o))
                console_publish_title(cl, o);
        }
        break;

    case C_SET_OBSCURED:
        D(bug("[console] unit %p OBSCURED\n", o));
        ICU(o)->conFlags |= CF_OBSCURED;
        break;

    case C_RESET_OBSCURED:
        D(bug("[console] unit %p REVEALED\n", o));
        /* Restore the unit's owned font or the shared caller baseline. */
        console_restore_font(cl, o);
        ICU(o)->conFlags &= ~CF_OBSCURED;
        console_publish_title(cl, o);
        break;

    case C_SET_UTF8_MODE:
        D(bug("Set UTF-8 mode ON\n"));
        ICU(o)->conFlags |= CF_UTF8;
        ICU(o)->utf8StashLen = 0;
        break;

    case C_RESET_UTF8_MODE:
        D(bug("Set UTF-8 mode OFF\n"));
        ICU(o)->conFlags &= ~CF_UTF8;
        ICU(o)->utf8StashLen = 0;
        break;

    case C_SELECT_GRAPHIC_RENDITION:
        D(bug("Select graphic Rendition, params=%d\n", msg->NumParams));
        {
            UBYTE i;
            IPTR param;

            for (i = 0; i < msg->NumParams; i++)
            {
                param = msg->Params[i];
                D(bug("param%d=%d\n", i, param));

                /* Consume extended colors in reference iterator order. Missing
                 * components cannot leak back into SGR; out-of-range values
                 * are rejected, not wrapped to another palette entry/channel. */
                if (param == 38 || param == 48 || param == 58)
                {
                    UBYTE *penptr = param == 38 ? (UBYTE *)&CU(o)->cu_FgPen : (UBYTE *)&CU(o)->cu_BgPen;
                    if (i + 1 >= msg->NumParams) continue;
                    IPTR format = msg->Params[++i];
                    if (format == 5 && i + 1 < msg->NumParams)
                    {
                        IPTR index = msg->Params[++i];
                        if (index >= 0 && index <= 255)
                        {
                            ULONG color = CONPEN_XTERM(index);
                            if (param == 58) ICU(o)->underlineColor = color + 1;
                            else Console_GetColorPen(o, color, penptr);
                        }
                    }
                    else if (format == 2)
                    {
                        IPTR rgb[3] = {0, 0, 0};
                        BOOL valid = TRUE;
                        for (UBYTE channel = 0; channel < 3; channel++)
                        {
                            if (i + 1 >= msg->NumParams) { valid = FALSE; break; }
                            rgb[channel] = msg->Params[++i];
                            if (rgb[channel] < 0 || rgb[channel] > 255)
                            { valid = FALSE; break; }
                        }
                        if (valid)
                        {
                            ULONG color = CONPEN_RGB(rgb[0], rgb[1], rgb[2]);
                            if (param == 58) ICU(o)->underlineColor = color + 1;
                            else Console_GetColorPen(o, color, penptr);
                        }
                    }
                    continue;
                }

                /* Bright ANSI: xterm palette entries 8-15 */
                if (param >= 90 && param <= 97)
                {
                    Console_GetColorPen(o, CONPEN_BRIGHT(param - 90),
                        &CU(o)->cu_FgPen);
                    continue;
                }
                if (param >= 100 && param <= 107)
                {
                    Console_GetColorPen(o, CONPEN_BRIGHT(param - 100),
                        &CU(o)->cu_BgPen);
                    continue;
                }

                switch (param)
                {
                case 59:
                    ICU(o)->underlineColor = 0;
                    break;
                case 0:
                    ICU(o)->underlineColor = 0;
                    ICU(o)->underlineStyle = 0;
                    Console_GetColorPen(o, CONUNIT_PEN_DEFBG,
                        &CU(o)->cu_BgPen);
                    Console_GetColorPen(o, CONUNIT_PEN_DEFFG,
                        &CU(o)->cu_FgPen);
                    CU(o)->cu_TxFlags = 0;
                    break;
                case 1:
                    CU(o)->cu_TxFlags |= CON_TXTFLAGS_BOLD;
                    break;
                case 2:
                    CU(o)->cu_TxFlags |= CON_TXTFLAGS_FAINT;
                    break;
                case 3:
                    CU(o)->cu_TxFlags |= CON_TXTFLAGS_ITALIC;
                    break;
                case -3: case -4: case -5:
                    ICU(o)->underlineStyle = -param - 2;
                    CU(o)->cu_TxFlags &= ~CON_TXTFLAGS_DOUBLEUNDERLINED;
                    CU(o)->cu_TxFlags |= CON_TXTFLAGS_UNDERLINED;
                    break;
                case 4:
                    ICU(o)->underlineStyle = 0;
                    CU(o)->cu_TxFlags &= ~CON_TXTFLAGS_DOUBLEUNDERLINED;
                    CU(o)->cu_TxFlags |= CON_TXTFLAGS_UNDERLINED;
                    break;
                case 21:
                    ICU(o)->underlineStyle = 0;
                    CU(o)->cu_TxFlags &= ~CON_TXTFLAGS_UNDERLINED;
                    CU(o)->cu_TxFlags |= CON_TXTFLAGS_DOUBLEUNDERLINED;
                    break;
                case 7:
                    CU(o)->cu_TxFlags |= CON_TXTFLAGS_REVERSED;
                    break;
                case 8:
                    CU(o)->cu_TxFlags |= CON_TXTFLAGS_CONCEALED;
                    break;
                case 9:
                    CU(o)->cu_TxFlags |= CON_TXTFLAGS_STRUCK;
                    break;
                case 29:
                    CU(o)->cu_TxFlags &= ~CON_TXTFLAGS_STRUCK;
                    break;
                case 22:
                    CU(o)->cu_TxFlags &= ~(CON_TXTFLAGS_BOLD | CON_TXTFLAGS_FAINT);
                    break;
                case 23:
                    CU(o)->cu_TxFlags &= ~CON_TXTFLAGS_ITALIC;
                    break;
                case 24:
                    ICU(o)->underlineStyle = 0;
                    CU(o)->cu_TxFlags &= ~(CON_TXTFLAGS_UNDERLINED | CON_TXTFLAGS_DOUBLEUNDERLINED);
                    break;
                case 27:
                    CU(o)->cu_TxFlags &= ~CON_TXTFLAGS_REVERSED;
                    break;
                case 28:
                    CU(o)->cu_TxFlags &= ~CON_TXTFLAGS_CONCEALED;
                    break;
                case 30:
                case 31:
                case 32:
                case 33:
                case 34:
                case 35:
                case 36:
                case 37:
                    Console_GetColorPen(o, param - 30, &CU(o)->cu_FgPen);
                    break;
                case 39:
                    Console_GetColorPen(o, CONUNIT_PEN_DEFFG,
                        &CU(o)->cu_FgPen);
                    break;

                case 40:
                case 41:
                case 42:
                case 43:
                case 44:
                case 45:
                case 46:
                case 47:
                    Console_GetColorPen(o, param - 40, &CU(o)->cu_BgPen);
                    break;

                case 49:
                    Console_GetColorPen(o, CONUNIT_PEN_DEFBG,
                        &CU(o)->cu_BgPen);
                    break;
                } /* switch(param) */
            } /* for(i = 0; i < msg->NumParams; i++) */
        }
        break;

    case C_SET_RAWEVENTS:
        D(bug("Set Raw Events\n"));
        {
            UBYTE i, param;

            for (i = 0; i < msg->NumParams; i++)
            {
                param = msg->Params[i];

                if (param <= IECLASS_MAX)
                {
                    SET_RAWEVENT(o, param);
                }

            } /* for(i = 0; i < msg->NumParams; i++) */
        }
        break;

    case C_RESET_RAWEVENTS:
        D(bug("Set Raw Events\n"));
        {
            UBYTE i, param;

            for (i = 0; i < msg->NumParams; i++)
            {
                param = msg->Params[i];

                if (param <= IECLASS_MAX)
                {
                    RESET_RAWEVENT(o, param);
                }

            } /* for(i = 0; i < msg->NumParams; i++) */
        }
        break;
    } /* switch (msg->Command) */

    ReturnVoid("Console::DoCommand");
}

/**********************************
**  Console::GetDefaultParams()  **
**********************************/
static VOID console_getdefaultparams(Class *cl, Object *o,
    struct P_Console_GetDefaultParams *msg)
{
    switch (msg->Command)
    {
    case C_REPEAT_CHAR:
    case C_INSERT_CHAR:
    case C_CURSOR_UP:
    case C_CURSOR_DOWN:
    case C_CURSOR_FORWARD:
    case C_CURSOR_BACKWARD:
    case C_CURSOR_NEXT_LINE:
    case C_CURSOR_PREV_LINE:
    case C_CURSOR_COLUMN:
    case C_CURSOR_ROW:
    case C_INSERT_LINE:
    case C_DELETE_LINE:
    case C_SET_TOP_OFFSET:
    case C_SET_PAGE_LENGTH:
        msg->Params[0] = 1;
        break;
    case C_CURSOR_POS:
        /* xterm CUP defaults to the home position, not the current cell.
           Full-screen clients commonly send ESC [ H before repainting. */
        msg->Params[0] = 1;
        msg->Params[1] = 1;
        break;

/* FIXME: Autodocs state commands in between here, has params RKRM: Devs saye the do not */
    case C_CURSOR_HTAB:
    case C_DELETE_CHAR:
    case C_ERASE_CHAR:
    case C_SCROLL_UP:
    case C_SCROLL_DOWN:
        msg->Params[0] = 1;
        break;

    case C_CLEAR_TABS:
    case C_CURSOR_TAB_CTRL:
        msg->Params[0] = 0;     /* set tab */
        break;

    case C_CURSOR_BACKTAB:
        msg->Params[0] = 1;
        break;

    case C_ERASE_IN_DISPLAY:
    case C_ERASE_IN_LINE:
        msg->Params[0] = 0;
        break;

    case C_SELECT_GRAPHIC_RENDITION:
        /* don't do anything, as params may be in any order */
        break;

    case C_SET_RAWEVENTS:
        /* don't do anything, as params may be in any order */
        break;

    case C_RESET_RAWEVENTS:
        /* don't do anything, as params may be in any order */
        break;
    } /* switch (msg->Command) */

    return;
}

/*******************************
**  Console::NewWindowSize()  **
*******************************/
static VOID console_newwindowsize(Class *cl, Object *o,
    struct P_Console_NewWindowSize *msg)
{
    struct ConUnit *unit;
    struct consoledata *data;
    struct Window *win;

    data = INST_DATA(cl, o);
    unit = (struct ConUnit *)data;

    win = unit->cu_Window;

    unit->cu_XMax =
        (win->Width - win->BorderRight -
        unit->cu_XROrigin) / unit->cu_XRSize - 1;
    unit->cu_YMax =
        (win->Height - win->BorderBottom -
        unit->cu_YROrigin) / unit->cu_YRSize - 1;

    unit->cu_XRExtant =
        unit->cu_XROrigin + (unit->cu_XRSize * (unit->cu_XMax + 1) - 1);
    unit->cu_YRExtant =
        unit->cu_YROrigin + (unit->cu_YRSize * (unit->cu_YMax + 1) - 1);

    if (unit->cu_XCCP > unit->cu_XMax)
        unit->cu_XCCP = unit->cu_XMax;
    if (unit->cu_YCCP > unit->cu_YMax)
        unit->cu_YCCP = unit->cu_YMax;

    unit->cu_XCP = unit->cu_XCCP;
    unit->cu_YCP = unit->cu_YCCP;

    return;
}

IPTR console_getpencolor(Class *cl, Object *o, struct P_Console_GetColorPen *msg)
{
    struct consoledata *data = INST_DATA(cl, o);
    if (msg->PenPtr == (UBYTE *)&CU(o)->cu_FgPen) ICU(o)->fgColor = msg->ColorIdx;
    if (msg->PenPtr == (UBYTE *)&CU(o)->cu_BgPen) ICU(o)->bgColor = msg->ColorIdx;

    /* extended (xterm/truecolor) indices need the stdcon colormap
       machinery - fall back to the default text pen here */
    if (msg->ColorIdx < CONUNIT_PEN_MAX)
        *msg->PenPtr = (UBYTE)data->intunit.pens[msg->ColorIdx];
    else
        *msg->PenPtr = (UBYTE)data->intunit.pens[CONUNIT_PEN_DEFFG];
    return TRUE;
}

static VOID console_dispose(Class *cl, Object *o, Msg msg)
{
    struct intPasteData *paste;
    /* Close has excluded the console task and unlinked the unit. Partial
     * pastes are owned by this unit, not by the last reader that consumed it. */
    while ((paste = (struct intPasteData *)RemHead((struct List *)&ICU(o)->pasteData)))
    {
        FreeMem(paste->pasteBuffer, paste->pasteAllocationSize);
        FreeMem(paste, sizeof(*paste));
    }
    console_title_stack(cl, o, C_CLEAR_TITLE_STACK);
    ICU(o)->pasteBufferPos = 0;
    console_detach_font_window(ConsoleDevice, o);
    ConsolengCloseFont(ConsoleDevice, ICU(o)->setFont);
    ICU(o)->setFont = NULL;

    DoSuperMethodA(cl, o, msg);
}

/********* Console class dispatcher **********************************/
AROS_UFH3S(IPTR, dispatch_consoleclass,
    AROS_UFHA(Class *, cl, A0),
    AROS_UFHA(Object *, o, A2), AROS_UFHA(Msg, msg, A1))
{
    AROS_USERFUNC_INIT

    IPTR retval = 0UL;

    switch (msg->MethodID)
    {
    case M_Console_GetColorPen:
        retval = (IPTR) console_getpencolor(cl, o, (struct P_Console_GetColorPen *)msg);
        break;

    case OM_NEW:
        retval = (IPTR) console_new(cl, o, (struct opSet *)msg);
        break;

    case OM_DISPOSE:
        console_dispose(cl, o, msg);
        break;

    case M_Console_CursorSpan:
        retval = 1; /* Plain units have no retained wide-cell metadata. */
        break;

    case M_Console_Left:
        console_left(cl, o, (struct P_Console_Left *)msg);
        break;

    case M_Console_Right:
        console_right(cl, o, (struct P_Console_Right *)msg);
        break;

    case M_Console_Up:
        console_up(cl, o, (struct P_Console_Up *)msg);
        break;

    case M_Console_Down:
        console_down(cl, o, (struct P_Console_Down *)msg);
        break;

    case M_Console_DoCommand:
        console_docommand(cl, o, (struct P_Console_DoCommand *)msg);
        break;

    case M_Console_GetDefaultParams:
        console_getdefaultparams(cl, o,
            (struct P_Console_GetDefaultParams *)msg);
        break;

    case M_Console_NewWindowSize:
        console_newwindowsize(cl, o, (struct P_Console_NewWindowSize *)msg);
        break;

    default:
        retval = DoSuperMethodA(cl, o, msg);
        break;
    }
    return (retval);

    AROS_USERFUNC_EXIT
}


/************************
**  normalizecoords()  **
************************/

/* Normalizes "out of window" coords, so that they
   fit into the window
*/

#define ABS(a) (((a) < 0) ? -(a) : (a))


#undef ConsoleDevice
#define ConsoleDevice ((struct ConsoleBase *)OCLASS(o)->cl_UserData)

/* FIXME: Currently dead code */
VOID normalizecoords(Object *o, WORD *x_ptr, WORD *y_ptr)
{
    EnterFunc(bug("normalizecoords(o=%p, x=%d, y=%d)\n",
            o, *x_ptr, *y_ptr));

    if (*x_ptr > CU(o)->cu_XMax)        /* charpos too far to the right */
    {
        D(bug("Pos right of window\n"));
        /* Increase y */
        Console_Down(o, *x_ptr / CHAR_XMAX(o));

        /* Normalize x */
        *x_ptr = *x_ptr % (CHAR_XMAX(o) - CHAR_XMIN(o));
    }
    else if (*x_ptr < CHAR_XMIN(o))
    {
        D(bug("Pos left of window\n"));

        /* Decrease y */
        Console_Up(o, ABS(*x_ptr) / CHAR_XMAX(o) - CHAR_XMIN(o));

        /* Normalize Z */
        *x_ptr = *x_ptr % (CHAR_XMAX(o) - CHAR_XMIN(o));
    }

    if (*y_ptr > CHAR_YMAX(o))  /* pos below window bounds */
    {

        *y_ptr = CHAR_YMAX(o);
    }
    else if (*y_ptr < CHAR_YMIN(o))  /* pos above window bounds */
    {
        UBYTE scroll_param = CHAR_YMIN(o) - *y_ptr;

        D(bug("Pos above window\n"));

        Console_DoCommand(o, C_SCROLL_DOWN, 1, &scroll_param);

        *y_ptr = CHAR_YMIN(o);
    }

    ReturnVoid("normalizecoords");
}


#undef ConsoleDevice

Class *makeConsoleClass(struct ConsoleBase *ConsoleDevice)
{
    Class *cl;

    cl = MakeClass(NULL, ROOTCLASS, NULL, sizeof(struct consoledata), 0UL);
    if (cl)
    {
        cl->cl_Dispatcher.h_Entry = (APTR) dispatch_consoleclass;
        cl->cl_Dispatcher.h_SubEntry = NULL;

        cl->cl_UserData = (IPTR) ConsoleDevice;

        return cl;
    }
    return NULL;
}
