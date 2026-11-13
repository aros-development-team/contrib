#ifndef DEVICES_CONSOLENG_H
#define DEVICES_CONSOLENG_H

/*
    Copyright (C) 1995-2026, The AROS Development Team. All rights reserved.

    Desc: consoleng.device's own additions to the console protocol.
*/

/* This is deliberately NOT a copy of <devices/console.h>. The stock
 * console.device owns that header and consoleng.device does not modify it, so
 * this file holds only what this device adds on top. It also must not reuse
 * the DEVICES_CONSOLE_H guard: <devices/conunit.h> already pulls the public
 * header in, so a duplicate guard silently guards this file out and the
 * additions vanish - which is exactly how the first build of this device
 * failed, with CD_ASKTITLE "undeclared" despite being right here.
 */

#include <devices/console.h>

/* Copy the unit title (as set by OSC 0/2) into io_Data. Ink-style TUIs set and
 * read the terminal title; the stock console has no equivalent command. */
#ifndef CD_ASKTITLE
#define CD_ASKTITLE             (CMD_NONSTD + 4)
#endif

/* Per-unit settings for a native preferences editor. GET ignores input bytes;
 * SET validates a complete v1 record before changing anything. io_Length is
 * capacity, io_Actual is the record size on success and zero on rejection.
 * This in-memory API record is not an on-disk format. These commands do not
 * save files or change other consoles/global defaults. */
#define CD_GETCURSORPREFS (CMD_NONSTD + 6)
#define CD_SETCURSORPREFS (CMD_NONSTD + 7)
/* Library-unit commands change defaults for FUTURE console units only.
 * Open CONU_LIBRARY to use these commands; existing units keep their settings.
 * Defaults last for this loaded device instance. The Preferences program owns
 * disk persistence and startup restoration, never the device task. */
#define CD_GETDEFAULTCURSORPREFS (CMD_NONSTD + 8)
#define CD_SETDEFAULTCURSORPREFS (CMD_NONSTD + 9)
#define CONSOLENG_CURSOR_PREFS_VERSION 1
#define CONSOLENG_ERR_BADPREFS 1
#define CONSOLENG_BLINK_MIN_MS 50
#define CONSOLENG_BLINK_MAX_MS 5000
#define CONSOLENG_TIMEOUT_MAX_MS 3600000
struct ConsolengCursorPrefs
{
    ULONG cp_Size;
    ULONG cp_Version;
    ULONG cp_Style;             /* DECSCUSR 0..6 */
    ULONG cp_BlinkIntervalMS;   /* requested phase, 50..5000; timer-granularity limited */
    ULONG cp_IdleTimeoutMS;     /* 0 disables; otherwise 1000..3600000 */
};

/* History budget belongs to the primary screen. Setting while DEC1049 is
 * active updates the saved primary budget, never enables alternate history.
 * Shrinking takes effect at the next history append; queries/edits do not
 * move a browsed viewport. Standard (non-charmap) units return NOCMD.
 * Library-unit defaults are shared device defaults, NOT per-user settings. */
#define CD_GETHISTORYPREFS (CMD_NONSTD + 10)
#define CD_SETHISTORYPREFS (CMD_NONSTD + 11)
#define CD_GETDEFAULTHISTORYPREFS (CMD_NONSTD + 12)
#define CD_SETDEFAULTHISTORYPREFS (CMD_NONSTD + 13)
#define CONSOLENG_HISTORY_PREFS_VERSION 1
/* Leave room for visible rows and signed position arithmetic. */
#define CONSOLENG_HISTORY_MAX_LINES 0x7fff0000UL
struct ConsolengHistoryPrefs
{
    ULONG hp_Size;
    ULONG hp_Version;
    ULONG hp_Lines; /* zero keeps only visible rows; storage allocated on demand */
};

/* Commit/query the editor's defaults as one snapshot. Individual legacy
 * cursor/history commands remain available and retain their command numbers. */
#define CD_GETDEFAULTPREFS (CMD_NONSTD + 14)
#define CD_SETDEFAULTPREFS (CMD_NONSTD + 15)
struct ConsolengDefaultPrefs
{
    ULONG dp_Size;
    ULONG dp_Version; /* 1 */
    struct ConsolengCursorPrefs dp_Cursor;
    struct ConsolengHistoryPrefs dp_History;
};

