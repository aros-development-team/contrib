#!/usr/bin/env python3
"""Cursor-origin selection against independently enumerated match positions."""
from pathlib import Path
import itertools,json,re,subprocess,tempfile
r=Path(__file__).resolve().parents[1]
def source(name):
 return '\n'.join(x for x in (r/name).read_text().splitlines() if not x.startswith('#include'))
line=(r/'charmap.h').read_text();line=line[line.index('struct charmap_line\n'):line.index('VOID charmap_dispose_lines')]
prefix='''
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
typedef uint32_t ULONG;typedef uint16_t UWORD;typedef uint8_t UBYTE;typedef int8_t BYTE;typedef uint64_t UQUAD;typedef int BOOL;typedef void VOID;
#define TRUE 1
#define FALSE 0
#define CHARMAP_ERASED 0x21
'''
cases=[]
for length in range(6):
 for chars in itertools.product('ab',repeat=length):
  text=''.join(chars)
  for regex,pattern in [(0,'a'),(0,'aa'),(0,'ab'),(1,'a'),(1,'aa'),(1,'a+'),(1,'a*'),(1,'[ab]+'),(1,'^a'),(1,'a$'),(1,r'\ba')]:
   forward=[m.start() for m in re.finditer(pattern,text) if m.end()>m.start()] if regex else [i for i in range(len(text)) if text.startswith(pattern,i)]
   mirrored={'a':'a','aa':'aa','a+':'a+','a*':'a*','[ab]+':'[ab]+','^a':'a$','a$':'^a',r'\ba':r'a\b'}
   backward=sorted(length-m.end() for m in re.finditer(mirrored[pattern],text[::-1]) if m.end()>m.start()) if regex else forward
   for origin in range(length+3):
    for reverse,inclusive in itertools.product(range(2),repeat=2):
     matches=backward if reverse else forward
     candidates=[i for i in matches if (i<origin or (inclusive and i==origin))] if reverse else [i for i in matches if (i>origin or (inclusive and i==origin))]
     expected=(candidates[-1] if reverse else candidates[0]) if candidates else (matches[-1] if reverse else matches[0]) if matches else -1
     cases.append('{'+','.join([json.dumps(text),json.dumps(pattern),str(regex),str(origin),str(reverse),str(inclusive),str(expected)])+'}')
main='''
struct test {const char *text,*query;unsigned regex,origin,reverse,inclusive;int expected;};
static const struct test tests[]={'''+',\n'.join(cases)+'''};
int main(void){
 for(unsigned i=0;i<sizeof(tests)/sizeof(*tests);i++){
  const struct test *t=&tests[i];ULONG text[32]={0},query[32]={0};
  for(unsigned j=0;t->text[j];j++)text[j]=(UBYTE)t->text[j];
  for(unsigned j=0;t->query[j];j++)query[j]=(UBYTE)t->query[j];
  struct charmap_line line={.text=text,.size=strlen(t->text)};
  struct con_search literal;struct con_regex regex;struct con_search_origin state;
  assert(con_search_origin_begin(&state,&literal,&regex,&line,query,strlen(t->query),t->regex,t->reverse,t->inclusive,0,t->origin));
  struct con_search_origin before=state;struct con_search_match match={{&line,999},{&line,999}},saved=match;
  assert(con_search_origin_step(&state,&literal,&regex,0,&match)==CON_SEARCH_MORE);
  assert(!memcmp(&state,&before,sizeof(state))&&!memcmp(&match,&saved,sizeof(match)));
  enum con_search_result result;unsigned work=0;
  do {saved=match;result=con_search_origin_step(&state,&literal,&regex,i%2?1:7,&match);assert(++work<10000);
   if(result!=CON_SEARCH_FOUND)assert(!memcmp(&match,&saved,sizeof(match)));
  }while(result==CON_SEARCH_MORE);
  if(t->expected<0)assert(result==CON_SEARCH_DONE);
  else if(result!=CON_SEARCH_FOUND||match.first.line!=&line||match.first.column!=(ULONG)t->expected){
   fprintf(stderr,"case %u text=%s query=%s origin=%u reverse=%u inclusive=%u expected=%d actual=%u\\n",i,t->text,t->query,t->origin,t->reverse,t->inclusive,t->expected,match.first.column);abort();
  }
  assert(con_search_origin_step(&state,&literal,&regex,99,&match)==CON_SEARCH_DONE);
 }
 /* Row coordinates, soft wraps, and wide leaders are physical positions. */
 {
  ULONG a[]={'a',0x4e2d,0xfffffffe,'b'},b[]={'a','b'};
  struct charmap_line rows[3]={{.text=a,.size=4,.wrapColumns=4},{0},{.text=b,.size=2}};
  rows[0].next=&rows[1];rows[1].prev=rows;rows[1].next=&rows[2];rows[2].prev=&rows[1];
  for(unsigned regex_mode=0;regex_mode<2;regex_mode++)for(unsigned row=0;row<3;row++)for(unsigned reverse=0;reverse<2;reverse++){
   ULONG query[]={'b'};struct con_search literal;struct con_regex regex;struct con_search_origin state;struct con_search_match match;
   assert(con_search_origin_begin(&state,&literal,&regex,rows,query,1,regex_mode,reverse,TRUE,row,0));
   enum con_search_result result;unsigned work=0;do{result=con_search_origin_step(&state,&literal,&regex,1,&match);assert(++work<1000);}while(result==CON_SEARCH_MORE);
   unsigned expected=reverse?(row?0:2):(row?2:0);assert(result==CON_SEARCH_FOUND&&match.first.line==rows+expected&&state.result_row==expected&&match.first.column==(expected?1:3));
  }
 }
 {
  struct charmap_line *rows=calloc(10000,sizeof(*rows));assert(rows);ULONG cell='x',query='x';
  for(unsigned i=0;i<10000;i++){rows[i].next=i<9999?rows+i+1:NULL;rows[i].prev=i?rows+i-1:NULL;}
  rows[9999].text=&cell;rows[9999].size=1;
  struct con_search literal;struct con_regex regex;struct con_search_origin state;struct con_search_match match;
  assert(con_search_origin_begin(&state,&literal,&regex,rows,&query,1,FALSE,FALSE,TRUE,0,0));
  assert(con_search_origin_step(&state,&literal,&regex,32,&match)==CON_SEARCH_MORE);
  unsigned ticks=0;enum con_search_result result;do{result=con_search_origin_step(&state,&literal,&regex,32,&match);assert(++ticks<5000);}while(result==CON_SEARCH_MORE);
  assert(ticks>100&&result==CON_SEARCH_FOUND&&match.first.line==rows+9999);free(rows);
 }
 printf("Origin search PASS: %zu independent positions, physical rows/wide cells and 10000-row bounded scan\\n",sizeof(tests)/sizeof(*tests));
}
'''
base=prefix+line+source('search.h')+source('search_regex.h')+source('search.c')+source('search_unicode.h')+source('search_case.h')+source('search_regex.c')
header=source('search_origin.h')
for label,body in [('production',header),('origin-column',header.replace('s->pending.first.column<s->origin_column','FALSE')),('wrap-choice',header.replace(':s->first;',':s->last;'))]:
 with tempfile.TemporaryDirectory() as d:
  p=Path(d)/'t.c';b=Path(d)/'t';p.write_text(base+body+main)
  subprocess.run(['cc','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p),'-o',str(b)],check=True)
  result=subprocess.run([str(b)],capture_output=True,text=True,timeout=60)
  if label=='production':print(result.stdout,end='');assert result.returncode==0,result.stderr
  else:assert result.returncode!=0;print('Negative control PASS:',label)
