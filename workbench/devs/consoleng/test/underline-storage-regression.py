#!/usr/bin/env python3
"""Optional underline color ownership: allocation rollback, resize and reflow."""
from pathlib import Path
from hyperlink_fixture import hyperlink_source
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
h=(r/'charmap.h').read_text();line=h[h.index('struct charmap_line\n'):h.index('VOID charmap_dispose_lines')]
anchors=h[h.index('#define CHARMAP_REFLOW_ANCHORS'):h.index('BOOL charmap_reflow(')]
s=(r/'charmap.c').read_text();source=s[s.index('struct charmap_line *charmap_dispose_line('):]
s=(r/'charmap_reflow.c').read_text();reflow=s[s.index('struct reflow_builder'):]
prefix=r'''
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
typedef uint64_t UQUAD;typedef uint32_t ULONG;typedef uint16_t UWORD;
typedef uint8_t UBYTE;typedef int8_t BYTE;typedef int BOOL;typedef void VOID;
#define TRUE 1
#define FALSE 0
#define CHARMAP_ERASED 0x21
#define CELL_WIDE_CONT 0xfffffffeUL
#define MEMF_ANY 0
#define SetMem(p,v,n) memset(p,v,n)
struct ConsoleBase{int unused;};
static int attempt,fail,live;
static void *allocate(size_t n){if(++attempt==fail)return NULL;void*p=malloc(n);if(p)live++;return p;}
static void release(void*p){if(p)live--;free(p);}
#define AllocMem(n,f) allocate(n)
#define FreeMem(p,n) ((void)(n),release(p))
static int con_cp_width(ULONG cp){return cp==0x2615?2:1;}
'''
main=r'''
static struct charmap_line *fixture(void){
 struct charmap_line*p=charmap_newline(0,0);assert(p);charmap_resize(0,p,6);assert(p->size==6);
 ULONG text[]={'A',0x2615,CELL_WIDE_CONT,'B','C','D'};
 for(int i=0;i<6;i++){p->text[i]=text[i];p->roles[i]=0x0102;p->flags[i]=1;assert(charmap_underline_colors(p,i,1,i==2?102:101+i));}
 return p;
}
int main(void){
 struct charmap_line*p=charmap_newline(0,0);charmap_resize(0,p,4);int baseline=live;
 assert(charmap_underline_colors(p,0,4,0)&&live==baseline&&!p->underlineColors);
 assert(charmap_underline_colors(p,4,0,99)&&!p->underlineColors);
 assert(!charmap_underline_colors(p,5,0,99)&&!charmap_underline_colors(p,3,2,99));
 struct charmap_line before=*p;attempt=0;fail=1;
 assert(!charmap_underline_colors(p,1,2,99)&&!memcmp(&before,p,sizeof before)&&live==baseline);
 fail=0;assert(charmap_underline_colors(p,1,2,99)&&live==baseline+1);
 assert(!p->underlineColors[0]&&p->underlineColors[1]==99&&p->underlineColors[2]==99&&!p->underlineColors[3]);
 for(int n=1;n<=6;n++){
  before=*p;attempt=0;fail=n;charmap_resize(0,p,80);
  assert(!memcmp(&before,p,sizeof before)&&live==baseline+1&&p->underlineColors[1]==99&&p->underlineColors[2]==99);
 }
 fail=0;charmap_resize(0,p,80);assert(p->size==80&&live==baseline+1);
 assert(p->underlineColors[1]==99&&p->underlineColors[2]==99);
 for(int i=4;i<80;i++)assert(!p->underlineColors[i]);
 charmap_resize(0,p,2);charmap_resize(0,p,80);for(int i=2;i<80;i++)assert(!p->underlineColors[i]);
 assert(charmap_underline_colors(p,0,80,0));for(int i=0;i<80;i++)assert(!p->underlineColors[i]);
 charmap_dispose_lines(p);assert(!live);
 int failures=0,runs=0;
 for(int width=2;width<=12;width++){
  p=fixture();baseline=live;struct charmap_line*out=0;ULONG rows=0;
  attempt=0;assert(charmap_reflow(0,p,width,0,0,&out,&rows));int allocations=attempt;
  int index=0;ULONG want[]={101,102,102,104,105,106};
  for(struct charmap_line*q=out;q;q=q->next)for(ULONG x=0;x<q->size;x++){
   if(q->wrapPadding&&x==q->size-1)continue;
   assert(index<6&&q->underlineColors&&q->underlineColors[x]==want[index++]);
  }
  assert(index==6);charmap_dispose_lines(out);assert(live==baseline);
  for(int n=1;n<=allocations;n++){
   before=*p;attempt=0;fail=n;out=0;
   assert(!charmap_reflow(0,p,width,0,0,&out,&rows));
   assert(!memcmp(&before,p,sizeof before)&&live==baseline);
   for(int x=0;x<6;x++)assert(p->underlineColors[x]==want[x]);
   failures++;
  }
  fail=0;charmap_dispose_lines(p);assert(!live);runs++;
 }
 printf("Underline storage PASS: lazy allocation, six resize failures, %d reflows, %d rollback points\n",runs,failures);
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';b=Path(d)/'test';code=prefix+line+anchors+hyperlink_source()+source+reflow+main;p.write_text(code)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(b)],check=True)
 subprocess.run([str(b)],check=True)
 p.write_text(code.replace('memcpy(newUnderlineColors, oldUnderlineColors, copycells * sizeof(ULONG));','SetMem(newUnderlineColors, 0, copycells * sizeof(ULONG));'))
 subprocess.run(['cc',str(p),'-o',str(b)],check=True)
 assert subprocess.run([str(b)],capture_output=True).returncode!=0
 print('Lost color ownership negative control rejected')
