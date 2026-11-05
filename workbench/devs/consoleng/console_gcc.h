/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc:
*/
#ifndef CONSOLE_GCC_H
#define CONSOLE_GCC_H

#include <aros/libcall.h>
#include <exec/execbase.h>
#include <exec/devices.h>
#include <exec/semaphores.h>
#include <dos/dos.h>
#include <devices/conunit.h>
#include "include/consoleng.h"
#include <intuition/classes.h>

#include <proto/alib.h>

/* Predeclaration */
struct ConsoleBase;

/* Constants */
#define COTASK_STACKSIZE (AROS_STACKSIZE + 4)
#define COTASK_PRIORITY  10


/* Minimum x & y char positions */

#define DEF_CHAR_XMIN 0
#define DEF_CHAR_YMIN 0


#define CHAR_XMIN(o) DEF_CHAR_XMIN
#define CHAR_YMIN(o) DEF_CHAR_YMIN

#define CHAR_XMAX(o) (CU(o)->cu_XMax)
#define CHAR_YMAX(o) (CU(o)->cu_YMax)


#define XCP (CU(o)->cu_XCP)     /* Character X pos */
#define YCP (CU(o)->cu_YCP)     /* Character Y pos */

#define XCCP (CU(o)->cu_XCCP)   /* Cursor X pos */
#define YCCP (CU(o)->cu_YCCP)   /* Cusror Y pos */

#define XRSIZE (CU(o)->cu_XRSize)
#define YRSIZE (CU(o)->cu_YRSize)

#define CP_X(o) (GFX_X(o, CU(o)->cu_XCCP))
#define CP_Y(o) (GFX_Y(o, CU(o)->cu_YCCP))

/* Macros that convert from char to gfx coords */

#define GFX_X(o, x) (CU(o)->cu_XROrigin + ((x) * CU(o)->cu_XRSize))
#define GFX_Y(o, y) (CU(o)->cu_YROrigin + ((y) * CU(o)->cu_YRSize))

#define GFX_XMIN(o) (GFX_X((o), CHAR_XMIN(o)))
#define GFX_YMIN(o) (GFX_Y((o), CHAR_YMIN(o)))

#define GFX_XMAX(o) ((GFX_X((o), CHAR_XMAX(o) + 1)) - 1)
#define GFX_YMAX(o) ((GFX_Y((o), CHAR_YMAX(o) + 1)) - 1)

/* Macros to set/reset/check rawevents */

#define SET_RAWEVENT(o, which) (CU(o)->cu_RawEvents[(which) / 8] |= (1 << ((which) & 7)))
#define RESET_RAWEVENT(o, which) (CU(o)->cu_RawEvents[(which) / 8] &= ~(1 << ((which) & 7)))
#define CHECK_RAWEVENT(o, which) (CU(o)->cu_RawEvents[(which) / 8] & (1 << ((which) & 7)))

#define SET_MODE(o, which) (CU(o)->cu_Modes[(which) / 8] |= (1 << ((which) & 7)))
#define CLEAR_MODE(o, which) (CU(o)->cu_Modes[(which) / 8] &= ~(1 << ((which) & 7)))
#define CHECK_MODE(o, which) (CU(o)->cu_Modes[(which) / 8] & (1 << ((which) & 7)))

#define CONSOLECLASSPTR		(ConsoleDevice->consoleClass)
#define STDCONCLASSPTR		(ConsoleDevice->stdConClass)
#define CHARMAPCLASSPTR		(ConsoleDevice->charMapClass)
#define SNIPMAPCLASSPTR		(ConsoleDevice->snipMapClass)

#define CU(x) ((struct ConUnit *)x)

#define ICU(x) ((struct intConUnit *)x)

/* Invalid margins after geometry changes fall back to the full screen. */
#define CON_SCROLL_VALID(o) (ICU(o)->scrollBottom > ICU(o)->scrollTop + 1 && ICU(o)->scrollTop >= 0 && ICU(o)->scrollTop < CHAR_YMAX(o))
#define CON_SCROLL_TOP(o) (CON_SCROLL_VALID(o) ? ICU(o)->scrollTop : 0)
#define CON_SCROLL_BOTTOM(o) (CON_SCROLL_VALID(o) ? MIN(ICU(o)->scrollBottom - 1, CHAR_YMAX(o)) : CHAR_YMAX(o))

#define WINDOW(o)	CU(o)->cu_Window
#define RASTPORT(o)	WINDOW(o)->RPort


#define MAX(a, b) ((a) > (b) ? a : b)
#define MIN(a, b) ((a) < (b) ? a : b)


