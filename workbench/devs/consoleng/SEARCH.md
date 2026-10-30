# Searching retained console text

Open Find with **Ctrl+Shift+F** (or your configured Search shortcut).
Type a query or paste using your configured Paste shortcut. **Enter** selects
the next match; **Shift+Enter** selects the previous match, wrapping through
retained history. **Escape** closes Find and leaves a completed nonempty match
selected. Use your Copy shortcut (normally Right-Amiga+C) to copy that match.
Unfinished, missing and empty matches do not create a selection. **Ctrl+U** clears the query and
**Ctrl+W** removes the preceding word. Up/Down recall previous queries.

The Keyboard page also offers **Search backward**. Assign it to a logical or
named special key to begin at the last retained match. Enter continues backward;
Shift+Enter moves forward. Query edits, paste and history recall preserve the
chosen direction. The existing Search action starts at the first retained match.
These starting positions are history endpoints, not the application cursor.
Search backward uses binding record version 4 and saved profile `ConsoleNG 11`;
older profiles and existing shortcuts remain supported.

**Ctrl+R**, while Find is open, switches between literal and regular-expression
search. The title changes from `Find:` to `Regex:`. A configured shortcut on
Ctrl+R takes precedence over this built-in toggle. The mode stays selected for
that console until toggled again; it is not a saved preference.

Literal Find uses ASCII case-insensitive matching; other Unicode scalars match
exactly. Regex uses Unicode 16 simple case folding when the raw query contains
no uppercase character. Any Unicode uppercase character makes the whole regex
case-sensitive, matching Alacritty smart case. This includes uppercase syntax
such as `\D`; use lowercase literals when you want folded matching. For example,
`ω` matches both `ω` and `Ω`, while `Ω` matches only `Ω`. Folding also applies
to bracket ranges and negated sets; it does not expand one scalar into multiple
characters (for example, `ß` does not match `ss`).

A wide glyph is one searchable scalar; a soft visual wrap does not
insert a newline. A hard line break does in literal mode. Regex mode treats
each hard logical line as a separate input, while continuing across soft
wraps. Erased background padding is excluded.

## Query display in native window titles

The native window title uses the screen's byte-oriented font. Characters outside
printable Latin-1 appear as `\u{03C9}` (ω), `\u{1F431}` (🐱), and similar scalar
escapes, rather than an ambiguous question mark. Actual newline, carriage-return
and tab characters appear as `\n`, `\r` and `\t`. Literal backslashes are doubled
in the title so they remain distinguishable from these display escapes.

Matching, history and editing retain the original Unicode query. Title escapes
do not rewrite that query; literal mode does not decode displayed escapes.
Long titles end the query with `...` before the search status; an escape is never
split. Closing Find restores the owner's title unless it was replaced externally.

## Regular expressions

Supported syntax:

| Syntax | Meaning |
| --- | --- |
| `abc` | Literal characters |
| `.` | Any scalar except newline |
| `[abc]`, `[a-z]`, `[^a-z]` | Character sets, ranges and negated sets |
| `\d`, `\s`, `\w` | Unicode decimal digits, whitespace and word characters |
| `\D`, `\S`, `\W` | Complements of those Unicode classes |
| `(one|two)` | Grouping and alternatives |
| `a*`, `a+`, `a?` | Zero or more, one or more, zero or one |
| `a{3}`, `a{2,4}`, `a{2,}` | Exactly three, two through four, or at least two |
| `a*?`, `a+?`, `a??` | Prefer shorter repetition |
| `a{2,4}?`, `a{2,}?` | Prefer the lower count in bounded or unbounded repetition |
| `^`, `$` | Beginning and end of a logical line |
| `\A`, `\z` | Beginning and end of the regex input |
| `\b`, `\B` | Unicode word boundary, or a position that is not a word boundary |
| `\n`, `\r`, `\t` | Escaped control characters |
| `\.`, `\[`, `\\` | Escaped punctuation |

