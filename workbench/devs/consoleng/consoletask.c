/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc: Code executed by the console.device task.
*/

#define DEBUG 0
#include <aros/debug.h>

#include <exec/lists.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/console.h>
#include <exec/io.h>
#include <exec/alerts.h>
#include <exec/rawfmt.h>
#include <dos/dos.h>
#include <devices/input.h>
#include <devices/rawkeycodes.h>

#include <string.h>

#include "console_gcc.h"
#include "consoleif.h"

/* The unit generation must be validated before this is called. Keep local
 * console commands out of both raw reports and converted application input.
 * Non-key events (notably close/resize/focus/timer) must continue normally. */
static BOOL con_filter_local_input(Object *unit, struct InputEvent *event)
{
    return event->ie_Class == IECLASS_RAWKEY && Console_FilterInput(unit, event);
}

/* Queue one complete injection. Clipboard framing is opt-in here; terminal
 * replies and the ConClip request spelling must always remain unframed. */
static VOID con_queue_data(struct ConsoleBase *ConsoleDevice, struct ConUnit *cu,
    const UBYTE *data, LONG size, BOOL clipboard)
{
    struct intConUnit *icu = ICU(cu);
    ObtainSemaphore(&ConsoleDevice->consoleTaskLock);
    if (data)
    {
        size_t length = size < 0 ? strlen((const char *)data) : (size_t)size;
        BOOL bracketed = clipboard && (icu->conFlags & CF_BRACKETED_PASTE);
        if ((length || bracketed) && length <= 0x7fffffffUL - (bracketed ? 12 : 0))
        {
            struct intPasteData *p = AllocMem(sizeof(*p), MEMF_ANY);
            if (p)
            {
                ULONG capacity = length + (bracketed ? 12 : 0);
                p->pasteBuffer = AllocMem(capacity, MEMF_ANY);
                if (p->pasteBuffer)
                {
                    ULONG used = 0, i;
                    if (bracketed)
                    {
                        CopyMem("\x1b[200~", p->pasteBuffer, 6);
                        used = 6;
                        for (i = 0; i < length; i++)
                            if (data[i] != 27 && data[i] != 3)
                                p->pasteBuffer[used++] = data[i];
                        CopyMem("\x1b[201~", p->pasteBuffer + used, 6);
                        used += 6;
                    }
                    else
                    {
                        CopyMem((APTR)data, p->pasteBuffer, length);
                        used = length;
                    }
                    p->pasteBufferSize = used;
                    p->pasteAllocationSize = capacity;
                    p->unit = icu;
                    AddTail((struct List *)&icu->pasteData, (struct Node *)p);
                }
                else FreeMem(p, sizeof(*p));
            }
        }
    }
    ReleaseSemaphore(&ConsoleDevice->consoleTaskLock);
    Signal(ConsoleDevice->consoleTask, SIGBREAKF_CTRL_D);
}

VOID con_inject(struct ConsoleBase *ConsoleDevice, struct ConUnit *cu,
    const UBYTE *data, LONG size)
{
    con_queue_data(ConsoleDevice, cu, data, size, FALSE);
}

VOID con_inject_paste(struct ConsoleBase *ConsoleDevice, struct ConUnit *cu,
    const UBYTE *data, LONG size)
{
    con_queue_data(ConsoleDevice, cu, data, size, TRUE);
}

/* Protos */
/* Translate only unmodified cursor-key sequences. Modified CSI sequences
 * and ordinary text retain their keymap spelling. The buffer has room for ESC O. */
static LONG con_application_cursor(UBYTE *bytes, LONG length, ULONG flags)
{
    LONG final;
    if (!(flags & CF_APPLICATION_CURSOR))
        return length;
    if (length == 2 && bytes[0] == 0x9b)
        final = 1;
    else if (length == 3 && bytes[0] == 0x1b && bytes[1] == '[')
        final = 2;
    else
        return length;
    if (!strchr("ABCDHF", bytes[final]) || bytes[final] == 0)
        return length;
    UBYTE key = bytes[final];
    bytes[0] = 0x1b; bytes[1] = 'O'; bytes[2] = key;
    return 3;
}

/* UTF-8 applications use xterm modifiers for navigation and function keys.
 * Classic mode, Amiga command shortcuts, releases and unmodified keymap
 * output remain untouched. The caller supplies at least seven writable bytes. */
