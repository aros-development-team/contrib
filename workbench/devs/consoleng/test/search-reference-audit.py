#!/usr/bin/env python3
"""Audit directional matches against the pinned Alacritty terminal implementation.

This is a gap detector, not a passing regression: exit 1 means parity gaps.
Optionally replay the checked expectations with --reference PATH. This copies
only alacritty_terminal into --work, preserves its source, and retains the
reference Cargo.lock versions. Cargo may download missing dependencies.
All spans printed here have an exclusive end (Alacritty uses inclusive ends).
The default uses identical 80-column, explicitly space-padded rows in both
engines. --tight retains the original minimal-width controls; several of those
use only two columns and behave differently in the reference terminal.
"""
import argparse
import json
from pathlib import Path
import shutil
import subprocess

PIN = "542147067dd016fc5f49120ef0ba605a98a4e3a7"
# Captured from Term::search_next, not a generic regex library approximation.
# Columns: pattern, text, forward span, backward span.
TIGHT_CASES = [
    ("aba", "ababa", (0, 3), (2, 5)),
    ("a|ab", "ab", (0, 1), (0, 1)),
    ("ab|a", "ab", (0, 1), (0, 1)),
    ("a*", "aaa", (0, 3), (0, 3)),
    ("a|ab*", "abb", (0, 1), (0, 3)),
    ("a?|ab", "ab", (0, 1), (0, 1)),
    ("a+|ab", "aab", (0, 2), (1, 3)),
    ("a.*b", "ab ab", (0, 5), (0, 5)),
    ("(ab|a){1,2}", "abab", (0, 4), (0, 4)),
    ("a$", "aba", (2, 3), (2, 3)),
    ("^a", "aba", (0, 1), (0, 1)),
]
CASES = [
    ("aba", "ababa", (0, 3), (2, 5)),
    ("a|ab", "ab", (0, 1), (0, 2)),
    ("ab|a", "ab", (0, 2), (0, 2)),
    ("a*", "aaa", (0, 3), (0, 3)),
    ("a|ab*", "abb", (0, 1), (0, 3)),
    ("a?|ab", "ab", (0, 2), (0, 2)),
    ("a+|ab", "aab", (0, 2), (1, 3)),
    ("a.*b", "ab ab", (0, 5), (0, 5)),
    ("(ab|a){1,2}", "abab", (0, 4), (0, 4)),
    ("a$", "aba", None, None),
    ("^a", "aba", (0, 1), (0, 1)),
]


def reference(root, work, cases, width):
    head = subprocess.check_output(
        ["git", "-C", str(root), "rev-parse", "HEAD"], text=True).strip()
    if head != PIN:
        raise SystemExit(f"Wrong reference revision: {head}; expected {PIN}")
    # Refuse modified reference code rather than attaching the pin to local edits.
    dirty = subprocess.check_output(
        ["git", "-C", str(root), "status", "--porcelain", "--",
         "alacritty_terminal", "Cargo.lock"], text=True)
    if dirty:
        raise SystemExit("Reference source or lockfile has local changes")
    dest = work / "reference"
    if dest.exists():
        raise SystemExit(f"Use a fresh work directory; {dest} already exists")
    shutil.copytree(root / "alacritty_terminal", dest)
    manifest = dest / "Cargo.toml"
    manifest.write_text(manifest.read_text()
                        .replace("edition.workspace = true", 'edition = "2024"')
                        .replace("rust-version.workspace = true",
                                 'rust-version = "1.85.0"'))
    shutil.copy2(root / "Cargo.lock", dest / "Cargo.lock")
    examples = dest / "examples"
    examples.mkdir(exist_ok=True)
    pairs = ",".join(f"({json.dumps(p)},{json.dumps(t)})" for p, t, _, _ in cases)
    (examples / "direction.rs").write_text("""
use alacritty_terminal::{
    term::{search::RegexSearch,test::mock_term},
    index::{Point,Line,Column,Direction,Side}
};
fn main() {
    for (pattern,text) in [""" + pairs + """] {
        let padded=format!("{text:<WIDTH}");
        let term=mock_term(&padded);
        for direction in [Direction::Right,Direction::Left] {
            let mut regex=RegexSearch::new(pattern).unwrap();
            let col=if direction==Direction::Right {0} else {padded.len()-1};
            let found=term.search_next(&mut regex,
                Point::new(Line(0),Column(col)),direction,Side::Left,None);
            match found {
                Some(m)=>println!("{} {}",m.start().column.0,m.end().column.0+1),
                None=>println!("NONE")
            }
        }
    }
}
""".replace("WIDTH", str(width)))
    with (work / "reference-build.log").open("w") as log:
        data = subprocess.check_output(
            ["cargo", "run", "--manifest-path", str(manifest),
             "--example", "direction"], text=True, stderr=log)
    (work / "reference-results.txt").write_text(data)
    # Removing the outer workspace prunes unrelated lock entries; retained
    # dependencies must still have the pinned versions and checksums.
    import tomllib
    original = tomllib.loads((root / "Cargo.lock").read_text())
    built = tomllib.loads((dest / "Cargo.lock").read_text())
    original_packages = {(p["name"], p["version"], p.get("checksum"))
                         for p in original["package"]}
    for package in built["package"]:
        key = (package["name"], package["version"], package.get("checksum"))
        if key not in original_packages:
            raise SystemExit(f"Reference dependency drift: {key}")
    expected = ["NONE" if span is None else f"{span[0]} {span[1]}"
                for _, _, f, r in cases for span in (f, r)]
    if data.splitlines() != expected:
        raise SystemExit("Pinned terminal results differ from checked expectations")
    print(f"Reference replay: all {len(expected)} spans verified with pinned dependency versions")