#define CON_TXTFLAGS_MASK (FSF_BOLD | FSF_ITALIC | FSF_UNDERLINED)
#define CON_TXTFLAGS_BOLD FSF_BOLD
#define CON_TXTFLAGS_ITALIC FSF_ITALIC
#define CON_TXTFLAGS_UNDERLINED FSF_UNDERLINED
#define CON_TXTFLAGS_REVERSED 0x08
#define CON_TXTFLAGS_CONCEALED 0x10
#define CON_TXTFLAGS_DOUBLEUNDERLINED 0x20
#define CON_TXTFLAGS_STRUCK 0x40
#define CON_TXTFLAGS_FAINT 0x80

/* Console write commands */
enum
{
    C_NIL = 0,

    C_ASCII,
    C_ESC,
    C_BELL,
    C_BACKSPACE,
    C_HTAB,
    C_LINEFEED,
    C_VTAB,
    C_FORMFEED,
    C_CARRIAGE_RETURN,
    C_SHIFT_IN,
    C_SHIFT_OUT,
    C_INDEX,
    C_NEXT_LINE,
    C_H_TAB_SET,
    C_REVERSE_IDX,

    C_SET_LF_MODE,
    C_RESET_LF_MODE,
    C_DEVICE_STATUS_REPORT,

    C_INSERT_CHAR,
    C_CURSOR_UP,
    C_CURSOR_DOWN,
    C_CURSOR_FORWARD,
    C_CURSOR_BACKWARD,
    C_CURSOR_NEXT_LINE,
    C_CURSOR_PREV_LINE,
    C_CURSOR_POS,
    C_CURSOR_COLUMN,
    C_CURSOR_HTAB,
    C_ERASE_IN_DISPLAY,
    C_ERASE_IN_LINE,
    C_INSERT_LINE,
    C_DELETE_LINE,
    C_DELETE_CHAR,
    C_SCROLL_UP,
    C_SCROLL_DOWN,
    C_CURSOR_TAB_CTRL,
    C_CURSOR_BACKTAB,

    C_SELECT_GRAPHIC_RENDITION,
    C_WINDOW_STATUS_REQUEST,

    C_CURSOR_VISIBLE,
    C_CURSOR_INVISIBLE,

    C_SET_RAWEVENTS,
    C_RESET_RAWEVENTS,

    C_SET_AUTOWRAP_MODE,
    C_RESET_AUTOWRAP_MODE,
    C_SET_AUTOSCROLL_MODE,
    C_RESET_AUTOSCROLL_MODE,
    C_SET_PAGE_LENGTH,
    C_SET_LINE_LENGTH,
    C_SET_LEFT_OFFSET,
    C_SET_TOP_OFFSET,

    C_ASCII_STRING,

    C_SET_UTF8_MODE,
    C_RESET_UTF8_MODE,

    C_SET_FONT,                 /* OSC 50: params = name ptr, ysize */
    C_SET_TITLE,                /* OSC 0/2: params = title ptr */
    C_SET_COLOR,                /* OSC 4/10/11/12/104/110/111/112: params = code,
                                   spec ptr, ST-terminated */

    C_SET_OBSCURED,             /* tab hidden: state only, no rendering */
    C_RESET_OBSCURED,           /* tab shown: full refresh from cells */

    C_SAVE_CURSOR,              /* ESC 7 (DECSC): position and attributes */
    C_RESTORE_CURSOR,           /* ESC 8 (DECRC) */

    C_ERASE_CHAR,              /* CSI Ps X: erase without moving cursor */

    C_ALTERNATE_SCREEN,       /* DEC 1049: private screen and saved cursor */

    C_SET_CURSOR_STYLE,      /* CSI Ps SP q: shape and requested blink mode */

    C_SET_CURSOR_BLINK,      /* DEC12: toggle blink while retaining shape */

    C_GET_CURSOR_PREFS,
    C_SET_CURSOR_PREFS,

    C_GET_HISTORY_PREFS,
    C_SET_HISTORY_PREFS,
    C_RESET_FONT,
    C_GET_COLOR_PREFS,
    C_SET_COLOR_PREFS,

    C_GET_MODE_STATE,       /* internal query: private, mode, output state */

    C_CURSOR_ROW,          /* CSI Ps d: absolute row, retaining column */

    C_CLEAR_TABS,          /* CSI Ps g: current/all horizontal tab stops */

    C_GET_EMOJI_PREFS,
    C_SET_EMOJI_PREFS,

    C_SET_SCROLL_REGION,   /* DECSTBM; internal command, no public vector */
    C_SET_ORIGIN_MODE,     /* DECOM; internal dispatch */

    C_TERMINAL_RESET,      /* RIS; internal dispatch, public ABI unchanged */

