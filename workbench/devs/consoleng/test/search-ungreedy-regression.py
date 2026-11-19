#!/usr/bin/env python3
"""Ungreedy mode, scope and case combinations against the pinned terminal."""
from pathlib import Path
import contextlib
import importlib.util
import io
import json
import tempfile

here = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("reference_audit",
                                             here / "search-reference-audit.py")
audit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit)
data = json.loads((here / "search-ungreedy-cases.json").read_text())
assert data["reference"] == audit.PIN and data["columns"] == 80
assert len(data["cases"]) == 1080
for label, old, new in [
    ("production", None, None),
    ("ungreedy defaults", "return p->ungreedy;", "return FALSE;"),
    ("suffix inversion", "return !p->ungreedy;", "return TRUE;"),
    ("ungreedy scope restoration", "p->ungreedy = saved_ungreedy;",
     "(void)saved_ungreedy;"),
]:
    def transform(body):
        assert old in body
        return body.replace(old, new)

    with tempfile.TemporaryDirectory() as d:
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            gaps = audit.production(Path(d), data["cases"], 80,
                                    transform if old else None)
        if old:
            assert gaps, label
            print("Negative control PASS:", label)
        else:
            assert not gaps, output.getvalue()
            print(output.getvalue(), end="")