For example, `error.*[0-9]+` locates an error line containing a number;
`^(WARN|ERROR):` locates either prefix at a logical line start.
The console presents each hard logical line as one regex input, including all
its soft-wrapped rows, so `\A...\z` anchors a whole logical line. In the
low-level API these anchors cover the entire supplied stream, including hard
newline tokens; `^` and `$` remain line anchors there. Smartcase examines the
raw query, including the uppercase `A` in `\A`; use `(?i)` to request case
folding explicitly.

A word boundary is a position between a Unicode `\w` character and a
non-word character (or the beginning/end of retained text). `\bneedle\b`
finds the whole word; `n\Beedle` matches inside it. These assertions consume
no characters and are supported outside bracket sets. Soft wrapping does not
create a word boundary; a hard newline can.

Each search direction has its own nonoverlapping match stream. For expressions
that cannot match empty text, alternatives are tried in their written order;
ordinary repetition is greedy and a following `?` makes repetition lazy.
For example, `a|ab` selects `a` in `ab`, while `ab|a` selects `ab`.
`a+?` selects one `a` in `aaa`; `a.*?b` stops at the first usable `b`.

As in the reference terminal, an expression that can match empty text uses
longest-match selection even if it contains a lazy repetition. For example,
`a*?` still selects `aaa`, and `a{0,3}?` selects up to three `a` characters.
Empty results advance safely and are omitted from the UI, so next/previous
navigation cannot loop at the same empty boundary. Backward search performs
actual reverse matching, so it can find a trailing overlapping occurrence
which does not appear in the forward match stream.

Noncapturing groups use `(?:...)`. Case flags override smartcase: `(?i)` enables
case folding for the rest of the enclosing group, and `(?-i)` disables it.
Scoped forms `(?i:...)` and `(?-i:...)` apply only inside that group. For example,
`(?i:a)(?-i:b)` matches `ab` and `Ab`, but not `aB`. Leaving a group restores
its enclosing case mode. Bare flag directives cannot be quantified.

`(?U)` makes repetition lazy by default: `(?U)a+` selects one `a` in `aaa`,
while `(?U)a+?` selects all three. This also applies to counted repetitions.
`(?-U)` restores ordinary greedy defaults; scoped `(?U:...)` and `(?-U:...)`
forms restore the enclosing mode when their group ends. A quantifier outside
a scoped group uses the outer mode, so `(?U:a)+` still has a greedy outer `+`.
Flags can be combined, for example `(?iU:a+)` or `(?i-U:a+)`. Nullable
expressions still follow the reference's longest-match policy described above.

Dot-all uses `(?s)` or `(?s:...)`; `(?-s)` and `(?-s:...)` restore newline
exclusion for dots. Like the other flags, the mode is local to its enclosing
group and can be combined, for example `(?si:a.b)`. In the raw regex API this
allows a dot to consume a newline. In the terminal search UI, hard row breaks
still end a search input, even with dot-all enabled, matching the pinned
terminal's behavior. Soft-wrapped rows remain continuous.

Other Unicode properties, unsupported flags, lookaround and
backreferences are not supported.
Invalid or unsupported patterns display an error rather than falling back to
literal matching. Escape a punctuation character to search for it literally.
Shorthand classes use Unicode 16.0.0, including non-Latin digits, letters,
combining marks and join controls in the word class. They can be combined in
brackets, such as `[\w-]` or `[\d\s]`, but cannot be range endpoints. In the low-level regex API, standalone
complement classes include newline where appropriate; the existing `[^...]`
line-oriented behavior excludes newline. Search operates on scalars, without
grapheme shaping or normalization. The regex UI never matches across a hard
line break, even with a class such as `[\s\S]`; use literal mode for an
exact multiline query.

Queries are limited to 256 scalars, with at most 32 nested groups.
Repetition counts cannot exceed 256. The compiled program has fixed state and
character-range limits; complex expansions may hit those limits sooner. A
separate compilation-work limit also rejects nested expansions that would be
expensive to construct even if their final program were small. Rejection leaves
Find usable; edit the query, switch to literal mode, or press Escape.

Search runs incrementally in the console task using fixed, bounded storage.
Editing the query restarts the scan; output changes or resize may close Find to
avoid retaining references to replaced history rows. Search keys are consumed
locally and are not delivered to the application.


