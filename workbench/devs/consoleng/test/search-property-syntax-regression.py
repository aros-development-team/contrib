#!/usr/bin/env python3
"""Reference-qualified short property and named-value operator parsing."""
from pathlib import Path
import contextlib,importlib.util,io,json,tempfile,subprocess
here=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('audit',here/'search-reference-audit.py')
audit=importlib.util.module_from_spec(spec);spec.loader.exec_module(audit)
data=json.loads((here/'search-property-syntax-cases.json').read_text());assert data['reference']==audit.PIN
bad=[r'\p',r'\P',r'\pQ','\\p\\',r'\p{}',r'\p{gc:}',r'\p{gc!=}',r'\p{gc!==N}',r'\p{gc!=Lu}',r'\P{gc!=Lu}',r'(?-u:\pN)',r'[\pN-a]',r'[a-\pN]',r'\p{N',r'\p{'+('N'*64)+'}']
checks='const char *bad[]={'+','.join(json.dumps(p) for p in bad)+'};'+r'''
 for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);i++){
  ULONG q[256];unsigned n=strlen(bad[i]);for(unsigned j=0;j<n;j++)q[j]=(UBYTE)bad[i][j];
  assert(!con_regex_select_begin(&selection,NULL,q,n,0,0,NULL));
 }
'''
def invalid_checks(source):return source.replace('int main(void) {','int main(void) {'+checks)
with tempfile.TemporaryDirectory() as d:
 assert not audit.production(Path(d),data['cases'],data['columns'],invalid_checks)
for label,old,new in [('short property','BOOL braced=p->q[p->at]==\'{\';','BOOL braced=TRUE;'),('colon equality',"if(op)*op='=';","if(op)*op=':';")]:
 def change(source):assert old in source;return source.replace(old,new)
 with tempfile.TemporaryDirectory() as d:
  try:
   with contextlib.redirect_stdout(io.StringIO()):failed=audit.production(Path(d),data['cases'],data['columns'],change)
  except subprocess.CalledProcessError as exc:
   assert Path(exc.cmd[0]).name=='production',exc
   failed=True
  assert failed,label
 print('Negative control PASS:',label)