static LONG con_modified_navigation(UBYTE *bytes, LONG length,
    const struct InputEvent *event, BOOL utf8)
{
    ULONG q = event->ie_Qualifier;
    UBYTE modifier = 1, base = '1', final, tens = 0;
    if (!utf8 || event->ie_Class != IECLASS_RAWKEY ||
        (event->ie_Code & IECODE_UP_PREFIX) ||
        (q & (IEQUALIFIER_LCOMMAND | IEQUALIFIER_RCOMMAND)))
        return length;
    if (q & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)) modifier += 1;
    if (q & (IEQUALIFIER_LALT | IEQUALIFIER_RALT)) modifier += 2;
    if (q & IEQUALIFIER_CONTROL) modifier += 4;
    if (modifier == 1) return length;
    switch (event->ie_Code)
    {
    case RAWKEY_UP: final = 'A'; break;
    case RAWKEY_DOWN: final = 'B'; break;
    case RAWKEY_RIGHT: final = 'C'; break;
    case RAWKEY_LEFT: final = 'D'; break;
    case RAWKEY_HOME: final = 'H'; break;
    case RAWKEY_END: final = 'F'; break;
    case RAWKEY_INSERT: base = '2'; final = '~'; break;
    case RAWKEY_DELETE: base = '3'; final = '~'; break;
    case RAWKEY_PAGEUP: base = '5'; final = '~'; break;
    case RAWKEY_PAGEDOWN: base = '6'; final = '~'; break;
    case RAWKEY_F1: final = 'P'; break;
    case RAWKEY_F2: final = 'Q'; break;
    case RAWKEY_F3: final = 'R'; break;
    case RAWKEY_F4: final = 'S'; break;
    case RAWKEY_F5: tens = '1'; base = '5'; final = '~'; break;
    case RAWKEY_F6: tens = '1'; base = '7'; final = '~'; break;
    case RAWKEY_F7: tens = '1'; base = '8'; final = '~'; break;
    case RAWKEY_F8: tens = '1'; base = '9'; final = '~'; break;
    case RAWKEY_F9: tens = '2'; base = '0'; final = '~'; break;
    case RAWKEY_F10: tens = '2'; base = '1'; final = '~'; break;
    case RAWKEY_F11: tens = '2'; base = '3'; final = '~'; break;
    case RAWKEY_F12: tens = '2'; base = '4'; final = '~'; break;
    default: return length;
    }
    LONG out = 0;
    bytes[out++] = 27; bytes[out++] = '[';
    if (tens) bytes[out++] = tens;
    bytes[out++] = base; bytes[out++] = ';';
    bytes[out++] = '0' + modifier; bytes[out++] = final;
    return out;
}

static BOOL checkconunit(Object *unit, UQUAD generation, struct ConsoleBase *ConsoleDevice);
static void answer_read_request(struct IOStdReq *req,
    struct ConsoleBase *ConsoleDevice);

static VOID answer_requests(APTR unit, struct ConsoleBase *ConsoleDevice)
{
    struct IOStdReq *req, *nextreq;
    /* See if there are any queued io read requests that wants to be replied */
    ForeachNodeSafe(&ConsoleDevice->readRequests, req, nextreq)
    {
        if ((APTR) req->io_Unit == (APTR) unit)
        {
            /* Paranoia */
            if (0 != ICU(req->io_Unit)->numStoredChars)
            {
                Remove((struct Node *)req);
                answer_read_request(req, ConsoleDevice);
            }
        }
    }
}

static BOOL report_raw_event(Object *unit, const struct InputEvent *event,
    struct ConsoleBase *ConsoleDevice)
{
    struct intConUnit *icu = ICU(unit);
    UBYTE report[128];
    LONG fields[8];
    LONG x, y;
    ULONG len;

    if (!CHECK_RAWEVENT(unit, event->ie_Class))
        return FALSE;

    /* The classic console.device reports mouse button transitions but not
     * IECODE_NOBUTTON movement events through this raw-event byte stream.
     * Consumers such as Ed read the mouse coordinates maintained by
     * Intuition when processing the press and release reports. */
    if (event->ie_Class == IECLASS_RAWMOUSE &&
        event->ie_Code == IECODE_NOBUTTON)
        return TRUE;

    if (event->ie_Class == IECLASS_RAWKEY)
    {
        x = ((ULONG)event->ie_Prev1DownCode << 8) |
            event->ie_Prev1DownQual;
        y = ((ULONG)event->ie_Prev2DownCode << 8) |
            event->ie_Prev2DownQual;
    }
    else
    {
        x = event->ie_X;
        y = event->ie_Y;
    }

    fields[0] = event->ie_Class;
    fields[1] = event->ie_SubClass;
    fields[2] = event->ie_Code;
    fields[3] = event->ie_Qualifier;
    fields[4] = x;
    fields[5] = y;
    fields[6] = event->ie_TimeStamp.tv_secs;
    fields[7] = event->ie_TimeStamp.tv_micro;

    /* RawDoFmt's explicit data stream is ABI-safe on m68k.  Passing this
     * many values through NewRawDoFmt's varargs interface corrupts the
     * stream there, producing more than the eight fields mandated by the
     * console.device input-event report protocol. */
    RawDoFmt("\x9b%ld;%ld;%ld;%ld;%ld;%ld;%ld;%ld|", (RAWARG)fields,
        RAWFMTFUNC_STRING, report);
    len = strnlen(report, sizeof(report));
    if (len == sizeof(report))
        return TRUE;

    if (len <= CON_INPUTBUF_SIZE - icu->numStoredChars)
    {
        CopyMem(report, icu->inputBuf + icu->numStoredChars, len);
        icu->numStoredChars += len;
        answer_requests(unit, ConsoleDevice);
    }

    return TRUE;
}

/* Application-negotiated focus reports share the normal input queue. The
 * caller gives classic Amiga raw-event requests precedence. */
static void report_focus_event(Object *unit, const struct InputEvent *event,
    struct ConsoleBase *ConsoleDevice)
{
    struct intConUnit *icu = ICU(unit);
    UBYTE report[3] = {27, '[', 'O'};

