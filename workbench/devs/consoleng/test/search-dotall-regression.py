#!/usr/bin/env python3
"""Scoped dot-all behavior and pinned hard/soft terminal line boundaries."""
from pathlib import Path
import contextlib,importlib.util,io,json,subprocess,tempfile
here=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('lines',here/'search-lines-regression.py')
lines=importlib.util.module_from_spec(spec);spec.loader.exec_module(lines)
data=json.loads((here/'search-dotall-cases.json').read_text())
assert data['reference']==lines.audit.PIN and len(data['cases'])==221
extra=r'''
 {
  ULONG cells[]={'a','\n','b','\n','c'};
  struct charmap_line row={0};row.text=cells;row.size=5;
  const struct {const char*q;int first,end;} cases[]={
   {"a.b",-1,0},{"(?s)a.b",0,3},{"(?s:a.b)",0,3},
   {"(?-s:a.b)",-1,0},{"(?s:a.)b.c",-1,0},
   {"(?s)a.(?-s:b).c",0,5},{"(?s)a.(?-s:b.c)",-1,0},
   {"(?s)(a(?-s).)b.c",-1,0},{"(?s)(?-s:a.)|b.c",2,5},
   {"(?s)a.{3}c",0,5},{"(?sU)a.*c",0,5},
   {"(?sx) a . b",0,3},{"(?si)A.B",0,3},
   {"(?s:a.b)(?-s:.)c",-1,0},{"(?s:a.b)(?s:.)c",0,5}};
  for(unsigned i=0;i<sizeof(cases)/sizeof(*cases);i++)for(int reverse=0;reverse<2;reverse++){
   ULONG q[128];unsigned n=strlen(cases[i].q);
   for(unsigned j=0;j<n;j++)q[j]=(unsigned char)cases[i].q[j];
   assert(con_regex_direction_begin(&selection.scan,&row,q,n,FALSE,reverse));
   struct con_search_match m;memset(&m,0xa5,sizeof(m));struct con_search_match before=m;
   assert(con_regex_step(&selection.scan,0,&m)==CON_SEARCH_MORE&&!memcmp(&m,&before,sizeof(m)));
   enum con_search_result result;unsigned steps=0;
   do{result=con_regex_step(&selection.scan,1,&m);assert(++steps<5000);}while(result==CON_SEARCH_MORE);
   if(cases[i].first<0)assert(result==CON_SEARCH_DONE);
   else assert(result==CON_SEARCH_FOUND&&m.first.line==&row&&m.end.line==&row&&
       m.first.column==(unsigned)cases[i].first&&m.end.column==(unsigned)cases[i].end);
  }
  const char*bad[]={"(?ss)a","(?s-s)a","(?s-)a","(?--s)a","(?s)*","(?s){2}","(?s:a","(?s :a)"};
  for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);i++){
   ULONG q[128];unsigned n=strlen(bad[i]);for(unsigned j=0;j<n;j++)q[j]=(unsigned char)bad[i][j];
   assert(!con_regex_direction_begin(&selection.scan,&row,q,n,FALSE,FALSE));
  }
 }
'''
for label,old,new in [('production',None,None),
 ('dot-all newline matching',"(n->flags & RX_DOTALL) ||", "FALSE ||"),
 ('dot-all scope restoration','p->dotall = saved_dotall;','(void)saved_dotall;')]:
 def transform(body):
  before,end=body.rsplit('\n}',1);body=before+extra+'\n}'+end
  if old:assert old in body;body=body.replace(old,new)
  return body
 with tempfile.TemporaryDirectory() as d:
  if old:
   try:
    with contextlib.redirect_stdout(io.StringIO()):failed=lines.production(Path(d),data,transform)
   except subprocess.CalledProcessError as error:
    assert error.returncode==-6,error;failed=True
   assert failed,label;print('Negative control PASS:',label)
  else:
   assert not lines.production(Path(d),data,transform)
   print('Dot-all PASS: scoped/global/disabled s, mixed flags, raw newlines, zero budget and malformed flags')
