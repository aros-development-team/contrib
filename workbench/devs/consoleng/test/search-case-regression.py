#!/usr/bin/env python3
"""Unicode case equivalence and smartcase against independent pinned-data oracle."""
from pathlib import Path
import ast,re,subprocess,tempfile,struct
r=Path(__file__).resolve().parents[1]
subprocess.run(['python3',str(r/'tools/generate-search-case.py'),'--check'],check=True)
v=r/'unicode/regex-syntax-0.8.6';fold={}
def chars(line):
 return [ord(ast.literal_eval(re.sub(r'\\u\{([0-9a-fA-F]+)\}',lambda m:'\\U%08x'%int(m[1],16),x))) for x in re.findall(r"'(?:\\.|[^'])*'",line)]
for line in (v/'case_folding_simple.rs').read_text().splitlines():
 if line.strip().startswith("('"):
  a,*b=chars(line);fold[a]=b
upper=bytearray(0x110000)
s=(v/'property_bool.rs').read_text().split('pub const UPPERCASE:',1)[1].split('];',1)[0]
for line in s.splitlines():
 if line.strip().startswith("('"):
  a,b=chars(line);upper[a:b+1]=bytes([1])*(b-a+1)
def source(n):return '\n'.join(x for x in (r/n).read_text().splitlines() if not x.startswith('#include'))
line=(r/'charmap.h').read_text();line=line[line.index('struct charmap_line\n'):line.index('VOID charmap_dispose_lines')]
prefix='''#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef uint32_t ULONG;typedef uint16_t UWORD;typedef uint8_t UBYTE;typedef int8_t BYTE;typedef uint64_t UQUAD;typedef int BOOL;typedef void VOID;
#define TRUE 1
#define FALSE 0
#define CHARMAP_ERASED 0x21
'''
pairs=[(cp,x) for cp,other in fold.items() for x in other]
main='static const ULONG pairs[][2]={'+','.join('{%d,%d}'%x for x in pairs)+'};\n'+r'''
static struct con_regex state;
static void scan(const ULONG*q,unsigned n,ULONG cp,BOOL folding,BOOL wanted){
 ULONG cells[]={cp,0xfffffffe};struct charmap_line row={0};row.text=cells;row.size=2;
 assert(con_regex_begin(&state,&row,q,n,folding));struct con_search_match m={{&row,999},{&row,999}};
 unsigned ticks=0;enum con_search_result result;
 do{result=con_regex_step(&state,1,&m);assert(++ticks<1000);}while(result==CON_SEARCH_MORE);
 assert((result==CON_SEARCH_FOUND)==wanted);if(wanted)assert(m.first.column==0&&m.end.column==2);else assert(m.first.column==999);
}
int main(int argc,char**argv){
 assert(argc==3);FILE*f=fopen(argv[1],"rb");assert(f);
 for(ULONG cp=0;cp<0x110000;cp++){ULONG expected;assert(fread(&expected,4,1,f)==1);assert(con_case_fold(cp)==expected);}fclose(f);
 f=fopen(argv[2],"rb");assert(f);
 for(ULONG cp=0;cp<0x110000;cp++){int expected=fgetc(f);assert(expected>=0);assert(con_case_upper(cp)==expected);assert(con_regex_smart_fold(&cp,1)==!expected);}fclose(f);
 for(unsigned i=0;i<sizeof(pairs)/sizeof(*pairs);i++){
  ULONG a=pairs[i][0],b=pairs[i][1];
  scan(&a,1,b,1,1);scan(&a,1,b,0,0);
  ULONG cl[]={'[',a,']'},neg[]={'[','^',a,']'},range[]={'[',a,'-',a,']'};
  scan(cl,3,b,1,1);scan(neg,4,b,1,0);scan(range,5,b,1,1);
  scan(cl,3,b,0,0);scan(neg,4,b,0,1);
 }
 ULONG ascii[]={'[','a','-','z',']'};scan(ascii,5,0x212a,1,1);scan(ascii,5,0x17f,1,1);scan(ascii,5,0x130,1,0);
 ULONG lower[]={0x3c3},upperq[]={0x3a3},escaped[]={'\\','D'};
 assert(con_regex_smart_fold(lower,1));assert(!con_regex_smart_fold(upperq,1));assert(!con_regex_smart_fold(escaped,2));
 scan(lower,1,0x3c2,con_regex_smart_fold(lower,1),1);scan(upperq,1,0x3c3,con_regex_smart_fold(upperq,1),0);
 puts("Unicode case PASS: all 1114112 codepoints fold/Uppercase/smartcase; all equivalence pairs literals/classes/negation/ranges under one-unit budgets; Kelvin/long-s/Turkic exclusions");
}
'''
header=prefix+line+source('search.h')+source('search_regex.h')+source('search.c')+source('search_unicode.h')
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';b=Path(d)/'test';f=Path(d)/'fold.bin';u=Path(d)/'upper.bin'
 f.write_bytes(b''.join(struct.pack('=I',min([cp]+fold.get(cp,[]))) for cp in range(0x110000)));u.write_bytes(upper)
 case=source('search_case.h');engine=source('search_regex.c')
 p.write_text(header+case+engine+main)
 subprocess.run(['cc','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p),'-o',str(b)],check=True);subprocess.run([str(b),str(f),str(u)],check=True)
 for old,new,label in [('entry?entry->canonical:cp','cp','Unicode folding removed'),('if (equiv) for','if (0 && equiv) for','range equivalence removed')]:
  broken=(case+engine).replace(old,new);assert broken!=case+engine
  # The disabled lookup is intentionally unused in the first negative control.
  p.write_text(header+broken+main);subprocess.run(['cc','-O2',str(p),'-o',str(b)],check=True)
  assert subprocess.run([str(b),str(f),str(u)],capture_output=True).returncode!=0,label
  print('Negative control PASS:',label)
