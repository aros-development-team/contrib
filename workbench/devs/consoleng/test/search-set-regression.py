#!/usr/bin/env python3
"""Nested set semantics, bounded compilation and behavioral controls."""
from pathlib import Path
import contextlib,importlib.util,io,json,subprocess,tempfile
here=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('audit',here/'search-reference-audit.py')
audit=importlib.util.module_from_spec(spec);spec.loader.exec_module(audit)
data=json.loads((here/'search-set-cases.json').read_text());assert data['reference']==audit.PIN
extra=r'''
 const struct {const char *pattern;ULONG cp;BOOL wanted;} unicode[]={
 {"(?i:[k&&\\p{ASCII}])",0x212a,TRUE},
 {"(?-i:[k&&\\p{ASCII}])",0x212a,FALSE},
 {"(?-i:[\\p{L}--\\p{ASCII}])",0x3a9,TRUE},
 {"(?-i:[\\p{L}--\\p{ASCII}])",0x212a,TRUE},
 {"(?i:[\\p{L}--\\p{ASCII}])",0x212a,FALSE},
 {"[\\p{Emoji}--\\p{ASCII}]",0x1f600,TRUE},
 {"[\\p{Emoji}--\\p{ASCII}]",'1',FALSE},
 {"[\\p{Any}&&\\p{Assigned}]",0x378,FALSE},
 {"[\\p{Any}--\\p{Assigned}]",0x378,TRUE},
 {"[[:^digit:]&&\\p{ASCII}]",'A',TRUE},
 {"[[:^digit:]&&\\p{ASCII}]",'4',FALSE},
 {"[\\p{Greek}~~\\p{L}]",0x3a9,FALSE},
 {"[\\p{Greek}~~\\p{L}]",'A',TRUE}
 };
 for(unsigned i=0;i<sizeof(unicode)/sizeof(*unicode);i++)for(int reverse=0;reverse<2;reverse++){
  ULONG q[256],cells[]={unicode[i].cp,0xfffffffe};unsigned n=strlen(unicode[i].pattern);
  for(unsigned j=0;j<n;j++)q[j]=(UBYTE)unicode[i].pattern[j];
  struct charmap_line row={0};row.text=cells;row.size=2;
  assert(con_regex_select_begin(&selection,&row,q,n,FALSE,reverse,NULL));
  struct con_search_match found;enum con_search_result status;unsigned ticks=0;
  do{status=con_regex_select_step(&selection,1,&found);assert(++ticks<10000);}while(status==CON_SEARCH_MORE);
  assert((status==CON_SEARCH_FOUND)==unicode[i].wanted);
 }
 const char *bad[]={"[[a]","[[a]-b]","[a-[b]]","[a&&[z-a]]","[[\\p{L}]-a]","[[a]]{257}"};
 for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);i++){
  ULONG q[256];unsigned n=strlen(bad[i]);for(unsigned j=0;j<n;j++)q[j]=(UBYTE)bad[i][j];
  assert(!con_regex_begin(&selection.scan,NULL,q,n,FALSE));
 }
 ULONG nested[100];for(unsigned i=0;i<40;i++)nested[i]='[';nested[40]='a';
 for(unsigned i=41;i<81;i++)nested[i]=']';
 assert(!con_regex_begin(&selection.scan,NULL,nested,81,FALSE));
 /* Valid bounded inputs and malformed combinations must not overrun postfix
  * storage or hang the cooperative scanner, even when groups repeat sets. */
 uint32_t seed=185;const char alphabet[]="[]^a-z&~\\d012?{}()|";
 for(unsigned sample=0;sample<10000;sample++){
  ULONG q[256];unsigned n=1+(sample%256);
  for(unsigned i=0;i<n;i++){seed=seed*1664525u+1013904223u;q[i]=(UBYTE)alphabet[seed%(sizeof(alphabet)-1)];}
  ULONG cells[]={'a','1','b'};struct charmap_line row={0};row.text=cells;row.size=3;
  if(con_regex_begin(&selection.scan,&row,q,n,sample&1)){
   struct con_search_match found;unsigned ticks=0;
   while(con_regex_step(&selection.scan,1,&found)!=CON_SEARCH_DONE)assert(++ticks<100000);
  }
 }
 puts("Unicode sets, depth/pool limits and 10000 bounded syntax probes PASS");
'''
with tempfile.TemporaryDirectory() as d:
 work=Path(d)
 def extra_checks(source):return source[:source.rfind('}')]+extra+'\n}\n'
 assert not audit.production(work,data['cases'],data['columns'])
 # The extra fixture prints its own result; compile/run separately from the
 # directional span harness so stdout remains a strict reference protocol.
 src=work/'production.c';src.write_text(extra_checks(src.read_text()))
 subprocess.run(['cc','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-g',str(src),'-o',str(work/'extra')],check=True)
 subprocess.run([str(work/'extra')],check=True,stdout=subprocess.DEVNULL)
 print('Unicode sets, depth/pool limits and 10000 bounded syntax probes PASS')
for label,old,new in [
 ('intersection','entry.high==RX_SET_INTERSECT ? left&&right :','entry.high==RX_SET_INTERSECT ? left||right :'),
 ('difference','entry.high==RX_SET_DIFFERENCE ? left&&!right :','entry.high==RX_SET_DIFFERENCE ? left&&right :'),
 ('inner complement','values[used-1]=!values[used-1];','values[used-1]=values[used-1];')]:
 def mutate(source):assert old in source;return source.replace(old,new)
 with tempfile.TemporaryDirectory() as d:
  with contextlib.redirect_stdout(io.StringIO()):failed=audit.production(Path(d),data['cases'],data['columns'],mutate)
  assert failed,label
 print('Negative control PASS:',label)
