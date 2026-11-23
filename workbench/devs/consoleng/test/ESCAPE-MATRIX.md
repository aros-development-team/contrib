# Pinned escape acceptance inventory

Reference Alacritty `542147067dd016fc5f49120ef0ba605a98a4e3a7`,
`extra/man/alacritty-escapes.7.scd`. Reference status is not ConsoleNG status.
Refreshed against source and retained native acceptance on 2026-10-19.
Every row requires native acceptance; private-mode parameters need separate cases.
Alacritty-rejected controls require matching rejection/ignore behavior, not new support.

| Escape | Reference | Reference scope | ConsoleNG acceptance |
| --- | --- | --- | --- |
| `ESC (` | Implemented | Only charsets *B* and *0* are supported | Native fragmented designation passes; G0/G1 pixel equivalence and G2/G3 isolation |
| `ESC )` | Implemented | Only charsets *B* and *0* are supported | Native fragmented designation passes; G0/G1 pixel equivalence and G2/G3 isolation |
| `ESC \*` | Implemented | Only charsets *B* and *0* are supported | Native fragmented designation passes; G0/G1 pixel equivalence and G2/G3 isolation |
| `ESC +` | Implemented | Only charsets *B* and *0* are supported | Native fragmented designation passes; G0/G1 pixel equivalence and G2/G3 isolation |
| `ESC =` | Implemented |  | Native mode/parser and numeric keypad acceptance pass. Configurable application-keypad/cursor/screen/navigation conditions pass the native 936-case binding test; Search conditions have separate acceptance |
| `ESC >` | Implemented |  | Native mode/parser and numeric keypad acceptance pass. Configurable application-keypad/cursor/screen/navigation conditions pass the native 936-case binding test; Search conditions have separate acceptance |
| `ESC 7` | Implemented |  | Native saved rendition, wrap, origin and charset cases pass |
| `ESC 8` | Implemented |  | Native saved rendition, wrap, origin and charset cases pass |
| `ESC # 8` | Implemented |  | Native 8 full-grid/cursor/mode/hidden/alternate/wrap cases pass |
| `ESC D` | Implemented |  | 270 native forward-control grid/cursor comparisons include IND/NEL; scrolling-margin acceptance covered by separate native fixtures. |
| `ESC E` | Implemented |  | 270 native forward-control grid/cursor comparisons include IND/NEL; scrolling-margin acceptance covered by separate native fixtures. |
| `ESC H` | Implemented |  | 64 native fragmented HT/CHT/CBT/HTS/TBC/CTC cases pass |
| `ESC M` | Implemented |  | 162 native reverse-index grid comparisons pass; scrolling-margin acceptance covered by separate native fixtures. |
| `ESC Z` | Implemented |  | Primary identification: 72 native exact reply/state/pixel cases pass; secondary also qualified separately |
| `CSI \`` | Implemented |  | 68 native fragmented absolute-position cases cover HPA; origin-mode cases covered by separate native fixtures. |
| `CSI @` | Implemented |  | Native ASCII fixtures plus retained Unicode ICH/DCH/ECH 288-case pixel comparison pass, including wide-half repair, clipping, bytewise controls, hide/reveal and selected resizes; standard-unit wide edits now pass the guarded 288-case native audit, plus 19 width-map lifecycle checks (previously 88 mismatches) |
| `CSI A` | Implemented |  | Native relative-position clipping/hidden/grid checks pass |
| `CSI a` | Implemented |  | Native relative-position clipping/hidden/grid checks pass |
| `CSI B` | Implemented |  | Native relative-position clipping/hidden/grid checks pass |
| `CSI b` | Implemented |  | Native 41 pixel/cursor comparisons pass, including Unicode, current rendition/charset, hidden redraw and wrap |
| `CSI C` | Implemented |  | Native relative-position clipping/hidden/grid checks pass |
| `CSI c` | Implemented |  | Primary identification: 72 native exact reply/state/pixel cases pass; secondary also qualified separately |
| `CSI D` | Implemented |  | Native relative-position clipping/hidden/grid checks pass |
| `CSI d` | Implemented |  | 68 native absolute-position cases cover VPA; 22 origin-mode cases include region-relative addressing |
| `CSI E` | Implemented |  | Native relative-position clipping/hidden/grid checks pass |
| `CSI e` | Implemented |  | Native relative-position clipping/hidden/grid checks pass |
| `CSI F` | Implemented |  | Native relative-position clipping/hidden/grid checks pass |
| `CSI f` | Implemented |  | 68 native absolute-position cases cover HVP; 22 origin-mode cases include region-relative addressing |
| `CSI G` | Implemented |  | 68 native absolute-position cases cover CHA |
| `CSI g` | Implemented |  | 64 native fragmented tab-control cases pass |
| `CSI H` | Implemented |  | 68 native absolute-position cases cover CUP; 22 origin-mode cases include region-relative addressing |
| `CSI h` | Partial | Modes: *4*, *20* | Modes 4 and 20 have native insertion/newline/state tests |
| `CSI ? h` | Partial | Modes: *1*, *3*, *6*, *7*, *12*, *25*, *1000*, *1002*, *1004*, *1005*, *1006*, *1007*, *1042*, *1049*, *2004*, *2026* | Partial: native tests cover 1/3/6/7/12/25, mouse/focus modes, 1007, 1049 and 2004; 3 implements reset side effects without resizing; 1042 absent; DEC2026 passes 348 native buffering/timeout/boundary/lifetime checks |
| `CSI I` | Implemented |  | 64 native fragmented tab-control cases pass |
| `CSI J` | Implemented |  | Native saved-history/visible-grid/wide-cell erase and background acceptance pass |
| `CSI K` | Implemented |  | Native wide-cell and logical-background erase acceptance pass |
| `CSI L` | Implemented |  | Native retained insert/delete-line and region tests pass |
| `CSI l` | Partial | See _CSI h_ for supported modes | Modes 4 and 20 have native insertion/newline/state tests |
| `CSI ? l` | Partial | See _CSI ? h_ for supported modes | Partial: same mode boundaries as set; do not infer complete private-mode parity. |
| `CSI M` | Implemented |  | Native retained insert/delete-line and region tests pass |
| `CSI m` | Implemented | Parameters: *0*-*9*, *21*-*25*, *27*-*49*, *58*, *59*, *90*-*97*, *100*-*107* | Partial: color, bold, italic, underline, inverse and conceal paths exist; faint passes 336 native pixel combinations including distinct named bright/indexed provenance; SGR21 and4:0/4:1/4:2 underline selectors pass48 native cases; empty/unknown/extra-part selectors pass288 additional native pixel comparisons; curly/dotted/dashed pass72 native groups plus432 selector checks; independent indexed/RGB colors across all five styles pass240 native groups plus1440 selectors, including SGR59 reset; real cooked/raw DOS streams add480 two-font groups and2880 selectors with explicit RGB, visible contrast guards and exact default-color restoration controls; strike passes 24 native pixel combinations including scrolling, resize and alternate-screen restoration. Full rendition acceptance pending. |
| `CSI m` | Rejected | Parameters: *11*-*19*, *51*-*55* | 112 native standard/retained, whole/bytewise cases preserve rendition, cursor and exact pixels; repeated Unicode rendering unchanged |
| `CSI n` | Implemented |  | Native exact DSR replies through device and DOS handler pass; modern/native origin coordinates intentionally differ |
| `CSI P` | Implemented |  | Native ASCII fixtures plus retained Unicode ICH/DCH/ECH 288-case pixel comparison pass, including wide-half repair, clipping, bytewise controls, hide/reveal and selected resizes; standard-unit wide edits now pass the guarded 288-case native audit, plus 19 width-map lifecycle checks (previously 88 mismatches) |
| `CSI $ p` | Implemented |  | Native ANSI20 set/reset and unsupported replies pass |
| `CSI ? $ p` | Implemented |  | Native1302 combined exact replies across standard/charmap/snipmap pass including hidden cursor visibility, UTF8/legacy, CSI/C1, color-scheme notification mode2031 and mouse-mode exclusion; standard1049 correctly unsupported |
| `CSI SP q` | Implemented |  | Native shape/blink and configurable cursor defaults pass |
| `CSI r` | Implemented |  | Native DECSTBM and DECOM integration pass |
| `CSI S` | Implemented |  | 72 native full-grid comparisons pass; region/background follow-ups covered by separate native fixtures. |
| `CSI s` | Implemented |  | Seven-bit alias: 16 native state/pixel/hidden/resize cases pass; native CSI u remains legacy |
| `CSI T` | Implemented |  | Native full-grid scroll-down comparisons pass; region/background follow-ups covered by separate native fixtures. |
| `CSI t` | Partial | Parameters: *22*, *23* | Seven-bit title stack: native nested/hidden/reveal/encoding/reset checks pass; native CSI t preserved |
| `CSI t` | Rejected | Parameters: *1*-*13*, *15*, *19*-*21*, *24* | Native144 standard/retained whole/bytewise cases preserve geometry, cursor, rendition and pixels; seven-bit parser consumes unsupported requests without native page-length dispatch. |
| `CSI u` | Implemented |  | Seven-bit alias: 16 native state/pixel/hidden/resize cases pass; native CSI u remains legacy |
| `CSI ? u` | Implemented |  | All five implemented flags (mask 31) have native DOS acceptance; queries report the active per-screen mask. Unknown bits are masked. |
| `CSI = u` | Implemented |  | Replace/union/difference supports flags 1/2/4/8/16; native event, alternate-key and associated-text suites qualify the encoders. |
| `CSI < u` | Implemented |  | Bounded independent primary/alternate stacks support all five flags; native split/screen/reset checks pass. |
| `CSI > u` | Implemented |  | Bounded independent primary/alternate stacks support all five flags; native split/screen/reset checks pass. |
| `CSI X` | Implemented |  | Native ASCII fixtures plus retained Unicode ICH/DCH/ECH 288-case pixel comparison pass; standard-unit wide edits now pass the guarded 288-case native audit, plus 19 width-map lifecycle checks (previously 88 mismatches) |
| `CSI Z` | Implemented |  | 64 native fragmented tab-control cases pass |
| `OSC 0` | Implemented | Icon names are not supported | Native title/hidden/reveal and fragmented termination cases pass; no icon-name support |
| `OSC 1` | Rejected | Icon names are not supported | Native176 cases preserve title/font/pens/cursor/pixels, short/oversized, hidden/visible and whole/bytewise input |
| `OSC 2` | Implemented |  | Native title/hidden/reveal and fragmented termination cases pass |
| `OSC 4` | Implemented |  | All256 entries mutable. Native960 extended-index set/query/change/reset combinations pass; retained/default-color and bounds/precision tests retained. |
| `OSC 8` | Implemented |  | Five native labelled-link exact-URI clicks pass, including saved/alternate state and 1023-byte URI; host fragmentation/overflow/lifetime tests |
| `OSC 10` | Implemented |  | Native default-color set/retention and save/alternate paths tested; reset passes 48 native combinations |
| `OSC 11` | Implemented |  | Native default-color set/retention and save/alternate paths tested; reset passes 48 native combinations |
| `OSC 12` | Implemented |  | Native truecolor set/query, bytewise writes, BEL/ST and malformed rejection pass; 288 raster restoration and72 wide-cell cases. Preferences/defaults and displayed15/16-bit highcolor pass; indexed renderer passes native planar bitmap tests, displayed indexed-screen acceptance remains |
| `OSC 50` | Implemented | Only *CursorShape* is supported | 448 native state/font/blink/reset cases pass; native font extension preserved |
| `OSC 52` | Implemented | Only *clipboard* and *primary selection* supported | Copy-only by default; independent copy/read permissions in native Clipboard preferences. Optional asynchronous read replies qualified through DOS with snapshot-capable ConClip and p/s local snip, BEL/ST and no paste framing. Existing copy tests cover 1 MiB; read codec host tests cover 1 MiB. Old providers deny reads |
| `OSC 104` | Implemented |  | All 256 logical entries reset individually or together; 960 native extended-index cases cover reset, queries, foreground/background and retention |
| `OSC 110` | Implemented |  | Native 48 reset combinations pass: independent defaults, explicit color preservation, saved cursor, alternate screen, hide/reveal and resize. |
| `OSC 111` | Implemented |  | Native 48 reset combinations pass: both reset orders, whole/bytewise writes, BEL/ST and legacy C1 ST; exact retained pixels. |
| `OSC 112` | Implemented |  | Reset to configured cursor-color defaults (legacy inversion when disabled) and subsequent chained color dispatch pass |
| `DCS = s` | Rejected | Bounded discard; DEC2026 synchronization is supported separately | 96 host suites; native DCS 48 cases plus long payloads |

