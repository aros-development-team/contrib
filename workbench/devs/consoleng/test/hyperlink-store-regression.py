#!/usr/bin/env python3
"""Actual private OSC8 store: ownership, overlap, quotas and rollback."""
from pathlib import Path
import subprocess, tempfile
root=Path(__file__).resolve().parents[1]
header=(root/'hyperlink.h').read_text().replace('#include <exec/types.h>','')
source='\n'.join(x for x in (root/'hyperlink.c').read_text().splitlines() if not x.startswith('#include'))
prefix=r'''
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
typedef uint8_t UBYTE;typedef uint32_t ULONG;typedef int BOOL;typedef void VOID;
#define TRUE 1
#define FALSE 0
#define MEMF_ANY 0
static unsigned allocations, fail;
static void *AllocMem(size_t size,int flags){(void)flags;if(fail)return NULL;void *p=malloc(size);if(p)allocations++;return p;}
static void FreeMem(void *p,size_t size){(void)size;assert(allocations);allocations--;free(p);}
'''
main=r'''
static struct con_link *make(struct con_link_pool *p,const char *uri,const char *id){return con_link_create(p,(const UBYTE*)uri,strlen(uri),(const UBYTE*)id,strlen(id));}
static void empty(struct con_link_pool *p){assert(!p->first&&!p->records&&!p->bytes);}
int main(void){
 struct con_link_pool pool={0},other={0};struct con_link *a=make(&pool,"https://example.org/a","id");assert(a);
 assert(!strcmp((char*)a->data,"https://example.org/a"));assert(!strcmp((char*)a->data+a->uriLength+1,"id"));
 struct con_link *b=make(&pool,"https://example.org/a","id");assert(a==b&&a->references==2&&pool.records==1);con_link_release(b);
 b=make(&pool,"https://example.org/b","id");assert(b&&b!=a);con_link_release(b);
 b=make(&other,"https://example.org/a","id");assert(b&&b!=a);con_link_release(b);empty(&other);
 b=make(&pool,"https://example.org/a","");struct con_link *c=make(&pool,"https://example.org/a","");assert(b&&c&&b!=c);con_link_release(b);con_link_release(c);
 fail=1;assert(!make(&pool,"https://new.org/","id"));b=make(&pool,"https://example.org/a","id");assert(b==a);con_link_release(b);fail=0;
 assert(!con_link_create(&pool,NULL,1,NULL,0));assert(!con_link_create(&pool,(UBYTE*)"",0,NULL,0));
 assert(!con_link_create(&pool,(UBYTE*)"a\0b",3,NULL,0));assert(!make(&pool,"https://a/\n","")&&!make(&pool,"https://a/","x\t"));
 con_link_release(a);empty(&pool);
 /* Every overlapping source/destination/count layout, including self-copy. */
 for(unsigned src=0;src<16;src++)for(unsigned dst=0;dst<16;dst++)for(unsigned n=0;n<=16;n++){
  if(src+n>16||dst+n>16)continue;
  struct con_link *cells[16]={0},*expected[16],*links[3];
  links[0]=make(&pool,"https://a/","");links[1]=make(&pool,"https://b/","");links[2]=NULL;
  for(unsigned i=0;i<16;i++)assert(con_link_fill(cells+i,1,links[i%3]));
  memcpy(expected,cells,sizeof(cells));memmove(expected+dst,expected+src,n*sizeof(*expected));
  assert(con_link_copy(cells+dst,cells+src,n));assert(!memcmp(cells,expected,sizeof(cells)));
  for(unsigned k=0;k<2;k++){unsigned refs=1;for(unsigned i=0;i<16;i++)refs+=cells[i]==links[k];assert(links[k]->references==refs);con_link_release(links[k]);}
  assert(con_link_fill(cells,16,NULL));empty(&pool);
 }
 /* Final source reference can be overwritten without premature free. */
 struct con_link *cells[4]={0};a=make(&pool,"https://last/","");assert(con_link_fill(cells,1,a));con_link_release(a);
 assert(con_link_fill(cells,4,cells[0]));assert(cells[0]->references==4);assert(con_link_fill(cells,4,NULL));empty(&pool);
 /* Retain overflow rolls back earlier acquisitions, leaving cells intact. */
 a=make(&pool,"https://a/","");b=make(&pool,"https://b/","");struct con_link *src[2]={a,b},*dst[2]={0};
 b->references=UINT32_MAX;assert(!con_link_copy(dst,src,2));assert(a->references==1&&b->references==UINT32_MAX&&!dst[0]&&!dst[1]);
 assert(!con_link_fill(dst,2,b));c=make(&pool,"https://b/","");assert(c&&c!=b);con_link_release(c);
 b->references=1;con_link_release(a);con_link_release(b);empty(&pool);
 /* Record and byte limits, then reclamation and reuse. */
 struct con_link *many[128];for(unsigned i=0;i<128;i++){many[i]=make(&pool,"https://a/","");assert(many[i]);}
 assert(!make(&pool,"https://a/",""));for(unsigned i=0;i<128;i++)con_link_release(many[i]);empty(&pool);
 char uri[1025],id[257];memset(uri,'a',sizeof(uri));memset(id,'b',sizeof(id));uri[1023]=0;id[255]=0;
 unsigned count=0;
 for(;;){id[0]="0123456789abcdef"[count/16];id[1]="0123456789abcdef"[count%16];a=make(&pool,uri,id);if(!a)break;many[count++]=a;assert(count<100);}
 assert(count>1&&pool.bytes<=CON_LINK_BYTES_MAX);
 assert(pool.bytes+sizeof(struct con_link)+1023+255+1>CON_LINK_BYTES_MAX);for(unsigned i=0;i<count;i++)con_link_release(many[i]);empty(&pool);
 assert(!con_link_create(&pool,(UBYTE*)uri,1024,NULL,0));assert(!con_link_create(&pool,(UBYTE*)uri,1023,(UBYTE*)id,256));
 /* Parse OSC8 parameters without splitting URI semicolons or percent decoding. */
 struct con_link *current=NULL,*saved=NULL;
 const char *payload="foo=ignored:id=group:id=later;https://host/a;b?x=%20";
 assert(con_link_open(&pool,&current,(UBYTE*)payload,strlen(payload)));
 assert(!strcmp((char*)current->data,"https://host/a;b?x=%20"));
 assert(!strcmp((char*)current->data+current->uriLength+1,"group"));
 assert(con_link_fill(&saved,1,current));
 assert(con_link_open(&pool,&current,(UBYTE*)payload,strlen(payload))&&current==saved);
 assert(con_link_open(&pool,&current,(UBYTE*)";",1)&&!current&&pool.records==1);
 assert(con_link_fill(&current,1,saved));
 assert(!con_link_open(&pool,&current,(UBYTE*)"no-separator",12)&&!current);
 assert(con_link_fill(&current,1,saved));fail=1;
 assert(!con_link_open(&pool,&current,(UBYTE*)";https://new/",12)&&!current);fail=0;
 assert(con_link_fill(&current,1,saved));
 assert(!con_link_open(&pool,&current,NULL,0)&&!current);
 assert(con_link_fill(&saved,1,NULL));empty(&pool);
 char longlink[1400];memcpy(longlink,"id=",3);memset(longlink+3,'i',255);longlink[258]=';';memset(longlink+259,'u',1023);
 assert(con_link_open(&pool,&current,(UBYTE*)longlink,1282));
 assert(current->idLength==255&&current->uriLength==1023);
 assert(!con_link_open(&pool,&current,(UBYTE*)longlink,1283)&&!current);empty(&pool);
 assert(!allocations);puts("hyperlink store PASS: overlap ownership, rollback, isolation, allocation failure and live quotas");return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';exe=Path(d)/'test';p.write_text(prefix+header+source+main)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
 # Prove that removing reference acquisition is caught, without editing source.
 broken=source.replace('link->references++;','/* deliberately omit retain */',1)
 p.write_text(prefix+header+broken+main)
 subprocess.run(['cc','-Wall','-Werror',str(p),'-o',str(exe)],check=True)
 failed=subprocess.run([str(exe)],capture_output=True,text=True)
 assert failed.returncode != 0 and 'Assertion' in failed.stderr, failed.stderr
 print('negative control PASS: missing retain rejected')
