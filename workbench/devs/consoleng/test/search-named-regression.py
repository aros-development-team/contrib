#!/usr/bin/env python3
"""Named-group syntax, complete-match equivalence, and Rust name properties."""
from pathlib import Path
import contextlib,importlib.util,io,json,subprocess,tempfile
here=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('audit',here/'search-reference-audit.py')
audit=importlib.util.module_from_spec(spec);spec.loader.exec_module(audit)
data=json.loads((here/'search-named-cases.json').read_text());assert data['reference']==audit.PIN
constructors=[]
for pattern,accepted in data['constructors']:
 constructors.append('{{'+','.join(str(ord(c)) for c in pattern)+'},'+str(len(pattern))+','+str(int(accepted))+'}')
bodies=['a+','(ab|a)+','(?i:a)b',r'(?-u:\w)+','^word$','(a?){2}','(?mR:^a$)','(?U:a+)','(?s:a.b)']
texts=['a','aaa','ababa','Ab','word',' aab ','','a\rb','a\nb','a\r\nb','éwordé']
span_cases=[]
for body in bodies:
 for text in texts:
  for opening in ['(?<name>','(?P<name>']:
   query=opening+body+')';plain='('+body+')'
   def array(s):return '{'+','.join(str(ord(c)) for c in s)+'}'
   span_cases.append('{'+','.join([array(query),array(plain),array(text),str(len(query)),str(len(plain)),str(len(text))])+'}')
extra=r'''
 {ULONG python_name[]={'(','?','P','<','n','>','a',')'};
  ULONG lower_name[]={'(','?','<','n','>','a',')'};
  ULONG upper_name[]={'(','?','<','N','>','a',')'};
  assert(!con_regex_smart_fold(python_name,8));
  assert(con_regex_smart_fold(lower_name,7));
  assert(!con_regex_smart_fold(upper_name,7));}

 {uint64_t hash=UINT64_C(14695981039346656037);
 for(ULONG cp=0;cp<=0x10ffff;cp++){
  unsigned bits=(rx_capture_char(cp,TRUE)?1:0)|(rx_capture_char(cp,FALSE)?2:0);
  hash=(hash^bits)*UINT64_C(1099511628211);
 }
 assert(hash==UINT64_C(HASH));}
 {static const struct {ULONG q[256];unsigned n;int accepted;} cases[]={CONSTRUCTORS};
 struct charmap_line row={0};
 for(unsigned i=0;i<sizeof(cases)/sizeof(*cases);i++)for(int reverse=0;reverse<2;reverse++){
  int actual=con_regex_direction_begin(&selection.scan,&row,cases[i].q,cases[i].n,FALSE,reverse);
  if(actual!=cases[i].accepted){fprintf(stderr,"Named constructor %u expected %d got %d\n",i,cases[i].accepted,actual);abort();}
 }
 }
 {static const struct {ULONG q[128],plain[128],text[128];unsigned n,pn,length;} cases[]={SPANS};
 for(unsigned i=0;i<sizeof(cases)/sizeof(*cases);i++)for(int reverse=0;reverse<2;reverse++){
  struct charmap_line row={0};row.text=(ULONG*)cases[i].text;row.size=cases[i].length;
  struct con_search_match found[2];enum con_search_result status[2];
  for(int named=0;named<2;named++){
   assert(con_regex_direction_begin(&selection.scan,&row,named?cases[i].q:cases[i].plain,named?cases[i].n:cases[i].pn,FALSE,reverse));
   memset(&found[named],0,sizeof(found[named]));unsigned steps=0;
   assert(con_regex_step(&selection.scan,0,&found[named])==CON_SEARCH_MORE);
   do{status[named]=con_regex_step(&selection.scan,1,&found[named]);assert(++steps<20000);}while(status[named]==CON_SEARCH_MORE);
  }
  assert(status[0]==status[1]);
  if(status[0]==CON_SEARCH_FOUND)assert(found[0].first.column==found[1].first.column&&found[0].end.column==found[1].end.column);
 }
 }
'''.replace('HASH',data['classifier_hash']).replace('CONSTRUCTORS',','.join(constructors)).replace('SPANS',','.join(span_cases))
for label,old,new in [('production',None,None),
 ('duplicate rejection','if (same) { p->failed = TRUE; return; }','if (same) { break; }'),
 ('first character rule',"if (cp == '_') return TRUE;","if (cp == '_' || cp == '0') return TRUE;"),
 ('group body',"p->at++; rx_capture_name(p);","p->at++; rx_capture_name(p); if (!p->failed && p->at < p->length) p->at++;")]:
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
   print('Named groups PASS:',len(data['constructors'])*2,'constructor checks;',len(span_cases)*2,'directional span equivalence checks; all Unicode name scalars match Rust')