### Control and hexadecimal Unicode escapes

The named control escapes `\a`, `\f`, `\v`, `\n`, `\r`, and `\t` match
bell, form feed, vertical tab, newline, carriage return, and tab scalars.
They also work inside character classes and as range endpoints, including
in extended mode. They search retained text; they do not execute terminal
controls or make discarded output controls searchable.

Regex mode accepts `\xNN`, `\uNNNN`, and `\UNNNNNNNN`, or braced forms such as
`\x{3a9}`, `\u{03A9}` and `\U{1f600}`. They denote Unicode scalar values, not
encoded bytes, and can appear in character classes and range endpoints.
Malformed digits, missing braces, surrogates and values above U+10FFFF are
rejected. Braced leading zeros are allowed within the existing query and
compilation limits.

Smartcase still examines the raw query: uppercase escape letters or A–F digits
can make it case-sensitive. Use `(?i)` or `(?-i)` for an explicit case policy.
For example, `(?i)\u03A9` finds both Greek capital and lowercase omega.


### Named ASCII classes

Brackets accept the POSIX names `alnum`, `alpha`, `ascii`, `blank`, `cntrl`,
`digit`, `graph`, `lower`, `print`, `punct`, `space`, `upper`, `word`, and
`xdigit`. For example, `[[:xdigit:]]+` finds a run of hexadecimal digits;
`[[:alpha:][:digit:]_]` combines letters, digits, and underscore.
These classes use ASCII definitions. Use `\d`, `\s`, or `\w` for the existing
Unicode shorthand classes.

Case-insensitive matching still applies Unicode simple folding: `(?i)[[:lower:]]`
also accepts uppercase ASCII, long s, and Kelvin sign. Use `(?-i)` to require
the exact ASCII set regardless of smartcase. A named class cannot be a range
endpoint. A caret inside the name negates that member: `[[:^digit:]]`
matches any scalar outside ASCII `0`–`9`, including non-ASCII digits.
Case folding happens before this inner negation, so `(?i)[[:^lower:]]`
excludes both ASCII cases, long s, and Kelvin sign. Negated members can be
combined with ordinary ranges, shorthands, and other named members.
Nested bracket set algebra and unknown names remain unsupported and report an error.
Class expansion uses the existing bounded range pool; large counted
repetitions can reach that limit.

### Extended mode

`(?x)` ignores Unicode whitespace and `#` comments through the next newline or
end of query. It applies inside character classes too. Use `\ ` or `\x20` for
literal space and `\#` for a literal hash. `(?-x)` disables this mode;
`(?x:...)` and `(?-x:...)` restore the enclosing mode when the group ends.
Case and greed flags can be combined with it, such as `(?ix: a b )`.

Spacing is accepted inside counts and hexadecimal escapes: `(?x)a { 1, 3 }`
and `(?x)\x{ 6 1 }` are valid. Flag names themselves cannot contain whitespace.
For simple repetition, a lazy `?` must immediately follow its operator:
`(?x)a +?` is lazy, while `(?x)a + ?` applies an optional outer repetition and
therefore follows the nullable longest-match policy. Counted repetition permits
spacing before its lazy suffix, matching the pinned reference. Use explicit
groups for other nested repetitions; directly stacked operators remain rejected.

Smartcase still examines the entire raw query, including comment text. Use an
explicit `i` flag when comments or escape spelling should not affect case mode.


## Decimal and whitespace Unicode properties

`\p{Nd}` and `\p{Decimal_Number}` match Unicode decimal digits;
`\p{White_Space}` matches Unicode whitespace. `\P{...}` matches the complement.
These forms also work inside bracket classes, including unions with literals
and shorthand classes. The underlying Unicode 16 tables are the same pinned
tables used by `\d` and `\s`; this does not narrow matching to ASCII.