/* Additive font/profile API; older records and command numbers are unchanged.
 * Empty name with zero pixels inherits the caller/window default. */
#define CD_GETFONTPREFS (CMD_NONSTD + 16)
#define CD_SETFONTPREFS (CMD_NONSTD + 17)
#define CD_GETDEFAULTPROFILE (CMD_NONSTD + 18)
#define CD_SETDEFAULTPROFILE (CMD_NONSTD + 19)
#define CD_VALIDATEFONTPREFS (CMD_NONSTD + 20) /* library unit, no mutation */
#define CONSOLENG_FONT_NAME_SIZE 128
struct ConsolengFontPrefs
{
    ULONG fp_Size;
    ULONG fp_Version;
    ULONG fp_Pixels;
    char fp_Name[CONSOLENG_FONT_NAME_SIZE];
};
struct ConsolengDefaultProfile
{
    ULONG dp_Size;
    ULONG dp_Version;
    struct ConsolengCursorPrefs dp_Cursor;
    struct ConsolengHistoryPrefs dp_History;
    struct ConsolengFontPrefs dp_Font;
};

/* RGB values are 0xRRGGBB. Slots 0/1 are default foreground/background,
 * slots 2..17 ANSI 0..15. A clear mask bit inherits that slot's baseline. */
#define CD_GETCOLORPREFS (CMD_NONSTD + 21)
#define CD_SETCOLORPREFS (CMD_NONSTD + 22)
#define CD_GETTHEMEPROFILE (CMD_NONSTD + 23)
#define CD_SETTHEMEPROFILE (CMD_NONSTD + 24)
/* Read-only per-unit negotiated paste mode, returned as one ULONG (0 or 1).
 * Handlers use this when accepting clipboard text; older devices return NOCMD.
 * io_Actual is sizeof(ULONG) on success. Not a preference or library default. */
#define CD_GETPASTEMODE (CMD_NONSTD + 25)
#define CONSOLENG_COLOR_COUNT 18
#define CONSOLENG_COLOR_MASK 0x3ffffUL
struct ConsolengColorPrefs {
    ULONG cp_Size, cp_Version, cp_Mask;
    ULONG cp_RGB[CONSOLENG_COLOR_COUNT];
};
struct ConsolengThemeProfile {
    ULONG tp_Size, tp_Version;
    struct ConsolengCursorPrefs tp_Cursor;
    struct ConsolengHistoryPrefs tp_History;
    struct ConsolengFontPrefs tp_Font;
    struct ConsolengColorPrefs tp_Colors;
};
/* Independent per-console rendering policy and defaults for new units.
 * Existing theme profiles retain their v1 layout and do not alter this policy. */
#define CD_GETEMOJIPREFS (CMD_NONSTD + 26)
#define CD_SETEMOJIPREFS (CMD_NONSTD + 27)
#define CD_GETDEFAULTEMOJIPREFS (CMD_NONSTD + 28)
#define CD_SETDEFAULTEMOJIPREFS (CMD_NONSTD + 29)
struct ConsolengEmojiPrefs {
    ULONG ep_Size, ep_Version, ep_Enabled; /* version 1; enabled is exactly 0/1 */
};
#define CD_GETRENDERPROFILE (CMD_NONSTD + 30)
#define CD_SETRENDERPROFILE (CMD_NONSTD + 31)
struct ConsolengRenderProfile {
    ULONG rp_Size, rp_Version;
    struct ConsolengThemeProfile rp_Theme;
    struct ConsolengEmojiPrefs rp_Emoji;
};
/* Additive visual-bell policy and atomic editor snapshot. All earlier
 * commands and record layouts remain unchanged. Enabled is exactly 0/1. */
#define CD_GETBELLPREFS (CMD_NONSTD + 32)
#define CD_SETBELLPREFS (CMD_NONSTD + 33)
#define CD_GETDEFAULTBELLPREFS (CMD_NONSTD + 34)
#define CD_SETDEFAULTBELLPREFS (CMD_NONSTD + 35)
#define CD_GETSESSIONPROFILE (CMD_NONSTD + 36)
#define CD_SETSESSIONPROFILE (CMD_NONSTD + 37)
struct ConsolengBellPrefs { ULONG bp_Size, bp_Version, bp_Enabled; };
struct ConsolengSessionProfile {
    ULONG rp_Size, rp_Version;
    struct ConsolengThemeProfile rp_Theme;
    struct ConsolengEmojiPrefs rp_Emoji;
    struct ConsolengBellPrefs rp_Bell;
};
/* OSC52 permission record. Reads are opt-in and disabled by default.
 * Older session/render records and command numbers retain their layout. */
