#!/usr/bin/env python3
"""Actual line allocation/disposal: forward/back links survive history trimming."""
from pathlib import Path
from hyperlink_fixture import hyperlink_source
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'charmap.c').read_text()
f=s.split('struct charmap_line *charmap_dispose_line(',1)[1].split('VOID charmap_resize(',1)[0]
f='struct charmap_line *charmap_dispose_line('+f
prefix=r'''
#include <stdlib.h>
#include <assert.h>
#define CHARMAP_ROLES(o) 0x0102
typedef unsigned short UWORD;typedef void VOID;typedef unsigned long ULONG;typedef signed char BYTE;
struct charmap_line {struct con_link **links;struct charmap_line *next,*prev;ULONG size,capacity;ULONG *text;BYTE *fgpen,*bgpen,*flags;unsigned long long *roles; unsigned long wrapColumns; unsigned char wrapPadding;ULONG *underlineColors;};
#define MEMF_ANY 0
static int failalloc;
#define AllocMem(n,f) (failalloc?NULL:calloc(1,n))
#define FreeMem(p,n) free(p)
'''
main=r'''
int main(void) {
 struct charmap_line *head=0,*tail=0;
 for(int i=0;i<2048;i++) {
  tail=charmap_newline(0,tail);if(!head)head=tail;
  tail->capacity=1;tail->text=malloc(sizeof(ULONG));tail->text[0]=i;
  tail->fgpen=malloc(1);tail->bgpen=malloc(1);tail->flags=malloc(1);
  if(i>=16)head=charmap_dispose_line(head);
  assert(head->prev==0&&tail->next==0);
  struct charmap_line *p=tail;int expected=i;
  while(p){assert(p->text[0]==expected--);if(p->prev)assert(p->prev->next==p);p=p->prev;}
  assert(expected==(i<16?-1:i-16));
 }
 failalloc=1;assert(!charmap_newline(0,tail)&&!tail->next);assert(!charmap_newline(head,0)&&!head->prev);assert(!charmap_newline(head->next,head)&&head->next->prev==head);failalloc=0;
 struct charmap_line *a=head,*middle=a->next,*c=middle->next;
 assert(charmap_dispose_line(middle)==c&&a->next==c&&c->prev==a);
 struct charmap_line *previous=tail->prev;
 assert(charmap_dispose_line(tail)==0&&previous->next==0);
 charmap_dispose_lines(head);assert(charmap_dispose_line(0)==0);
 return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';b=Path(d)/'test';p.write_text(prefix+hyperlink_source()+f+main)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(b)],check=True)
 subprocess.run([str(b)],check=True)
print('Line lifecycle PASS: 2048 append/trim/backwalk cycles, middle/tail unlink, full disposal')
