/*
 * aros_shell.c - the AROS side of PhotoCraft's shell.
 *
 * One Intuition window on the default public screen, its IDCMP events
 * translated into plain records the Rust event loop (src/ffi.rs) reads, the
 * egui frame buffer blitted into it with cybergraphics, keys mapped through
 * keymap.library, ASL file requesters, the FTXT clipboard through iffparse,
 * and a timer.device request so the event loop can sleep until egui's next
 * repaint is due. Everything the Rust side needs is a C function taking the
 * handle returned by pc_window_open(); library bases come from the AROS
 * autoinit link libraries.
 */

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/cybergraphics.h>
#include <proto/keymap.h>
#include <proto/asl.h>
#include <proto/iffparse.h>
#include <proto/utility.h>

#include <exec/memory.h>
#include <exec/ports.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <cybergraphx/cybergraphics.h>
#include <devices/inputevent.h>
#include <devices/rawkeycodes.h>
#include <devices/timer.h>
#include <libraries/asl.h>
#include <libraries/iffparse.h>
#include <workbench/startup.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define ID_FTXT MAKE_ID('F', 'T', 'X', 'T')
#define ID_CHRS MAKE_ID('C', 'H', 'R', 'S')

/* Event kinds, mirrored in src/ffi.rs. */
enum pc_event_kind {
    PC_EV_NONE = 0,
    PC_EV_CLOSE = 1,
    PC_EV_RESIZE = 2,
    PC_EV_MOUSE_MOVE = 3,
    PC_EV_BUTTON = 4,
    PC_EV_KEY = 5,
    PC_EV_WHEEL = 6,
    PC_EV_FOCUS = 7,
    PC_EV_REFRESH = 8,
};

/* One translated IDCMP message, mirrored in src/ffi.rs (keep the layout in sync). */
struct pc_event {
    int32_t kind;
    int32_t x, y;          /* pointer position in inner-window pixels */
    int32_t button;        /* PC_EV_BUTTON: 0 left, 1 right, 2 middle */
    int32_t down;          /* PC_EV_BUTTON / PC_EV_KEY: pressed */
    int32_t rawkey;        /* PC_EV_KEY: RAWKEY_* code */
    uint32_t qualifier;    /* IEQUALIFIER_* at the time of the event */
    int32_t wheel_x, wheel_y; /* PC_EV_WHEEL: steps, right and up positive */
    int32_t width, height; /* PC_EV_RESIZE: the new inner size */
    int32_t focus;         /* PC_EV_FOCUS: 1 activated, 0 deactivated */
    char text[16];         /* PC_EV_KEY: the key with its qualifiers (system charset), NUL ended */
    char plain[8];         /* PC_EV_KEY: the key with no qualifiers, for the logical key name */
};

struct pc_window {
    struct Window *win;
    struct MsgPort *timer_port;
    struct timerequest *timer_req;
    int timer_open;
    int timer_pending;
    struct Task *task;
    LONG wake_sig;         /* signal other threads raise to wake the event loop */
    char title[256];
    uint32_t qualifier;    /* last seen qualifiers (Caps Lock state for the app) */
};

static void stop_timer(struct pc_window *w)
{
    if (w->timer_pending) {
        if (!CheckIO((struct IORequest *)w->timer_req))
            AbortIO((struct IORequest *)w->timer_req);
        WaitIO((struct IORequest *)w->timer_req);
        w->timer_pending = 0;
    }
}

static void start_timer(struct pc_window *w, int ms)
{
    stop_timer(w);
    if (!w->timer_open || ms <= 0)
        return;
    w->timer_req->tr_node.io_Command = TR_ADDREQUEST;
    w->timer_req->tr_time.tv_secs = ms / 1000;
    w->timer_req->tr_time.tv_micro = (ms % 1000) * 1000;
    SendIO((struct IORequest *)w->timer_req);
    w->timer_pending = 1;
}

/* The public screen's size, for placing and limiting the window. */
void pc_screen_size(int32_t *width, int32_t *height)
{
    struct Screen *scr = LockPubScreen(NULL);
    *width = 1024;
    *height = 768;
    if (scr) {
        *width = scr->Width;
        *height = scr->Height;
        UnlockPubScreen(NULL, scr);
    }
}

