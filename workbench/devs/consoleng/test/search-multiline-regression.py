#!/usr/bin/env python3
"""Scoped multiline flags, raw newline control, and pinned hard/soft row spans."""
from pathlib import Path
import contextlib,importlib.util,io,json,subprocess,tempfile
here=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('lines',here/'search-lines-regression.py')
lines=importlib.util.module_from_spec(spec);spec.loader.exec_module(lines)
data=json.loads((here/'search-multiline-cases.json').read_text())
assert data['reference']==lines.audit.PIN and len(data['cases'])==136
extra=r'''
 {
  ULONG cells[]={'a','\n','b','\n','c'};struct charmap_line row={0};row.text=cells;row.size=5;
  const struct {const char*q;int first,end;} cases[]={
   {"(?m)^b$",2,3},{"(?-m)^b$",-1,0},{"(?m:^b$)",2,3},{"(?-m:^b$)",-1,0},
   {"(?m)^a",0,1},{"(?-m)^a",0,1},{"(?m)c$",4,5},{"(?-m)c$",4,5},
   {"(?m:^b(?-m:$))",-1,0},{"(?-m:^b(?m:$))",-1,0},
   {"(?-m)(?m:^b)$",-1,0},{"(?m)(?-m:b)$",2,3},
   {"(?-m)(?m:^b$)",2,3},{"(?m)(?-m:^b$)",-1,0},
   {"(?ms:^a.*c$)",0,5},{"(?-m)(?s:^a.*c$)",0,5},
   {"(?mx: ^ b $ )",2,3},{"(?im:^B$)",2,3},
   {"(?-m)(?m)^b$",2,3},{"(?m)(?-m)^b$",-1,0}};
  for(unsigned i=0;i<sizeof(cases)/sizeof(*cases);i++)for(int reverse=0;reverse<2;reverse++){
   ULONG q[128];unsigned n=strlen(cases[i].q);for(unsigned j=0;j<n;j++)q[j]=(unsigned char)cases[i].q[j];
   assert(con_regex_direction_begin(&selection.scan,&row,q,n,FALSE,reverse));
   struct con_search_match m;memset(&m,0xa5,sizeof(m));struct con_search_match before=m;
   assert(con_regex_step(&selection.scan,0,&m)==CON_SEARCH_MORE&&!memcmp(&m,&before,sizeof(m)));
   enum con_search_result result;unsigned steps=0;
   do{result=con_regex_step(&selection.scan,1,&m);assert(++steps<5000);}while(result==CON_SEARCH_MORE);
   if(cases[i].first<0)assert(result==CON_SEARCH_DONE);
   else assert(result==CON_SEARCH_FOUND&&m.first.line==&row&&m.end.line==&row&&m.first.column==(unsigned)cases[i].first&&m.end.column==(unsigned)cases[i].end);
  }
  const char*bad[]={"(?mm)a","(?m-m)a","(?m-)a","(?--m)a","(?m)*","(?m){2}","(?m:a","(?m :a)"};
  for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);i++){
   ULONG q[128];unsigned n=strlen(bad[i]);for(unsigned j=0;j<n;j++)q[j]=(unsigned char)bad[i][j];assert(!con_regex_direction_begin(&selection.scan,&row,q,n,FALSE,FALSE));
  }
 }
'''
for label,old,new in [('production',None,None),
 ('disabled anchors','cp == \'^\' ? (p->multiline ? RX_BOL : RX_BOS)',"cp == '^' ? RX_BOL"),
 ('scope restoration','p->multiline = saved_multiline;','(void)saved_multiline;'),
 ('reverse absolute anchors','else if (p->r->reverse && (op == RX_BOS || op == RX_EOS))','else if (FALSE && (op == RX_BOS || op == RX_EOS))')]:
 def transform(body):
  before,end=body.rsplit('\n}',1);body=before+extra+'\n}'+end
  if old:assert old in body;body=body.replace(old,new)
  return body
 with tempfile.TemporaryDirectory() as d:
  if old:
   try:
    with contextlib.redirect_stdout(io.StringIO()):failed=lines.production(Path(d),data,transform)
   except subprocess.CalledProcessError as error:assert error.returncode==-6,error;failed=True
   assert failed,label;print('Negative control rejected:',label)
  else:
   assert not lines.production(Path(d),data,transform)
   print('Multiline PASS: global/scoped m, disabled mode, scope restoration, both directions, raw newlines, hard/soft rows and malformed flags')