#define CD_GETCLIPBOARDPREFS (CMD_NONSTD + 38)
#define CD_SETCLIPBOARDPREFS (CMD_NONSTD + 39)
#define CD_GETDEFAULTCLIPBOARDPREFS (CMD_NONSTD + 40)
#define CD_SETDEFAULTCLIPBOARDPREFS (CMD_NONSTD + 41)
#define CD_GETTERMINALPROFILE (CMD_NONSTD + 42)
#define CD_SETTERMINALPROFILE (CMD_NONSTD + 43)
struct ConsolengClipboardPrefs { ULONG cbp_Size, cbp_Version, cbp_Enabled; };
/* Clipboard version 2 permissions; version 1 remains a copy-only boolean. */
#define CONSOLENG_CLIPBOARD_COPY 1
#define CONSOLENG_CLIPBOARD_READ 2
struct ConsolengTerminalProfile {
    ULONG rp_Size, rp_Version;
    struct ConsolengThemeProfile rp_Theme;
    struct ConsolengEmojiPrefs rp_Emoji;
    struct ConsolengBellPrefs rp_Bell;
    struct ConsolengClipboardPrefs rp_Clipboard;
};
/* Append-only cursor color controls; existing records keep their layouts.
 * Enabled=0 selects inversion; RGB retains the editor's selected color.
 * Appearance is an atomic defaults snapshot, not a live-session broadcast. */
#define CD_GETCURSORCOLORPREFS (CMD_NONSTD + 44)
#define CD_SETCURSORCOLORPREFS (CMD_NONSTD + 45)
#define CD_GETDEFAULTCURSORCOLORPREFS (CMD_NONSTD + 46)
#define CD_SETDEFAULTCURSORCOLORPREFS (CMD_NONSTD + 47)
#define CD_GETAPPEARANCEPROFILE (CMD_NONSTD + 48)
#define CD_SETAPPEARANCEPROFILE (CMD_NONSTD + 49)
struct ConsolengCursorColorPrefs {
    ULONG ccp_Size, ccp_Version, ccp_Enabled, ccp_RGB;
};
struct ConsolengAppearanceProfile {
    ULONG rp_Size, rp_Version;
    struct ConsolengThemeProfile rp_Theme;
    struct ConsolengEmojiPrefs rp_Emoji;
    struct ConsolengBellPrefs rp_Bell;
    struct ConsolengClipboardPrefs rp_Clipboard;
    struct ConsolengCursorColorPrefs rp_CursorColor;
};
/* Version 1 logical-key bindings. Keys are unmodified native Latin-1
 * characters (ASCII letters lowercase), independent of physical layout.
 * Count zero disables shortcuts. Unused entries must be zero. */
#define CD_GETBINDINGPREFS (CMD_NONSTD + 50)
#define CD_SETBINDINGPREFS (CMD_NONSTD + 51)
#define CD_GETDEFAULTBINDINGPREFS (CMD_NONSTD + 52)
#define CD_SETDEFAULTBINDINGPREFS (CMD_NONSTD + 53)
/* Version 2 additionally accepts named native special keys as RAW + code.
 * Explicit physical matches take priority over logical keymap characters. */
#define CONSOLENG_BIND_RAW 0x10000UL
#define CONSOLENG_BINDING_MAX 16
#define CONSOLENG_BIND_SHIFT 1
#define CONSOLENG_BIND_ALT 2
#define CONSOLENG_BIND_CTRL 4
#define CONSOLENG_BIND_LAMIGA 8
#define CONSOLENG_BIND_RAMIGA 16
/* Version 12 adds mutually exclusive mode conditions in kb_Modifiers.
 * Navigation excludes query editing. Zero keeps the legacy always-active rule. */
