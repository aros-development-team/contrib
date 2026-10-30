#!/usr/bin/env python3
"""Bounded semantic search seed against an independent word/escape oracle."""
from pathlib import Path
import itertools,json,subprocess,tempfile
r=Path(__file__).resolve().parents[1]
def source(n):return '\n'.join(x for x in (r/n).read_text().splitlines() if not x.startswith('#include'))
line=(r/'charmap.h').read_text();line=line[line.index('struct charmap_line\n'):line.index('VOID charmap_dispose_lines')]
prefix='''#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
typedef uint32_t ULONG;typedef uint16_t UWORD;typedef uint8_t UBYTE;typedef int8_t BYTE;typedef uint64_t UQUAD;typedef int BOOL;typedef void VOID;
#define TRUE 1
#define FALSE 0
#define CHARMAP_ERASED 0x21
'''
def escape(s):return ''.join(('\\' if c in '$()*+?[\\]^{|}' else '')+c for c in s)
def word(s,col):
 text=s.ljust(8);non=[i for i in list(range(col,8))+list(range(col)) if text[i] not in " ,`|:\"'()[]{}<>\t"]
 if not non:return ''
 i=j=non[0]
 while i and text[i-1] not in " ,`|:\"'()[]{}<>\t":i-=1
 while j+1<8 and text[j+1] not in " ,`|:\"'()[]{}<>\t":j+=1
 return escape(text[i:j+1])
cases=[]
texts=[''.join(c) for n in range(6) for c in itertools.product('a, ',repeat=n)]+['a+b','a.b','a\\b','(ab)','ab-c']
for text in texts:
 for col in range(8):cases.append('{'+json.dumps(text)+','+str(col)+','+json.dumps(word(text,col))+'}')
