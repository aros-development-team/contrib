# ConsoleNG Preferences coverage

Source audit: 2026-10-21. This is the editor implementation inventory,
not a claim of full Alacritty parity. A control counts as implemented only when
its value reaches the device/handler and survives the relevant persistence and
runtime tests. Application protocol state is not a user preference checkbox.

| Area | Existing implementation | Preferences work |
| --- | --- | --- |
| Cursor | DECSCUSR styles, DEC12 blink, interval and idle deadline; cursor request API | Cursor page; preview on a separate unit, future-unit defaults, versioned profiles |
| Scrollback | Retained charmap lines, scrolling, selection, resize/redraw, primary/alternate buffers; transactional primary resize reflow implemented; native direct-device fixtures | History API and Scrollback page implemented; zero retains only visible rows; changing budget while alternate screen is active changes primary budget only. Wheel lines controls local distance from 0 (disabled) through 100 |
| Font | OSC50 name/size, OpenDiskFont, fixed-width check; owned-window font selection | Font page, native fixed-width requester, size, inherit/reset, Test/Revert and versioned persistence implemented and native-tested. Shared/hidden font ownership and release qualified; Color emoji fallback checkbox, isolated Test/Revert and new-session defaults implemented; fixed installed Noto Sans CJK fallback is native-tested across bitmap/outline primary fonts; configurable fallback faces/locales and broader shaping remain |
| Colors | ANSI 16 preference slots; all 256 OSC palette slots mutable; 256-color and RGB output; OSC4/10/11 and reset paths in stdconclass | Colors page implements foreground/background, 16 palette entries, swatch, RGB sliders and per-slot inheritance; Test/Revert/Use/Save and version3 persistence native-tested. Logical roles prevent shared-pen recoloring. Editable Dark/Light/Ocean/Amber presets and Window defaults pass native editor/preview tests; custom cursor RGB is implemented on the Cursor page, with truecolor native acceptance; displayed indexed-screen acceptance remains; the preferences editor exposes 16 ANSI slots; applications can change all 256 slots through OSC4 |
| Bell | Deferred native all-screen visual flash; direct and DOS cooked/raw acceptance | Bell page enables/disables per-console preview and future-session defaults; Test/Revert/Use/Save/Restore and version5 persistence native-tested. Sound and DEC1042 attention behavior remain |
| Encoding | UTF-8 default, byte-mode switch and fragmented parsing | Initial encoding and compatibility defaults remain; application escape changes must remain effective |
| Wrapping/scrolling | Deferred autowrap, auto-scroll and LF modes | Initial defaults and scroll policy remain; do not overwrite negotiated application state during unrelated edits |
| Selection/clipboard | OSC52 strict UTF-8 copy through1MiB with native ConClip; native selection, logical-line copy across automatic wraps, copy/paste and wheel handling; negotiated ConClip/DOS and direct-device bracketed paste pass native byte comparisons (direct path through 64 KiB) | Clipboard page independently controls OSC52 application copies and reads per preview/future-session defaults. Reads default to disabled and require the snapshot-capable ConClip companion for system clipboard access; native permission and reply tests pass. Selection page controls semantic word separators; other selection behavior and paste policy controls remain; wheel distance is now controlled on Scrollback; bracketed paste is application-negotiated |
| Keyboard | Native keymap, copy/paste shortcuts, application cursor/keypad modes | Keyboard page edits sixteen action/key/modifier slots with duplicate rejection and preview Test/Revert. Binding versions 1 through 19 cover configurable actions, logical keys, named special keys and physical raw keys; actions include forward/backward search, retained navigation, line/page/half-page/top/bottom history, clear history, font zoom, clear selection, navigation centering and link activation. Combined navigation, primary/alternate screen, cursor-key and keypad conditions are available, alongside configurable movement, selection, view, inline-find and navigation-search actions; Search/Not searching conditions are also available; application cursor/keypad toggles are protocol state |
| Titles/windows | OSC0/2 title, geometry query, native resize, hide/reveal handling | Initial title/geometry and title policy remain; shared-window ownership must be qualified; no arbitrary window-size ceiling |
| Tabs | Tab management belongs to the console handler; the current ConsoleNG integration does not provide a tab bar | Dedicated Tabs page must integrate actual tab creation/switch/close/title controls, shared defaults versus per-tab overrides, and font/palette ownership. No placeholder controls or assumption that the device owns the tab bar |
| Focus/paste/color reports | DEC1004 emits native focus-in/out reports with DOS-stream acceptance; DEC2004 is integrated with the private ConClip/DOS handler; direct clipboard fallback also passes; ConClip streaming and snapshot reads pass native protocol tests through 1 MiB; DEC2031 reports configured dark/light scheme changes after successful color preference updates; private DSR996 queries and notification silence/reset cases pass 803 native checks | Preserve application negotiation. Policy controls need distinct semantics rather than forcing reporting on every application |
| Mouse protocols | Native selection/wheel plus DEC1000/1002/1003 tracking and1005 UTF-8/1006 SGR encoder/event routing; native SGR drag/free motion/wheel/Control and Shift selection/copy pass; legacy click passes | Outside left/middle/right-button releases pass all four edges through native DOS in SGR/UTF-8/legacy (36 cases); host tests cover all buttons and capture reset. Native menu strips retain right clicks unless the application sets RMBTRAP; removing the strip restores reporting (12 native cases). Native Alt, requester/sticky-menu combinations and remaining protocols are open. A guest-qualified Alt probe passes left-Alt SGR but fails right-Alt input.device readiness before clicking; application negotiation remains separate from user preferences |
| Search/vi/hints/hyperlinks | Native local search, clipboard query paste, word deletion/clear and bounded query history; plain URLs and OSC8 links qualified | Regex search and extensive retained-text navigation are implemented; see [Search](../SEARCH.md) and [Navigation](../NAVIGATION.md) for exact semantics and native fixture coverage. Semantic delimiters are editable on Selection; configurable hints and remaining Unicode regex syntax remain; do not add nonfunctional pages |
| Profiles/startup | Versioned validated files, atomic replacement, FROM/USE/SAVE/RESTORE; package startup | Profiles through version 29 retain binding versions 1 through 19, cursor color, OSC52 copy permission, bell policy and emoji fallback; the version 17 wrapper enables OSC52 reads; version 19 wraps this payload with wheel distance; version 29 adds semantic separators. Older profiles migrate while preserving omitted settings and clearing read permission; native Zune menus and isolated Test/Cancel behavior implemented. Private RAM-backed SAVE/USE/RESTORE tests pass; physical disk durability is a separate gate |
| Per-user settings | Current library defaults are shared across the loaded device | Reuse AROS user/environment resolution when available; no invented user registry. Per-user files alone do not isolate future-unit defaults. User-aware creation/inheritance is a separate required integration gate |