struct pc_window *pc_window_open(int32_t width, int32_t height, const char *title)
{
    struct pc_window *w = calloc(1, sizeof *w);
    int32_t sw, sh;
    if (!w)
        return NULL;
    strncpy(w->title, title ? title : "PhotoCraft", sizeof w->title - 1);
    w->task = FindTask(NULL);
    w->wake_sig = AllocSignal(-1);
    if (w->wake_sig >= 0)
        SetSignal(0, 1UL << w->wake_sig);

    pc_screen_size(&sw, &sh);
    if (width > sw - 16) width = sw - 16;
    if (height > sh - 48) height = sh - 48;
    if (width < 320) width = 320;
    if (height < 240) height = 240;

    w->win = OpenWindowTags(NULL,
        WA_Left, (sw - width) / 2 > 0 ? (sw - width) / 2 : 0,
        WA_Top, (sh - height) / 2 > 0 ? (sh - height) / 2 : 0,
        WA_InnerWidth, width,
        WA_InnerHeight, height,
        WA_MinWidth, 320,
        WA_MinHeight, 240,
        WA_MaxWidth, ~0,
        WA_MaxHeight, ~0,
        WA_Title, (IPTR)w->title,
        WA_Flags, WFLG_DRAGBAR | WFLG_DEPTHGADGET | WFLG_CLOSEGADGET | WFLG_SIZEGADGET
                | WFLG_SIZEBBOTTOM | WFLG_ACTIVATE | WFLG_REPORTMOUSE | WFLG_RMBTRAP
                | WFLG_GIMMEZEROZERO | WFLG_NOCAREREFRESH,
        WA_IDCMP, IDCMP_CLOSEWINDOW | IDCMP_NEWSIZE | IDCMP_MOUSEBUTTONS | IDCMP_MOUSEMOVE
                | IDCMP_RAWKEY | IDCMP_ACTIVEWINDOW | IDCMP_INACTIVEWINDOW,
        TAG_DONE);
    if (!w->win) {
        if (w->wake_sig >= 0) FreeSignal(w->wake_sig);
        free(w);
        return NULL;
    }

    w->timer_port = CreateMsgPort();
    if (w->timer_port) {
        SetSignal(0, 1UL << w->timer_port->mp_SigBit);
        w->timer_req = (struct timerequest *)CreateIORequest(w->timer_port, sizeof(struct timerequest));
        if (w->timer_req && OpenDevice((CONST_STRPTR)"timer.device", UNIT_MICROHZ, (struct IORequest *)w->timer_req, 0) == 0)
            w->timer_open = 1;
    }
    return w;
}

void pc_window_close(struct pc_window *w)
{
    if (!w)
        return;
    stop_timer(w);
    if (w->timer_open)
        CloseDevice((struct IORequest *)w->timer_req);
    if (w->timer_req)
        DeleteIORequest((struct IORequest *)w->timer_req);
    if (w->timer_port)
        DeleteMsgPort(w->timer_port);
    if (w->win)
        CloseWindow(w->win);
    if (w->wake_sig >= 0)
        FreeSignal(w->wake_sig);
    free(w);
}

void pc_window_inner_size(struct pc_window *w, int32_t *width, int32_t *height)
{
    *width = w->win->GZZWidth;
    *height = w->win->GZZHeight;
}

/* Sleeps until a window message, a wake-up from pc_window_wake() or the
 * timeout (milliseconds; negative waits for messages only). Returns 1 when a
 * window message or wake-up arrived, 0 on timeout. The timer is judged by its
 * request (CheckIO), never by its signal alone: a signal bit can be left set
 * from an earlier use, and that must not count as a timeout. */
int32_t pc_window_wait(struct pc_window *w, int32_t timeout_ms)
{
    ULONG winsig = 1UL << w->win->UserPort->mp_SigBit;
    ULONG wakesig = w->wake_sig >= 0 ? 1UL << w->wake_sig : 0;
    ULONG timersig = 0;
    ULONG got;

    /* egui wants the next frame at once: still block for a moment so the
     * other tasks run (the file-open and save workers, the font scan); a
     * frame loop that never waits starves them. A millisecond is nothing
     * next to a software-rendered frame. */
    if (timeout_ms == 0)
        timeout_ms = w->timer_open ? 1 : 0;
    if (timeout_ms == 0)
        return 1;
    if (w->timer_open && timeout_ms > 0) {
        start_timer(w, timeout_ms);
        timersig = 1UL << w->timer_port->mp_SigBit;
    }
    for (;;) {
        got = Wait(winsig | wakesig | timersig);
        if (got & (winsig | wakesig)) {
            stop_timer(w);
            return 1;
        }
        if (w->timer_pending && CheckIO((struct IORequest *)w->timer_req)) {
            WaitIO((struct IORequest *)w->timer_req);
            w->timer_pending = 0;
            return 0;
        }
        if (!timersig)
            return 1;
        /* a stale timer signal: the request is still running, keep waiting */
    }
}