Decimal aliases accept `gc=Nd`, `gc=Decimal_Number`, and the corresponding
`General_Category=` spelling. Whitespace aliases include `Wspace` and `space`.
Names ignore ASCII case, underscores, hyphens and ASCII whitespace.
Single-letter categories also work without braces: `\pL`, `\pN`, `\PN`.
Exactly one letter belongs to the property, so `\pNu` means Number followed
by literal `u`. Quantifiers and bracket unions work with these forms.
Named values accept `:` as well as `=` (`\p{gc:Lu}`). Extended mode `(?x)`
also skips whitespace and `#` comments within property syntax.
Property syntax is rejected inside `(?-u:...)`; a property
cannot be a range endpoint. Unsupported properties, malformed names and
overlong normalized names fail compilation without falling back to literal text.

General categories also accept their pinned Unicode aliases: for example
`\p{L}`, `\p{Letter}`, `\p{Lu}`, `\p{Mark}`, `\p{Punctuation}` and
`\p{gc=Number}`. All 37 scalar categories in regex-syntax 0.8.6 are supported.
Case-insensitive matching expands a category by simple case folding before
complementing it: `(?i:\P{Lu})` excludes both upper- and lowercase equivalents.
The surrogate category is rejected, matching the reference. Each category
uses one compiled range slot and immutable, binary-searched data; it does
not expand thousands of ranges into the query's allocation.

Script properties accept bare names (`\p{Greek}`, `\p{Latn}`), `sc=` or
`Script=`. Script extensions require `scx=` or `Script_Extensions=`; these
are distinct sets. For example U+30FC belongs to Common in Script but is
included in Hiragana's Script_Extensions. All 170 backed scripts and 170
extension sets from the pinned Unicode 16 data are available, including
aliases, complements and bracket unions. Unknown/Zzzz and
Katakana_Or_Hiragana/Hrkt have aliases but no backing tables in the pinned
reference, so they are rejected. Script families cannot be mixed with
category names (`\p{gc=Latin}` and `\p{sc=Letter}` are errors).

Binary properties accept 64 backed Unicode sets and 121 pinned aliases,
including `\p{Alphabetic}`/`\p{Alpha}`, `\p{Uppercase}`, `\p{Lowercase}`,
`\p{Emoji}`, `\p{Emoji_Presentation}`, `\p{Extended_Pictographic}` and
`\p{Grapheme_Extend}`. These are actual property sets: Uppercase includes
characters outside the Lu category, and Emoji includes digits that are not
Emoji_Presentation. Complements and case folding retain their existing order.
Property-value booleans (`Alphabetic=Yes`) and InCB are rejected, matching the
pinned reference. Binary names cannot be used as category or script values.

Special sets `Any`, `ASCII` and `Assigned` are available as bare names or
category values. `Assigned` excludes unassigned code points; `Any` covers
Unicode scalars, never surrogate code points. ASCII is case-folded before
complementing, so case-insensitive ASCII also includes the Kelvin sign.

Named property values support `Age`, `Grapheme_Cluster_Break`/`gcb`,
`Sentence_Break`/`sb` and `Word_Break`/`wb`, with their pinned aliases.
Age is cumulative: `Age=6.0` includes characters assigned in version 6.0 or
earlier. The tables contain 27 ages, 13 grapheme-break values, 14 sentence-break
values and 18 word-break values. These are scalar classifications; they do
not introduce grapheme-cluster matching or word-boundary segmentation.

Bracket classes support nested union (`[ab[c-e]]`), intersection
(`[a-z&&[^aeiou]]`), difference (`[a-z--[aeiou]]`) and symmetric difference
(`[a-f~~d-z]`). Implicit union binds before the binary operators, which
associate left-to-right. Explicit nested brackets control grouping. Empty
operands and empty results follow the reference engine; a leading literal
hyphen keeps its existing meaning. Case folding applies to operands before
set operations and complements.

Compilation uses the existing fixed range pool and depth limit. Complex sets
compile to a bounded postfix program and execute iteratively; they do not
expand Unicode properties into thousands of mutable ranges. An expression
that exhausts the pool or nesting limit is rejected. ASCII-mode complement
restrictions remain conservative; this does not claim every reference byte-set
expression is supported. Named-value inequality (`!=`) is explicitly
rejected: the pinned reference parses it but its translator ignores the
operator and applies only the outer `\P` negation. ConsoleNG does not silently
present inequality as equality.
