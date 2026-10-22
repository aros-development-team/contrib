#!/usr/bin/env python3
"""Exhaustive Unicode shorthand classification against pinned original data."""
from pathlib import Path
import ast,re,subprocess,tempfile
r=Path(__file__).resolve().parents[1]
subprocess.run(['python3',str(r/'tools/generate-search-unicode.py'),'--check'],check=True)
oracle=bytearray(0x110000);points={0,9,10,13,32,65,95,0x85,0x301,0x3a9,0x661,0x966,0x2003,0x200c,0x4e2d,0x1f600,0x10ffff}
for bit,name in [(1,'perl_decimal.rs'),(2,'perl_space.rs'),(4,'perl_word.rs')]:
 for line in (r/'unicode/regex-syntax-0.8.6'/name).read_text().splitlines():
  if not line.strip().startswith("('"):continue
  literal=re.sub(r'\\u\{([0-9a-fA-F]+)\}',lambda m:'\\U%08x'%int(m[1],16),line.strip().rstrip(','))
  low,high=map(ord,ast.literal_eval(literal))
  for cp in range(low,high+1):oracle[cp]|=bit
  points.update(x for x in [low-1,low,high,high+1] if 0<=x<=0x10ffff)
def source(name):return '\n'.join(x for x in (r/name).read_text().splitlines() if not x.startswith('#include'))
line=(r/'charmap.h').read_text();line=line[line.index('struct charmap_line\n'):line.index('VOID charmap_dispose_lines')]
prefix=r'''
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
main='static const ULONG points[]={'+','.join(map(str,sorted(points)))+'};\n'+r'''
static struct con_regex state;
static UBYTE expected[0x110000];
static void verify(ULONG cp,const char *pattern,BOOL wanted){
 ULONG q[64],cells[]={cp,0xfffffffe};unsigned n=strlen(pattern);for(unsigned i=0;i<n;i++)q[i]=(UBYTE)pattern[i];
 struct charmap_line row={0};row.text=cells;row.size=2;
 assert(con_regex_begin(&state,&row,q,n,0));
 struct con_search_match match={{&row,999},{&row,999}};enum con_search_result result;unsigned ticks=0;
 do{result=con_regex_step(&state,1,&match);assert(++ticks<1000);}while(result==CON_SEARCH_MORE);
 if((result==CON_SEARCH_FOUND)!=wanted){fprintf(stderr,"Mismatch U+%04x %s expected=%d result=%d\n",cp,pattern,wanted,result);abort();}
 if(wanted)assert(match.first.column==0&&match.end.column==2);else assert(match.first.column==999&&match.end.column==999);
}
int main(int argc,char **argv){
 assert(argc==2);FILE*f=fopen(argv[1],"rb");assert(f&&fread(expected,1,sizeof(expected),f)==sizeof(expected));fclose(f);
 unsigned classifications=0,scans=0;
 for(ULONG cp=0;cp<0x110000;cp++)for(unsigned i=0;i<6;i++){
  BOOL valid=cp<0xd800||cp>0xdfff;BOOL member=!!(expected[cp]&(1U<<(i%3)));
  assert(rx_unicode_class(cp,1U<<i)==(valid&&(i<3?member:!member)));classifications++;
 }
 assert(!rx_unicode_class(0x110000,63)&&!rx_unicode_class(0xfffffffe,63));
 for(unsigned p=0;p<sizeof(points)/sizeof(*points);p++){
  ULONG cp=points[p];if(!cp||(cp>=0xd800&&cp<=0xdfff))continue;
  const char *patterns[]={"\\d","\\s","\\w","\\D","\\S","\\W","[\\d]","[\\s]","[\\w]","[\\D]","[\\S]","[\\W]"};
  for(unsigned i=0;i<12;i++){unsigned k=i%6;BOOL member=!!(expected[cp]&(1U<<(k%3)));verify(cp,patterns[i],k<3?member:!member);scans++;}
  const char *properties[]={"\\p{Nd}","\\p{Decimal_Number}","\\p{gc=Nd}","\\p{General_Category=Decimal_Number}","\\p{White_Space}","\\p{space}","\\P{Nd}","\\P{White_Space}","[\\p{Nd}]","[\\P{White_Space}]"};
  const unsigned masks[]={1,1,1,1,2,2,8,16,1,16};
  for(unsigned i=0;i<10;i++){unsigned mask=masks[i];BOOL member=!!(expected[cp]&(mask<8?mask:mask>>3));verify(cp,properties[i],mask<8?member:!member);scans++;}
  verify(cp,"[\\d\\s]",!!(expected[cp]&3));verify(cp,"[\\w-]",!!(expected[cp]&4)||cp=='-');
  verify(cp,"[^\\w]",!(expected[cp]&4)&&cp!='\n');scans+=3;
 }
 const char *bad[]={"[\\d-a]","[a-\\w]","\\p{Not_A_Property}","\\q","\\p{}","\\p{Nd","(?-u:\\p{Nd})","[\\p{Nd}-a]","[a-\\p{Nd}]","\\p{gc=White_Space}"};
 for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);i++){ULONG q[64];unsigned n=strlen(bad[i]);for(unsigned j=0;j<n;j++)q[j]=bad[i][j];assert(!con_regex_begin(&state,NULL,q,n,0));}
 printf("Unicode regex PASS: %u classifications, %u Unicode-scalar engine scans, range endpoints, complements, unions and invalid syntax\n",classifications,scans);
}
'''
header=prefix+line+source('search.h')+source('search_regex.h')+source('search.c')
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';b=Path(d)/'test';data=Path(d)/'oracle.bin';data.write_bytes(oracle)
 p.write_text(header+source('search_unicode.h')+source('search_case.h')+source('search_regex.c')+main)
 subprocess.run(['cc','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p),'-o',str(b)],check=True)
 subprocess.run([str(b),str(data)],check=True)
 for old,new,label in [('cp > table[mid].high','cp >= table[mid].high','inclusive range endpoint'),('cp > 0x10ffff','cp > 255','non-Latin1 classification')]:
  broken=source('search_unicode.h').replace(old,new);assert broken!=source('search_unicode.h')
  p.write_text(header+broken+source('search_case.h')+source('search_regex.c')+main)
  subprocess.run(['cc','-O2','-Wall','-Wextra','-Werror',str(p),'-o',str(b)],check=True)
  assert subprocess.run([str(b),str(data)],capture_output=True).returncode!=0,label
  print('Negative control PASS:',label)
