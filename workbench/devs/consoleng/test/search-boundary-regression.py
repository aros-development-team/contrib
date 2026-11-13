#!/usr/bin/env python3
"""Unicode word-boundary matches against a retained-text-independent oracle."""
from pathlib import Path
import ast,re,subprocess,tempfile
r=Path(__file__).resolve().parents[1]
def source(n):return '\n'.join(x for x in (r/n).read_text().splitlines() if not x.startswith('#include'))
word=bytearray(0x110000)
for line in (r/'unicode/regex-syntax-0.8.6/perl_word.rs').read_text().splitlines():
 if line.strip().startswith("('"):
  a,b=map(ord,ast.literal_eval(re.sub(r'\\u\{([0-9a-fA-F]+)\}',lambda m:'\\U%08x'%int(m[1],16),line.strip().rstrip(','))))
  word[a:b+1]=bytes([1])*(b-a+1)
line=(r/'charmap.h').read_text();line=line[line.index('struct charmap_line\n'):line.index('VOID charmap_dispose_lines')]
prefix=r'''
#include <stdint.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
typedef uint32_t ULONG;typedef uint16_t UWORD;typedef uint8_t UBYTE;typedef int8_t BYTE;typedef uint64_t UQUAD;typedef int BOOL;typedef void VOID;
#define TRUE 1
#define FALSE 0
#define CHARMAP_ERASED 0x21
'''
main=r'''
static struct con_regex state;static UBYTE word[0x110000];static unsigned scans;
static void verify(ULONG *text,unsigned length,BOOL negative,unsigned budget,BOOL repeated){
 ULONG cells[10];for(unsigned i=0;i<length;i++){cells[i*2]=text[i];cells[i*2+1]=0xfffffffe;}
 struct charmap_line row={0};row.text=cells;row.size=length*2;
 ULONG simple[]={'\\',negative?'B':'b'},repeat[]={'(','\\',negative?'B':'b',')','+'};
 assert(con_regex_begin(&state,&row,repeated?repeat:simple,repeated?5:2,0));
 unsigned expected[6],count=0;
 for(unsigned i=0;i<=length;i++)if(((i&&word[text[i-1]])!=(i<length&&word[text[i]]))!=negative)expected[count++]=i*2;
 unsigned got=0,ticks=0;struct con_search_match m;enum con_search_result status;
 do{m=(struct con_search_match){{&row,999},{&row,999}};status=con_regex_step(&state,budget,&m);assert(++ticks<1000);
  if(status==CON_SEARCH_FOUND){assert(got<count);assert(m.first.line==&row&&m.end.line==&row);assert(m.first.column==expected[got]&&m.end.column==expected[got]);got++;}
  else assert(m.first.column==999&&m.end.column==999);
 }while(status!=CON_SEARCH_DONE);
 assert(got==count);scans++;
}
int main(int argc,char**argv){
 assert(argc==2);FILE*f=fopen(argv[1],"rb");assert(f&&fread(word,1,sizeof(word),f)==sizeof(word));fclose(f);
 const ULONG alphabet[]={'a',' ',0x3a9,0x301,0x200c,0x661,0x4e2d,0x1f431,'\n'};
 for(unsigned len=0,total=1;len<=4;len++,total*=9)for(unsigned seed=0;seed<total;seed++){
  ULONG text[5];unsigned n=seed;for(unsigned i=0;i<len;i++){text[i]=alphabet[n%9];n/=9;}
  for(unsigned neg=0;neg<2;neg++)for(unsigned repeat=0;repeat<2;repeat++)verify(text,len,neg,repeat?17:1,repeat);
 }
 /* A soft-wrap continuation stays within a word; a hard break ends it. */
 ULONG a[]={0x3a9,0xfffffffe},b[]={0x301,0xfffffffe};struct charmap_line rows[2]={{0},{0}};
 rows[0].text=a;rows[0].size=2;rows[0].next=&rows[1];rows[1].prev=&rows[0];rows[1].text=b;rows[1].size=2;
 for(unsigned soft=0;soft<2;soft++){
  rows[0].wrapColumns=soft?2:0;ULONG q[]={'\\','b','\\','w','\\','B','\\','w','\\','b'};
  assert(con_regex_begin(&state,rows,q,10,0));struct con_search_match m;enum con_search_result status;unsigned ticks=0;
  do{status=con_regex_step(&state,1,&m);assert(++ticks<1000);}while(status==CON_SEARCH_MORE);
  assert((status==CON_SEARCH_FOUND)==soft);if(soft)assert(m.first.line==rows&&m.first.column==0&&m.end.line==rows+1&&m.end.column==2);
 }
 printf("Word boundary PASS: %u oracle scans; Unicode marks/joiners/digits/CJK/emoji, zero-width progress, repeated assertions, wide endpoints, hard/soft breaks\n",scans);
}
'''
header=prefix+line+source('search.h')+source('search_regex.h')+source('search.c')+source('search_unicode.h')+source('search_case.h')
engine=source('search_regex.c')
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'t.c';b=Path(d)/'t';data=Path(d)/'words';data.write_bytes(word)
 p.write_text(header+engine+main);subprocess.run(['cc','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p),'-o',str(b)],check=True);subprocess.run([str(b),str(data)],check=True)
 for old,new,label in [('rx_mode_class(r->previous, 4, n->flags & RX_ASCII)','FALSE','left neighbor removed'),('!r->eof && rx_mode_class(r->token.cp, 4, n->flags & RX_ASCII)','rx_mode_class(r->token.cp, 4, n->flags & RX_ASCII)','EOF guard removed')]:
  broken=engine.replace(old,new);assert broken!=engine;p.write_text(header+broken+main);subprocess.run(['cc','-O2',str(p),'-o',str(b)],check=True)
  assert subprocess.run([str(b),str(data)],capture_output=True).returncode!=0,label
  print('Negative control PASS:',label)