    if (!(icu->conFlags & CF_FOCUS_REPORTING) ||
        (event->ie_Class != IECLASS_ACTIVEWINDOW &&
         event->ie_Class != IECLASS_INACTIVEWINDOW))
        return;

    if (event->ie_Class == IECLASS_ACTIVEWINDOW)
        report[2] = 'I';
    if (sizeof(report) <= CON_INPUTBUF_SIZE - icu->numStoredChars)
    {
        CopyMem(report, icu->inputBuf + icu->numStoredChars, sizeof(report));
        icu->numStoredChars += sizeof(report);
        answer_requests(unit, ConsoleDevice);
    }
}

/* Mode1007 is opt-in for native CON compatibility. Mouse protocols and
 * classic raw events have priority; Shift keeps native history scrolling. */
static BOOL report_alternate_scroll(Object *unit, const struct InputEvent *event,
    struct ConsoleBase *ConsoleDevice)
{
    struct intConUnit *icu = ICU(unit);
    ULONG alternate = 0;
    IPTR params[] = {TRUE, 1049, (IPTR)&alternate};
    if (!(icu->conFlags & CF_ALTERNATE_SCROLL) || icu->mouseMode ||
        CON_IS_OBSCURED(unit) || !CU(unit)->cu_Window ||
        event->ie_Class != IECLASS_RAWKEY ||
        (event->ie_Qualifier & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT)) ||
        (event->ie_Code < RAWKEY_NM_WHEEL_UP || event->ie_Code > RAWKEY_NM_WHEEL_RIGHT))
        return FALSE;
    Console_DoCommand(unit, C_GET_MODE_STATE, 3, params);
    if (alternate != 1) return FALSE;
    ULONG count=icu->wheelLines;
    ULONG available=icu->numStoredChars<=CON_INPUTBUF_SIZE?
        (CON_INPUTBUF_SIZE-icu->numStoredChars)/3:0;
    if(count>available)count=available;
    if(count){
        for(ULONG i=0;i<count;i++){
            UBYTE *out=icu->inputBuf+icu->numStoredChars;
            out[0]=27;out[1]='O';
            out[2]="ABDC"[event->ie_Code-RAWKEY_NM_WHEEL_UP];
            icu->numStoredChars+=3;
        }
        answer_requests(unit,ConsoleDevice);
    }
    return TRUE;
}

/* Classic raw-event consumers run first. Shift keeps native selection and
 * wheel scrolling even while a terminal application requests mouse reports. */
static BOOL report_mouse_event(Object *unit, const struct InputEvent *event,
    LONG px, LONG py, struct ConsoleBase *ConsoleDevice)
{
    struct intConUnit *icu=ICU(unit);
    struct ConUnit *cu=CU(unit);
    struct Window *w=cu->cu_Window;
    BOOL wheel=event->ie_Class==IECLASS_RAWKEY &&
        (event->ie_Code>=RAWKEY_NM_WHEEL_UP && event->ie_Code<=RAWKEY_NM_WHEEL_RIGHT);
    BOOL motion=event->ie_Class==IECLASS_RAWMOUSE && event->ie_Code==IECODE_NOBUTTON;
    BOOL release=FALSE;
    ULONG button, len, captureBit=0;
    BOOL captured=FALSE;
    LONG x,y;
    UBYTE report[40];
    /* Only a press delivered to the application owns an outside release.
     * Native border gadgets and Shift selection retain their own events. */
    if (event->ie_Class==IECLASS_RAWMOUSE && !motion)
    {
        switch (event->ie_Code & ~IECODE_UP_PREFIX)
        {
        case IECODE_LBUTTON: captureBit=1; break;
        case IECODE_MBUTTON: captureBit=2; break;
        case IECODE_RBUTTON: captureBit=4; break;
        }
        release=!!(event->ie_Code & IECODE_UP_PREFIX);
        captured=release && (icu->mouseReportedButtons & captureBit);
        if (release) icu->mouseReportedButtons &= ~captureBit;
    }
    /* Intuition retains raw mouse events for lower-priority handlers even
     * when a native menu/requester owns them. Do not duplicate its right
     * button gesture into the terminal unless the application traps it.
     * A press already delivered to the terminal still owns its release. */
    if (!captured && w && !(w->Flags & WFLG_RMBTRAP) &&
        (w->MenuStrip || w->DMRequest) &&
        (captureBit == 4 || (motion && (event->ie_Qualifier & IEQUALIFIER_RBUTTON))))
        return FALSE;
    if (!icu->mouseMode || !w || CON_IS_OBSCURED(unit) ||
        (event->ie_Qualifier & (IEQUALIFIER_LSHIFT|IEQUALIFIER_RSHIFT)) ||
        (event->ie_Class!=IECLASS_RAWMOUSE && !wheel)) return FALSE;
    if (w->Flags & WFLG_GIMMEZEROZERO) { px-=w->BorderLeft; py-=w->BorderTop; }
    px-=cu->cu_XROrigin; py-=cu->cu_YROrigin;
    if (cu->cu_XRSize<=0 || cu->cu_YRSize<=0 ||
        cu->cu_XMax<0 || cu->cu_YMax<0) return FALSE;
    if (!captured && (px<0 || py<0)) return FALSE;
    x=px<0 ? 0 : px/cu->cu_XRSize;
    y=py<0 ? 0 : py/cu->cu_YRSize;
    if (x>cu->cu_XMax || y>cu->cu_YMax)
    {
        if (!captured) return FALSE;
        if (x>cu->cu_XMax) x=cu->cu_XMax;
        if (y>cu->cu_YMax) y=cu->cu_YMax;
    }
    if (wheel) button=64+event->ie_Code-RAWKEY_NM_WHEEL_UP;
    else if (motion)
    {
        if (event->ie_Qualifier & IEQUALIFIER_LEFTBUTTON) button=0;
        else if (event->ie_Qualifier & IEQUALIFIER_MIDBUTTON) button=1;
        else if (event->ie_Qualifier & IEQUALIFIER_RBUTTON) button=2;
        else button=3;
        if (icu->mouseMode==1000 || (icu->mouseMode==1002 && button==3)) return TRUE;
        if (icu->mouseLastValid && x==icu->mouseLastX && y==icu->mouseLastY) return TRUE;
        button+=32;
    }
    else
    {
        switch (event->ie_Code & ~IECODE_UP_PREFIX)
        {
        case IECODE_LBUTTON: button=0; break;
        case IECODE_MBUTTON: button=1; break;
        case IECODE_RBUTTON: button=2; break;
        default: return FALSE;
        }
        release=!!(event->ie_Code & IECODE_UP_PREFIX);
    }
    if (event->ie_Qualifier & (IEQUALIFIER_LALT|IEQUALIFIER_RALT)) button|=8;
    if (event->ie_Qualifier & IEQUALIFIER_CONTROL) button|=16;
    len=con_mouse_encode(icu->mouseSGR,icu->mouseUTF8,button,release,x,y,report,sizeof(report));
    if (len && len<=CON_INPUTBUF_SIZE-icu->numStoredChars)
    {
        CopyMem(report,icu->inputBuf+icu->numStoredChars,len);
        icu->numStoredChars+=len;
        if (!release) icu->mouseReportedButtons |= captureBit;
        icu->mouseLastValid=TRUE; icu->mouseLastX=x; icu->mouseLastY=y;
        answer_requests(unit,ConsoleDevice);
    }
    return TRUE;
}

