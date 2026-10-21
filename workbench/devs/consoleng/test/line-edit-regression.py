#!/usr/bin/env python3
"""Actual retained row ownership edits: links, colors, bounds and allocation rollback."""
from pathlib import Path
from hyperlink_fixture import hyperlink_source
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
h=(r/'charmap.h').read_text();line=h[h.index('struct charmap_line\n'):h.index('VOID charmap_dispose_lines')]
a=(r/'charmap.c').read_text();a=a[a.index('struct charmap_line *charmap_dispose_line('):]
a=next(x for x in h.splitlines() if x.startswith('#define CHARMAP_ERASED'))+'\n'+a
s=(r/'charmapconclass.c').read_text();columns=s[s.index('static ULONG charmap_copy_columns('):s.index('static VOID charmap_selection_span(')]
prefix=r'''
#include <assert.h>
#include <stdint.h>
typedef uint64_t UQUAD;
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
typedef uint32_t ULONG;typedef uint16_t UWORD;typedef uint8_t UBYTE;typedef int8_t BYTE;typedef int BOOL;typedef void VOID;
#define TRUE 1
#define FALSE 0
#define MEMF_ANY 0
#define SetMem(p,v,n) memset(p,v,n)
struct ConsoleBase {int unused;};
static int attempt,fail,live;
static void *allocate(size_t n){if(++attempt==fail)return NULL;void*p=malloc(n);if(p)live++;return p;}
static void release(void*p){if(p)live--;free(p);}
#define AllocMem(n,f) allocate(n)
#define FreeMem(p,n) ((void)(n),release(p))
'''
main=r'''
static struct charmap_line* build(struct charmap_line**rows,int h){struct charmap_line*prev=0;
 for(int y=0;y<h;y++){rows[y]=charmap_newline(0,prev);assert(rows[y]);charmap_resize(0,rows[y],5);assert(rows[y]->size==5);prev=rows[y];
  for(int x=0;x<5;x++){prev->text[x]=100+y*5+x;prev->fgpen[x]=y;prev->bgpen[x]=y+1;prev->roles[x]=0x102+y;prev->flags[x]=2;}
  prev->wrapColumns=5;
 }return rows[0];}
int main(void){int cases=0;
 for(int insert=0;insert<2;insert++)for(int h=1;h<=12;h++)for(int y=0;y<h;y++)for(int n=1;n<=h-y;n++){
 struct charmap_line*rows[14];fail=0;struct charmap_line*head=build(rows,h+2); /* sentinel before and after */
 assert(charmap_edit_lines(0,rows[y+1],rows[h],n,insert,5,3,4,0x708));
 for(int j=0;j<h+2;j++){
  assert(rows[j]->prev==(j?rows[j-1]:0)&&rows[j]->next==(j<h+1?rows[j+1]:0));
  int src=j;
  if(j>=y+1&&j<=h){src=insert?j-n:j+n;if((insert&&src<y+1)||(!insert&&src>h))src=-1;}
  int boundary=insert?h:h-n;
  int wrapped=src>=0&&j!=y&&j!=boundary;
  assert(rows[j]->wrapColumns==(ULONG)(wrapped?5:0));
  if(src<0)assert(charmap_copy_columns(rows[j])==0);
  for(int x=0;x<5;x++){
   assert(rows[j]->text[x]==(ULONG)(src<0?' ':100+src*5+x));
   assert(rows[j]->fgpen[x]==(src<0?3:src));assert(rows[j]->bgpen[x]==(src<0?4:src+1));
   assert(rows[j]->roles[x]==(src<0?0x708:0x102+src));assert(rows[j]->flags[x]==(src<0?CHARMAP_ERASED:2));
  }
 }
 charmap_dispose_lines(head);assert(!live);cases++;
 }
 for(int insert=0;insert<2;insert++)for(int f=1;f<=18;f++){
 struct charmap_line*rows[6];fail=0;struct charmap_line*head=build(rows,6);int before=live;attempt=0;fail=f;
 assert(!charmap_edit_lines(0,rows[1],rows[4],3,insert,5,3,4,0x708));assert(live==before);
 for(int y=0;y<6;y++){assert(rows[y]->wrapColumns==5);for(int x=0;x<5;x++)assert(rows[y]->text[x]==(ULONG)(100+y*5+x));}
 fail=0;charmap_dispose_lines(head);assert(!live);
 }
 printf("Line edits PASS: %d range/direction/count cases; 36 allocation-failure rollbacks\n",cases);
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'probe.c';exe=Path(d)/'probe';p.write_text(prefix+hyperlink_source()+line+a+columns+main)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