DEC1049: private native buffer switch/restore and retained history qualified on2026-10-17. Other DEC private modes in the combined reference row are not implied complete.

Secondary CSI > c / > 0 c: 72 native version reply cases pass with DSR/DA1
sentinels, standard/retained and visible/hidden state. Uses ConsoleNG version
from the loaded device, not the reference implementation version.

Modern CSI14t/18t geometry queries: 72 native cell/pixel/legacy query cases
with DSR sentinels pass; private unit state and resize tested.

DEC1005 UTF-8 mouse: eight native DOS click/release cases pass at zero-based
columns94/95, including1005/1006 mutual exclusion, combined ordering, reset,
mode queries and DSR sentinels. High-end2014/2015 boundary is host-tested.

This refresh records historical component acceptance; it is not a fresh native
replay of every row on the combined HEAD. The host regression programs and native
fixture sources are included here. Still-pending rejection rows are deliberately
not promoted merely because the parser lacks a dedicated implementation.

### Color-scheme reporting (2026-10-21)

`CSI ? 996 n` returns `CSI ? 997 ; 1 n` (dark) or `CSI ? 997 ; 2 n`
(light). DEC2031 enables reports on successful changed color preferences,
including palette changes within the same light/dark scheme. Reapplying an
identical palette is silent. Disabling mode2031 or RIS disables notifications.
Queries remain available regardless of the notification flag and tab visibility.

