#!/usr/bin/env python3
"""Incremental copy against retained legacy serialization and explicit bytes."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'charmapconclass.c').read_text()
t=(r/'test/softwrap-copy-regression.py').read_text();prefix=t.split("prefix=r'''",1)[1].split("'''",1)[0]
prefix='#include <stdio.h>\n'+'\n'.join(x for x in prefix.splitlines() if not x.startswith('static void check('))
prefix+='\n#define TRUE 1\n#define FALSE 0\nstruct con_search_position {const struct charmap_line *line;ULONG column;};\nstruct con_search_match {struct con_search_position first,end;};\n'
u=(r/'support.c').read_text();u=u[u.index('ULONG con_utf8_encode('):u.index('ULONG con_utf8_count(')]
legacy=s[s.index('static BOOL charmap_search_space('):s.index('static VOID charmap_search_delete_word(')]+s[s.index('static VOID charmap_block_span('):s.index('/* Overlay only:')]+s[s.index('static ULONG enc_cells_len('):s.index('struct ConClipData')]
header=(r/'copy_encode.h').read_text()
main=r'''
static ULONG encode(struct charmap_line *first,struct charmap_line *last,ULONG left,ULONG right,BOOL utf8,BOOL exact,BOOL block,UBYTE **out){
 struct con_search_match span={{first,left},{last,right}};struct con_copy_encode state;
 con_copy_begin(&state,&span,utf8,exact,block);struct con_copy_encode before=state;
 assert(con_copy_step(&state,0)==CON_COPY_MORE&&!memcmp(&before,&state,sizeof(state)));
 enum con_copy_result result;unsigned ticks=0;
 do{result=con_copy_step(&state,1+left%7);assert(++ticks<100000);}while(result==CON_COPY_MORE);
 assert(result==CON_COPY_MEASURED);ULONG size=state.size;
 *out=malloc(size+2);assert(*out);memset(*out,0xa5,size+2);
 before=state;assert(!con_copy_write_begin(&state,*out,size+1)&&!memcmp(&before,&state,sizeof(state)));
 assert(con_copy_write_begin(&state,*out,size));before=state;
 assert(con_copy_step(&state,0)==CON_COPY_MORE&&!memcmp(&before,&state,sizeof(state)));
 do{result=con_copy_step(&state,1+right%7);assert(++ticks<200000);}while(result==CON_COPY_MORE);
 assert(result==CON_COPY_DONE&&state.offset==size&&(*out)[size]==0xa5&&(*out)[size+1]==0xa5);
 assert(con_copy_step(&state,99)==CON_COPY_DONE);return size;
}
int main(void){
 ULONG text[][8]={{'a','b',' ',' '},{'A',0x4e2d,0xfffffffe,'B'}, {0}, {'x','\t',' '}, {0x3a9,0x2003,'z'}, {'a',0xa0}, {'a','b',' '}, {0x1f431,'x'}};
 ULONG lengths[]={4,4,0,3,3,2,3,2};struct charmap_line rows[8]={0};UBYTE erased[]={0,0,CHARMAP_ERASED,CHARMAP_ERASED};
 for(unsigned i=0;i<8;i++){rows[i].text=text[i];rows[i].size=lengths[i];rows[i].next=i<7?rows+i+1:NULL;rows[i].wrapColumns=i%2?lengths[i]:0;}
 rows[0].flags=erased;rows[6].wrapColumns=3;rows[6].wrapPadding=1;
 unsigned cases=0;
 for(unsigned a=0;a<8;a++)for(unsigned b=a;b<8;b++)for(unsigned left=0;left<9;left++)for(unsigned right=0;right<9;right++)
 for(unsigned utf=0;utf<2;utf++)for(unsigned exact=0;exact<2;exact++)for(unsigned block=0;block<2;block++){
  UBYTE *actual;ULONG size=encode(rows+a,rows+b,left,right,utf,exact,block,&actual);
  ULONG expected_size=charmapcon_calc_selection_size(rows+a,rows+b,left,right,utf,exact,block);
  char *expected=charmapcon_get_selection(expected_size,rows+a,rows+b,left,right,utf,exact,block);
  assert(size==expected_size&&(!size||!memcmp(actual,expected,size)));free(actual);free(expected);cases++;
 }
 /* Independent byte expectation: Greek, wide CJK, no continuation or spacer,
  * explicit hard newline, and Latin-1 replacement. */
 {
  ULONG a[]={0x3a9,0x4e2d,0xfffffffe,' '},b[]={0x1f431};
  struct charmap_line lines[2]={{.text=a,.size=4,.wrapColumns=4,.wrapPadding=1},{.text=b,.size=1}};lines[0].next=lines+1;
  UBYTE *out;ULONG size=encode(lines,lines+1,0,1,TRUE,FALSE,FALSE,&out);
  const char expected[]="\xce\xa9\xe4\xb8\xad\xf0\x9f\x90\xb1\r";
  assert(size==sizeof(expected)-1&&!memcmp(out,expected,size));free(out);
  size=encode(lines,lines+1,0,1,FALSE,FALSE,FALSE,&out);assert(size==4&&!memcmp(out,"???\r",4));free(out);
 }
 {
  ULONG *text=calloc(10000,sizeof(*text));UBYTE *flags=malloc(10000);assert(text&&flags);
  for(unsigned i=0;i<10000;i++){text[i]=' ';flags[i]=CHARMAP_ERASED;}
  struct charmap_line row={.text=text,.size=10000,.flags=flags};struct con_search_match span={{&row,0},{&row,10000}};
  struct con_copy_encode state;con_copy_begin(&state,&span,TRUE,TRUE,FALSE);
  assert(con_copy_step(&state,32)==CON_COPY_MORE);unsigned ticks=0;enum con_copy_result result;
  do{result=con_copy_step(&state,32);assert(++ticks<1000);}while(result==CON_COPY_MORE);
  assert(ticks>100&&result==CON_COPY_MEASURED&&!state.size);free(flags);free(text);
 }
 {
  struct charmap_line row={0};struct con_search_match span={{&row,0},{&row,0}};struct con_copy_encode state;
  con_copy_begin(&state,&span,TRUE,FALSE,FALSE);state.offset=~(ULONG)0-1;
  assert(!con_copy_put(&state,'x')&&state.failed&&con_copy_step(&state,1)==CON_COPY_ERROR);
  con_copy_begin(&state,&span,TRUE,FALSE,FALSE);state.size=1;state.finished=TRUE;UBYTE guard[2]={0xa5,0xa5};
  assert(con_copy_write_begin(&state,guard,1));assert(!con_copy_put(&state,0x1f431)&&guard[0]==0xa5&&guard[1]==0xa5);
 }
 printf("Copy encoder PASS: %u legacy-equivalence cases, independent Unicode/Latin1 bytes, bounded erased trimming, zero budget, guards and overflow\n",cases);
}
'''
for label,body in [('production',header),('soft-wrap separator',header.replace('!s->line->wrapColumns &&','TRUE &&')),('wide continuation',header.replace('if(cp==0xfffffffeUL)return TRUE;','if(FALSE && cp==0xfffffffeUL)return TRUE;'))]:
 with tempfile.TemporaryDirectory() as d:
  p=Path(d)/'t.c';b=Path(d)/'t';p.write_text(prefix+u+legacy+body+main)
  subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(b)],check=True)
  result=subprocess.run([str(b)],capture_output=True,text=True,timeout=60)
  if label=='production':assert result.returncode==0,result.stderr;print(result.stdout,end='')
  else:assert body!=header and result.returncode!=0;print('Negative control PASS:',label)
