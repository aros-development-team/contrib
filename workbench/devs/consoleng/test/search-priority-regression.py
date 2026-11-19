#!/usr/bin/env python3
"""Console search policy against captured pinned-terminal directional spans."""
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
data = json.loads((here / "search-priority-cases.json").read_text())
assert data["reference"] == audit.PIN
assert data["columns"] == 80
cases = data["cases"]
assert len(cases) == 836
for label, policy in [
    ("production", None),
    ("alternative priority", "r->prefer_first = FALSE;"),
    ("nullable longest policy", "r->prefer_first = TRUE;"),
]:
    def transform(body):
        original = "r->prefer_first = !nullable;"
        assert original in body
        # Keep nullability computation exercised in negative controls too.
        return body.replace(original, "(void)nullable; " + policy)

    with tempfile.TemporaryDirectory() as d:
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            gaps = audit.production(Path(d), cases, 80,
                                    transform if policy else None)
        if policy:
            assert gaps, label
            print("Negative control PASS:", label)
        else:
            assert not gaps, output.getvalue()
            print(output.getvalue(), end="")