/* Wakes the event loop from any task (egui's request_repaint from a worker thread). */
void pc_window_wake(struct pc_window *w)
{
    if (w && w->wake_sig >= 0)
        Signal(w->task, 1UL << w->wake_sig);
}

static void map_key(struct pc_window *w, struct IntuiMessage *imsg, struct pc_event *ev)
{
    struct InputEvent ie;
    UWORD code = imsg->Code & ~IECODE_UP_PREFIX;
    LONG n;

    memset(&ie, 0, sizeof ie);
    ie.ie_Class = IECLASS_RAWKEY;
    ie.ie_Code = code;
    /* Control and Amiga combinations map to control characters; the app wants
     * the plain key under them, so those qualifiers are left out here. */
    ie.ie_Qualifier = imsg->Qualifier & ~(IEQUALIFIER_CONTROL | IEQUALIFIER_LCOMMAND | IEQUALIFIER_RCOMMAND);
    if (imsg->IAddress)
        ie.ie_EventAddress = (APTR *)*((IPTR *)imsg->IAddress);
    n = MapRawKey(&ie, (STRPTR)ev->text, sizeof ev->text - 1, NULL);
    if (n < 0) n = 0;
    ev->text[n] = 0;

    ie.ie_Qualifier = 0;
    ie.ie_EventAddress = NULL;
    n = MapRawKey(&ie, (STRPTR)ev->plain, sizeof ev->plain - 1, NULL);
    if (n < 0) n = 0;
    ev->plain[n] = 0;
    (void)w;
}

/* Fills the next translated event; returns 0 when there is none waiting. */
int32_t pc_window_poll(struct pc_window *w, struct pc_event *ev)
{
    struct IntuiMessage *imsg;

    memset(ev, 0, sizeof *ev);
    while ((imsg = (struct IntuiMessage *)GetMsg(w->win->UserPort))) {
        ULONG cls = imsg->Class;
        UWORD code = imsg->Code;
        UWORD qual = imsg->Qualifier;
        /* The message's position is relative to the outer window; the frame is the
         * GimmeZeroZero inner area, which starts after the borders. */
        WORD mx = imsg->MouseX - w->win->BorderLeft, my = imsg->MouseY - w->win->BorderTop;

        w->qualifier = qual;
        ev->x = mx;
        ev->y = my;
        ev->qualifier = qual;

        switch (cls) {
        case IDCMP_CLOSEWINDOW:
            ReplyMsg((struct Message *)imsg);
            ev->kind = PC_EV_CLOSE;
            return 1;
        case IDCMP_NEWSIZE:
            ReplyMsg((struct Message *)imsg);
            ev->kind = PC_EV_RESIZE;
            ev->width = w->win->GZZWidth;
            ev->height = w->win->GZZHeight;
            return 1;
        case IDCMP_MOUSEMOVE:
            ReplyMsg((struct Message *)imsg);
            ev->kind = PC_EV_MOUSE_MOVE;
            return 1;
        case IDCMP_MOUSEBUTTONS:
            ReplyMsg((struct Message *)imsg);
            ev->kind = PC_EV_BUTTON;
            ev->down = (code & IECODE_UP_PREFIX) ? 0 : 1;
            switch (code & ~IECODE_UP_PREFIX) {
            case IECODE_LBUTTON: ev->button = 0; break;
            case IECODE_RBUTTON: ev->button = 1; break;
            case IECODE_MBUTTON: ev->button = 2; break;
            default: ev->kind = PC_EV_NONE; continue;
            }
            return 1;
        case IDCMP_RAWKEY: {
            UWORD key = code & ~IECODE_UP_PREFIX;
            if (key >= RAWKEY_NM_WHEEL_UP && key <= RAWKEY_NM_WHEEL_RIGHT) {
                ReplyMsg((struct Message *)imsg);
                if (code & IECODE_UP_PREFIX)
                    continue;
                ev->kind = PC_EV_WHEEL;
                switch (key) {
                case RAWKEY_NM_WHEEL_UP: ev->wheel_y = 1; break;
                case RAWKEY_NM_WHEEL_DOWN: ev->wheel_y = -1; break;
                case RAWKEY_NM_WHEEL_LEFT: ev->wheel_x = -1; break;
                default: ev->wheel_x = 1; break;
                }
                return 1;
            }
            ev->kind = PC_EV_KEY;
            ev->rawkey = key;
            ev->down = (code & IECODE_UP_PREFIX) ? 0 : 1;
            if (ev->down)
                map_key(w, imsg, ev);
            ReplyMsg((struct Message *)imsg);
            return 1;
        }
        case IDCMP_ACTIVEWINDOW:
        case IDCMP_INACTIVEWINDOW:
            ReplyMsg((struct Message *)imsg);
            ev->kind = PC_EV_FOCUS;
            ev->focus = cls == IDCMP_ACTIVEWINDOW;
            return 1;
        default:
            ReplyMsg((struct Message *)imsg);
            continue;
        }
    }
    return 0;
}

