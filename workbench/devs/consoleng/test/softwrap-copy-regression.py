#!/usr/bin/env python3
"""Actual selection serialization preserves soft wraps and explicit newlines."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'charmapconclass.c').read_text()
body=s[s.index('static BOOL charmap_search_space('):s.index('static VOID charmap_search_delete_word(')]+s[s.index('static VOID charmap_block_span('):s.index('/* Overlay only:')]+s[s.index('static ULONG enc_cells_len('):s.index('struct ConClipData')]
u=(r/'support.c').read_text();u=u[u.index('ULONG con_utf8_encode('):u.index('ULONG con_utf8_count(')]
prefix=r'''
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
typedef uint32_t ULONG;typedef unsigned char UBYTE;typedef int BOOL;typedef void VOID;
#define CHARMAP_ERASED 0x21
#define CELL_WIDE_CONT 0xfffffffeUL
#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))
#define MEMF_ANY 0
#define AllocMem(n,f) malloc(n)
struct charmap_line {struct charmap_line *next;ULONG size,*text,wrapColumns;UBYTE wrapPadding;UBYTE *flags;ULONG *underlineColors;};
static void check(struct charmap_line *a,struct charmap_line *b,ULONG x,ULONG y,int utf,const char *expected);
'''
main=r'''
static void check(struct charmap_line *a,struct charmap_line *b,ULONG x,ULONG y,int utf,const char *expected){
 ULONG n=charmapcon_calc_selection_size(a,b,x,y,utf,0,0);assert(n==strlen(expected));
 char *s=charmapcon_get_selection(n,a,b,x,y,utf,0,0);assert(s&&!memcmp(s,expected,n));free(s);
}
int main(void){
 ULONG exactText[]={'a','b'},otherText[]={'c','d'};
 struct charmap_line other={0,2,otherText,0,0},exact={&other,2,exactText,0,0};
 for(int multiple=0;multiple<2;multiple++)for(int end=0;end<=2;end++){
  struct charmap_line *last=multiple?&other:&exact;
  const char *expected=multiple?(end==0?"ab\r":end==1?"ab\rc":"ab\rcd"):(end==0?"":end==1?"a":"ab");
  ULONG n=charmapcon_calc_selection_size(&exact,last,0,end,1,1,0);assert(n==strlen(expected));
  char *copy=charmapcon_get_selection(n,&exact,last,0,end,1,1,0);assert(copy&&!memcmp(copy,expected,n));free(copy);
 }

 ULONG t0[]={'A','B','C',' '},t1[]={0x2615,CELL_WIDE_CONT,'D'},t2[]={'X','Y'};
 struct charmap_line c={0,2,t2,0,0},b={&c,3,t1,0,0},a={&b,4,t0,4,1};
 check(&a,&c,0,2,1,"ABC\xe2\x98\x95" "D\rXY\r");
 check(&a,&b,0,2,1,"ABC\xe2\x98\x95");
 check(&a,&a,0,4,1,"ABC");check(&a,&a,4,0,1,"ABC");
 check(&a,&b,3,1,1,"\xe2\x98\x95");
 check(&a,&c,999,999,1,"\xe2\x98\x95" "D\rXY\r");
 a.wrapPadding=0;check(&a,&b,0,3,1,"ABC \xe2\x98\x95" "D\r");
 a.wrapColumns=0;check(&a,&b,0,3,1,"ABC \r\xe2\x98\x95" "D\r");
 check(&a,&a,4,0,1,"ABC \r");
 check(&a,&b,0,3,0,"ABC \r?D\r");
 UBYTE erased[4]={0,0,0,CHARMAP_ERASED};a.flags=erased;
 check(&a,&a,0,4,1,"ABC\r");a.flags=NULL;
 a.size=0;a.text=NULL;check(&a,&c,0,2,1,"\r\xe2\x98\x95" "D\rXY\r");
 for(int n=1;n<64;n++){
  ULONG text[64];for(int i=0;i<n;i++)text[i]='a'+i%26;
  struct charmap_line tail={0,n,text,0,0},head={&tail,n,text,n,0};
  char expected[130];for(int i=0;i<2*n;i++)expected[i]='a'+(i%n)%26;expected[2*n]='\r';expected[2*n+1]=0;
  check(&head,&tail,0,n,1,expected);
 }
 return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';b=Path(d)/'test';p.write_text(prefix+u+body+main)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(b)],check=True)
 subprocess.run([str(b)],check=True)
print('Soft-wrap copy PASS: logical lines, hard CRs, wide padding/continuations, partial/reversed/clamped endpoints, Latin-1 and empty rows')
