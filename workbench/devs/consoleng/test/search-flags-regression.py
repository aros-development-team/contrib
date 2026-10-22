#!/usr/bin/env python3
"""Inline case scope and noncapturing groups against the pinned terminal."""
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
data = json.loads((here / "search-flags-cases.json").read_text())
assert data["reference"] == audit.PIN and data["columns"] == 80
assert len(data["cases"]) == 1000
for label, old, new in [
    ("production", None, None),
    ("per-node case mode",
     "(p->fold ? RX_FOLD : 0)",
     "(p->r->fold ? RX_FOLD : 0)"),
    ("group scope restoration",
     "p->fold = saved_fold;", "(void)saved_fold;"),
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
