#!/usr/bin/env python3
"""Production bounded Unicode NFA against independent POSIX ERE oracle."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
def source(name):
 return '\n'.join(x for x in (r/name).read_text().splitlines() if not x.startswith('#include'))
line=(r/'charmap.h').read_text();line=line[line.index('struct charmap_line\n'):line.index('VOID charmap_dispose_lines')]
prefix=r'''
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <regex.h>
typedef uint32_t ULONG;typedef uint16_t UWORD;typedef uint8_t UBYTE;typedef int8_t BYTE;typedef uint64_t UQUAD;typedef int BOOL;typedef void VOID;
#define TRUE 1
#define FALSE 0
#define CHARMAP_ERASED 0x21
'''
main=r'''
static uint32_t rng=13;
static unsigned rnd(void){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
static struct con_regex state;
static unsigned cases;
static void verify(const char *pattern,const char *text,int fold,unsigned budget){
 ULONG q[256],cells[128];unsigned len=strlen(pattern),size=strlen(text);
 for(unsigned i=0;i<len;i++)q[i]=(unsigned char)pattern[i];
 for(unsigned i=0;i<size;i++)cells[i]=(unsigned char)text[i];
 struct charmap_line row={0};row.text=cells;row.size=size;
 regex_t oracle;assert(!regcomp(&oracle,pattern,REG_EXTENDED|REG_NEWLINE|(fold?REG_ICASE:0)));
 regmatch_t expected;int found=!regexec(&oracle,text,1,&expected,0);
 assert(con_regex_begin(&state,&row,q,len,fold));
 struct con_regex saved=state;struct con_search_match result={{&row,999},{&row,999}},before=result;
 assert(con_regex_step(&state,0,&result)==CON_SEARCH_MORE&&!memcmp(&state,&saved,sizeof(state))&&!memcmp(&result,&before,sizeof(result)));
 enum con_search_result status;unsigned ticks=0;
 do {before=result;status=con_regex_step(&state,budget,&result);assert(++ticks<100000);
 if(status!=CON_SEARCH_FOUND)assert(!memcmp(&result,&before,sizeof(result)));}while(status==CON_SEARCH_MORE);
 if((status==CON_SEARCH_FOUND)!=found||(found&&(result.first.column!=(unsigned)expected.rm_so||result.end.column!=(unsigned)expected.rm_eo))){
 fprintf(stderr,"Mismatch pattern=%s text=%s fold=%d expected=%d %d:%d actual=%d %u:%u\n",pattern,text,fold,found,(int)expected.rm_so,(int)expected.rm_eo,status,result.first.column,result.end.column);abort();}
 if(found){unsigned offset=expected.rm_eo;
  if(expected.rm_so==expected.rm_eo){if(offset==size)offset=size+1;else offset++;}
  while(offset<=size){
   found=!regexec(&oracle,text+offset,1,&expected,offset&&text[offset-1]!='\n'?REG_NOTBOL:0);
   do{status=con_regex_step(&state,budget,&result);assert(++ticks<100000);}while(status==CON_SEARCH_MORE);
   assert((status==CON_SEARCH_FOUND)==found);
   if(!found)break;
   assert(result.first.column==offset+(unsigned)expected.rm_so&&result.end.column==offset+(unsigned)expected.rm_eo);
   offset+=expected.rm_eo;
   if(expected.rm_so==expected.rm_eo){if(offset==size)break;offset++;}
  }
 }
 regfree(&oracle);
 con_regex_cancel(&state);assert(con_regex_step(&state,1,&result)==CON_SEARCH_DONE);cases++;
}
int main(void){
 const char *fixed[]={"a","a*","a+","a?","a{0}","([ab][cd]){0}[ab]","a{1}","a{2}","a{0,3}","a{2,4}","a{0,}","a{2,}","(ab|a){2,3}","(a?){0,3}","(a{1,2}b){0,3}","a|ab","ab|a","^|a+b|$","(a|ab)*b","(a?)*","(a*)*","^a+$","a$","^","$","a.*b","[^a]+","[A-Z]+","[a-cB-D]*","(a|)b","\\.","[a-]+"};
 const char *texts[]={"","a","b","aba","aabb","bbab","abcD","baaa","aaaab","\nabc\na","..","---", "]]"};
 for(unsigned p=0;p<sizeof(fixed)/sizeof(*fixed);p++)for(unsigned t=0;t<sizeof(texts)/sizeof(*texts);t++)for(int fold=0;fold<2;fold++)verify(fixed[p],texts[t],fold,1);
 const char *atoms[]={"a","b","A",".","[a-c]","[^b]","(a|b)","(ab|a)","(a?)","^","$"};
 const char *quant[]={"","*","+","?","{0}","{2}","{0,2}","{2,4}","{1,}"};
 for(unsigned i=0;i<12000;i++){
 char pattern[256]="",text[80];unsigned count=1+rnd()%5;
 for(unsigned a=0;a<count;a++) {unsigned atom=rnd()%11;strcat(pattern,atoms[atom]);if(atom<9)strcat(pattern,quant[rnd()%9]);if(a+1<count&&rnd()%4==0)strcat(pattern,"|");}
 unsigned length=rnd()%40;for(unsigned j=0;j<length;j++)text[j]="abcAB\n"[rnd()%6];text[length]=0;
 verify(pattern,text,rnd()%2,1+rnd()%23);
 }
 /* Exact Unicode/wide endpoints and nonoverlapping/empty progress. */
 ULONG cells[]={0x4e2d,0xfffffffe,0x3a9,'a','a'},q[]={0x4e2d,'.','a','+'};
 struct charmap_line row={0};row.text=cells;row.size=5;
 assert(con_regex_begin(&state,&row,q,4,0));struct con_search_match m;enum con_search_result status;
 do{status=con_regex_step(&state,1,&m);}while(status==CON_SEARCH_MORE);
 assert(status==CON_SEARCH_FOUND&&m.first.column==0&&m.end.column==5);
 ULONG empty[]={'b','*'};assert(con_regex_begin(&state,&row,empty,2,0));unsigned starts[]={0,2,3,4,5};
 for(unsigned i=0;i<5;i++){unsigned ticks=0;do{status=con_regex_step(&state,1,&m);assert(++ticks<1000);}while(status==CON_SEARCH_MORE);assert(status==CON_SEARCH_FOUND&&m.first.column==starts[i]&&m.end.column==starts[i]);}
 assert(con_regex_step(&state,100,&m)==CON_SEARCH_DONE);
 const char *bad[]={"(?","(?:","(?i","(?-i","(?i:","(?-i:","(?UU:a)","(?U-U:a)","(?U-:a)","(?-:a)","(?--U:a)","(?U)*","(?U){2}","(?ii:a)","(?i-i:a)","(?mm:a)","(?ss:a)","(?uu:a)","(?xx:a)","(?i)*","(?-i)+","(?i){2}","(?=a)","(?!a)","[","[z-a]","(","a)","a**","a{2,1}","a{", "a{,2}", "a{257}", "a{9999999999999999999}","a{2}{2}","a{256}b{256}c","(a{128}){128}","[abc]{256}","((a{256}){0}){256}","\\1","\\p","a+??", "a{2}??", "a??+", "a{0,2}?*","[]","\\"};
 for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);i++){ULONG query[256];unsigned n=strlen(bad[i]);for(unsigned j=0;j<n;j++)query[j]=bad[i][j];assert(!con_regex_begin(&state,&row,query,n,0));assert(!state.valid);}
 /* Deep nesting rejects cleanly; maximal literal and ambiguous repetition stay bounded. */
 ULONG nested[80];for(unsigned i=0;i<40;i++)nested[i]='(';for(unsigned i=40;i<80;i++)nested[i]=')';
 assert(!con_regex_begin(&state,&row,nested,80,0));
 ULONG dense[4096],pattern[]={'(','a','|','a','a',')','*','b'};for(unsigned i=0;i<4096;i++)dense[i]='a';
 row.text=dense;row.size=4096;assert(con_regex_begin(&state,&row,pattern,8,0));
 unsigned work=0;do{status=con_regex_step(&state,1,&m);assert(++work<250000);}while(status==CON_SEARCH_MORE);assert(status==CON_SEARCH_DONE);
 ULONG lazy_dense[]={'(','a','|','a','a',')','*','?','b'};
 assert(con_regex_ui_begin(&state,&row,lazy_dense,9,0,FALSE));
 work=0;do{status=con_regex_step(&state,1,&m);assert(++work<250000);}while(status==CON_SEARCH_MORE);assert(status==CON_SEARCH_DONE);
 ULONG maximal[256];for(unsigned i=0;i<256;i++)maximal[i]='a';
 assert(con_regex_begin(&state,&row,maximal,256,0));do{status=con_regex_step(&state,31,&m);}while(status==CON_SEARCH_MORE);
 assert(status==CON_SEARCH_FOUND&&m.first.column==0&&m.end.column==256);
 printf("Regex PASS: %u POSIX-oracle cases, Unicode endpoints, bounded ticks, empty progress, cancellation, invalid syntax\n",cases);
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';b=Path(d)/'test';p.write_text(prefix+line+source('search.h')+source('search_regex.h')+source('search.c')+source('search_unicode.h')+source('search_case.h')+source('search_regex.c')+main)
 subprocess.run(['cc','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-g',str(p),'-o',str(b)],check=True)
 subprocess.run([str(b)],check=True)

production=source('search_regex.c')
for old,new,label in [
 ("++p->work > CON_REGEX_COMPILE_WORK", "FALSE", "compilation work budget"),
 ("memset(&p->r->nodes[n], 0, sizeof(p->r->nodes[n]));", "/* stale discarded class metadata */", "zero-repeat node reuse"),
 ("r->phase = RX_SKIP; if (r->best_eof)","r->phase = RX_FETCH; if (r->best_eof)","empty-match progress"),
 ("(!r->ordinal || r->previous == '\\n')", "TRUE", "line anchor"),
 ("return rx_mode_fold(cp, n->flags & RX_FOLD, !(n->flags & RX_ASCII)) == n->cp;", "return cp == n->cp;", "case folding"),
]:
 broken=production.replace(old,new);assert broken!=production,label
 with tempfile.TemporaryDirectory() as d:
  p=Path(d)/'test.c';b=Path(d)/'test';p.write_text(prefix+line+source('search.h')+source('search_regex.h')+source('search.c')+source('search_unicode.h')+source('search_case.h')+broken+main)
  subprocess.run(['cc','-Wall','-Wextra','-Werror',str(p),'-o',str(b)],check=True)
  assert subprocess.run([str(b)],capture_output=True,timeout=30).returncode!=0,label
  print('Negative control PASS:',label)