#define CONSOLENG_BIND_NAVONLY 32
#define CONSOLENG_BIND_NOTNAV 64
#define CONSOLENG_BIND_MODEMASK 96
/* Version 18 adds independent required/excluded terminal-state pairs. */
#define CONSOLENG_BIND_ALTSCREEN 128
#define CONSOLENG_BIND_PRIMARY 256
#define CONSOLENG_BIND_APPCURSOR 512
#define CONSOLENG_BIND_NORMALCURSOR 1024
#define CONSOLENG_BIND_APPKEYPAD 2048
#define CONSOLENG_BIND_NORMALKEYPAD 4096
#define CONSOLENG_BIND_SEARCHMODE 8192
#define CONSOLENG_BIND_NOTSEARCH 16384
#define CONSOLENG_BIND_CONTEXTMASK 32736
#define CONSOLENG_BIND_COPY 1
#define CONSOLENG_BIND_PASTE 2
#define CONSOLENG_BIND_SELECTALL 3
#define CONSOLENG_BIND_SEARCH 4
/* Version 3 adds local history navigation; existing action values stay fixed. */
#define CONSOLENG_BIND_LINEUP 5
#define CONSOLENG_BIND_LINEDOWN 6
#define CONSOLENG_BIND_PAGEUP 7
#define CONSOLENG_BIND_PAGEDOWN 8
#define CONSOLENG_BIND_TOP 9
#define CONSOLENG_BIND_BOTTOM 10
/* Version 4 adds reverse search without renumbering existing actions.
 * Version 5 adds physical character and keypad bindings.
 * Version 6 adds retained-text navigation. */
#define CONSOLENG_BIND_SEARCH_BACKWARD 11
#define CONSOLENG_BIND_NAVIGATE 12
/* Version 7 adds an optional single-shot history clear; existing values stay fixed. */
#define CONSOLENG_BIND_CLEARHISTORY 13
/* Version 8 adds font zoom actions; existing records keep their layout. */
#define CONSOLENG_BIND_FONTLARGER 14
#define CONSOLENG_BIND_FONTSMALLER 15
#define CONSOLENG_BIND_FONTRESET 16
/* Version 9 adds viewport half-page history actions. */
#define CONSOLENG_BIND_HALFUP 17
#define CONSOLENG_BIND_HALFDOWN 18
/* Version 10 adds clear selection without leaving retained navigation. */
#define CONSOLENG_BIND_CLEARSELECTION 19
/* Version 11 adds single-shot retained-navigation actions. Outside navigation
 * these shortcuts are consumed without changing the application input. */
