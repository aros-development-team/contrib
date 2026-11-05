#!/usr/bin/env python3
"""Seed anchor and directional destination against independently listed matches."""
from pathlib import Path
import itertools,json,re,subprocess,tempfile
r=Path(__file__).resolve().parents[1]
def source(name):return '\n'.join(x for x in (r/name).read_text().splitlines() if not x.startswith('#include'))
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
for length in range(5):
 for chars in itertools.product('ab ',repeat=length):
  text=''.join(chars)
  for query in ['a','ab','a.b','a+']:
   matches=[(m.start(),m.end()) for m in re.finditer(query,text)]
   for origin in range(length+3):
    anchor=next((m for m in matches if m[1]>origin),matches[0] if matches else None)
    for reverse in range(2):
     backwards={'a':'a','ab':'ba','a.b':'b.a','a+':'a+'}
     directional=sorted((length-m.end(),length-m.start()) for m in re.finditer(backwards[query],text[::-1])) if reverse else matches
     candidates=[m for m in directional if m[0]<anchor[0]] if reverse and anchor else [m for m in directional if m[0]>=anchor[1]] if anchor else []
     target=(candidates[-1] if reverse else candidates[0]) if candidates else (directional[-1] if reverse else directional[0]) if directional else None
     cases.append('{'+','.join([json.dumps(text),json.dumps(query),str(origin),str(reverse),str(anchor[0] if anchor else -1),str(anchor[1] if anchor else -1),str(target[0] if target else -1)])+'}')
main='''
struct test {const char *text,*query;unsigned origin,reverse;int first,end,target;};
static const struct test tests[]={'''+',\n'.join(cases)+'''};
int main(void){
 for(unsigned i=0;i<sizeof(tests)/sizeof(*tests);i++){
  const struct test*t=&tests[i];ULONG text[32]={0},query[32]={0};
  for(unsigned j=0;t->text[j];j++)text[j]=(UBYTE)t->text[j];
  for(unsigned j=0;t->query[j];j++)query[j]=(UBYTE)t->query[j];
  struct charmap_line line={.text=text,.size=strlen(t->text)};
  struct con_regex regex;assert(con_regex_begin(&regex,&line,query,strlen(t->query),TRUE));
  struct con_search_anchor anchor;con_search_anchor_begin(&anchor,&line,0,t->origin);
  struct con_search_anchor before=anchor;struct con_search_match match={{&line,999},{&line,999}},saved=match;
  assert(con_search_anchor_step(&anchor,&regex,0,&match)==CON_SEARCH_MORE);
  assert(!memcmp(&before,&anchor,sizeof(anchor))&&!memcmp(&match,&saved,sizeof(match)));
  enum con_search_result result;unsigned ticks=0;
  do {saved=match;result=con_search_anchor_step(&anchor,&regex,i%2?1:7,&match);assert(++ticks<10000);
   if(result!=CON_SEARCH_FOUND)assert(!memcmp(&match,&saved,sizeof(match)));
  }while(result==CON_SEARCH_MORE);
  if(t->first<0)assert(result==CON_SEARCH_DONE);
  else{
   assert(result==CON_SEARCH_FOUND&&match.first.column==(ULONG)t->first&&match.end.column==(ULONG)t->end);
   assert(!anchor.result_row&&!anchor.result_end_row);
   struct con_search literal;struct con_search_origin target;
   assert(con_search_origin_begin(&target,&literal,&regex,&line,query,strlen(t->query),TRUE,t->reverse,!t->reverse,0,t->reverse?match.first.column:match.end.column));
   do {result=con_search_origin_step(&target,&literal,&regex,3,&match);assert(++ticks<10000);}while(result==CON_SEARCH_MORE);
   assert(result==CON_SEARCH_FOUND&&match.first.column==(ULONG)t->target);
  }
  assert(con_search_anchor_step(&anchor,&regex,99,&match)==CON_SEARCH_DONE);
 }
 {
  ULONG a[]={'a',0x4e2d,0xfffffffe},b[]={'b'},query[]={'a',0x4e2d,'b'};
  struct charmap_line rows[3]={{.text=a,.size=3,.wrapColumns=3},{.text=b,.size=1},{.text=a,.size=3}};
  for(unsigned i=0;i<3;i++){rows[i].next=i<2?rows+i+1:NULL;rows[i].prev=i?rows+i-1:NULL;}
  for(unsigned row=0;row<3;row++)for(unsigned column=0;column<4;column++){
   struct con_regex regex;assert(con_regex_begin(&regex,rows,query,3,TRUE));
   struct con_search_anchor anchor;con_search_anchor_begin(&anchor,rows,row,column);struct con_search_match match;
   enum con_search_result result;unsigned ticks=0;do{result=con_search_anchor_step(&anchor,&regex,1,&match);assert(++ticks<1000);}while(result==CON_SEARCH_MORE);
   assert(result==CON_SEARCH_FOUND&&match.first.line==rows&&match.end.line==rows+1&&match.end.column==1);
   assert(anchor.result_row==0&&anchor.result_end_row==1);
  }
 }
 {
  struct charmap_line *rows=calloc(10000,sizeof(*rows));assert(rows);ULONG cell='x';
  for(unsigned i=0;i<10000;i++){rows[i].next=i<9999?rows+i+1:NULL;rows[i].prev=i?rows+i-1:NULL;}
  rows[9999].text=&cell;rows[9999].size=1;
  struct con_regex regex;assert(con_regex_begin(&regex,rows,&cell,1,TRUE));
  struct con_search_anchor anchor;con_search_anchor_begin(&anchor,rows,0,0);struct con_search_match match;
  assert(con_search_anchor_step(&anchor,&regex,32,&match)==CON_SEARCH_MORE);
  enum con_search_result result;unsigned ticks=0;do{result=con_search_anchor_step(&anchor,&regex,32,&match);assert(++ticks<10000);}while(result==CON_SEARCH_MORE);
  assert(ticks>100&&result==CON_SEARCH_FOUND&&anchor.result_row==9999&&anchor.result_end_row==9999);free(rows);
 }
 printf("Anchor PASS: %zu independent end-anchor/destination cases, wide soft wraps, physical rows and 10000-row bounded scan\\n",sizeof(tests)/sizeof(*tests));
}
'''
base=prefix+line+source('search.h')+source('search_regex.h')+source('search.c')+source('search_unicode.h')+source('search_case.h')+source('search_regex.c')+source('search_origin.h')
header=source('search_anchor.h')
for label,body in [('production',header),('start-not-end',header.replace('s->pending.end.column>s->origin_column','s->pending.first.column>s->origin_column')),('inclusive-end',header.replace('s->pending.end.column>s->origin_column','s->pending.end.column>=s->origin_column'))]:
 with tempfile.TemporaryDirectory() as d:
  p=Path(d)/'t.c';b=Path(d)/'t';p.write_text(base+body+main)
  subprocess.run(['cc','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p),'-o',str(b)],check=True)
  result=subprocess.run([str(b)],capture_output=True,text=True,timeout=60)
  if label=='production':print(result.stdout,end='');assert result.returncode==0,result.stderr
  else:assert body!=header and result.returncode!=0;print('Negative control PASS:',label)
