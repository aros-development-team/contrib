#!/usr/bin/env python3
"""Lazy repetition against actual pinned-terminal directional expectations."""
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
data = json.loads((here / "search-lazy-cases.json").read_text())
assert data["reference"] == audit.PIN and data["columns"] == 80
assert len(data["cases"]) == 704
for label, occurrence in [
    ("production", None),
    ("counted lazy repetition", 0),
    ("simple lazy repetition", 1),
]:
    def transform(body):
        original = "BOOL lazy = rx_lazy(p);"
        assert body.count(original) == 2
        at = body.index(original) if occurrence == 0 else body.rindex(original)
        return body[:at] + body[at:].replace(original, original + " lazy = FALSE;", 1)

    with tempfile.TemporaryDirectory() as d:
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            gaps = audit.production(Path(d), data["cases"], 80,
                                    transform if occurrence is not None else None)
        if occurrence is not None:
            assert gaps, label
            print("Negative control PASS:", label)
        else:
            assert not gaps, output.getvalue()
            print(output.getvalue(), end="")