static ULONG pasteData(struct intConUnit *icu,
    struct ConsoleBase *ConsoleDevice)
{
    /* Check if we have data to paste to the input buffer */
    struct intPasteData *pd =
        (struct intPasteData *)GetHead(&icu->pasteData);
    if (pd)
    {
        ULONG tocopy =
            MIN(pd->pasteBufferSize - icu->pasteBufferPos,
            CON_INPUTBUF_SIZE - icu->numStoredChars);

        D(bug("Pasting %ld bytes\n", tocopy));
        CopyMem(pd->pasteBuffer + icu->pasteBufferPos,
            icu->inputBuf + icu->numStoredChars, tocopy);
        icu->numStoredChars += tocopy;
        icu->pasteBufferPos += tocopy;

        if (icu->pasteBufferPos >= pd->pasteBufferSize)
        {
            FreeMem(pd->pasteBuffer, pd->pasteAllocationSize);
            Remove((struct Node *)pd);
            FreeMem(pd, sizeof(struct intPasteData));
            icu->pasteBufferPos = 0;
        }

        answer_requests((APTR) icu, ConsoleDevice);
        return tocopy;
    }
    return 0;
}

VOID consoleTaskEntry(struct ConsoleBase *ConsoleDevice)
{
    BOOL success = FALSE;
    LONG waitsigs = 0L, wakeupsig, commandsig, inputsig;


    /* CD input handler puts data into this port */
    struct MsgPort *inputport;


    /* Used for input.device */
    struct MsgPort *inputmp;

    /* Add the CDInputHandler to input.device's list of input handlers */
    inputmp = CreateMsgPort();
    if (inputmp)
    {
        struct IOStdReq *inputio;
        inputio =
            (struct IOStdReq *)CreateIORequest(inputmp,
            sizeof(struct IOStdReq));
        if (inputio)
        {
            /* Open the input.device */
            if (!OpenDevice("input.device", -1, (struct IORequest *)inputio,
                    0UL))
            {
                /* Initialize the inputhandler itself */
                ConsoleDevice->inputHandler = initCDIH(ConsoleDevice);
                if (ConsoleDevice->inputHandler)
                {
                    inputio->io_Data = ConsoleDevice->inputHandler;
                    inputio->io_Command = IND_ADDHANDLER;

                    DoIO((struct IORequest *)inputio);
                    success = TRUE;
                }
                CloseDevice((struct IORequest *)inputio);

            }

            DeleteIORequest((struct IORequest *)inputio);
        }

        DeleteMsgPort(inputmp);
    }

    NEWLIST(&ConsoleDevice->readRequests);

    /* if not successful, throw an alert */
    if (!success)
    {
        Alert(AT_DeadEnd | AN_ConsoleDev | AG_OpenDev | AO_Unknown);
    }

    D(bug("Console task initialized\n"));

    /* Get console.device input handler's port */
    inputport = ConsoleDevice->consIHData.inputPort;

    inputsig = 1 << inputport->mp_SigBit;
    commandsig = 1 << ConsoleDevice->commandPort->mp_SigBit;

    waitsigs = inputsig | commandsig | SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_D |
        SIGBREAKF_CTRL_E;

    for (;;)
    {
        wakeupsig = Wait(waitsigs);

        /* Anyone wanting to kill us ? */
        if (wakeupsig & SIGBREAKF_CTRL_C)
        {
            break;
        }

        ObtainSemaphore(&ConsoleDevice->consoleTaskLock);

        if (wakeupsig & SIGBREAKF_CTRL_D)
        {
            con_sync_poll(ConsoleDevice);
            con_copy_clipboard_poll();
            con_osc_read_poll(ConsoleDevice, checkconunit);
            con_query_clipboard_poll(ConsoleDevice, checkconunit);
            struct IOStdReq *req;
            BOOL active = TRUE;
            D(bug("SIGBREAKF_CTRL_D\n"));
            /* console.device pasted data? */
            while (active)
            {
                active = FALSE;
                ForeachNode(&ConsoleDevice->readRequests, req)
                {
                    if (pasteData(ICU(req->io_Unit), ConsoleDevice))
                    {
                        answer_requests(NULL, ConsoleDevice);
                        active = TRUE;
                        break;
                    }
                }
            }
        }

        if (wakeupsig & inputsig)
        {
            /* A message from the console device input handler */
            struct cdihMessage *cdihmsg;

            while ((cdihmsg = (struct cdihMessage *)GetMsg(inputport)))
            {
                /* Check that the ConUnit has not been disposed,
                   while the message was passed
                 */
                if (checkconunit(cdihmsg->unit, cdihmsg->unitGeneration, ConsoleDevice) &&
                    !con_filter_local_input(cdihmsg->unit, &cdihmsg->ie))
                {
                    BOOL rawEvent = report_raw_event(cdihmsg->unit,
                        &cdihmsg->ie, ConsoleDevice);

                    BOOL mouseEvent = !rawEvent && report_mouse_event(cdihmsg->unit,
                        &cdihmsg->ie, cdihmsg->mouseX, cdihmsg->mouseY, ConsoleDevice);

                    if (!rawEvent)
                        report_focus_event(cdihmsg->unit, &cdihmsg->ie, ConsoleDevice);

                    switch (cdihmsg->ie.ie_Class)
                    {
                    case IECLASS_CLOSEWINDOW:
                        /* this is a hack. It would actually be up to the
                         * console.device user (CON: handler for example) to
                         * activate CLOSEWINDOW raw events (SET RAW EVENTS cmd)
                         * and then look out for this in the input stream
                         * (CMD_READ) */
                        /* fall through */

                    case IECLASS_RAWKEY:
                        {
#define MAPRAWKEY_BUFSIZE 80
/* Decimal associated-text fields expand every native byte to up to four. */
#define KEYINPUT_BUFSIZE (MAPRAWKEY_BUFSIZE * 4 + 32)

                            UBYTE inputBuf[KEYINPUT_BUFSIZE + 1];
                            LONG actual;
                            ULONG tocopy;
                            struct InputEvent e;

                            if (rawEvent || mouseEvent)
                                break;

                            if (cdihmsg->ie.ie_Class == IECLASS_RAWKEY) {
                                UBYTE *held = ICU(cdihmsg->unit)->bindingSwallowed;
                                UBYTE *original = ICU(cdihmsg->unit)->bindingHeldAction;
                                ULONG code = cdihmsg->ie.ie_Code & 127;
                                ULONG context=ICU(cdihmsg->unit)->keyboardAlternate?CONSOLENG_BIND_ALTSCREEN:0;
                                if(ICU(cdihmsg->unit)->conFlags&CF_APPLICATION_CURSOR)context|=CONSOLENG_BIND_APPCURSOR;
                                if(ICU(cdihmsg->unit)->conFlags&CF_APPLICATION_KEYPAD)context|=CONSOLENG_BIND_APPKEYPAD;
                                ULONG action = con_binding_action_in_context(&ICU(cdihmsg->unit)->bindings,
                                    &cdihmsg->ie, ConsoleDevice,context);
                                if (con_binding_release(held, &cdihmsg->ie)) {
                                    if (con_binding_repeat(original[code], action, &cdihmsg->ie)) {
                                        if (action == CONSOLENG_BIND_FONTLARGER || action == CONSOLENG_BIND_FONTSMALLER) {
                                            IPTR zoom = action;
                                            Console_DoCommand(cdihmsg->unit,C_ZOOM_FONT,1,&zoom);
                                        } else Console_HistoryAction(cdihmsg->unit, action);
                                    }
                                    if (cdihmsg->ie.ie_Code & IECODE_UP_PREFIX) original[code]=0;
                                    break;
                                }
                                original[code]=0;
                                BOOL handled = FALSE;
                                switch (action) {
                                case CONSOLENG_BIND_COPY:
                                    Console_Copy(cdihmsg->unit); handled=TRUE; break;
                                case CONSOLENG_BIND_PASTE:
                                    Console_Paste(cdihmsg->unit);
                                    if (ICU(cdihmsg->unit)->numStoredChars)
                                        answer_requests(cdihmsg->unit, ConsoleDevice);
                                    handled=TRUE; break;
                                case CONSOLENG_BIND_NAVCENTER:
                                case CONSOLENG_BIND_NAVOPENLINK:
                                case CONSOLENG_BIND_NAVSELECTCHAR:
                                case CONSOLENG_BIND_NAVSELECTLINE:
                                case CONSOLENG_BIND_NAVSELECTBLOCK:
                                case CONSOLENG_BIND_NAVSELECTSEMANTIC:
                                case CONSOLENG_BIND_NAVLEFT:
                                case CONSOLENG_BIND_NAVRIGHT:
                                case CONSOLENG_BIND_NAVUP:
                                case CONSOLENG_BIND_NAVDOWN:
                                case CONSOLENG_BIND_NAVFIRST:
                                case CONSOLENG_BIND_NAVLAST:
                                case CONSOLENG_BIND_NAVTOP:
                                case CONSOLENG_BIND_NAVBOTTOM:
                                case CONSOLENG_BIND_NAVWORDNEXT:
                                case CONSOLENG_BIND_NAVWORDEND:
                                case CONSOLENG_BIND_NAVWORDPREV:
                                case CONSOLENG_BIND_NAVSEMANTICNEXT:
                                case CONSOLENG_BIND_NAVSEMANTICEND:
                                case CONSOLENG_BIND_NAVSEMANTICPREV:
                                case CONSOLENG_BIND_NAVPARAGRAPHUP:
                                case CONSOLENG_BIND_NAVPARAGRAPHDOWN:
                                case CONSOLENG_BIND_NAVBRACKET:
                                case CONSOLENG_BIND_NAVOCCUPIED:
                                case CONSOLENG_BIND_NAVVIEWTOP:
                                case CONSOLENG_BIND_NAVVIEWMIDDLE:
                                case CONSOLENG_BIND_NAVVIEWBOTTOM:
                                case CONSOLENG_BIND_NAVPAGEUP:
                                case CONSOLENG_BIND_NAVPAGEDOWN:
                                case CONSOLENG_BIND_NAVHALFUP:
                                case CONSOLENG_BIND_NAVHALFDOWN:
                                case CONSOLENG_BIND_NAVLINEUP:
                                case CONSOLENG_BIND_NAVLINEDOWN:
                                case CONSOLENG_BIND_NAVFINDFORWARD:
                                case CONSOLENG_BIND_NAVFINDBACKWARD:
                                case CONSOLENG_BIND_NAVTILLFORWARD:
                                case CONSOLENG_BIND_NAVTILLBACKWARD:
                                case CONSOLENG_BIND_NAVFINDREPEAT:
                                case CONSOLENG_BIND_NAVFINDREVERSE:
                                case CONSOLENG_BIND_NAVSEARCHFORWARD:
                                case CONSOLENG_BIND_NAVSEARCHBACKWARD:
                                case CONSOLENG_BIND_NAVSEEDFORWARD:
                                case CONSOLENG_BIND_NAVSEEDBACKWARD:
                                case CONSOLENG_BIND_NAVSEARCHNEXT:
                                case CONSOLENG_BIND_NAVSEARCHPREVIOUS:
                                    /* Navigation handles these before ordinary dispatch. */
                                    handled=TRUE; break;
                                case CONSOLENG_BIND_SELECTALL:
                                    handled=Console_SelectAll(cdihmsg->unit); break;
                                case CONSOLENG_BIND_FONTLARGER:
                                case CONSOLENG_BIND_FONTSMALLER:
                                case CONSOLENG_BIND_FONTRESET:
                                    {
                                        IPTR zoom = action;
                                        Console_DoCommand(cdihmsg->unit,C_ZOOM_FONT,1,&zoom);
                                        handled=TRUE;
                                    }
                                    break;
                                case CONSOLENG_BIND_LINEUP:
                                case CONSOLENG_BIND_LINEDOWN:
                                case CONSOLENG_BIND_HALFUP:
                                case CONSOLENG_BIND_HALFDOWN:
                                case CONSOLENG_BIND_PAGEUP:
                                case CONSOLENG_BIND_PAGEDOWN:
                                case CONSOLENG_BIND_TOP:
                                case CONSOLENG_BIND_BOTTOM:
                                case CONSOLENG_BIND_CLEARHISTORY:
                                case CONSOLENG_BIND_CLEARSELECTION:
                                    handled=Console_HistoryAction(cdihmsg->unit, action); break;
                                }
                                if (handled) {
                                    con_binding_hold(held, &cdihmsg->ie);
                                    original[code]=action;
                                    break; /* Never encode a locally handled shortcut. */
                                }
                            }

                            if (report_alternate_scroll(cdihmsg->unit, &cdihmsg->ie, ConsoleDevice))
                                break;

                            /* Native history is vertical. Unhandled horizontal
                             * wheel events must not become application keys. */
                            if (cdihmsg->ie.ie_Code == RAWKEY_NM_WHEEL_LEFT ||
                                cdihmsg->ie.ie_Code == RAWKEY_NM_WHEEL_RIGHT)
                                break;

                            /* Mouse wheel support */
                            if (cdihmsg->ie.ie_Code == RAWKEY_NM_WHEEL_UP ||
                                cdihmsg->ie.ie_Code == RAWKEY_NM_WHEEL_DOWN)
                            {
                                e.ie_EventAddress =
                                    (cdihmsg->ie.ie_Code ==
                                    RAWKEY_NM_WHEEL_UP) ? (APTR) 1 : (APTR)
                                    2;
                                e.ie_Class = IECLASS_GADGETDOWN;
                                Console_HandleGadgets(cdihmsg->unit, &e);
                                e.ie_Class = IECLASS_GADGETUP;
                                Console_HandleGadgets(cdihmsg->unit, &e);
                            }
                            else
                            {
                                /* Convert it to ANSI chars */

                                if (cdihmsg->ie.ie_Class ==
                                    IECLASS_CLOSEWINDOW)
                                {
                                    /* HACK */
                                    inputBuf[0] = 28;   /* CTRL-\ */
                                    actual = 1;
                                    /* HACK */
                                }
                                else
                                {
                                    actual =
                                        RawKeyConvert(&cdihmsg->ie,
                                        inputBuf, MAPRAWKEY_BUFSIZE, NULL);


                                }

                                D(bug("RawKeyConvert returned %ld\n",
                                        actual));


                                actual = con_modified_navigation(inputBuf, actual,
                                    &cdihmsg->ie, CON_IS_UTF8(cdihmsg->unit));

                                actual = con_application_cursor(inputBuf, actual,
                                    ICU(cdihmsg->unit)->conFlags);

                                LONG keyboard = con_keyboard_event(CU(cdihmsg->unit),
                                    &cdihmsg->ie, inputBuf, ConsoleDevice);
                                if (keyboard >= 0) actual = keyboard;

                                if (actual > 0
                                    && CON_IS_UTF8(cdihmsg->unit))
                                {
                                    /* Expand keymap output (Latin-1 bytes)
                                     * to UTF-8 so apps in UTF-8 mode see a
                                     * consistent encoding on input too. */
                                    UBYTE utfBuf[KEYINPUT_BUFSIZE * 2 + 1];
                                    LONG i, outlen = 0;

                                    for (i = 0; i < actual; i++)
                                    {
                                        /*
                                         * The Amiga keymap represents ANSI
                                         * special keys with the 8-bit CSI
                                         * byte (0x9b).  That byte is a C1
                                         * control, not Latin-1 text: encoding
                                         * it as UTF-8 (c2 9b) makes modern
                                         * terminal clients see an ordinary
                                         * character and lose cursor keys.
                                         *
                                         * UTF-8 terminals use the equivalent
                                         * 7-bit CSI spelling, ESC '[', which
                                         * is also what xterm-compatible
                                         * clients expect.
                                         */
                                        if (inputBuf[i] == 0x9b)
                                        {
                                            utfBuf[outlen++] = 0x1b;
                                            utfBuf[outlen++] = '[';
                                        }
                                        else
                                            outlen +=
                                                con_utf8_encode(
                                                    utfBuf + outlen,
                                                    inputBuf[i]);
                                    }

                                    memcpy(inputBuf, utfBuf,
                                        MIN(outlen, KEYINPUT_BUFSIZE));
                                    actual = MIN(outlen, KEYINPUT_BUFSIZE);
                                }

                                if (actual > 0)
                                {
                                    /* Let the console update input-related
                                     * visual state (for example, dropping a
                                     * text selection) before the character is
                                     * handed to its reader. */
                                    Console_HandleGadgets(cdihmsg->unit,
                                        &cdihmsg->ie);

                                    /* Copy received characters to the console
                                     * unit input buffer. If the buffer is
                                     * full, then console input will be lost
                                     */
                                    tocopy =
                                        MIN(actual,
                                        CON_INPUTBUF_SIZE -
                                        ICU(cdihmsg->unit)->numStoredChars);

                                    /* Copy the input over to the unit's
                                     * buffer */
                                    CopyMem(inputBuf,
                                        ICU(cdihmsg->unit)->inputBuf +
                                        ICU(cdihmsg->unit)->numStoredChars,
                                        tocopy);

                                    ICU(cdihmsg->unit)->numStoredChars +=
                                        tocopy;

                                    answer_requests(cdihmsg->unit,
                                        ConsoleDevice);
                                } /* if (actual > 0) */
                            }
                        }
                        break;

                    case IECLASS_GADGETDOWN:
                    case IECLASS_GADGETUP:
                    case IECLASS_RAWMOUSE:
                        if (!mouseEvent)
                            Console_HandleGadgets(cdihmsg->unit, &(cdihmsg->ie));
                        break;

                    case IECLASS_TIMER:
                    case IECLASS_ACTIVEWINDOW:
                    case IECLASS_INACTIVEWINDOW:
                        Console_HandleGadgets(cdihmsg->unit,
                            &(cdihmsg->ie));
                        //.ie_Class, (APTR)cdihmsg->ie.ie_EventAddress);
                        break;

                    case IECLASS_REFRESHWINDOW:
                    case IECLASS_SIZEWINDOW:
                        {
                            /* Intentionally empty (Begin|End)Refresh() pair,
                             * as we are required to call them to keep
                             * Intuition informed, but we don't want to be
                             * restricted to rendering just in the damaged
                             * area (WFLG_NOCAREREFRESH is another possibility,
                             * but it's not our window). */
                            BeginRefresh(WINDOW(cdihmsg->unit));
                            EndRefresh(WINDOW(cdihmsg->unit), TRUE);

                            /* Refresh window contents */
                            Console_NewWindowSize(cdihmsg->unit);
                        }
                        break;
                    default:
                        D(bug("cdihmsg->ie.ie_Class = %d\n",
                                cdihmsg->ie.ie_Class));
                    } /* switch(cdihmsg->ie.ie_Class) */
                } /* if (checkconunit(cdihmsg->unit, cdihmsg->unitGeneration, ConsoleDevice)) */

                /* The input handler sent this asynchronously (it must never
                   wait for us, see CDInputHandler); we own the message. */
                FreeMem(cdihmsg, sizeof(struct cdihMessage));
            } /* while ((cdihmsg = (struct cdihMessage *)GetMsg(inputport))) */
        } /* if (wakeupsig & inputsig) */

        if (wakeupsig & commandsig)
        {
            /* We got a command from the outside. Investigate it */
            struct IOStdReq *req;

            while ((req =
                    (struct IOStdReq *)GetMsg(ConsoleDevice->commandPort)))
            {
                pasteData(ICU(req->io_Unit), ConsoleDevice);

                switch (req->io_Command)
                {
                case CMD_READ:
                    if (0 != ICU(req->io_Unit)->numStoredChars)
                    {
                        answer_read_request(req, ConsoleDevice);
                    }
                    else
                    {
                        /* Not enough bytes in the buffer to fill the request,
                         * put it on hold. ioReq already removed from the
                         * queue with GetMsg() */
                        AddTail((struct List *)&ConsoleDevice->readRequests,
                            (struct Node *)req);
                    }
                    break;

                default:
                    bug
                        ("!!! UNKNOWN COMMAND RECEIVED BY CONSOLE TASK !!!\n");
                    bug("!!! THIS SHOULD NEVER HAPPEN !!!\n");
                    break;
                }

            } /* while ((req = (struct IOStdReq *)GetMsg(ConsoleDevice->commandPort))) */

        } /* if (wakeupsig & commandsig) */

        ReleaseSemaphore(&ConsoleDevice->consoleTaskLock);

        /* Native visual bell, outside console and drawing locks. NULL asks
         * Intuition to resolve its live screens, avoiding stale window data. */
        if (wakeupsig & SIGBREAKF_CTRL_E)
            DisplayBeep(NULL);
    } /* forever */

/* FIXME: Do cleanup here */
}