## Scope and precedence

Current Use/Save targets shared future-console defaults. It does not claim to
be per-user and must say so in the UI. Preview changes only the editor's own
console. Existing application sessions retain their settings.

Planned multi-user precedence: built-in fallback, resolved user/session defaults,
explicit console-open overrides, then application protocol changes. The actual
identity and environment interfaces must come from AROS's multi-user work;
never derive identity from a guessed home path, editor process name, or global
ENV assignment. Persistence runs in the caller's context, not the device task.

The Zune migration must override the generic PrefsEditor Test behavior: the
standard implementation writes ENV:, while this editor promises an isolated
preview. Use/Save use validated snapshots and retain atomic file replacement;
framework Import/Export also need explicit error and overwrite handling.

## Reverse search shortcut

The Keyboard action list includes Search backward alongside Search and the six
history navigation actions. Profiles containing this action use binding record
version 4 / ConsoleNG 11. Existing action numbers, record layout and older saved
profiles remain compatible. Search backward starts at the last retained match;
Enter continues backward and Shift+Enter goes forward. See SEARCH.md.

## Physical-key shortcuts

Choose Physical code (hex) on the Keyboard page and enter two hexadecimal
AROS raw-key digits, for example `23` for the physical letter F position. This
binds a position independently of the active keymap; Character continues to
bind the unshifted mapped character. Physical/named bindings take precedence
when both resolve to the same event. Modifiers remain the five checkboxes.

