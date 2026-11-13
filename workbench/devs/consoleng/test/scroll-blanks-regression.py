#!/usr/bin/env python3
"""Scrolling blanks preserve history, future tail and wrap links atomically."""
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
int main(void){int cases=0;
 for(int n=1;n<=12;n++)for(int f=0;f<=n*6;f++){
 fail=0;struct charmap_line*head=charmap_newline(0,0);struct charmap_line*last=charmap_newline(0,head);struct charmap_line*future=charmap_newline(0,last);
 last->wrapColumns=5;last->wrapPadding=1;
 int before=live;attempt=0;fail=f;
 int ok=charmap_append_blanks(0,last,n,5,3,4,0x708);
 assert(ok==!f);assert(last->wrapColumns==5&&last->wrapPadding==1);
 if(f){assert(live==before&&last->next==future&&future->prev==last);}
 else {struct charmap_line*p=last;for(int y=0;y<n;y++){
  assert(p->next&&p->next->prev==p);p=p->next;assert(p->size==5&&!p->wrapColumns&&!p->wrapPadding);
  for(int x=0;x<5;x++)assert(p->text[x]==' '&&p->fgpen[x]==3&&p->bgpen[x]==4&&p->roles[x]==0x708&&p->flags[x]==CHARMAP_ERASED);
  assert(charmap_copy_columns(p)==0);p->flags[4]=0;assert(charmap_copy_columns(p)==5);p->flags[4]=CHARMAP_ERASED;p->text[0]='X';p->flags[0]=0;assert(charmap_copy_columns(p)==1);
 }assert(p->next==future&&future->prev==p);}
 fail=0;charmap_dispose_lines(head);assert(!live);cases++;
 }
 printf("Append blanks PASS: %d success/allocation-failure cases, links/colors/wrap/tail preserved\n",cases);
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'probe.c';exe=Path(d)/'probe';p.write_text(prefix+hyperlink_source()+line+a+columns+main)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