def production(work, cases, width, transform=None):
    root = Path(__file__).resolve().parents[1]

    def source(name):
        return "\n".join(line for line in (root / name).read_text().splitlines()
                         if not line.startswith("#include"))

    charmap = (root / "charmap.h").read_text()
    charmap = charmap[charmap.index("struct charmap_line\n"):
                      charmap.index("VOID charmap_dispose_lines")]
    prelude = """
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
typedef uint32_t ULONG; typedef uint16_t UWORD; typedef uint8_t UBYTE;
typedef int8_t BYTE; typedef uint64_t UQUAD; typedef int BOOL; typedef void VOID;
#define TRUE 1
#define FALSE 0
#define CHARMAP_ERASED 0x21
"""
    def c_string(value):
        # C still diagnoses ??) and similar trigraphs even inside strings.
        return json.dumps(value).replace("?", r"\?")

    fixtures = ",".join("{" + c_string(p) + "," + c_string(t.ljust(width)) + "}"
                        for p, t, _, _ in cases)
    main = """
static struct con_regex_selection selection;
int main(void) {
    const char *cases[][2]={""" + fixtures + """};
    for(unsigned i=0;i<sizeof(cases)/sizeof(cases[0]);i++) {
        ULONG query[256],cells[256];
        unsigned n=strlen(cases[i][0]),len=strlen(cases[i][1]);
        for(unsigned j=0;j<n;j++)query[j]=(unsigned char)cases[i][0][j];
        for(unsigned j=0;j<len;j++)cells[j]=(unsigned char)cases[i][1][j];
        struct charmap_line row={0};row.text=cells;row.size=len;
        for(int reverse=0;reverse<2;reverse++) {
            assert(con_regex_select_begin(&selection,&row,query,n,
                con_regex_smart_fold(query,n),reverse,NULL));
            struct con_search_match found;
            enum con_search_result result;
            unsigned steps=0;
            do {
                result=con_regex_select_step(&selection,1,&found);
                assert(++steps<100000);
            } while(result==CON_SEARCH_MORE);
            if(result==CON_SEARCH_FOUND)
                printf("%u %u\\n",found.first.column,found.end.column);
            else puts("NONE");
        }
    }
}
"""
    src = work / "production.c"
    body = prelude + charmap + "\n".join(source(n) for n in (
        "search.h", "search_regex.h", "search.c", "search_unicode.h",
        "search_case.h", "search_regex.c")) + main
    src.write_text(transform(body) if transform else body)
    binary = work / "production"
    subprocess.run(["cc", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=address,undefined", "-g", str(src),
                    "-o", str(binary)], check=True)
    data = subprocess.check_output([str(binary)], text=True)
    (work / "production-results.txt").write_text(data)
    values = data.splitlines()
    assert len(values) == 2 * len(cases)
    report = []
    for i, (pattern, text, forward, reverse) in enumerate(cases):
        for j, (direction, expected) in enumerate(
                (("forward", forward), ("backward", reverse))):
            actual = values[2 * i + j]
            want = "NONE" if expected is None else f"{expected[0]} {expected[1]}"
            report.append(dict(pattern=pattern, text=text, direction=direction,
                               expected=want, actual=actual, matches=actual == want))
    (work / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    gaps = [r for r in report if not r["matches"]]
    for gap in gaps:
        print(f"GAP {gap['direction']} /{gap['pattern']}/ on {gap['text']!r}: "
              f"reference {gap['expected']}, ConsoleNG {gap['actual']}")
    print(f"{len(report)-len(gaps)}/{len(report)} reference spans match; "
          f"{len(gaps)} demonstrated parity gaps")
    return bool(gaps)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--work", type=Path, required=True)
    parser.add_argument("--reference", type=Path)
    parser.add_argument("--tight", action="store_true",
                        help="Replay the original minimal-width reference controls")
    parser.add_argument("--cases", type=Path,
                        help="Replay a captured reference matrix JSON file")
    args = parser.parse_args()
    args.work.mkdir(parents=True, exist_ok=True)
    cases, width = (TIGHT_CASES, 0) if args.tight else (CASES, 80)
    if args.cases:
        if args.tight:
            parser.error("--cases and --tight cannot be combined")
        data = json.loads(args.cases.read_text())
        if data["reference"] != PIN or not 2 <= data["columns"] <= 128:
            parser.error("Matrix reference revision or column count is invalid")
        cases, width = data["cases"], data["columns"]
    if args.reference:
        reference(args.reference.resolve(), args.work.resolve(), cases, width)
    raise SystemExit(production(args.work.resolve(), cases, width))