With no global OS theme interface, ConsoleNG classifies its configured default
background using integer luma (299R + 587G + 114B, light at 128000). Application
OSC palette changes do not change that configured scheme or emit notifications.
This is local console preference reporting, not automatic OS theme integration.
Protocol: https://contour-terminal.org/vt-extensions/color-palette-update-notifications/

803 native checks cover standard/charmap/snipmap, fragmented CSI/C1, UTF8/legacy,
visible/hidden units, dark/light changes, unchanged palette, ANSI palette changes,
OSC isolation, rejected preference structures, disable and reset. The fixture is
[color-scheme-native.c](color-scheme-native.c).

The same committed device also passes 329 checks through the real CON handler
in raw DOS mode: exact unframed replies with bracketed paste on/off, fragmented
CSI/C1, UTF8/legacy, preference notifications, OSC isolation and disabled silence.
`color-scheme-handler-native.c` uses a separate IO request borrowing the
open handler's unit for preference control; it never reuses the handler's live
request. Program and script return zero. This is terminal raw mode, not cooked
Shell line-editing acceptance. See
[color-scheme-handler-native.c](color-scheme-handler-native.c).

### Three-button capture (2026-10-21)

Native QEMU input passes 36 exact DOS-stream cases: left, middle and right
buttons, four outside-window release edges, SGR/UTF-8/legacy encodings. Releases
clamp to the closest terminal cell, and a DSR sentinel verifies stream framing
after each case. This qualifies ordinary console windows; modifier handoffs
and physical mouse hardware remain separate.

### Native menu ownership (2026-10-21)

A native MenuStrip keeps right-button presses and releases unless the application
sets WFLG_RMBTRAP. Removing the menu restores ordinary terminal reporting.
Twelve native DOS-stream cases cover these transitions in SGR, UTF-8 and legacy
formats, with DSR sentinels and unchanged application trapping flags. The
unmodified device leaks a right-button press while the native menu is visible.
A release belonging to a previously delivered press remains deliverable if a
menu is attached during the gesture. Host tests cover that transition, held-right
motion, and the double-menu requester ownership guard. Native requester and
sticky-menu combinations remain unqualified.
