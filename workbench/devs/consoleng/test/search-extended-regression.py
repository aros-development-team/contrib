#!/usr/bin/env python3
"""Extended-mode spacing, comments and scope against the pinned terminal."""
from pathlib import Path
import contextlib,importlib.util,io,json,subprocess,tempfile
here=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('audit',here/'search-reference-audit.py')
audit=importlib.util.module_from_spec(spec);spec.loader.exec_module(audit)
data=json.loads((here/'search-extended-cases.json').read_text())
assert data['reference']==audit.PIN and data['columns']==80 and len(data['cases'])==1200
extra=r'''
 {
  const ULONG spaces[]={9,10,11,12,13,32,0x85,0xa0,0x1680,0x2000,0x2003,0x200a,0x2028,0x2029,0x202f,0x205f,0x3000};
  ULONG cells[]={'a','b'};struct charmap_line row={0};row.text=cells;row.size=2;
  for(unsigned i=0;i<sizeof(spaces)/sizeof(*spaces);i++)for(int reverse=0;reverse<2;reverse++){
   ULONG q[]={'(','?','x',')','a',spaces[i],'b'};
   assert(con_regex_direction_begin(&selection.scan,&row,q,7,FALSE,reverse));
   struct con_search_match m;enum con_search_result result;unsigned steps=0;
   do{result=con_regex_step(&selection.scan,1,&m);assert(++steps<1000);}while(result==CON_SEARCH_MORE);
   assert(result==CON_SEARCH_FOUND&&!m.first.column&&m.end.column==2);
  }
  const char*bad[]={"(?x-)","(?xx)a","(?x-x)a","(?-xx)a","(?x--i)a","(?x)(? x:ab)",
   "(?x)(?x :ab)","(?x)[a#comment]","(?x)(a#comment)","(?x)\\x{#comment}",
   "(?x)a{#comment}","(?x)[a - #comment]","(?x) #comment\n +","(?x)*",
   "(?x)[\\w - a]","(?x)[ ]","(?x)\\x{ 1 1 0 0 0 0 }","(?x)\\u d 8 0 0"};
  for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);i++){
   ULONG q[128];unsigned len=strlen(bad[i]);for(unsigned j=0;j<len;j++)q[j]=(unsigned char)bad[i][j];
   assert(!con_regex_direction_begin(&selection.scan,&row,q,len,FALSE,FALSE));
  }
  /* End-of-query comments terminate within existing parser limits. */
  ULONG q[CON_SEARCH_MAX];q[0]='(';q[1]='?';q[2]='x';q[3]=')';q[4]='#';
  for(unsigned i=5;i<CON_SEARCH_MAX;i++)q[i]='a';
  assert(con_regex_direction_begin(&selection.scan,&row,q,CON_SEARCH_MAX,FALSE,FALSE));
 }
'''
for label,old,new in [('production',None,None),
 ('extended mode skipping','if (!p->extended) return;','return;'),
 ('extended scope restoration','p->extended = saved_extended;','(void)saved_extended;'),
 ('comment newline termination',"if (cp == '\\n') comment = FALSE;",'(void)cp;')]:
 def transform(body):
  before,end=body.rsplit('\n}',1);body=before+extra+'\n}'+end
  if old:assert old in body;body=body.replace(old,new)
  return body
 with tempfile.TemporaryDirectory() as d:
  if old:
   try:
    with contextlib.redirect_stdout(io.StringIO()):failed=audit.production(Path(d),data['cases'],80,transform)
   except subprocess.CalledProcessError as error:
    assert error.returncode==-6,error;failed=True
   assert failed,label;print('Negative control PASS:',label)
  else:
   assert not audit.production(Path(d),data['cases'],80,transform)
   print('Extended regex PASS: Unicode whitespace, scoped modes, comments, malformed rejection and bounded EOF comments')