    C_REPEAT_CHAR,        /* REP; internal dispatch */

    C_SCREEN_ALIGNMENT,   /* DECALN; internal dispatch */

    C_PRIMARY_ATTRIBUTES, /* DA1 / DECID; internal dispatch */

    C_SECONDARY_ATTRIBUTES, /* DA2; internal dispatch */

    C_PUSH_TITLE, C_POP_TITLE, C_CLEAR_TITLE_STACK, /* internal dispatch */

    C_REPORT_CELL_GEOMETRY, C_REPORT_PIXEL_GEOMETRY, /* internal dispatch */

    C_SET_HYPERLINK,       /* OSC8; private metadata, no public vector */

    C_SET_CURSOR_SHAPE,    /* OSC50 CursorShape, preserving blink */

    C_GET_BELL_PREFS, C_SET_BELL_PREFS,

    C_SET_OSC_CLIPBOARD, /* OSC52 copy-only; private dispatch */

    C_GET_CLIPBOARD_PREFS, C_SET_CLIPBOARD_PREFS,

    C_GET_CURSOR_COLOR_PREFS, C_SET_CURSOR_COLOR_PREFS,

    C_SET_SYNC_OUTPUT,

    C_ZOOM_FONT, /* private keyboard action; appended dispatch value */

    NUM_CONSOLE_COMMANDS
};

/**************
**  structs  **
**************/

struct coTaskParams
{
    struct ConsoleBase *consoleDevice;
    struct Task *parentTask;
    ULONG initSignal;
};

struct intPasteData
{
    struct MinNode node;
    struct intConUnit *unit;
    STRPTR pasteBuffer;
    ULONG pasteBufferSize;
    ULONG pasteAllocationSize;
};

/* Pen map: 0-7 = ANSI colors (SGR 30-37/40-47), 8 = default background,
   9 = default text (SGR 0/39/49) */
#define CONUNIT_PEN_MAX    10
#define CONUNIT_PEN_DEFBG  8
#define CONUNIT_PEN_DEFFG  9

/* Extended color encodings for Console_GetColorPen's ColorIdx:
   xterm-256 palette entries (SGR 38;5;n / 48;5;n, bright 90-97/100-107)
   and 24-bit truecolor (SGR 38;2;r;g;b / 48;2;r;g;b). */
#define CONPEN_XTERM(n)    (0x100UL + (n))
#define CONPEN_ISXTERM(v)  ((v) >= 0x100UL && (v) <= 0x1FFUL)
#define CONPEN_XTERMIDX(v) ((v) - 0x100UL)
#define CONPEN_BRIGHT(n) (0x200UL + (n))
#define CONPEN_ISBRIGHT(v) ((v) >= 0x200UL && (v) <= 0x207UL)
#define CONPEN_BRIGHTIDX(v) ((v) - 0x200UL)
#define CONPEN_RGB(r,g,b)  (0x01000000UL | ((ULONG)(r) << 16) | \
                            ((ULONG)(g) << 8) | (ULONG)(b))
#define CONPEN_ISRGB(v)    ((v) >= 0x01000000UL)

#define CON_INPUTBUF_SIZE 512

struct intConUnit
{
    struct ConUnit unit;
    ULONG conFlags;
    BOOL emojiDisabled;
    BOOL bellDisabled;
    BOOL oscCopyDisabled; /* private policy bits: 1 disables copy, 2 permits read */
    WORD scrollTop, scrollBottom; /* zero-based top, exclusive bottom; 0 = full screen */
    UQUAD inputGeneration; /* immutable while published; rejects stale events */
    UWORD mouseMode;
    BOOL mouseSGR, mouseLastValid;
    BOOL mouseUTF8;
    LONG mouseLastX, mouseLastY;

    /* Incomplete UTF-8 sequence carried across CMD_WRITEs (CF_UTF8 mode) */
    UBYTE utf8Stash[4];
    UBYTE utf8StashLen;

#define CON_TITLE_SIZE 64
    /* Tab/window title set via OSC 0/2 (xterm convention) */
    UBYTE unitTitle[CON_TITLE_SIZE];
    UBYTE displayTitle[CON_TITLE_SIZE]; /* stable while hidden title changes */
    struct ConsoleTitleEntry *titleStack;
    UWORD titleDepth;

