#!/usr/bin/env python3
"""Hex scalar escapes: pinned terminal spans, scalar boundaries and rejection."""
from pathlib import Path
import contextlib,importlib.util,io,json,subprocess,tempfile
here=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('audit',here/'search-reference-audit.py')
audit=importlib.util.module_from_spec(spec);spec.loader.exec_module(audit)
data=json.loads((here/'search-hex-cases.json').read_text())
assert data['reference']==audit.PIN and data['columns']==80
extra=r'''
 /* Non-ASCII scalar matching uses native ULONG cells, not a byte-string oracle. */
 {
  const ULONG scalars[]={0,1,9,10,13,0x20,0x2d,0x5c,0x7f,0x80,0xff,0x100,
   0x3a9,0x3c9,0xd7ff,0xe000,0xffff,0x10000,0x1f600,0x10ffff};
  for(unsigned i=0;i<sizeof(scalars)/sizeof(*scalars);i++)for(unsigned form=0;form<6;form++){
   ULONG cp=scalars[i];if(form==0&&cp>255)continue;if(form==2&&cp>65535)continue;
   char pattern[64];const char*formats[]={"\\x%02x","\\x{%x}","\\u%04x","\\u{%x}","\\U%08x","\\U{%x}"};
   snprintf(pattern,sizeof(pattern),formats[form],cp);
   ULONG q[64],cells[]={0x21,cp,0x21};unsigned len=strlen(pattern);
   for(unsigned j=0;j<len;j++)q[j]=(unsigned char)pattern[j];
   struct charmap_line row={0};row.text=cells;row.size=3;
   for(int reverse=0;reverse<2;reverse++){
    assert(con_regex_direction_begin(&selection.scan,&row,q,len,FALSE,reverse));
    struct con_search_match found;enum con_search_result result;unsigned steps=0;
    do{result=con_regex_step(&selection.scan,1,&found);assert(++steps<5000);}while(result==CON_SEARCH_MORE);
    /* Zero cells are padding in the retained-text representation. */
    if(cp)assert(result==CON_SEARCH_FOUND&&found.first.column==1&&found.end.column==2);
    else assert(result==CON_SEARCH_DONE);
   }
  }
  const char*bad[]={"\\x","\\x0","\\xGG","\\x{}","\\x{","\\x{61","\\x{g}",
   "\\x{110000}","\\x{ffffffff}","\\x{100000000}","\\x{d800}","\\x{dfff}",
   "\\u","\\u061","\\u0g61","\\ud800","\\udfff","\\u{}","\\u{110000}",
   "\\U","\\U0000061","\\U00110000","\\Uffffffff","\\U0000d800","\\U{dfff}",
   "[\\x61-\\x60]","[\\x{110000}]","[\\uD800]"};
  for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);i++){
   ULONG q[128];unsigned len=strlen(bad[i]);for(unsigned j=0;j<len;j++)q[j]=(unsigned char)bad[i][j];
   for(int reverse=0;reverse<2;reverse++)assert(!con_regex_direction_begin(&selection.scan,NULL,q,len,FALSE,reverse));
  }
  /* Braced leading zeros are valid, and compilation still has a fixed budget. */
  ULONG q[CON_SEARCH_MAX];q[0]='\\';q[1]='x';q[2]='{';
  for(unsigned i=3;i<CON_SEARCH_MAX-1;i++)q[i]='0';
  q[CON_SEARCH_MAX-1]='}';
  assert(con_regex_direction_begin(&selection.scan,NULL,q,CON_SEARCH_MAX,FALSE,FALSE));
 }
'''
for label,old,new in [('production',None,None),
 ('scalar overflow guard','value > (0x10ffff - digit) / 16','FALSE'),
 ('surrogate rejection','(value >= 0xd800 && value <= 0xdfff)','FALSE')]:
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
   print('Hex escapes PASS: six forms, scalar boundaries both directions, malformed/range rejection and bounded long zeros')