uint32_t pc_window_qualifier(struct pc_window *w)
{
    return w->qualifier;
}

/* Copies an RGBA (byte order R, G, B, A) frame buffer into the window's inner area. */
void pc_window_blit(struct pc_window *w, const uint8_t *rgba, int32_t width, int32_t height, int32_t stride)
{
    int32_t iw = w->win->GZZWidth, ih = w->win->GZZHeight;
    if (width > iw) width = iw;
    if (height > ih) height = ih;
    if (width <= 0 || height <= 0)
        return;
    WritePixelArray((APTR)rgba, 0, 0, stride, w->win->RPort, 0, 0, width, height, RECTFMT_RGBA);
}

void pc_window_set_busy(struct pc_window *w, int32_t busy)
{
    SetWindowPointer(w->win, WA_BusyPointer, busy ? TRUE : FALSE, TAG_DONE);
}

void pc_window_set_title(struct pc_window *w, const char *title)
{
    strncpy(w->title, title ? title : "", sizeof w->title - 1);
    w->title[sizeof w->title - 1] = 0;
    SetWindowTitles(w->win, (CONST_STRPTR)w->title, (CONST_STRPTR)~0);
}

void pc_window_activate(struct pc_window *w)
{
    ActivateWindow(w->win);
}

/* The event loop's task priority. A task that keeps the CPU is not time-sliced
 * against equal-priority tasks on every AROS flavour, so while background
 * work runs (file opens and saves, the font scan) the loop drops below the
 * workers' priority 0 and only draws when they wait. */
void pc_window_set_priority(struct pc_window *w, int32_t pri)
{
    SetTaskPri(w->task, pri);
}

void pc_beep(void)
{
    DisplayBeep(NULL);
}

/*
 * An ASL file requester. `save` selects save mode, `multi` multi-selection;
 * `dir`, `file` and `pattern` may be empty. The chosen paths are written to
 * `out`, separated by newlines; returns the number of paths (0: cancelled or
 * nothing fits).
 */