    /* OSC (ESC ]) string accumulation - OSC 50 = live font switch */
#define CON_OSCBUF_SIZE 2048
#define OSC_IDLE     0
#define OSC_COLLECT  1
#define OSC_OVERFLOW 2
#define OSC_GOT_ESC  3          /* awaiting '\' of the ST terminator */
#define OSC_OVERFLOW_ESC 4      /* overflow must survive the ST introducer */
    UBYTE oscBuf[CON_OSCBUF_SIZE];
    UWORD oscLen;
    UBYTE oscState;
    struct ConsolengSync *sync;
    struct ConsolengKeyboard *keyboard;
    BOOL keyboardAlternate;
    UBYTE *oscClipboard;
    ULONG oscClipboardLength, oscClipboardCapacity;
    BOOL oscClipboardInvalid;
    UBYTE dcsDiscard; /* DCS/SOS/PM/APC payload, no allocation */

    /* Font installed via OSC 50 (owned by the unit, closed at dispose) */
    struct TextFont *setFont;
    BOOL fontSetOK;
    struct ConsolengFontWindow *fontWindow; /* shared borrowed baseline */

    UWORD pens[CONUNIT_PEN_MAX];

    /* Buffer where characters received from the console input handler
       will be stored
     */
    UBYTE inputBuf[CON_INPUTBUF_SIZE];
    /* Number of charcters currently stored in the buffer */
    ULONG numStoredChars;

    /* Data to be copied into the inputBuf for processing */
    struct MinList pasteData;

    /* Position in the first pasteData element */
    ULONG pasteBufferPos;

    /* An escape/CSI sequence split across several writes is held here until
       the terminating byte arrives, then prepended to the next write. */
    UBYTE pendingCSI[64];
    UWORD pendingCSILen;

    /* Cursor and rendition saved by ESC 7, restored by ESC 8 */
    ULONG fgColor, bgColor, savedFgColor, savedBgColor;
    WORD savedX, savedY;
    BYTE savedFgPen, savedBgPen;
    UBYTE savedTxFlags;
    ULONG underlineColor, savedUnderlineColor; /* color key + 1; zero follows text */
    UBYTE underlineStyle, savedUnderlineStyle; /* private: 0=normal,1=curly,2=dotted,3=dashed */
    BOOL savedCursor;
    BOOL savedWrapPending;
    BOOL savedOrigin;
    UBYTE charsets, savedCharsets; /* bit n: G0..G3 uses DEC special graphics */
    ULONG lastPrintable;
    BOOL hasLastPrintable;
    BOOL renderUTF8;              /* internal output span, never input policy */
    UBYTE activeCharset;          /* SI=G0, SO=G1; independent of saved cursor */
    ULONG extendedPaletteRevision; /* private OSC palette repaint generation */
    struct ConsolengBindingPrefs bindings;
    UBYTE bindingSwallowed[16];
    UBYTE bindingHeldAction[128]; /* original action, one per native key */
    struct ConsolengFontPrefs fontZoomBase; /* appended private zoom snapshot */
    BOOL fontZoomActive, fontZoomBorrowed;
    ULONG wheelLines; /* appended private per-unit wheel distance */
    UBYTE mouseReportedButtons; /* presses successfully delivered to the application */
    struct ConsolengSemanticPrefs semanticPrefs; /* navigation snapshots this policy */
};

/* Dynamic slot identity, distinct from its possibly shared physical pen. */
#define CON_COLOR_ROLE(n) ((n)==CONUNIT_PEN_DEFFG?1:((n)==CONUNIT_PEN_DEFBG?2:((n)<8?3+(n):(CONPEN_ISXTERM(n)&&CONPEN_XTERMIDX(n)<16?11+CONPEN_XTERMIDX(n):(CONPEN_ISBRIGHT(n)?27+CONPEN_BRIGHTIDX(n):0)))))
/* Keep RGB values, not cache pen numbers, in retained cells. Low role byte
 * 0x80 identifies truecolor; 0x81 carries an extended palette key in the same
 * payload. Existing default/ANSI role encodings stay unchanged. */
#define CON_RETAINED_ROLE(n) (CONPEN_ISRGB(n) ? 0x80 : (CONPEN_ISXTERM(n) && CONPEN_XTERMIDX(n)>=16 ? 0x81 : CON_COLOR_ROLE(n)))
#define CON_RETAINED_VALUE(n) ((CONPEN_ISRGB(n) || (CONPEN_ISXTERM(n) && CONPEN_XTERMIDX(n)>=16)) ? ((n) & 0xffffff) : 0)
#define CHARMAP_ROLES(o) (((UQUAD)CON_RETAINED_ROLE(ICU(o)->fgColor)<<8) | \
    CON_RETAINED_ROLE(ICU(o)->bgColor) | \
    ((UQUAD)CON_RETAINED_VALUE(ICU(o)->fgColor)<<16) | \
    ((UQUAD)CON_RETAINED_VALUE(ICU(o)->bgColor)<<40) | \
    ((UQUAD)(ICU(o)->underlineStyle & 1)<<6) | \
    ((UQUAD)(ICU(o)->underlineStyle & 2)<<13))

