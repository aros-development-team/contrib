#!/usr/bin/env python3
"""Replay property aliases and directional spans captured from pinned Alacritty."""
from pathlib import Path
import contextlib,importlib.util,io,json,tempfile
here=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('audit',here/'search-reference-audit.py')
audit=importlib.util.module_from_spec(spec);spec.loader.exec_module(audit)
data=json.loads((here/'search-property-cases.json').read_text());assert data['reference']==audit.PIN
with tempfile.TemporaryDirectory() as d:
 assert not audit.production(Path(d),data['cases'],data['columns'])
for label,old,new in [('complement','return neg?mask<<3:mask;','return neg?mask:mask;'),('digit property','))mask=1;', '))mask=2;')]:
 def change(source):
  assert old in source;return source.replace(old,new)
 with tempfile.TemporaryDirectory() as d:
  with contextlib.redirect_stdout(io.StringIO()):failed=audit.production(Path(d),data['cases'],data['columns'],change)
  assert failed,label
 print('Negative control PASS:',label)