main='''
struct test{const char *text;unsigned column;const char *expected;};
static const struct test tests[]={'''+',\n'.join(cases)+'''};
static enum con_seed_result finish(struct con_search_seed *s,unsigned *ticks){
 enum con_seed_result result;*ticks=0;do{result=con_seed_step(s,32);assert(++*ticks<10000);}while(result==CON_SEED_MORE);return result;
}
int main(void){
 {
  ULONG text[]={'a','a','/','b','b',',','c','c'},delimiter[]={'/'};
  struct charmap_line row={.text=text,.size=8};struct con_nav nav;
  con_nav_begin(&nav,&row,0,3,8);assert(con_nav_set_delimiters(&nav,delimiter,1));
  struct con_search_seed seed;con_seed_begin(&seed,&nav,&row);
  delimiter[0]=',';nav.semantic_chars[0]=',';
  unsigned ticks;assert(finish(&seed,&ticks)==CON_SEED_DONE&&seed.length==5);
  const char*expected="bb,cc";for(unsigned i=0;i<5;i++)assert(seed.query[i]==(UBYTE)expected[i]);
  con_nav_begin(&nav,&row,0,0,8);assert(con_nav_set_delimiters(&nav,NULL,0));
  con_seed_begin(&seed,&nav,&row);assert(finish(&seed,&ticks)==CON_SEED_DONE&&seed.length==8);
 }
 for(unsigned i=0;i<sizeof(tests)/sizeof(*tests);i++){
  const struct test*t=&tests[i];ULONG text[16]={0};for(unsigned j=0;t->text[j];j++)text[j]=(UBYTE)t->text[j];
  struct charmap_line row={.text=text,.size=strlen(t->text)};struct con_nav nav;con_nav_begin(&nav,&row,0,t->column,8);
  struct con_search_seed seed;con_seed_begin(&seed,&nav,&row);struct con_search_seed before=seed;
  assert(con_seed_step(&seed,0)==CON_SEED_MORE&&!memcmp(&seed,&before,sizeof(seed)));
  unsigned ticks;enum con_seed_result result=finish(&seed,&ticks);
  assert(result==(*t->expected?CON_SEED_DONE:CON_SEED_EMPTY));assert(seed.length==strlen(t->expected));
  for(unsigned j=0;j<seed.length;j++)assert(seed.query[j]==(UBYTE)t->expected[j]);
 }
 {
  ULONG text[257];struct charmap_line row={.text=text};struct con_nav nav;unsigned ticks;
  for(unsigned escaped=0;escaped<2;escaped++)for(unsigned excess=0;excess<2;excess++){
   unsigned length=(escaped?128:256)+excess;for(unsigned i=0;i<length;i++)text[i]=escaped?'+':'a';row.size=length;
   con_nav_begin(&nav,&row,0,0,length);nav.selecting=TRUE;nav.anchor=nav.cursor;nav.cursor.column=length-1;
   struct con_search_seed seed;con_seed_begin(&seed,&nav,&row);
   assert(finish(&seed,&ticks)==(excess?CON_SEED_LIMIT:CON_SEED_DONE));assert(seed.length==256);
  }
 }
 {
  ULONG a[]={'a','b'},b[]={'c','d'};struct charmap_line rows[2]={{.text=a,.size=2},{.text=b,.size=2}};rows[0].next=rows+1;rows[1].prev=rows;
  for(unsigned wrap=0;wrap<2;wrap++)for(unsigned selection=0;selection<3;selection++){
   rows[0].wrapColumns=wrap?2:0;struct con_nav nav;con_nav_begin(&nav,rows,0,0,2);
   if(selection){nav.selecting=TRUE;nav.anchor=nav.cursor;nav.cursor=(struct con_nav_cursor){rows+1,1,1};nav.linewise=selection==2;}
   struct con_search_seed seed;con_seed_begin(&seed,&nav,rows);unsigned ticks;assert(finish(&seed,&ticks)==CON_SEED_DONE);
   const char*expected=selection?(wrap?(selection==2?"abcd\\n":"abcd"):(selection==2?"ab\\ncd\\n":"ab\\ncd")):(wrap?"abcd":"ab");
   assert(seed.length==strlen(expected));for(unsigned j=0;j<seed.length;j++)assert(seed.query[j]==(UBYTE)expected[j]);
  }
 }
 {
  ULONG a[]={'A',0x4e2d,0xfffffffe,'B'},b[]={'x','y','z','w'};
  struct charmap_line rows[2]={{.text=a,.size=4},{.text=b,.size=4}};rows[0].next=rows+1;rows[1].prev=rows;
  struct con_nav nav;con_nav_begin(&nav,rows,0,1,4);struct con_search_seed seed;unsigned ticks;
  con_seed_begin(&seed,&nav,rows);assert(finish(&seed,&ticks)==CON_SEED_DONE&&seed.length==3&&seed.query[1]==0x4e2d);
  nav.selecting=nav.blockwise=TRUE;nav.anchor=nav.cursor;nav.cursor=(struct con_nav_cursor){rows+1,1,3};
  con_seed_begin(&seed,&nav,rows);assert(finish(&seed,&ticks)==CON_SEED_DONE);
  ULONG expected[]={0x4e2d,'B','\\n','y','z','w'};assert(seed.length==6&&!memcmp(seed.query,expected,sizeof(expected)));
 }
 {
  struct charmap_line *rows=calloc(10000,sizeof(*rows));assert(rows);for(unsigned i=0;i<10000;i++){rows[i].next=i<9999?rows+i+1:NULL;rows[i].prev=i?rows+i-1:NULL;}
  struct con_nav nav;con_nav_begin(&nav,rows,0,0,1);struct con_search_seed seed;con_seed_begin(&seed,&nav,rows);
  assert(con_seed_step(&seed,32)==CON_SEED_MORE);unsigned ticks;assert(finish(&seed,&ticks)==CON_SEED_EMPTY&&ticks>100);free(rows);
 }
 printf("Seed PASS: %zu semantic-word oracle cases, escapes, wide/soft/hard/line/block selections, limits and10000row bound\\n",sizeof(tests)/sizeof(*tests));
}
'''
base=prefix+line+source('search.h')+source('navigation.h')
header=source('search_seed.h')
for label,body in [('production',header),('escape',header.replace("if(escape)s->query[s->length++]='\\\\';","if(0&&escape)s->query[s->length++]='\\\\';")),('whole-word',header.replace('s->nav.match_phase=3;', 's->nav.match_phase=4;'))]:
 with tempfile.TemporaryDirectory() as d:
  p=Path(d)/'t.c';b=Path(d)/'t';p.write_text(base+body+main)
  subprocess.run(['cc','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p),'-o',str(b)],check=True)
  result=subprocess.run([str(b)],capture_output=True,text=True,timeout=60)
  if label=='production':print(result.stdout,end='');assert result.returncode==0,result.stderr
  else:assert body!=header and result.returncode!=0;print('Negative control PASS:',label)
