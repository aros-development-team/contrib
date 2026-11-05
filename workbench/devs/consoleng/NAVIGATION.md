# Navigating retained text

Use **Ctrl+Shift+Space**, or assign **Navigate retained text** on the Keyboard
page, to enter navigation. An explicit binding overrides the built-in shortcut.
It starts at the application cursor when that cursor is visible, or at the
top-left of the view when it is off-screen. Navigation then uses an independent
highlight. Blank rows at entry are retained within the visible screen bounds.
Input stays local while navigation is active; it is not sent to the application.

| Key | Action |
| --- | --- |
| h / Left | Previous cell, crossing soft wraps |
| l / Right | Next cell, crossing soft wraps |
| k / Up | Previous row |
| j / Down | Next row |
| 0 | First column; repeat there to follow preceding soft wraps |
| Shift+4 ($ on the usual keymap) | Last occupied cell; repeat for right edge or wrapped-line end |
| Shift+6 (^ on the usual keymap) | First occupied cell; repeat to follow the logical line |
| g / Shift+G | First / last retained row |
| H / M / L | First occupied cell on the top / middle / bottom visible row |
| Ctrl+B / Ctrl+F | Scroll up / down a full page |
| Ctrl+U / Ctrl+D | Scroll up / down half a page |
| Ctrl+Y / Ctrl+E | Scroll up / down one row |
| w / e / b | Next semantic word, word end, previous word |
| W / E / B | The same motions using whitespace-separated words |
| Shift+[ / Shift+] ({ / }) | Previous / next paragraph boundary |
| Shift+5 (%) | Matching bracket at the cursor |
| f / F, then a character | Find the next / previous matching character |
| t / T, then a character | Find forward / backward, stopping short of the match |
| ; / , | Repeat inline find in its original / opposite direction |
| v / V / Ctrl+V / Alt+V | Character / logical-line / rectangular / semantic selection; repeat to clear, or switch type |
| y / configured Copy shortcut | Copy and clear the selection; keep navigating |
| Escape / configured Clear selection shortcut | Clear the selection; keep navigating |
| Shift+8 / Shift+3 (* / # on the usual keymap) | Search selected text or the next semantic word, forward / backward |
| `/` / `?` | Start a forward / backward query from the cursor |
| n / Shift+N | Next / previous match relative to the original query direction |
| Enter / keypad Enter | Open the retained URL at the cursor |
| z | Center the viewport around the cursor |
| i / navigation shortcut | Leave navigation and return to the live screen |
| Ctrl+C | Leave navigation and keep the current view |

Single-cell motions process each key event immediately with a one-step budget,
so rapid arrow or h/j/k/l input cannot collapse between redraw ticks.
Wide glyphs are indivisible movement and selection units. Top/bottom traversal
and locating the resulting view run in bounded slices, keeping long histories
from blocking the console input task. New output, resize/reflow and title
ownership changes cancel navigation before retained rows can be invalidated.

The entry action uses binding record version 6 and ConsoleNG 13 profiles.
Earlier saved profiles and action numbers remain compatible.

This is not complete Alacritty vi-mode parity. Semantic search shortcuts are
implemented as described below; movement, selection, inline-find and search actions are configurable in Preferences. Additional mode conditions and semantic delimiters remain open.

Word movement is incremental, including within a single very long row. Lowercase
commands use the pinned reference default delimiters (comma, vertical box line,
backtick, vertical bar, colon, quotes, parentheses, brackets, braces and angle
brackets). Uppercase commands separate words only at spaces and tabs. Delimiter
characters are individual semantic tokens. Soft wraps preserve words; hard line
boundaries separate them. Configurable semantic delimiters remain open.

Paragraph movement skips initial blank rows, then the paragraph, and stops at
the next blank row or the history boundary. Matching brackets supports nested
parentheses, square brackets, braces and angle brackets in either direction.
It starts only when the cursor is on a bracket. An unmatched bracket leaves the
cursor at its original position. Both operations use the bounded navigation
budget, including scans through very long rows.

Navigation preserves the viewport while the cursor remains visible and scrolls
only enough to reveal a cursor crossing an edge. H/M/L preserve that viewport;
blank target rows fall back to column zero. Middle uses the upper of the two
central rows on an even-height viewport, matching the pinned reference.

Page and half-page scrolling move both the viewport and navigation cursor,
landing on the first occupied cell. Line scrolling preserves the cursor
position unless it would leave the viewport, in which case it follows the
nearest edge. Both stop at the oldest history and live-screen boundaries.

Horizontal movement crosses soft wraps in either direction, including synthetic
padding before a wide glyph, but stops at explicit line breaks. At column zero,
0 follows preceding wrapped rows to the start of the logical line in bounded
slices. From another column it first moves to column zero of the current row.

V selects whole logical lines, extending through soft wraps in either direction.
Moving extends the selected range. Switching between v and V preserves the
anchor; pressing the active selection key again clears it. Copy includes the
final native line separator for line selections, including an empty line,
while character selections retain their exact endpoints. Expansion across
long wrapped lines uses the same bounded work budget as navigation.

Ctrl+V selects a rectangle. Each row uses the same column bounds; an edge that
touches a wide glyph includes that complete glyph. Copy trims trailing Unicode
whitespace from each row and places a native CR between rows, including empty
rows, with no final separator. Soft-wrapped rows remain separate in rectangular
copies. Switching among v, V and Ctrl+V preserves the anchor.

Alt+V expands selection endpoints to semantic boundaries using the pinned
default delimiters. A single-point selection on a matching bracket selects
the entire bracketed span first; an unmatched bracket falls back to semantic
expansion. Expansion examines neighboring cells, so a delimiter at the cursor
can join the words on either side. Soft wraps preserve semantic words; hard
line breaks stop endpoint expansion. Wide glyphs remain indivisible. Bracket
matching and endpoint expansion share the bounded navigation work budget.
Switching selection types preserves the anchor; repeating Alt+V clears it.
Configurable delimiters remain a separate gap.

Horizontal and vertical cursor movement covers the visible grid, including
unallocated blank columns on short or empty rows. Vertical movement preserves
the column unless a wide cell or synthetic wrap spacer requires adjustment.
Blank-cell highlights use the existing overlay without allocating text cells.
Character copies stop at stored text, so selected blank tails do not add NULs,
padding or an extra final newline. Semantic selection treats virtual blanks
as delimiters instead of jumping into adjacent stored text. Endpoint motions below use the same bounded traversal model.

$ first seeks the last occupied cell. When already at or beyond it, the motion
moves to the physical right edge, or follows soft wraps from that edge to the
last occupied cell of the final row. ^ seeks the first occupied cell; repeating
there follows earlier soft-wrapped rows to their earliest occupied cell. If
none exists, it falls forward through wrapped blank rows. An unoccupied row
uses its physical right edge. Space and tab cells are unoccupied; wide glyphs
remain indivisible. Both row traversal and within-row scans consume the bounded
navigation budget, including very long retained rows.

Inline find excludes the starting cell, crosses soft wraps and stops at hard
line breaks. An absent target restores the starting position. Each step consumes
one cell from the bounded work budget. While awaiting a character, navigation
commands such as i and y are target characters; Escape cancels the prompt and
Ctrl+C exits navigation. Repeat keeps the original direction even after using
comma to search the opposite way. As in the pinned reference, repeating a
stop-short search may stay put when its target is immediately adjacent.
The engine handles Unicode scalar cells and wide glyphs; interactive target
entry currently uses the native keymap's single-byte character result, matching
the existing search input path. Broader Unicode text-input integration remains
separate from the engine's scalar support.

Enter opens a recognized plain or OSC8 URL at the navigation cursor through the
same native worker used for Ctrl+Shift-click. Unsupported URI schemes remain
rejected. Repeated key events and incomplete navigation positions do not launch
commands; Enter while awaiting an inline-find character stays in that prompt.
The z action preserves cursor and selection positions while moving the view to
the upper middle row, limited by the available history. Keyboard URL integration
does not certify downstream browser behavior or the separate OpenURL dependency.
The native regression's --url-probe option requires an isolated C:OpenURL probe;
without that explicit option it does not launch URLs.

Leaving with i or the navigation shortcut restores the saved live-screen view
without changing the application cursor. Ctrl+C leaves the current scrollback
view in place. Copying a completed selection does not wait for a display redraw. If a long
motion or selection expansion is unfinished, y queues a snapshot and displays
"copying selection" until it is ready. Later navigation does not change the
snapshot or get overwritten when it finishes. A newer copy replaces a pending
copy; exiting navigation, output, reflow or title replacement cancels it.
With no intervening navigation, successful publication clears the selection.
Snapshot expansion, size measurement and UTF-8/Latin-1 byte encoding are
incremental. Partially encoded buffers are never published and are freed on
cancellation or replacement. Allocation failure preserves the selection and
previous clipboard. The completed buffer uses the existing clipboard/provider
path; allocator and legacy callback latency are outside the work budget.
Ordinary mouse copies retain their existing behavior.

In a navigation query, Enter confirms the match and returns to navigation;
Escape or Ctrl+C restores the cursor and view from before the query. Enter
while a bounded scan is still running waits for its result. A missing match
restores the query origin. Query editing, clipboard paste, history recall and
Ctrl+R literal/regex switching use the existing search editor; navigation
queries start in regex mode. n follows the original direction, N its opposite,
wrapping through retained history. Cancellation disables repeats for that query.
Standalone search retains its existing Escape-to-selection behavior.
Backward regex searches now reverse both the compiled expression and retained
token traversal, rather than reversing a forward match list. Each direction
uses a nonoverlapping stream in its own scan order. For expressions that cannot
match empty text, the UI respects alternative order while retaining greedy
continuations with higher priority. Expressions that can match empty text use
the reference's longest-match policy; empty results themselves are not selected.
This correctly finds the trailing `aba` in `ababa` when searching backward.
Empty low-level matches advance safely but are omitted from the search UI.
Tail discovery, erased-padding trimming, regex work and row location are
bounded by the caller's work budget. Wide-character endpoints retain their
actual physical row at a soft wrap. Literal search is unchanged.
`test/search-priority-regression.py` checks 1,672 captured spans from the pinned
terminal, covering 38 supported expressions and 22 inputs in both directions.
The original 22-case, 80-column audit also agrees. The original minimal-width
reference controls remain available with `--tight`; those narrow-window quirks,
erased-padding policy and broader regex syntax remain separate qualification
boundaries. The low-level regex API keeps its POSIX-style longest-match policy.
Lazy repetitions (`*?`, `+?`, `??`, and counted forms ending in `?`) are also
available in navigation queries. Their shorter-match preference applies to
expressions that cannot match empty text; nullable expressions retain the
reference's longest-match policy. Noncapturing groups and global/scoped `i`
case flags are supported too; explicit case flags override smartcase within
their lexical scope. The `U` flag reverses the default greedy preference,
including counted forms, and can be combined with `i`. See SEARCH.md for examples.

Seeded searches use the active selection, or the semantic word at or after the
cursor. They clear the selection after accepting its text, find the first
matching occurrence whose end reaches or follows the original cursor, then
skip that occurrence in the requested direction and wrap through history. n/N repeat the resulting query.
Collection and selection expansion share a bounded timer budget; another
navigation key cancels unfinished collection. The escaped query must fit the
256-scalar limit. Failure leaves the previous query and selection intact and
reports the limit in the window title. No clipboard request is needed.
The escape set follows the pinned reference, including its unescaped period
wildcard. Initial occurrence location is a separate bounded pass; cancellation
releases its borrowed rows. This initial source-occurrence pass always scans
forward with the UI's matching policy, before the destination pass starts in
the requested direction.
Regex matching restarts at each hard logical line boundary; soft-wrapped rows
stay in the same input. Literal mode still supports exact multiline queries.
The low-level regex API retains its previous newline-token contract.
Absolute `\A` and `\z` anchors cover a whole logical line in navigation
queries, including its soft wraps. Use `(?i)` for explicit case folding when
the uppercase `A` would otherwise disable smartcase.

The configurable Clear selection action also cancels a pending deferred copy
without publishing its partial buffer. It keeps navigation and query state;
Escape retains its separate query-cancellation behavior while editing a query.

Preferences → Keyboard can assign Center navigation cursor and Open navigation
link to a different key/modifier combination. These are single-shot local
actions: they do nothing outside navigation and do not leak shortcut bytes to
the application. Open uses the current retained cursor, including OSC8 labels;
centering keeps that cursor at its logical position. Pending find/search/motion
work is not interrupted to activate a possibly stale link.

Keyboard's Active in selector can restrict an existing action to Navigation or
Outside navigation. Search-query editing is outside navigation for this rule.
Disjoint contexts may share a key/modifier chord; overlapping contexts may not.
These conditions do not yet cover application-cursor, alternate-screen or other
terminal protocol modes.

The four selection toggles can also be assigned in Keyboard Preferences as
Select navigation characters/lines/rectangle/words. They reuse v, Shift+V,
Ctrl+V and Alt+V behavior, including type switching and toggling the current
selection off. Repeats do not toggle selection; navigation must be settled.

Keyboard Preferences can assign 18 retained cursor movements: Left/Right/Up/Down,
First column/Line end, History start/end, next/end/previous whitespace word,
next/end/previous semantic word, previous/next paragraph, Matching bracket and
First occupied cell. Configured movements use the same bounded path as built-in
keys, retain selection, and support held-key repeats. They do not move the
application cursor. Use Active in: Navigation to keep the key available to apps
outside navigation. Always bindings are consumed without movement while editing
a search query or using normal input. A pending inline find still consumes its
next character before ordinary navigation controls, as with built-in keys.

Keyboard also offers Navigate: View top/middle/bottom and page, half-page or
line scrolling in either direction. View motions match H/M/L and find the first
occupied cell of the target visible row. Page and half-page scrolling preserve
the cursor's screen row; line scrolling only clamps it into the new viewport.
They retain selections and support held repeats, with the same bounded work
and history-limit clipping as Ctrl+B/F/U/D/Y/E.

## Configurable find and search actions

Keyboard Preferences includes Find/Till forward and backward, Repeat/Reverse
find, Search forward/backward, Search word forward/backward and Next/Previous
match. Find waits for its printable target; Search opens the regex query editor
and uses Enter to confirm or Escape to restore the navigation origin. Word
search uses the selection when present, otherwise the semantic word.
Configured actions use the same paths as f/F/t/T, semicolon/comma, slash/question
mark, asterisk/hash and n/N. Always bindings consume their chord without
modifying an already-open query editor or ordinary application input. Use
Navigation conditions to leave the chord available to applications outside
navigation. Binding versions 16
and 17 append these actions without changing older values or record layouts.

## Terminal-state binding conditions

Keyboard Preferences combines Navigation/Outside navigation with independent
screen, cursor-key and keypad conditions. Select Any, required state or its
opposite for each dimension. The same chord may have complementary bindings;
combinations that can match simultaneously are rejected. Query editing is
outside navigation but keeps its actual screen/cursor/keypad state. Held keys
already consumed by a shortcut cannot leak repeat/release when a mode changes.

Search-mode shortcut conditions can distinguish the navigation query editor
from navigation itself. With version 19 bindings, Navigation + Searching
matches while editing a navigation query; Navigation + Not searching matches
after returning to navigation. Older profiles preserve their previous
query-exclusion behavior until a Search condition is configured.
