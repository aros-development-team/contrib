#!/usr/bin/env python3
"""Absolute anchors in the UI and across raw multiline input."""
from pathlib import Path
import contextlib
import importlib.util
import io
import json
import subprocess
import tempfile

here=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('lines',here/'search-lines-regression.py')
lines=importlib.util.module_from_spec(spec);spec.loader.exec_module(lines)
data=json.loads((here/'search-absolute-cases.json').read_text())
assert data['reference']==lines.audit.PIN and len(data['cases'])==720
extra=r'''
 /* Absolute anchors cover the entire raw stream, including hard newlines. */
 {
  const ULONG queries[][9]={{'\\','A','a','\\','n','b','\\','z'},
   {'\\','A','b'},{'a','\\','z'}};
  const unsigned lengths[]={8,3,3};
  for(unsigned i=0;i<3;i++)for(int reverse=0;reverse<2;reverse++){
   assert(con_regex_direction_begin(&selection.scan,raw,queries[i],lengths[i],FALSE,reverse));
   struct con_search_match m;enum con_search_result result;unsigned steps=0;
   do{result=con_regex_step(&selection.scan,1,&m);assert(++steps<1000);}while(result==CON_SEARCH_MORE);
   if(!i)assert(result==CON_SEARCH_FOUND&&m.first.line==raw&&!m.first.column&&m.end.line==raw+1&&m.end.column==1);
   else assert(result==CON_SEARCH_DONE);
  }
 }
'''
for label,old,new in [
    ('production',None,None),
    ('raw start anchor',"!r->ordinal || (r->line_boundaries && r->previous == '\\n')", "!r->ordinal || r->previous == '\\n'"),
    ('raw end anchor','if (n->op == RX_EOS) { if (r->eof)',"if (n->op == RX_EOS) { if (r->eof || r->token.cp == '\\n')"),
]:
    def transform(body):
        before,end=body.rsplit('\n}',1)
        body=before+extra+'\n}'+end
        if old:
            assert old in body
            body=body.replace(old,new)
        return body
    with tempfile.TemporaryDirectory() as d:
        if old:
            try:
                with contextlib.redirect_stdout(io.StringIO()):
                    failed=lines.production(Path(d),data,transform)
            except subprocess.CalledProcessError as error:
                assert error.returncode == -6, error
                failed=True
            assert failed,label
            print('Negative control PASS:',label)
        else:
            assert not lines.production(Path(d),data,transform)
            print('Absolute raw-input anchors PASS: both directions, hard newlines, rejected partial lines')