/* The conFlags */
#define CF_DELAYEDDISPOSE	(1L << 0)
#define CF_DISPOSE		(1L << 1)
/* Unit interprets the write stream (and emits input) as UTF-8 */
#define CF_UTF8			(1L << 2)
/* Unit is a background tab: state updates apply, rendering is
   suppressed, input is not routed here. Cleared via CSI >9l which
   also triggers a full refresh from the cell store. */
#define CF_OBSCURED		(1L << 3)
/* DEC private modes negotiated by modern terminal clients. Focus reports
   are emitted by the device from native activation events; handlers must
   not duplicate them. Clipboard producers own paste framing; generic replies
   stay unframed. Theme notifications still need integration. Classic CON
   is unchanged until mode negotiation. */
#define CF_BRACKETED_PASTE	(1L << 4)
#define CF_FOCUS_REPORTING	(1L << 5)
#define CF_COLOR_SCHEME_NOTIFY	(1L << 6)
/* A printable character filled the rightmost cell while autowrap was
   enabled.  VT terminals defer the actual wrap until the next printable
   character; cursor/control commands cancel the pending wrap. */
#define CF_WRAP_PENDING		(1L << 7)
/* DECCKM: application cursor keys use SS3 instead of CSI. */
#define CF_APPLICATION_CURSOR   (1L << 8)
/* DECKPAM/DECKPNM state, available to keypad bindings. */
#define CF_APPLICATION_KEYPAD   (1L << 9)
#define CF_INSERT_MODE          (1L << 10)
#define CF_ORIGIN_MODE          (1L << 11)
#define CF_ALTERNATE_SCROLL     (1L << 12)

#define CON_IS_OBSCURED(o) (ICU(o)->conFlags & CF_OBSCURED)

#define CON_IS_UTF8(o) (ICU(o)->conFlags & CF_UTF8)
#define CON_TEXT_IS_UTF8(o) (CON_IS_UTF8(o) || ICU(o)->renderUTF8)

#if 0
/* Determining whether linefeed (LF==LF+CR) mode is on */
#define CF_LF_MODE_ON		(1L << 2)
#endif

struct cdihMessage
{
    struct Message msg;
    /* The unit that the user input should go to */
    Object *unit;
    UQUAD unitGeneration;

    struct InputEvent ie;
    LONG mouseX, mouseY; /* Intuition position at event receipt */
};

/* Data passed to the console device input handler */
struct cdihData
{
    /* Port to which we send input to the console device
       from the console device input handler.

       The handoff is asynchronous: one cdihMessage is allocated per
       forwarded event and the console task frees it after processing.
       The input handler must never wait for the console task: the task
       renders (RectFill needs the layer lock), and during an interactive
       window drag intuition holds LockLayers() across many input events,
       so a synchronous round-trip deadlocks the whole input chain.
     */
    struct MsgPort *inputPort;
};



/*****************
**  Prototypes  **
*****************/

struct Interrupt *initCDIH(struct ConsoleBase *ConsoleDevice);
VOID cleanupCDIH(struct Interrupt *cdihandler,
    struct ConsoleBase *ConsoleDevice);

VOID consoleTaskEntry(struct ConsoleBase *ConsoleDevice);

struct Task *createConsoleTask(APTR taskparams,
    struct ConsoleBase *ConsoleDevice);

/* Prototypes */
VOID con_keyboard_dispose(struct ConUnit *unit);
VOID con_keyboard_switch(struct ConUnit *unit, BOOL alternate);
ULONG con_keyboard_csi(struct ConUnit *unit, const UBYTE *s, ULONG n, struct ConsoleBase *base);
LONG con_keyboard_event(struct ConUnit *unit, const struct InputEvent *event, UBYTE *out, struct ConsoleBase *base);
BOOL con_sync_active(struct ConUnit *unit);
BOOL con_sync_buffering(struct ConUnit *unit);
VOID con_sync_set(struct ConUnit *unit, struct ConsoleBase *base, BOOL enabled);
VOID con_sync_dispose(struct ConUnit *unit);
VOID con_sync_poll(struct ConsoleBase *base);
ULONG con_parse_output(struct ConUnit *unit, STRPTR buf, ULONG length,
    struct ConsoleBase *base, LONG *error);
ULONG writeToConsole(struct ConUnit *unit, STRPTR buf, ULONG towrite,
    struct ConsoleBase *ConsoleDevice, LONG *error);

Class *makeConsoleClass(struct ConsoleBase *ConsoleDevice);
Class *makeStdConClass(struct ConsoleBase *ConsoleDevice);
Class *makeCharMapConClass(struct ConsoleBase *ConsoleDevice);
Class *makeSnipMapConClass(struct ConsoleBase *ConsoleDevice);