#define CONSOLENG_BIND_NAVCENTER 20
#define CONSOLENG_BIND_NAVOPENLINK 21
/* Version 13 adds configurable retained-selection toggles. */
#define CONSOLENG_BIND_NAVSELECTCHAR 22
#define CONSOLENG_BIND_NAVSELECTLINE 23
#define CONSOLENG_BIND_NAVSELECTBLOCK 24
#define CONSOLENG_BIND_NAVSELECTSEMANTIC 25
/* Version 14 adds assignable retained cursor movements. */
#define CONSOLENG_BIND_NAVLEFT 26
#define CONSOLENG_BIND_NAVRIGHT 27
#define CONSOLENG_BIND_NAVUP 28
#define CONSOLENG_BIND_NAVDOWN 29
#define CONSOLENG_BIND_NAVFIRST 30
#define CONSOLENG_BIND_NAVLAST 31
#define CONSOLENG_BIND_NAVTOP 32
#define CONSOLENG_BIND_NAVBOTTOM 33
#define CONSOLENG_BIND_NAVWORDNEXT 34
#define CONSOLENG_BIND_NAVWORDEND 35
#define CONSOLENG_BIND_NAVWORDPREV 36
#define CONSOLENG_BIND_NAVSEMANTICNEXT 37
#define CONSOLENG_BIND_NAVSEMANTICEND 38
#define CONSOLENG_BIND_NAVSEMANTICPREV 39
#define CONSOLENG_BIND_NAVPARAGRAPHUP 40
#define CONSOLENG_BIND_NAVPARAGRAPHDOWN 41
#define CONSOLENG_BIND_NAVBRACKET 42
#define CONSOLENG_BIND_NAVOCCUPIED 43
/* Version 15 adds view-relative movements and navigation scrolling. */
#define CONSOLENG_BIND_NAVVIEWTOP 44
#define CONSOLENG_BIND_NAVVIEWMIDDLE 45
#define CONSOLENG_BIND_NAVVIEWBOTTOM 46
#define CONSOLENG_BIND_NAVPAGEUP 47
#define CONSOLENG_BIND_NAVPAGEDOWN 48
#define CONSOLENG_BIND_NAVHALFUP 49
#define CONSOLENG_BIND_NAVHALFDOWN 50
#define CONSOLENG_BIND_NAVLINEUP 51
#define CONSOLENG_BIND_NAVLINEDOWN 52
/* Version 16 adds configurable inline find and repeat actions. */
#define CONSOLENG_BIND_NAVFINDFORWARD 53
#define CONSOLENG_BIND_NAVFINDBACKWARD 54
#define CONSOLENG_BIND_NAVTILLFORWARD 55
#define CONSOLENG_BIND_NAVTILLBACKWARD 56
#define CONSOLENG_BIND_NAVFINDREPEAT 57
#define CONSOLENG_BIND_NAVFINDREVERSE 58
/* Version 17 adds navigation search prompts, word search and repeat. */
#define CONSOLENG_BIND_NAVSEARCHFORWARD 59
#define CONSOLENG_BIND_NAVSEARCHBACKWARD 60
#define CONSOLENG_BIND_NAVSEEDFORWARD 61
#define CONSOLENG_BIND_NAVSEEDBACKWARD 62
#define CONSOLENG_BIND_NAVSEARCHNEXT 63
#define CONSOLENG_BIND_NAVSEARCHPREVIOUS 64
struct ConsolengKeyBinding { ULONG kb_Key, kb_Modifiers, kb_Action; };
struct ConsolengBindingPrefs {
    ULONG bp_Size, bp_Version, bp_Count;
    struct ConsolengKeyBinding bp_Entries[CONSOLENG_BINDING_MAX];
};
#define CD_GETINTERACTIONPROFILE (CMD_NONSTD + 54)
#define CD_SETINTERACTIONPROFILE (CMD_NONSTD + 55)
struct ConsolengInteractionProfile {
    ULONG ip_Size, ip_Version;
    struct ConsolengAppearanceProfile ip_Appearance;
    struct ConsolengBindingPrefs ip_Bindings;
};
/* Local wheel distance and DEC1007 arrow repetition; application mouse
 * reporting remains one report per event. Zero disables local wheel scrolling. */
#define CD_GETSCROLLPREFS (CMD_NONSTD + 56)
#define CD_SETSCROLLPREFS (CMD_NONSTD + 57)
#define CD_GETDEFAULTSCROLLPREFS (CMD_NONSTD + 58)
#define CD_SETDEFAULTSCROLLPREFS (CMD_NONSTD + 59)
#define CONSOLENG_SCROLL_MAX_LINES 100
struct ConsolengScrollPrefs { ULONG sp_Size, sp_Version, sp_Lines; };
/* Combined future-console defaults, including wheel distance. */
#define CD_GETINPUTPROFILE (CMD_NONSTD + 60)
#define CD_SETINPUTPROFILE (CMD_NONSTD + 61)
struct ConsolengInputProfile {
    ULONG ip_Size, ip_Version;
    struct ConsolengInteractionProfile ip_Interaction;
    struct ConsolengScrollPrefs ip_Scroll;
};
/* Semantic word boundaries for navigation, selection and seeded search.
 * Unicode scalar values; an empty set is valid. Existing public layouts stay
 * unchanged. A running navigation operation retains its initial snapshot. */
#define CD_GETSEMANTICPREFS (CMD_NONSTD + 62)
#define CD_SETSEMANTICPREFS (CMD_NONSTD + 63)
#define CD_GETDEFAULTSEMANTICPREFS (CMD_NONSTD + 64)
#define CD_SETDEFAULTSEMANTICPREFS (CMD_NONSTD + 65)
#define CONSOLENG_SEMANTIC_MAX 64
struct ConsolengSemanticPrefs {
    ULONG sp_Size, sp_Version, sp_Count;
    ULONG sp_Chars[CONSOLENG_SEMANTIC_MAX];
};
/* Atomic future-console defaults, including semantic selection policy. */
#define CD_GETSELECTIONPROFILE (CMD_NONSTD + 66)
#define CD_SETSELECTIONPROFILE (CMD_NONSTD + 67)
struct ConsolengSelectionProfile {
    ULONG sp_Size, sp_Version;
    struct ConsolengInputProfile sp_Input;
    struct ConsolengSemanticPrefs sp_Semantic;
};
#endif /* DEVICES_CONSOLENG_H */
