#!/usr/bin/env python3
"""Named ASCII classes against the pinned regex engine's captured memberships."""
from pathlib import Path
import contextlib
import importlib.util
import io
import json
import subprocess
import tempfile

here = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("audit", here / "search-reference-audit.py")
audit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit)
data = json.loads((here / "search-ascii-classes-cases.json").read_text())
assert data["reference"] == audit.PIN
extra = """
 {
 const ULONG scalars[] = {%s};
 const char *patterns[] = {%s};
 const char *expected[] = {%s};
 for(unsigned i=0;i<sizeof(patterns)/sizeof(*patterns);i++){
  ULONG q[128];unsigned n=strlen(patterns[i]);
  for(unsigned j=0;j<n;j++)q[j]=(unsigned char)patterns[i][j];
  for(unsigned j=0;j<sizeof(scalars)/sizeof(*scalars);j++){
   ULONG cells[]={scalars[j],scalars[j]};struct charmap_line row={0};
   row.text=cells;row.size=2;
   for(int reverse=0;reverse<2;reverse++){
    assert(con_regex_direction_begin(&selection.scan,&row,q,n,FALSE,reverse));
    struct con_search_match m;enum con_search_result result;unsigned steps=0;
    do{result=con_regex_step(&selection.scan,1,&m);assert(++steps<10000);}while(result==CON_SEARCH_MORE);
    assert((result==CON_SEARCH_FOUND)==(expected[i][j]=='1'));
    if(result==CON_SEARCH_FOUND){
     BOOL twice=patterns[i][n-1]=='+'||patterns[i][n-1]=='}';
     assert(m.first.column==(twice||!reverse?0:1));
     assert(m.end.column==(twice||reverse?2:1));
    }
   }
  }
 }
 const char *bad[]={"[[:unknown:]]","[[:Alpha:]]","[[:^^digit:]]",
  "[[:digit:]-a]","[a-[:digit:]]","[[:digit]]","[[:]]","[[:digit:]",
  "(?x)[[:di git:]]","[[:alpha:]]{256}"};
 for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);i++){
  ULONG q[128];unsigned n=strlen(bad[i]);
  for(unsigned j=0;j<n;j++)q[j]=(unsigned char)bad[i][j];
  assert(!con_regex_direction_begin(&selection.scan,NULL,q,n,FALSE,FALSE));
 }
 }
""" % (",".join(map(str, data["scalars"])),
       ",".join(json.dumps(p) for p in data["patterns"]),
       ",".join(json.dumps(p) for p in data["matches"]))

for label, old, new in [
    ("production", None, None),
    ("ASCII space includes vertical tab", '{"space",2,{9,13,32,32}}', '{"space",2,{9,10,32,32}}'),
    ("Unicode case equivalence", "(n->flags & RX_FOLD) && (!(n->flags & RX_ASCII) || cp < 128) ? con_case_lookup(cp)", "FALSE ? con_case_lookup(cp)"),
    ("inner class complement", "if (!member) found = TRUE;", "if (member) found = TRUE;"),
]:
    def transform(body):
        before, end = body.rsplit("\n}", 1)
        body = before + extra + "\n}" + end
        if old:
            assert old in body
            body = body.replace(old, new)
        return body
    with tempfile.TemporaryDirectory() as directory:
        try:
            with contextlib.redirect_stdout(io.StringIO()):
                failed = audit.production(Path(directory), [("a", "a", (0, 1), (0, 1))], 80, transform)
        except subprocess.CalledProcessError as error:
            assert old and error.returncode == -6, error
            failed = True
        assert failed == bool(old), label
    print(("Negative control PASS: " if old else "PASS: ") + label)
print("ASCII classes:", len(data["patterns"])*len(data["scalars"])*2, "directional membership and endpoint checks")