int32_t pc_file_request(struct pc_window *w, int32_t save, int32_t multi, const char *title,
                        const char *dir, const char *file, const char *pattern, char *out, int32_t outlen)
{
    struct FileRequester *req;
    int32_t count = 0;
    char path[1024];

    if (!out || outlen < 2)
        return 0;
    out[0] = 0;
    req = AllocAslRequestTags(ASL_FileRequest,
        ASLFR_Window, (IPTR)(w ? w->win : NULL),
        ASLFR_TitleText, (IPTR)(title ? title : "Select file"),
        ASLFR_InitialDrawer, (IPTR)(dir ? dir : ""),
        ASLFR_InitialFile, (IPTR)(file ? file : ""),
        ASLFR_DoSaveMode, save ? TRUE : FALSE,
        ASLFR_DoMultiSelect, (multi && !save) ? TRUE : FALSE,
        ASLFR_DoPatterns, (pattern && pattern[0]) ? TRUE : FALSE,
        ASLFR_InitialPattern, (IPTR)((pattern && pattern[0]) ? pattern : "#?"),
        ASLFR_RejectIcons, TRUE,
        TAG_DONE);
    if (!req)
        return 0;
    if (AslRequestTags(req, TAG_DONE)) {
        if (multi && !save && req->fr_NumArgs > 0) {
            LONG i;
            for (i = 0; i < req->fr_NumArgs; i++) {
                strncpy(path, req->fr_Drawer ? (const char *)req->fr_Drawer : "", sizeof path - 1);
                path[sizeof path - 1] = 0;
                if (!AddPart((STRPTR)path, (CONST_STRPTR)req->fr_ArgList[i].wa_Name, sizeof path))
                    continue;
                if (strlen(out) + strlen(path) + 2 > (size_t)outlen)
                    break;
                if (count)
                    strcat(out, "\n");
                strcat(out, path);
                count++;
            }
        } else if (req->fr_File && req->fr_File[0]) {
            strncpy(path, req->fr_Drawer ? (const char *)req->fr_Drawer : "", sizeof path - 1);
            path[sizeof path - 1] = 0;
            if (AddPart((STRPTR)path, (CONST_STRPTR)req->fr_File, sizeof path) && strlen(path) + 1 <= (size_t)outlen) {
                strcpy(out, path);
                count = 1;
            }
        }
    }
    FreeAslRequest(req);
    return count;
}

/* Writes `len` bytes of text (system charset) to the clipboard as FTXT. */
int32_t pc_clip_write_text(const char *text, int32_t len)
{
    struct IFFHandle *iff = AllocIFF();
    int32_t ok = 0;
    if (!iff)
        return 0;
    iff->iff_Stream = (IPTR)OpenClipboard(0);
    if (iff->iff_Stream) {
        InitIFFasClip(iff);
        if (OpenIFF(iff, IFFF_WRITE) == 0) {
            if (PushChunk(iff, ID_FTXT, ID_FORM, IFFSIZE_UNKNOWN) == 0) {
                if (PushChunk(iff, 0, ID_CHRS, len) == 0) {
                    ok = WriteChunkBytes(iff, (APTR)text, len) == len;
                    PopChunk(iff);
                }
                PopChunk(iff);
            }
            CloseIFF(iff);
        }
        CloseClipboard((struct ClipboardHandle *)iff->iff_Stream);
    }
    FreeIFF(iff);
    return ok;
}

/* Reads the clipboard's FTXT text into `out` (NUL ended); returns its length, or 0. */
int32_t pc_clip_read_text(char *out, int32_t outlen)
{
    struct IFFHandle *iff = AllocIFF();
    int32_t total = 0;
    if (!iff || !out || outlen < 1)
        return 0;
    out[0] = 0;
    iff->iff_Stream = (IPTR)OpenClipboard(0);
    if (iff->iff_Stream) {
        InitIFFasClip(iff);
        if (OpenIFF(iff, IFFF_READ) == 0) {
            if (StopChunk(iff, ID_FTXT, ID_CHRS) == 0) {
                while (ParseIFF(iff, IFFPARSE_SCAN) == 0) {
                    struct ContextNode *cn = CurrentChunk(iff);
                    if (cn && cn->cn_Type == ID_FTXT && cn->cn_ID == ID_CHRS) {
                        LONG want = cn->cn_Size;
                        if (want > outlen - 1 - total)
                            want = outlen - 1 - total;
                        if (want > 0) {
                            LONG got = ReadChunkBytes(iff, out + total, want);
                            if (got > 0)
                                total += got;
                        }
                    }
                }
            }
            CloseIFF(iff);
        }
        CloseClipboard((struct ClipboardHandle *)iff->iff_Stream);
    }
    FreeIFF(iff);
    out[total] = 0;
    return total;
}

/* Hands a URL to the system's OpenURL command (openurl.library's front end). */
int32_t pc_open_url(const char *url)
{
    char cmd[2048];
    if (!url || strlen(url) > sizeof cmd - 16)
        return 0;
    strcpy(cmd, "OpenURL \"");
    strcat(cmd, url);
    strcat(cmd, "\"");
    return SystemTags((CONST_STRPTR)cmd, SYS_Asynch, FALSE, TAG_DONE) == 0;
}
