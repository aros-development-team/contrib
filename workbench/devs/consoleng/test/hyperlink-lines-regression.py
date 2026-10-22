#!/usr/bin/env python3
"""Actual retained hyperlink lifecycle/reflow, including each allocation failure."""
from pathlib import Path
from hyperlink_fixture import hyperlink_source
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
h=(r/'charmap.h').read_text();line=h[h.index('struct charmap_line\n'):h.index('VOID charmap_dispose_lines')]
anchors=h[h.index('#define CHARMAP_REFLOW_ANCHORS'):h.index('BOOL charmap_reflow(')]
a=(r/'charmap.c').read_text();a=a[a.index('struct charmap_line *charmap_dispose_line('):]
b=(r/'charmap_reflow.c').read_text();b=b[b.index('struct reflow_builder'):]
prefix=r'''
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
typedef uint64_t UQUAD;typedef uint32_t ULONG;typedef uint16_t UWORD;typedef uint8_t UBYTE;typedef int8_t BYTE;typedef int BOOL;typedef void VOID;
#define TRUE 1
#define FALSE 0
#define MEMF_ANY 0
#define CHARMAP_ERASED 0x21
#define CELL_WIDE_CONT 0xfffffffeUL
#define SetMem(p,v,n) memset(p,v,n)
struct ConsoleBase {int unused;};
static int attempt,fail,live;
static void *allocate(size_t n){if(++attempt==fail)return NULL;void*p=malloc(n);if(p)live++;return p;}
static void release(void*p){if(p)live--;free(p);}
#define AllocMem(n,f) allocate(n)
#define FreeMem(p,n) ((void)(n),release(p))
static int con_cp_width(ULONG cp){return cp==0x2615?2:1;}
'''
main=r'''
static struct charmap_line *fixture(struct con_link *link){
 struct charmap_line *p=charmap_newline(NULL,NULL);assert(p);charmap_resize(NULL,p,5);assert(p->size==5&&!p->links);
 ULONG text[]={'A',0x2615,CELL_WIDE_CONT,'B',' '};memcpy(p->text,text,sizeof(text));
 for(unsigned i=0;i<5;i++)p->roles[i]=0x0102;
 assert(charmap_links(p,0,5,link));return p;
}
static void empty(struct con_link_pool*p){assert(!p->first&&!p->records&&!p->bytes&&!live);}
int main(void){
 struct con_link_pool pool={0};
 for(unsigned width=1;width<=16;width++) {
  fail=0;struct con_link *link=con_link_create(&pool,(UBYTE*)"https://a/",10,NULL,0);assert(link);
  struct charmap_line *old=fixture(link),*out=NULL;ULONG rows=0;int original=live;
  attempt=0;assert(charmap_reflow(NULL,old,width,NULL,0,&out,&rows));int total=attempt;
  unsigned cells=0,characters=0;ULONG text[]={'A',0x2615,'B',' '};
  for(struct charmap_line*p=out;p;p=p->next)for(unsigned i=0;i<p->size;i++){
   if(p->wrapPadding&&i==p->size-1){assert(!p->links||!p->links[i]);continue;}
   assert(p->links&&p->links[i]==link);cells++;
   if(p->text[i]!=CELL_WIDE_CONT)assert(characters<4&&p->text[i]==text[characters++]);
  }
  assert(characters==4&&link->references==6+cells);
  charmap_dispose_lines(out);assert(live==original&&link->references==6);
  for(int f=1;f<=total;f++){
   attempt=0;fail=f;out=(void*)1;rows=123;
   assert(!charmap_reflow(NULL,old,width,NULL,0,&out,&rows));
   assert(out==(void*)1&&rows==123&&live==original&&link->references==6);
   assert(old->size==5&&old->links[4]==link&&old->text[4]==' ');
  }
  fail=0;
  /* Array growth is transactional, including its metadata allocation. */
  for(int f=1;f<=6;f++){
   attempt=0;fail=f;struct con_link **before=old->links;charmap_resize(NULL,old,100);
   assert(old->size==5&&old->links==before&&link->references==6&&live==original);
  }
  fail=0;charmap_resize(NULL,old,100);assert(old->size==100&&link->references==6);
  for(unsigned i=5;i<100;i++)assert(!old->links[i]);
  charmap_resize(NULL,old,2);assert(link->references==3);
  charmap_resize(NULL,old,5);for(unsigned i=2;i<5;i++)assert(!old->links[i]);
  charmap_resize(NULL,old,0);assert(!old->links&&link->references==1);
  charmap_dispose_lines(old);con_link_release(link);empty(&pool);
 }

 /* Row insertion/deletion transfers cells and reclaims discarded links. */
 for(int insert=0;insert<2;insert++)for(unsigned count=1;count<=3;count++){
  struct con_link *item=con_link_create(&pool,(UBYTE*)"https://a/",10,NULL,0);assert(item);
  struct charmap_line *rows[3];
  for(int i=0;i<3;i++){rows[i]=fixture(item);if(i){rows[i-1]->next=rows[i];rows[i]->prev=rows[i-1];}}
  assert(item->references==16);
  assert(charmap_edit_lines(NULL,rows[0],rows[2],count,insert,5,1,2,0x0102));
  assert(item->references==1+(3-count)*5);
  for(unsigned i=0;i<3;i++){
   int blank=insert?i<count:i>=3-count;
   for(unsigned x=0;x<5;x++)assert(blank?(!rows[i]->links||!rows[i]->links[x]):rows[i]->links[x]==item);
  }
  assert(charmap_append_blanks(NULL,rows[2],2,5,1,2,0x0102));
  assert(!rows[2]->next->links&&!rows[2]->next->next->links);
  charmap_dispose_lines(rows[0]);assert(item->references==1);con_link_release(item);empty(&pool);
 }
 /* First metadata allocation fails atomically, invalid spans do nothing. */
 struct con_link *link=con_link_create(&pool,(UBYTE*)"https://a/",10,NULL,0);assert(link);
 struct charmap_line *p=charmap_newline(NULL,NULL);charmap_resize(NULL,p,5);
 attempt=0;fail=1;assert(!charmap_links(p,0,5,link)&&!p->links&&link->references==1);fail=0;
 assert(!charmap_links(p,4,2,link)&&!charmap_links(p,6,0,link));
 assert(charmap_links(p,0,5,NULL)&&!p->links);assert(charmap_links(p,0,5,link));
 con_link_release(link);charmap_dispose_lines(p);empty(&pool);
 puts("hyperlink lines PASS: widths1..16, wide cells/padding/linked blanks, every reflow allocation failure, resize ownership and disposal");return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';exe=Path(d)/'test';p.write_text(prefix+hyperlink_source()+line+anchors+a+b+main)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
