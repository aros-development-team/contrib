#!/usr/bin/env python3
"""Control escapes against an independent regex oracle, with bounded scans."""
from pathlib import Path
import contextlib
import importlib.util
import io
import re
import subprocess
import tempfile

here = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("audit", here / "search-reference-audit.py")
audit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit)
# The pinned regex-syntax 0.8.6 parser maps a/f/v to U+0007/U+000C/U+000B.
# These expressions avoid alternation and overlapping matches, so Python's
# independent engine and terminal reverse search select the same endpoints.
patterns = [form % escape for escape in ("a", "f", "v")
            for form in (r"\%s", r"[\%s]", r"\%s+", r"(?x: \%s )")]
patterns += [r"[\a-\f]", r"[\v-\r]", r"[\a\f\v]", r"[\a\f\v]+"]
cases = []
for pattern in patterns:
    for scalar in range(1, 128):
        text = "!" + chr(scalar) * 2 + "!" + chr(scalar) + "!"
        matches = list(re.finditer(pattern, text.ljust(80)))
        cases.append((pattern, text, matches[0].span() if matches else None,
                      matches[-1].span() if matches else None))

def transform(body):
    # JSON emits \u0007, which is not a legal C universal character name.
    # Three-digit octal leaves subsequent digits unambiguous.
    return re.sub(r"\\u00([0-9a-f]{2})",
                  lambda m: "\\" + format(int(m[1], 16), "03o"), body)

with tempfile.TemporaryDirectory() as directory:
    assert not audit.production(Path(directory), cases, 80, transform)

for escape in ("a", "f", "v"):
    def broken(body):
        needle = "    if (cp == '%s') return '\\%s';" % (escape, escape)
        assert needle in body
        return transform(body.replace(needle, ""))
    with tempfile.TemporaryDirectory() as directory:
        try:
            with contextlib.redirect_stdout(io.StringIO()):
                failed = audit.production(Path(directory), cases, 80, broken)
        except subprocess.CalledProcessError as error:
            assert error.returncode == -6, error
            failed = True
        assert failed, escape
    print("Negative control PASS: missing \\" + escape + " rejected")