VOID con_inject(struct ConsoleBase *ConsoleDevice, struct ConUnit *cu,
    const UBYTE *data, LONG size);
LONG con_copy_clipboard_request(struct ConsoleBase *ConsoleDevice,
    const UBYTE *bytes, ULONG length);
VOID con_copy_clipboard_poll(VOID);
VOID con_osc_clipboard_reset(struct intConUnit *unit);
VOID con_osc_clipboard_byte(struct intConUnit *unit, UBYTE byte);
VOID con_osc_clipboard_apply(struct ConsoleBase *base, struct intConUnit *unit);
BOOL con_osc_clipboard_read(struct ConsoleBase *base, struct intConUnit *unit, BOOL st);
BOOL con_osc_clipboard_utf8(const UBYTE *bytes, ULONG length);
BOOL con_osc_read_request(struct ConsoleBase *base, Object *unit, UBYTE target, BOOL st);
VOID con_osc_read_cancel(Object *unit);
VOID con_osc_read_poll(struct ConsoleBase *base,
    BOOL (*valid_unit)(Object *, UQUAD, struct ConsoleBase *));
BOOL con_query_clipboard_request(struct ConsoleBase *ConsoleDevice, Object *unit, UQUAD revision);
VOID con_query_clipboard_poll(struct ConsoleBase *ConsoleDevice,
    BOOL (*valid_unit)(Object *, UQUAD, struct ConsoleBase *));
VOID con_inject_paste(struct ConsoleBase *ConsoleDevice, struct ConUnit *cu,
    const UBYTE *data, LONG size);

VOID printstring(STRPTR string, ULONG len,
    struct ConsoleBase *ConsoleDevice);

/* UTF-8 helpers (support.c) shared by the parser and the render classes */
ULONG con_utf8_decode(const UBYTE **sp, LONG *left);
ULONG con_utf8_encode(UBYTE *buf, ULONG cp);
ULONG con_utf8_count(const UBYTE *s, LONG len);
WORD con_cp_width(ULONG cp);
ULONG con_utf8_cellcount(const UBYTE *s, LONG len);

/* Cell value marking the second column of a double-width glyph */
#define CELL_WIDE_CONT 0xFFFFFFFEUL

VOID setabpen(struct Library *GfxBase, struct RastPort *rp, UBYTE tflags,
    UBYTE FgPen, UBYTE BgPen);

/* The caller keeps its original window font alive while using the window.
 * All units borrow that same baseline, never a sibling's OSC-owned font. */
struct ConsolengFontWindow
{
    struct MinNode node;
    struct Window *window;
    struct TextFont *original;
    ULONG users;
};

struct ConsoleBase
{
    struct Device device;

    struct MinList fontWindows;
    struct SignalSemaphore fontWindowLock;
    struct MinList unitList;
    struct SignalSemaphore unitListLock;
    UQUAD nextInputGeneration; /* assigned under unitListLock */
    struct SignalSemaphore consoleTaskLock;
    struct ConsolengEmojiPrefs emojiDefaults;
    struct ConsolengBellPrefs bellDefaults;
    struct ConsolengClipboardPrefs clipboardDefaults;
    struct ConsolengColorPrefs colorDefaults;
    struct ConsolengFontPrefs fontDefaults;
    struct ConsolengHistoryPrefs historyDefaults; /* guarded by unitListLock */
    struct ConsolengCursorPrefs cursorDefaults; /* guarded by unitListLock */
    struct ConsolengCursorColorPrefs cursorColorDefaults;

    /* Unicode glyph rendering above Latin-1 (consolengglyph.c). This device
       opens its OWN outline engine for the font in use, because diskfont
       closes its engine after pre-rendering codepoints 0-255 and nothing can
       rasterise a higher one afterwards. Cached because binding costs a file
       read; guarded because units render from more than one task. Kept on the
       device rather than in file statics so a second unit or a font change
       cannot corrupt another unit's engine. */
    struct SignalSemaphore glyphLock;
    struct Library     *glyphEngineBase;    /* the outline engine library */
    struct GlyphEngine *glyphEngine;
    struct TextFont    *glyphFont;          /* font the engine is bound to */
    APTR                glyphOTag;          /* struct ConsolengOTag * */
    UBYTE              *glyphMask;          /* 1-bit scratch for BltTemplate */
    ULONG               glyphMaskSize;
    BOOL                glyphFailed;        /* this font has no engine */
    LONG                glyphWhy;           /* CONSOLENG_GLYPH_* reason */
    struct Library     *emojiEngineBase;
    struct GlyphEngine *emojiEngine;
    struct Library     *emojiCyberBase;
    BOOL                emojiFailed;        /* retry after font binding reset */
    struct Library     *cjkEngineBase;
    struct GlyphEngine *cjkEngine;
    BOOL                cjkFailed;

