#!/usr/bin/env python3
"""ASCII mode anchors and dot matching against the pinned regex-automata engine."""
from pathlib import Path
import contextlib,importlib.util,io,json,subprocess,tempfile
here=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('audit',here/'search-reference-audit.py')
audit=importlib.util.module_from_spec(spec);spec.loader.exec_module(audit)
data=json.loads((here/'search-ascii-mode-cases.json').read_text());assert data['reference']==audit.PIN
fixtures=[]
for p,t,f,b in data['cases']:
 def span(s):return ','.join(map(str,s if s is not None else [-1,0]))
 fixtures.append('{{'+','.join(str(ord(c)) for c in p)+'},{'+','.join(str(ord(c)) for c in t)+'},'+str(len(p))+','+str(len(t))+',{'+span(f)+'},{'+span(b)+'}}')
capabilities=json.loads((here/'search-ascii-mode-capabilities.json').read_text())
capabilities['cases'] += [
 ('extended scalar',r'(?x-u:\x {e9})',True),
 ('extended unsafe byte',r'(?x-u:\x 80)',False),
 ('global ASCII',r'(?-u)\w+',True),
 ('global restore',r'(?-u)(?u)\w+',True)]
constructors=[]
for _,pattern,accepted in capabilities['cases']:
 # Preserve ConsoleNG's existing Unicode-boundary extension.
 if pattern==r'(?-u:(?u:\b))':accepted=True
 constructors.append('{{'+','.join(str(ord(c)) for c in pattern)+'},'+str(len(pattern))+','+str(int(accepted))+'}')
constructor_body=r'''
 {static const struct {ULONG q[256];unsigned n;int accepted;} cases[]={CASES};
 struct charmap_line row={0};
 for(unsigned i=0;i<sizeof(cases)/sizeof(*cases);i++)for(int reverse=0;reverse<2;reverse++){
  int actual=con_regex_direction_begin(&selection.scan,&row,cases[i].q,cases[i].n,FALSE,reverse);
  if(actual!=cases[i].accepted){fprintf(stderr,"Constructor %u reverse %d expected %d got %d\n",i,reverse,cases[i].accepted,actual);abort();}
 }
 }
'''.replace('CASES',','.join(constructors))
extra=r'''
 {static const struct {ULONG query[256],text[128];unsigned n,length;int forward[2],backward[2];} cases[]={FIXTURES};
 for(unsigned i=0;i<sizeof(cases)/sizeof(*cases);i++)for(int reverse=0;reverse<2;reverse++){
  ULONG q[256],cells[128];unsigned n=cases[i].n,length=cases[i].length;
  for(unsigned j=0;j<n;j++)q[j]=cases[i].query[j];
  for(unsigned j=0;j<length;j++)cells[j]=cases[i].text[j];
  struct charmap_line row={0};row.text=cells;row.size=length;
  assert(con_regex_direction_begin(&selection.scan,&row,q,n,FALSE,reverse));
  struct con_search_match m;memset(&m,0xa5,sizeof(m));struct con_search_match before=m;
  assert(con_regex_step(&selection.scan,0,&m)==CON_SEARCH_MORE&&!memcmp(&m,&before,sizeof(m)));
  enum con_search_result status;unsigned steps=0;
  do{status=con_regex_step(&selection.scan,1,&m);assert(++steps<20000);}while(status==CON_SEARCH_MORE);
  const int *expected=reverse?cases[i].backward:cases[i].forward;
  if(!((expected[0]<0&&status==CON_SEARCH_DONE)||(expected[0]>=0&&status==CON_SEARCH_FOUND&&m.first.column==(unsigned)expected[0]&&m.end.column==(unsigned)expected[1]))){
   fprintf(stderr,"ASCII mode case %u reverse %d expected %d..%d actual %d %u..%u\n",i,reverse,expected[0],expected[1],status,m.first.column,m.end.column);abort();
  }
 }
 }
'''.replace('FIXTURES',','.join(fixtures))
for label,old,new in [('production',None,None),
 ('ASCII classes','if (!ascii) return rx_unicode_class(cp, mask);','if (!ascii || ascii) return rx_unicode_class(cp, mask);'),
 ('ASCII folding',"if (!unicode) return fold && cp >= 'A' && cp <= 'Z' ? cp + ('a'-'A') : cp;",'(void)unicode;'),
 ('scope restoration','p->unicode = saved_unicode;','(void)saved_unicode;'),
 ('reverse word edges','if (op == RX_WORDSTART) op = RX_WORDEND;','if (FALSE) op = RX_WORDEND;')]:
 def transform(body):
  before,end=body.rsplit('\n}',1);body=before+extra+constructor_body+'\n}'+end
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
   print('ASCII mode PASS:',len(data['cases'])*2,'directional raw spans, scoped flags, ASCII classes and folding, zero budget and malformed flags')
