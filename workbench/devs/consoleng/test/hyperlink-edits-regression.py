#!/usr/bin/env python3
"""Actual ICH/DCH/EL/wide repair preserve only surviving explicit link cells."""
from pathlib import Path
from hyperlink_fixture import hyperlink_source
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
h=(r/'charmap.h').read_text();line=h[h.index('struct charmap_line\n'):h.index('VOID charmap_dispose_lines')]
a=(r/'charmap.c').read_text();a=a[a.index('struct charmap_line *charmap_dispose_line('):]
s=(r/'charmapconclass.c').read_text();edit=s[s.index('static VOID charmap_repair_wide_cells('):s.index('static VOID charmap_formfeed(')]
el=s[s.index('static VOID charmap_erase_line('):s.index('/* DoCommand restores')]
ascii_write=s[s.index('static BOOL charmap_ascii('):s.index('static VOID charmap_scroll_up(')]
ech=s[s.index('    case C_ERASE_CHAR:'):s.index('    case C_ERASE_IN_LINE:')]
ech='static void erase_char(Object *o, IPTR *params) {Class *cl=NULL;switch(0){'+ech.replace('case C_ERASE_CHAR:', 'case 0:')+'}}'
prefix=r'''
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
typedef uint64_t UQUAD;typedef uint32_t ULONG;typedef uint16_t UWORD;typedef int16_t WORD;typedef uint8_t UBYTE;typedef int8_t BYTE;typedef int BOOL;typedef void VOID;typedef intptr_t IPTR;typedef int32_t LONG;typedef void Class;
#define CON_TXTFLAGS_BOLD 2
#define CON_TXTFLAGS_ITALIC 4
#define TRUE 1
#define FALSE 0
#define MEMF_ANY 0
#define CHARMAP_ERASED 0x21
#define CELL_WIDE_CONT 0xfffffffeUL
#define SetMem(p,v,n) memset(p,v,n)
struct ConsoleBase {int unused;};
static int live;
static void *allocate(size_t n){void*p=malloc(n);if(p)live++;return p;}
static void release(void*p){if(p)live--;free(p);}
#define AllocMem(n,f) allocate(n)
#define FreeMem(p,n) ((void)(n),release(p))
static int con_cp_width(ULONG cp){return cp==0x2615?2:1;}
'''
wrapper=r'''
struct charmapcondata {struct con_link *current_link;};
static struct charmapcondata link_data;
#define INST_DATA(cl,o) (&link_data)
typedef struct {int max,fg,bg,flags,x,y;ULONG underlineColor;} Object;
static struct charmap_line *fixture;
#define CHAR_XMAX(o) ((o)->max)
#define CU(o) (o)
#define ICU(o) (o)
#define cu_FgPen fg
#define cu_BgPen bg
#define cu_TxFlags flags
#define CON_TEXT_IS_UTF8(o) 1
#define CHARMAP_ROLES(o) 0x0102
#define ConsoleDevice NULL
#define charmapcon_find_line(cl,o,y) ((void)(cl),(void)(y),fixture)
#define XCP o->x
#define YCP o->y
#define MIN(a,b) ((a)<(b)?(a):(b))
#define CON_IS_OBSCURED(o) 1
#define charmapcon_refresh_lines(cl,o,a,b) ((void)0)
/* This fixture exercises ASCII overwrite, not UTF-8 decoding (covered elsewhere). */
static ULONG con_utf8_cellcount(const UBYTE *s,ULONG len){(void)s;(void)len;assert(0);return 0;}
static ULONG con_utf8_decode(const UBYTE **s,LONG *len){(void)s;(void)len;assert(0);return 0;}
static BOOL con_url_in_row(struct charmap_line *l){(void)l;return FALSE;}
'''
main=r'''
int main(void){Object o={7,1,2,0,0,0};struct con_link_pool pool={0};unsigned cases=0;
 for(int wide=0;wide<2;wide++)for(int action=0;action<5;action++)for(unsigned x=0;x<8;x++)for(unsigned n=0;n<=9;n++){
  struct con_link *links[2]={con_link_create(&pool,(UBYTE*)"https://a/",10,NULL,0),con_link_create(&pool,(UBYTE*)"https://b/",10,NULL,0)};assert(links[0]&&links[1]);
  fixture=charmap_newline(NULL,NULL);charmap_resize(NULL,fixture,8);
  for(unsigned i=0;i<8;i++){fixture->text[i]='A'+i;assert(charmap_links(fixture,i,1,links[i%2]));}
  if(wide){fixture->text[3]=0x2615;fixture->text[4]=CELL_WIDE_CONT;assert(charmap_links(fixture,3,2,links[1]));}
  if(action==0)assert(charmap_insert_char(NULL,&o,x,0,n));
  else if(action==1)assert(charmap_delete_char(NULL,&o,x,0,n));
  else if(action==2)charmap_erase_line(NULL,&o,x,0,n%3);
  else if(action==3){IPTR params[]={n};o.x=x;erase_char(&o,params);}
  else {link_data.current_link=links[0];charmap_ascii(NULL,&o,x,0,"XXXXXXXXX",n,FALSE);link_data.current_link=NULL;}
  unsigned refs[2]={1,1};
  for(unsigned i=0;i<8;i++){
   ULONG cp=fixture->text[i];struct con_link *expected=NULL;
   if(cp==0x2615||cp==CELL_WIDE_CONT)expected=links[1];
   else if(cp>='A'&&cp<='H')expected=links[(cp-'A')%2];
   else if(cp=='X')expected=links[0];else assert(cp==' ');
   assert(fixture->links[i]==expected);
   for(unsigned k=0;k<2;k++)refs[k]+=expected==links[k];
  }
  for(unsigned k=0;k<2;k++)assert(links[k]->references==refs[k]);
  charmap_dispose_lines(fixture);for(unsigned k=0;k<2;k++){assert(links[k]->references==1);con_link_release(links[k]);}
  assert(!live&&!pool.records&&!pool.bytes);cases++;
 }
 printf("hyperlink edits PASS: %u actual ICH/DCH/EL/ECH/ASCII overwrite cases, wide repair, exact surviving references and cleanup\n",cases);return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';exe=Path(d)/'test';p.write_text(prefix+hyperlink_source()+line+a+wrapper+edit+el+ascii_write+ech+main)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