    struct Interrupt *inputHandler;
    /* Second input handler, BELOW intuition (priority < 50). Intuition
       delivers window events (IECLASS_SIZEWINDOW, IECLASS_REFRESHWINDOW,
       IECLASS_CLOSEWINDOW, IECLASS_GADGETDOWN/UP) for IDCMP-less windows
       by generating input events that only handlers below priority 50 can
       see, so the priority-51 handler above never receives them. */
    struct Interrupt *winEventHandler;
    struct Task *consoleTask;
    struct MsgPort *commandPort;

    /* Queued read requests */
    struct MinList readRequests;

    Class *consoleClass;
    Class *stdConClass;
    Class *charMapClass;
    Class *snipMapClass;

    struct cdihData consIHData;
    /* Copy buffer

       This buffer is shared across all console units. It is used by
       conunits of type CONU_SNIPMAP to provide built in copy/paste.

       If ConClip is running, this buffer is passed to ConClip on copy,
       and is not used on Paste. Instead, <CSI> 0 v (Hex 9B 30 20 76)
       is put into the console units input buffer to signal to the app
       (console handler or otherwise) to paste.

       Access to the copyBuffer is protected by a semaphore.
     */
    const char *copyBuffer;
    ULONG copyBufferSize;
    struct SignalSemaphore copyBufferLock;
    struct MinList sniphooks;

    /* Windows this device opened FOR ITSELF, because the opener passed none.
       See the long note at ConsolengOpenOwnWindow() in console.c: it is what
       lets the stock con-handler's plain-device mount route a CON:-style
       stream here with no change to rom/filesys/console_handler. Only
       self-opened windows go on this list, so a caller-supplied window is
       never closed out from under its owner. Guarded by unitListLock, which
       already covers unit create/dispose. */
    struct MinList ownWindows;

    struct Library *cb_IntuitionBase;
    struct Library *cb_KeymapBase;
    struct Library *cb_UtilityBase;
    struct ConsolengBindingPrefs bindingDefaults;
    ULONG wheelLinesDefault; /* guarded by unitListLock */
    struct ConsolengSemanticPrefs semanticDefaults; /* guarded by unitListLock */
};

/* One self-opened window, tied to the unit it was opened for.  font is the
   outline font installed on its RastPort (NULL when the screen font had to
   do); closed after the window. */
struct ConsolengOwnWin
{
    struct MinNode   node;
    struct Unit     *unit;
    struct Window   *win;
    struct TextFont *font;
};

#undef CB
#define CB(x) ((struct ConsoleBase *)x)

#undef IntuitionBase
#define IntuitionBase (((const struct ConsoleBase *)ConsoleDevice)->cb_IntuitionBase)

#undef KeymapBase
#define KeymapBase (((const struct ConsoleBase *)ConsoleDevice)->cb_KeymapBase)

#undef UtilityBase
#define UtilityBase (((const struct ConsoleBase *)ConsoleDevice)->cb_UtilityBase)

/* Device-local stand-in for graphics.library/TextUTF8(), which this tree does
 * not have and which this device deliberately does not add - see
 * consolengtext.c for what that costs. The macro keeps the existing call sites
 * in stdconclass.c and charmapconclass.c unchanged; it picks GfxBase out of
 * scope exactly as the Text() macro does under __GRAPHICS_NOLIBBASE__. */
/* GfxBase is a struct Library * throughout this device (see setabpen above),
 * not a struct GfxBase *. */
UBYTE ConsoleNGFaintPen(Object *o, struct Library *GfxBase, UBYTE flags,
    UBYTE foreground, ULONG key);

/* Private retained role bits 6 and 14 carry underline style; color role
 * identities use 0..34,0x80,0x81 and must mask off bit6 when decoded. */
void ConsoleNGColorUnderline(Object *o, struct Library *GfxBase, struct RastPort *rp,
    UBYTE flags, LONG x, LONG y, LONG width, LONG height, UBYTE style, ULONG color);
void ConsoleNGStyledUnderline(struct Library *GfxBase, struct RastPort *rp,
    UBYTE flags, LONG x, LONG y, LONG width, LONG height, UBYTE fg, UBYTE bg, UBYTE style);
void ConsoleNGStrike(struct Library *GfxBase, struct RastPort *rp,
    UBYTE flags, LONG x, LONG y, LONG width, LONG height, UBYTE fg, UBYTE bg);