Character and keypad positions through 40 hex, keypad minus (4A), keypad
parentheses/operators (5A–5E), and the named special keys are accepted.
Modifier/lock keys, mouse-wheel codes and release codes are rejected. Version 5
binding records / ConsoleNG 12 profiles preserve these bindings; older profiles
and existing action IDs remain supported. Disabled rows are omitted on save.

## Retained-text navigation action

Keyboard offers Navigate retained text, saved with binding record version 6 /
ConsoleNG 13. This enters the bounded navigation foundation documented in
NAVIGATION.md; it does not claim complete vi-mode parity.

## Clear scrollback shortcut

Keyboard offers Clear scrollback as an optional action; no shortcut is assigned
by default. It removes saved rows while preserving the live screen and
application cursor. If browsing history, searching or navigating, it first
returns to the live screen and cancels the active search/navigation operation.
It does nothing on an alternate or obscured screen. Holding the shortcut does
not repeatedly clear newly produced history.

Profiles containing this action use binding record version 7 / ConsoleNG 14.
Earlier formats and action numbers remain supported; the record layout and
public device command numbers are unchanged.


## Font zoom shortcuts

Keyboard offers Larger font, Smaller font and Reset font zoom. No default
chords are assigned. Larger/smaller request an exact one-pixel step and repeat
while held; reset is single-shot. Missing sizes, proportional faces and failed
opens leave the installed font unchanged. Bitmap fonts therefore need the
requested size to be available; scalable fixed-width fonts support smooth steps.

Zoom belongs to the current console. The first successful step snapshots its
font name and size, or records the inherited window font. Reset restores that
baseline. A successful OSC50 or font-preferences change ends the old zoom
sequence, so reset cannot undo an application's newer font choice. Search and
navigation close before a font change; a key held through that cancellation
must be released before further zoom presses. Retained text uses the existing
resize/reflow path. Future-console and shared defaults are untouched.

These actions use binding version 8 / ConsoleNG 15 profiles. Existing action
numbers, binding layout and device commands are unchanged.

## Half-page history shortcuts

Keyboard also offers History half page up and History half page down.
They move the ordinary history viewport by half the current number of screen
rows, rounded down, and clamp at the oldest/live endpoints. Resizing changes
the step immediately. They repeat while held, provided the binding has not
changed. No default chords are assigned.

The application cursor and input stream stay unchanged. Alternate and hidden
screens ignore these actions. Active search/navigation retain their own modal
controls. A one-row viewport has a zero-row half-page step, matching the pinned
reference. Profiles using either action require binding version 9 / ConsoleNG
16; existing action numbers and record layouts remain unchanged.

## Clear selection shortcut

Keyboard offers Clear selection as an optional action, with no default chord.
It clears the selection without deleting text, scrolling, moving the application
cursor or leaving retained navigation. Pending navigation copy is cancelled;
previously published clipboard contents remain unchanged. Search query text
and its mode are preserved. Held repeats do not retrigger the action, and its
matching release is consumed even if the binding changes before release.

Binding record version 10 appends action 19 without changing record size or
existing action numbers. Its persistent profile is version 18, leaving version
17 reserved for the existing OSC52-read wrapper. Read-enabled profiles may wrap
version 18. Older files still load; older devices reject the new binding version
rather than interpreting it as another action.