/********** checkconunit()  *******************************/

/* Compare only against live objects: the queued address may have been freed
 * and reused. The generation is immutable for each published lifetime. */
static BOOL checkconunit(Object *unit, UQUAD generation, struct ConsoleBase *ConsoleDevice)
{
    Object *o, *ostate;
    BOOL found = FALSE;

    ObtainSemaphoreShared(&ConsoleDevice->unitListLock);

    ostate = (Object *) ConsoleDevice->unitList.mlh_Head;
    while ((o = NextObject(&ostate)) && (!found))
    {
        if (o == unit && ICU(o)->inputGeneration == generation)
        {
            found = TRUE;
        }
    }

    ReleaseSemaphore(&ConsoleDevice->unitListLock);
    return found;
}

/******** answer_read_request() ***************************/

static void answer_read_request(struct IOStdReq *req,
    struct ConsoleBase *ConsoleDevice)
{
    Object *unit;

    D(bug("answer_read_request\n"));
    /* This function assumes that there are at least one character
       available in the unitsinput buffer
     */

    unit = (Object *) req->io_Unit;

    req->io_Actual = MIN(ICU(unit)->numStoredChars, req->io_Length);

    /* Copy characters from the buffer into the request */
    CopyMem((APTR) ICU(unit)->inputBuf, req->io_Data, req->io_Actual);

    if (ICU(unit)->numStoredChars > req->io_Length)
    {
        ULONG i;

        ICU(unit)->numStoredChars -= req->io_Actual;

        /* We have to move the rest of the bytes to the start
           of the buffer.

           NOTE: we could alternatively use a circular buffer
         */

        for (i = 0; i < ICU(unit)->numStoredChars; i++)
        {
            ICU(unit)->inputBuf[i] =
                ICU(unit)->inputBuf[i + req->io_Actual];
        }
    }
    else
    {
        /* No more unread characters in the buffer */
        ICU(unit)->numStoredChars = 0;

    }

    req->io_Error = 0;
    /* All done. Just reply the request */

    /* stegerg: caller of answer_read_request is responsible for the Remove,
       because the Remove must be used only if the req was in
       the readrequests list
       Remove((struct Node *)req);

     */

/* bug("receiving task=%s, sigbit=%d\n, mode=%d"
        , ((struct Task *)req->io_Message.mn_ReplyPort->mp_SigTask)->tc_Node.ln_Name
        , req->io_Message.mn_ReplyPort->mp_SigBit
        , req->io_Message.mn_ReplyPort->mp_Flags
);
*/
    ReplyMsg((struct Message *)req);

    return;
}