void ConsoleNGTextUTF8(struct Library *GfxBase,
                       struct ConsoleBase *ConsoleDevice,
                       struct RastPort *rp,
                       CONST_STRPTR string, ULONG byteLen, BOOL emoji);

/* consolengglyph.c: this device's own Unicode glyph path above Latin-1.
 *
 * ConsolengGlyphDraw retains detailed failure results for internal callers and
 * diagnostic probes. Normal text output uses a cell-bounded missing-glyph box
 * rather than exposing those numeric/letter codes as apparent document text. */
#define CONSOLENG_GLYPH_OK             0
#define CONSOLENG_GLYPH_NOFONT         1   /* no rastport font / no name */
#define CONSOLENG_GLYPH_NOOTAG         2   /* no .otag: bitmap font */
#define CONSOLENG_GLYPH_NOENGINENAME   3   /* .otag has no OT_Engine */
#define CONSOLENG_GLYPH_NOLIB          4   /* engine .library would not open */
#define CONSOLENG_GLYPH_NOOPENENGINE   5   /* OpenEngine() failed */
#define CONSOLENG_GLYPH_SETINFO        6   /* SetInfoA rejected otag/size */
#define CONSOLENG_GLYPH_NOGLYPH        7   /* ObtainInfoA left gm NULL */
#define CONSOLENG_GLYPH_NOMEM          8
#define CONSOLENG_GLYPH_GM_NOBITMAP    9   /* gm returned, glm_BitMap NULL */
#define CONSOLENG_GLYPH_GM_EMPTY      10   /* gm+bitmap, zero black box ('A') */
#define CONSOLENG_GLYPH_MASKFAIL      11   /* mask build failed ('B') */
#define CONSOLENG_GLYPH_SETCODE       12   /* engine rejected OT_GlyphCode ('C') */
#define CONSOLENG_GLYPH_NOFACE        13   /* engine has no usable face ('D') */
/* These two come from ObtainInfoA's OWN return code rather than from gm being
   NULL, and they point at opposite halves of the system - 'E' means the face
   never opened or the instance (point size / DPI) was rejected, so the
   codepoint is irrelevant; 'F' means the face is open and working and simply
   does not cover this codepoint. See the long note at the gm==NULL branch in
   consolengglyph.c. */
#define CONSOLENG_GLYPH_OBT_NOFACE    14   /* OTERR_Failure: no face/instance ('E') */
#define CONSOLENG_GLYPH_OBT_NOCP      15   /* OTERR_UnknownGlyph: face ok ('F') */

LONG ConsolengGlyphDraw(struct Library *GfxBase,
                        struct ConsoleBase *ConsoleDevice,
                        struct RastPort *rp, ULONG cp, LONG x, LONG base_y, BOOL emoji);
void ConsolengGlyphUnbind(struct ConsoleBase *ConsoleDevice);
void ConsolengCloseFont(struct ConsoleBase *device, struct TextFont *font);
#define TextUTF8(rp, string, byteLen) \
    ConsoleNGTextUTF8(GfxBase, ConsoleDevice, (rp), (string), (byteLen), !ICU(o)->emojiDisabled)

struct charmap_line;
BOOL con_url_run(struct charmap_line *line, ULONG column, ULONG *end);
BOOL con_url_in_row(struct charmap_line *line);
BOOL con_url_at(struct charmap_line *line, ULONG column, UBYTE *url, ULONG capacity);
BOOL con_open_url(const UBYTE *url);
ULONG con_mouse_encode(BOOL sgr, BOOL utf8, ULONG button, BOOL release, LONG x, LONG y,
    UBYTE *out, ULONG capacity);


/* Native logical-key shortcut policy, serialized by consoleTaskLock. */
BOOL con_bindings_valid(const struct ConsolengBindingPrefs *p);
VOID con_bindings_defaults(struct ConsolengBindingPrefs *p);
ULONG con_binding_action(const struct ConsolengBindingPrefs *p,
    const struct InputEvent *event, struct ConsoleBase *ConsoleDevice);
ULONG con_binding_action_in_context(const struct ConsolengBindingPrefs *p,
    const struct InputEvent *event, struct ConsoleBase *ConsoleDevice,ULONG context);
ULONG con_binding_action_in_mode(const struct ConsolengBindingPrefs *p,
    const struct InputEvent *event, struct ConsoleBase *ConsoleDevice,BOOL navigating);
BOOL con_binding_release(UBYTE *held, const struct InputEvent *event);
VOID con_binding_hold(UBYTE *held, const struct InputEvent *event);
BOOL con_binding_repeat(ULONG original, ULONG current, const struct InputEvent *event);

#endif /* CONSOLE_GCC_H */