Native GUI acceptance: configure Clear selection as Ctrl+Shift+X in unused slot
5, Save, reopen FROM the saved ENVARC: profile, and verify the action/key/modifiers
are restored. ENV: and guest RAM-backed ENVARC: match and retain the original
four shortcuts. Physical durability and per-user defaults remain separate gates.


## Wheel distance

Scrollback offers Wheel lines (0..100, default 1). Zero disables local wheel
scrolling; nonzero values apply to primary-screen history and DEC1007 alternate
arrow repetition. Application mouse reporting remains independent. Test changes
only the preview, Revert restores its initial value, and Use/Save publishes the
wheel setting together with appearance and bindings under one device lock.

The new combined input-profile commands are appended; earlier command numbers
and public record layouts are unchanged. Legacy interaction-profile requests
retain their exact size and leave wheel defaults alone. Profile version 19 is
a canonical outer wrapper around the existing payload, including optional
OSC52-read permission. Old files retain the caller's wheel setting. Malformed
or nested wheel wrappers are rejected without modifying the editor model.

### Navigation action shortcuts

Keyboard also offers Center navigation cursor and Open navigation link. Assign
a logical character, named special key or physical key with the usual modifiers.
Both actions apply only in retained-text navigation: center matches built-in z,
and open matches Return at the navigation cursor. They consume their shortcut
outside navigation, including query editing, without passing it to the app.
Held repeats do not reopen links; incomplete navigation/query/find operations
must settle before either action runs. The ordinary Return and z controls remain.

Binding version 11 appends actions 20/21 without moving existing values or
changing record sizes. The payload uses ConsoleNG 20, within the existing
ConsoleNG 19 wheel wrapper and optional ConsoleNG 17 OSC52-read wrapper. Older
profiles remain readable; older devices reject unsupported binding versions.

The editor's common change notifications use the widget-array length, including
the final font-size field after the wheel-distance control was added. This
avoids silently omitting pixel-size edits from modified-state notifications.

### Binding context

Each Keyboard slot has Active in: Always, Navigation, or Outside navigation.
Navigation means retained-text navigation while not editing a search query;
query editing belongs to Outside navigation. This filters existing actions; it
does not enable actions that have no effect in that context. For example, bind
F1 to Navigate retained text outside navigation and to Center navigation cursor
inside it. A navigation-only shortcut passes through to the application outside
navigation when no other binding matches.

The same key/modifier chord can have one Navigation and one Outside navigation
assignment. Overlapping assignments, including Always plus either condition,
are rejected. Matching physical/named keys still take priority over logical
characters, but only when their context matches. Binding version12 stores the
condition in previously rejected modifier bits32/64; both bits together remain
invalid. Existing record sizes and device commands stay fixed. Profile payload21
retains those conditions within the established wheel/read wrappers.

### Configurable navigation selection

Keyboard offers Select navigation characters, Select navigation lines, Select
navigation rectangle and Select navigation words. These match the built-in v,
Shift+V, Ctrl+V and Alt+V toggles. Pressing the active type again clears its
selection; choosing another type retains selection and changes its shape.
Held repeats and releases do not toggle it again. Pending navigation operations
must settle first. These actions have no effect outside retained navigation;
use the Navigation condition when their keys should reach the application in
normal input mode.

Binding version 13 appends actions 22 through 25 with unchanged record size,
commands and earlier action values. Profile payload 22 preserves the actions,
mode conditions, wheel setting and OSC52 permissions through the existing
wrappers. Older profile versions remain readable.

### Configurable navigation movements

Keyboard actions beginning Navigate: assign retained cursor movements to any
supported chord: cells, rows, first column, line end, history start/end,
whitespace/semantic word boundaries, paragraphs, matching bracket and first
occupied cell. Held repeats and active selections follow built-in-key behavior.
Navigation conditions leave the chord available to the application outside
navigation; Always consumes it there. Search-query editing never moves the
retained cursor through these bindings.

