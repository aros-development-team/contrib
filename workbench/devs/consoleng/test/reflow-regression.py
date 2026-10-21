#!/usr/bin/env python3
"""Actual transactional row repacking: text/roles, anchors and every allocation failure."""
from pathlib import Path
from hyperlink_fixture import hyperlink_source
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
h=(r/'charmap.h').read_text();line=h[h.index('struct charmap_line\n'):h.index('VOID charmap_dispose_lines')]
anchors=h[h.index('#define CHARMAP_REFLOW_ANCHORS'):h.index('BOOL charmap_reflow(')]
a=(r/'charmap.c').read_text();a=a[a.index('struct charmap_line *charmap_dispose_line('):]
b=(r/'charmap_reflow.c').read_text();b=b[b.index('struct reflow_builder'):]
prefix=r'''
#define CHARMAP_ERASED 0x21
#include <assert.h>
#include <stdint.h>
typedef uint64_t UQUAD;
#include <stdlib.h>
#include <string.h>
typedef uint32_t ULONG;typedef uint16_t UWORD;typedef uint8_t UBYTE;typedef int8_t BYTE;typedef int BOOL;typedef void VOID;
#define TRUE 1
#define FALSE 0
#define CELL_WIDE_CONT 0xfffffffeUL
#define MEMF_ANY 0
#define SetMem(p,v,n) memset(p,v,n)
struct ConsoleBase {int unused;};
static int attempt,fail,live;
static void *allocate(size_t n){if(++attempt==fail)return NULL;void *p=malloc(n);if(p)live++;return p;}
static void release(void *p){if(p)live--;free(p);}
#define AllocMem(n,f) allocate(n)
#define FreeMem(p,n) ((void)(n),release(p))
static int con_cp_width(ULONG cp){return cp==0x2615?2:1;}
'''
ch=(r/'charmapconclass.c').read_text()
trim=ch[ch.index('static VOID charmap_trim_selection('):ch.index('static struct charmap_line *charmapcon_find_line(')]
scroll=ch[ch.index('static VOID charmap_scroll_up('):ch.index('static VOID charmap_scroll_down(')]
wrapper=ch[ch.index('static struct charmap_line *charmap_row_at('):ch.index('static VOID charmapcon_newwindowsize(')]
remap=ch[ch.index('static VOID charmap_remap_colors('):ch.index('/* Cells keep')]
wrapperprefix=r'''
typedef int Class;typedef long LONG;
struct charmapcondata {struct charmap_line *top_of_window,*top_of_scrollback,*saved_top_of_window,*select_line_min,*select_line_max;ULONG scrollback_size,scrollback_pos,saved_scrollback_pos,scrollback_max;int unrendered;LONG select_x_min,select_x_max,select_y_min,select_y_max;BOOL alternate;};
typedef struct {struct charmapcondata data;ULONG x,xc,y,yc,savedX,savedY;BOOL savedCursor,savedWrapPending;int conFlags,xmax,ymax;} Object;
#define INST_DATA(cl,o) (&(o)->data)
#define ICU(o) (o)
#define XCP (o->x)
#define XCCP (o->xc)
#define YCP (o->y)
#define YCCP (o->yc)
#define CHAR_XMAX(o) ((o)->xmax)
#define CHAR_YMAX(o) ((o)->ymax)
#define CF_WRAP_PENDING 128
#define ConsoleDevice NULL
#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))
#define Console_RenderCursor(o) ((void)0)
#define CHARMAP_COLORPENS 34
static void charmapcon_colorpens(Object*o,UBYTE*p){(void)o;for(int i=0;i<34;i++)p[i]=40+i;}
'''
main=r'''
static int flatten(struct charmap_line *p,ULONG *out){int n=0;for(;p;p=p->next){
 ULONG end=p->size;if(p->wrapColumns&&p->wrapPadding)end--;
 for(ULONG x=0;x<end;x++)if(p->text[x]!=CELL_WIDE_CONT){out[n++]=p->text[x];assert(p->roles[x]==0x0102);}
 if(!p->wrapColumns)out[n++]=10;
}return n;}
static struct charmap_line *fixture(void){
 struct charmap_line *a=charmap_newline(0,0),*b=charmap_newline(0,a),*c=charmap_newline(0,b);
 ULONG first[]={'A','B','C',0x2615,CELL_WIDE_CONT,'D','E',' '},second[]={'Z',' ',' '};
 charmap_resize(0,a,8);charmap_resize(0,b,3);a->wrapColumns=8;
 memcpy(a->text,first,sizeof(first));memcpy(b->text,second,sizeof(second));
 for(struct charmap_line *p=a;p;p=p->next)for(ULONG i=0;i<p->size;i++){p->roles[i]=0x0102;p->fgpen[i]=4;p->bgpen[i]=6;p->flags[i]=2;}
 assert(c);return a;
}
int main(void){
 /* Erased colored tails are rendition, not extra wrapped paragraphs. */
 for(int mixed=0;mixed<2;mixed++)for(int w=3;w<=12;w++){
  struct charmap_line *h=charmap_newline(0,0);charmap_resize(0,h,12);
  for(int x=0;x<12;x++){h->text[x]=' ';h->fgpen[x]=4;h->bgpen[x]=6;h->flags[x]=CHARMAP_ERASED;h->roles[x]=0x708;}
  if(mixed)for(int x=0;x<2;x++){h->bgpen[x]=2;h->flags[x]=0;h->roles[x]=0x0102;}
  for(int repeat=0;repeat<20;repeat++){
   struct charmap_line *out=0;ULONG rows=0;int width=repeat%2?12:w;
   assert(charmap_reflow(0,h,width,0,0,&out,&rows));assert(rows==1&&out->size==(ULONG)width);
   for(int x=0;x<width;x++){assert(out->text[x]==' ');assert(out->bgpen[x]==(mixed&&x<2?2:6));assert(out->roles[x]==(mixed&&x<2?0x0102:0x708));}
   charmap_dispose_lines(h);h=out;
  }
  charmap_dispose_lines(h);assert(!live);
 }

 {Object o={0};o.data.top_of_window=o.data.top_of_scrollback=charmap_newline(0,0);o.data.scrollback_size=1;o.data.scrollback_max=100;o.xmax=3;o.ymax=8;
 assert(charmap_reflow_primary(0,&o,6,0,FALSE));assert(o.x==2&&o.y==1);
 for(struct charmap_line*q=o.data.top_of_scrollback;q;q=q->next)for(ULONG x=0;x<q->size;x++)assert(q->fgpen[x]==40&&q->bgpen[x]==41);
 charmap_dispose_lines(o.data.top_of_scrollback);assert(!live);}

 /* Default empty rows must not multiply when the viewport narrows. Explicit
    colors/styles are retained cells, even when their text is a space. */
 for(int kind=0;kind<4;kind++)for(ULONG width=1;width<=16;width++){
  struct charmap_line *empty=charmap_newline(0,0),*out=0;ULONG rows=0;
  charmap_resize(0,empty,8);
  for(int x=0;x<8;x++){empty->text[x]=' ';empty->fgpen[x]=7;empty->bgpen[x]=9;empty->roles[x]=kind==1?0x0103:kind==2?0:0x0102;empty->flags[x]=kind==3?1:0;}
  assert(charmap_reflow(0,empty,width,0,0,&out,&rows));
  assert(rows==(kind?(8+width-1)/width:1));
  if(!kind){assert(out->size==width);for(ULONG x=0;x<width;x++)assert(out->text[x]==' '&&out->fgpen[x]==7&&out->bgpen[x]==9&&out->roles[x]==0x0102);}
  charmap_dispose_lines(out);
  if(!kind){struct charmap_anchor anchor={empty,6,TRUE,0,0,0};
   assert(charmap_reflow(0,empty,4,&anchor,1,&out,&rows));assert(rows==2&&anchor.newRow==1&&anchor.newColumn==2);
   for(struct charmap_line*q=out;q;q=q->next)for(ULONG x=0;x<q->size;x++)assert(q->fgpen[x]==7&&q->bgpen[x]==9&&q->roles[x]==0x0102);
   charmap_dispose_lines(out);
  }
  charmap_dispose_lines(empty);assert(!live);
 }
 struct charmap_line *old=fixture();ULONG expected[1024],actual[1024];int count=flatten(old,expected),baseline=live;
 for(ULONG w=1;w<=40;w++){
  struct charmap_line *out=NULL;ULONG rows=0;struct charmap_anchor p[2]={{old,3,0,0,0,0},{old->next,2,0,0,0,0}};
  assert(charmap_reflow(0,old,w,p,2,&out,&rows));assert(out&&rows);
  assert(flatten(out,actual)==count&&!memcmp(actual,expected,count*sizeof(ULONG)));
  assert(p[0].newLine->text[p[0].newColumn]==0x2615);
  assert(p[1].newColumn<=w);
  struct charmap_line *prev=NULL;unsigned seen=0;
  for(struct charmap_line *q=out;q;prev=q,q=q->next){assert(q->prev==prev&&q->size<=w);seen++;}
  assert(seen==rows);
  struct charmap_line *back=NULL;ULONG backrows;
  assert(charmap_reflow(0,out,8,0,0,&back,&backrows));
  assert(flatten(back,actual)==count&&!memcmp(actual,expected,count*sizeof(ULONG)));
  charmap_dispose_lines(back);charmap_dispose_lines(out);assert(live==baseline);
 }
 int failures=0;
 for(int nth=1;nth<200;nth++){
  struct charmap_line *out=(void*)1;ULONG rows=777;
  struct charmap_anchor p={old,3,0,(void*)2,333,444},before=p;
  fail=nth;attempt=0;BOOL ok=charmap_reflow(0,old,3,&p,1,&out,&rows);fail=0;
  if(ok){charmap_dispose_lines(out);break;}
  failures++;assert(out==(void*)1&&rows==777&&!memcmp(&p,&before,sizeof(p))&&live==baseline);
  assert(flatten(old,actual)==count&&!memcmp(actual,expected,count*sizeof(ULONG)));
 }
 assert(failures>20);
 struct charmap_line *out=NULL;ULONG rows=0;struct charmap_anchor cursor={old->next,8,1,0,0,0};
 assert(charmap_reflow(0,old,5,&cursor,1,&out,&rows));assert(cursor.newColumn<=5&&cursor.newLine);charmap_dispose_lines(out);
 assert(!charmap_reflow(0,old,0,0,0,&out,&rows));
 /* Different retained colors/styles must follow each moved cell, not just
    the paragraph's first rendition. */
 old->roles[0]=0xabcdef1234568080ULL;old->fgpen[0]=23;old->bgpen[0]=47;old->flags[0]=7;
 old->roles[1]=0x0000;old->fgpen[1]=89;old->bgpen[1]=101;old->flags[1]=3;
 assert(charmap_reflow(0,old,1,0,0,&out,&rows));
 assert(out->roles[0]==0xabcdef1234568080ULL&&out->fgpen[0]==23&&out->bgpen[0]==47&&out->flags[0]==7);
 assert(out->next->roles[0]==0&&out->next->fgpen[0]==89&&out->next->bgpen[0]==101&&out->next->flags[0]==3);
 charmap_dispose_lines(out);
 charmap_dispose_lines(old);assert(!live);
 /* Saved right-edge cursor is a logical insertion point, not the last cell. */
 for(int width=6;width<=10;width+=2){
  Object edge={0};edge.xmax=width-1;edge.ymax=24;edge.savedCursor=1;edge.savedX=7;edge.savedWrapPending=1;
  edge.data.top_of_window=edge.data.top_of_scrollback=charmap_newline(0,0);edge.data.scrollback_size=1;
  charmap_resize(0,edge.data.top_of_window,8);for(int i=0;i<8;i++)edge.data.top_of_window->text[i]='A';
  assert(charmap_reflow_primary(0,&edge,0,0,0));
  assert(edge.savedX==(width==6?2:width==8?7:8));assert(edge.savedY==(width==6?1:0));assert(edge.savedWrapPending==(width==8));
  charmap_dispose_lines(edge.data.top_of_scrollback);assert(!live);
 }
 Object o={0};o.xmax=2;o.ymax=24;o.savedCursor=1;o.savedY=1;o.savedX=1;
 o.data.top_of_window=o.data.top_of_scrollback=fixture();o.data.scrollback_size=3;
 o.data.select_line_min=o.data.top_of_window;o.data.select_x_min=3;
 o.data.select_line_max=o.data.top_of_window->next;o.data.select_x_max=2;
 Object before=o;fail=1;attempt=0;assert(!charmap_reflow_primary(0,&o,2,1,0));fail=0;assert(!memcmp(&o,&before,sizeof(o)));
 assert(charmap_reflow_primary(0,&o,2,1,0));assert(o.x==1&&o.y==3&&o.savedX==0&&o.savedY==3);
 assert(o.data.select_x_min==0&&o.data.select_y_min==1&&o.data.select_x_max==1&&o.data.select_y_max==3);
 o.xmax=7;assert(charmap_reflow_primary(0,&o,o.x,o.y,0));assert(o.x==2&&o.y==1);
 assert(flatten(o.data.top_of_scrollback,actual)==count&&!memcmp(actual,expected,count*sizeof(ULONG)));
 charmap_dispose_lines(o.data.top_of_scrollback);assert(!live);
 /* Cursor and saved cursor on rows never allocated by output. Every failed
    temporary-suffix or repack allocation must leave the live chain intact. */
 for(int nth=1;nth<250;nth++){
  memset(&o,0,sizeof(o));o.xmax=2;o.ymax=24;o.savedCursor=1;o.savedY=5;o.savedX=1;
  o.data.top_of_window=o.data.top_of_scrollback=fixture();o.data.scrollback_size=3;
  before=o;int original_live=live;fail=nth;attempt=0;
  int ok=charmap_reflow_primary(0,&o,2,4,0);fail=0;
  if(!ok){assert(!memcmp(&o,&before,sizeof(o))&&live==original_live);
   assert(!o.data.top_of_scrollback->next->next->next);
  } else {assert(o.x==2&&o.y==6&&o.savedX==1&&o.savedY==7);}
  charmap_dispose_lines(o.data.top_of_scrollback);assert(!live);
  if(ok)break;
  assert(nth<249);
 }
 /* Reflow and actual history trimming together, including selected lines
    that are removed by the retained-row budget. */
 for(int budget=0;budget<12;budget++)for(int endpoint=0;endpoint<100;endpoint++){
  memset(&o,0,sizeof(o));o.xmax=2;o.ymax=3;
  struct charmap_line *nodes[100],*tail=NULL;
  for(int i=0;i<100;i++){
   nodes[i]=tail=charmap_newline(NULL,tail);assert(tail);charmap_resize(NULL,tail,8);
   for(int x=0;x<8;x++)tail->text[x]='A'+i%26;
  }
  o.data.top_of_scrollback=nodes[0];o.data.top_of_window=nodes[96];
  o.data.scrollback_size=100;o.data.scrollback_pos=96;o.data.scrollback_max=budget;
  o.data.select_line_min=nodes[endpoint];o.data.select_x_min=1;
  o.data.select_line_max=nodes[99];o.data.select_x_max=6;
  assert(charmap_reflow_primary(0,&o,7,3,0));
  struct charmap_line *q=o.data.top_of_scrollback,*prev=NULL;unsigned seen=0;int foundtop=0,foundmin=0,foundmax=0;
  for(;q;prev=q,q=q->next,seen++){
   assert(q->prev==prev);
   if(q==o.data.top_of_window){assert(seen==o.data.scrollback_pos);foundtop=1;}
   if(q==o.data.select_line_min){assert((LONG)seen-(LONG)o.data.scrollback_pos==o.data.select_y_min);foundmin=1;}
   if(q==o.data.select_line_max){assert((LONG)seen-(LONG)o.data.scrollback_pos==o.data.select_y_max);foundmax=1;}
  }
  assert(foundtop&&foundmin&&foundmax&&seen==o.data.scrollback_size);
  assert(o.data.scrollback_size<=o.data.scrollback_max+(ULONG)o.ymax+1);
  assert(o.data.saved_top_of_window==o.data.top_of_window);
  assert(o.data.saved_scrollback_pos==o.data.scrollback_pos);
  assert(o.x==1&&o.y==3);
  charmap_dispose_lines(o.data.top_of_scrollback);assert(!live);
 }
 return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'t.c';binary=Path(d)/'t';p.write_text(prefix+hyperlink_source()+line+anchors+a+b+wrapperprefix+remap+trim+scroll+wrapper+main)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(binary)],check=True)
 subprocess.run([str(binary)],check=True)
print('Reflow model PASS: widths1..40 roundtrip, wide cells/padding, hard lines/trailing spaces, roles, anchors, every allocation failure and 1200 real history/selection trim cases')
