#!/usr/bin/env python3
"""CRLF anchors and dot matching against the pinned regex-automata engine."""
from pathlib import Path
import contextlib,importlib.util,io,json,subprocess,tempfile
here=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('audit',here/'search-reference-audit.py')
audit=importlib.util.module_from_spec(spec);spec.loader.exec_module(audit)
data=json.loads((here/'search-crlf-cases.json').read_text());assert data['reference']==audit.PIN
fixtures=[]
for p,t,f,b in data['cases']:
 def span(s):return ','.join(map(str,s if s is not None else [-1,0]))
 fixtures.append('{'+json.dumps(p)+','+json.dumps(t)+',{'+span(f)+'},{'+span(b)+'}}')
extra=r'''
 {static const struct {const char *query,*text;int forward[2],backward[2];} cases[]={FIXTURES};
 for(unsigned i=0;i<sizeof(cases)/sizeof(*cases);i++)for(int reverse=0;reverse<2;reverse++){
  ULONG q[256],cells[128];unsigned n=strlen(cases[i].query),length=strlen(cases[i].text);
  for(unsigned j=0;j<n;j++)q[j]=(unsigned char)cases[i].query[j];
  for(unsigned j=0;j<length;j++)cells[j]=(unsigned char)cases[i].text[j];
  struct charmap_line row={0};row.text=cells;row.size=length;
  assert(con_regex_direction_begin(&selection.scan,&row,q,n,FALSE,reverse));
  struct con_search_match m;memset(&m,0xa5,sizeof(m));struct con_search_match before=m;
  assert(con_regex_step(&selection.scan,0,&m)==CON_SEARCH_MORE&&!memcmp(&m,&before,sizeof(m)));
  enum con_search_result status;unsigned steps=0;
  do{status=con_regex_step(&selection.scan,1,&m);assert(++steps<20000);}while(status==CON_SEARCH_MORE);
  const int *expected=reverse?cases[i].backward:cases[i].forward;
  if(!((expected[0]<0&&status==CON_SEARCH_DONE)||(expected[0]>=0&&status==CON_SEARCH_FOUND&&m.first.column==(unsigned)expected[0]&&m.end.column==(unsigned)expected[1]))){
   fprintf(stderr,"CRLF case %u reverse %d expected %d..%d actual %d %u..%u\n",i,reverse,expected[0],expected[1],status,m.first.column,m.end.column);abort();
  }
 }
 const char*bad[]={"(?RR)a","(?R-R)a","(?R-)a","(?--R)a","(?R)*","(?R){2}","(?R:a","(?R :a)"};
 for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);i++){
  ULONG q[128];unsigned n=strlen(bad[i]);for(unsigned j=0;j<n;j++)q[j]=(unsigned char)bad[i][j];
  struct charmap_line row={0};assert(!con_regex_direction_begin(&selection.scan,&row,q,n,FALSE,FALSE));
 }
 }
'''.replace('FIXTURES',','.join(fixtures))
for label,old,new in [('production',None,None),
 ('CRLF anchors','if (n->flags & RX_CRLF) {','if (FALSE) {'),
 ('CRLF dot',"(!(n->flags & RX_CRLF) || cp != '\\r')",'TRUE'),
 ('scope restoration','p->crlf = saved_crlf;','(void)saved_crlf;'),
 ('reverse pairs',"ULONG lead = r->reverse ? '\\n' : '\\r';","ULONG lead = '\\r';")]:
 def transform(body):
  before,end=body.rsplit('\n}',1);body=before+extra+'\n}'+end
  if old:assert old in body;body=body.replace(old,new)
  return body
 with tempfile.TemporaryDirectory() as d:
  if old:
   try:
    with contextlib.redirect_stdout(io.StringIO()):failed=audit.production(Path(d),[('a','a',(0,1),(0,1))],80,transform)
   except subprocess.CalledProcessError as error:assert error.returncode==-6,error;failed=True
   assert failed,label;print('Negative control rejected:',label)
  else:
   assert not audit.production(Path(d),[('a','a',(0,1),(0,1))],80,transform)
   print('CRLF PASS:',len(data['cases'])*2,'directional raw spans, scoped flags, dot-all precedence, zero budget and malformed flags')

# Actual terminal row/wrap behavior supplements the raw regex oracle above.
spec=importlib.util.spec_from_file_location('lines',here/'search-lines-regression.py')
lines=importlib.util.module_from_spec(spec);spec.loader.exec_module(lines)
terminal=json.loads((here/'search-crlf-terminal-cases.json').read_text())
assert terminal['reference']==audit.PIN
with tempfile.TemporaryDirectory() as d:
 assert not lines.production(Path(d),terminal)
print('CRLF terminal PASS: 272 hard/soft-row directional spans with four documented reference omissions')