Binding version 14 appends actions 26 through 43 with unchanged record sizes,
commands and prior action values. Profile payload 23 preserves them through the
existing wrappers and older versions remain readable. Inline-find setup/repeat and seed-search mappings remain separate.

### View movement and scrolling shortcuts

Nine more Navigate actions expose top/middle/bottom visible-row movement and
full-page, half-page or single-line scrolling, up and down. These match the
built-in H/M/L and Ctrl+B/F/U/D/Y/E behavior, including repeats and selection.
The Navigation condition keeps the chord available to apps outside navigation;
Always consumes it there and during query editing without moving retained text.

Binding version 15 appends actions 44..52; profile payload 24 retains them.
Existing action numbers, record sizes and commands remain fixed. The earlier
History scroll actions retain their original behavior; these Navigate actions
specifically use retained-navigation cursor and selection semantics.

### Configurable inline find

Keyboard bindings expose Find forward/backward, Till forward/backward, Repeat
find and Reverse find while navigating retained text. Find and Till wait for
one printable target; Escape cancels target entry. Repeat retains that target
and direction, while Reverse find repeats in the opposite direction. These
bindings share the built-in f/F/t/T and semicolon/comma paths. They do not type
into an application or alter an active search query. Binding version 16 appends
six action values (53..58); profile payload 25 preserves them and existing
wheel/OSC52 wrappers. Older action values and record layouts are unchanged.

### Configurable navigation search

Keyboard bindings expose Search forward/backward, Search word forward/backward,
Next match and Previous match during retained-text navigation. Search opens the
regex query editor; Enter confirms and Escape restores the original navigation
position. Word search uses the existing selection/semantic-word seed path.
Next/Previous repeat the stored query in its original/opposite direction.
These shortcuts consume their chord in an already-open query editor without
changing its text or restarting the query. Binding version 17 appends actions
59..64; profile payload 26 retains them without changing older action values,
record layouts or wheel/OSC52 wrapper formats.

### Combined terminal-state conditions

Keyboard adds Screen (any/alternate/primary), Cursor keys and Keypad
(any/application/normal) conditions alongside Active in Navigation/Outside
navigation. All chosen conditions must match. The editor rejects conflicting
shortcuts when their key/modifier and condition ranges overlap; complementary
conditions can share a chord. Query editing is outside navigation, but retains
its actual screen/cursor/keypad state. A consumed key remains consumed through
repeat/release even if the terminal changes modes while it is held.

Binding version 18 uses previously unused bits in the existing ULONG modifier
field; record sizes, previous bits and action values stay unchanged. Profile
payload 27 retains all combinations with the existing wheel/OSC52 wrappers.

## Search-mode shortcut conditions

The Keyboard page combines Search: Any/Searching/Not searching with the other
conditions. Searching means the ordinary query editor or navigation's query
editor is open. Navigation without a query is not Searching. Version 19
bindings allow Navigation and Searching simultaneously; versions 12 through
18 keep their earlier behavior that excludes query editing from Navigation.
Using either Search condition selects version 19 and profile payload 28.
Existing record layouts and action values are unchanged.


## Word separators

The Selection page controls semantic word navigation, word selection, and seeded
word search. Enter up to 64 unique Unicode characters. `\t`, `\n`, `\r`,
`\\`, `\u2502`, and `\U0001f600` represent characters which may be inconvenient
to enter directly. Duplicate characters, malformed UTF-8 or escapes, zero,
surrogates, and values above U+10FFFF are rejected without applying settings.
An empty field disables explicit separators; it does not remove row boundaries.
Cleared cells containing spaces are part of a word when space is not a separator.

Test changes the preview unit; Revert restores its initial policy. Use and Save
publish combined appearance, input and separator defaults atomically for future
consoles. Existing consoles keep their policy, and active navigation retains the
snapshot taken when it opened. New navigation on a changed preview uses the new
policy. Version 29 profiles preserve the policy, including an explicitly empty
set; importing an older profile preserves the supplied existing policy.
