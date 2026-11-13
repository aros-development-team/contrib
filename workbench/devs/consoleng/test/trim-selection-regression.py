#!/usr/bin/env python3
"""Exercise actual scroll-up trimming with every endpoint ordering under ASan."""
from pathlib import Path
import subprocess, tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'charmapconclass.c').read_text()
helper='static VOID charmap_trim_selection('+s.split('static VOID charmap_trim_selection(',1)[1].split('static struct charmap_line *charmapcon_find_line(',1)[0]
scroll='static VOID charmap_scroll_up('+s.split('static VOID charmap_scroll_up(',1)[1].split('static VOID charmap_scroll_down(',1)[0]
limit='static ULONG charmap_history_limit('+s.split('static ULONG charmap_history_limit(',1)[1].split('static VOID charmapcon_adj_prop(',1)[0]
find='static struct charmap_line *charmapcon_find_line('+s.split('static struct charmap_line *charmapcon_find_line(',1)[1].split('/* Store a byte span',1)[0]
source=r'''
#include <stdlib.h>
#include <assert.h>
typedef void VOID;typedef void Class;typedef unsigned long ULONG;
struct charmap_line {struct charmap_line *next,*prev;int value;ULONG *underlineColors;};
struct charmapcondata {struct charmap_line *top_of_window,*top_of_scrollback,*select_line_min,*select_line_max;ULONG scrollback_size,scrollback_max,scrollback_pos,saved_scrollback_pos;int select_x_min,select_x_max,select_y_min,select_y_max,unrendered;};
typedef struct charmapcondata Object;
#define D(x) ((void)0)
#define INST_DATA(cl,o) (o)
#define CHAR_YMAX(o) 3
#define Console_RenderCursor(o) ((void)0)
static struct charmap_line *charmap_newline(int unused,struct charmap_line *prev) {struct charmap_line *p=calloc(1,sizeof *p);assert(p);p->prev=prev;if(prev)prev->next=p;return p;}
static struct charmap_line *charmap_dispose_line(struct charmap_line *p) {struct charmap_line *next=p->next;if(next)next->prev=p->prev;if(p->prev)p->prev->next=next;free(p);return next;}
'''+helper+limit+find+scroll+r'''
int main(void) {
 for(int a=0;a<12;a++)for(int b=0;b<12;b++) {
  Object o={0};struct charmap_line *nodes[12],*p=0;
  for(int i=0;i<12;i++){nodes[i]=p=charmap_newline(0,p);p->value=i;}
  o.top_of_scrollback=nodes[0];o.top_of_window=nodes[9];o.scrollback_pos=9;
  o.scrollback_size=12;o.scrollback_max=2;
  o.select_line_min=nodes[a];o.select_line_max=nodes[b];o.select_x_min=2;o.select_x_max=4;
  o.select_y_min=a-9;o.select_y_max=b-9;
  charmap_scroll_up(0,&o,1);
  assert(o.scrollback_size==5&&o.scrollback_pos==3&&o.top_of_scrollback==nodes[7]);
  assert(o.select_line_min->value==(a<7?7:a));
  assert(o.select_line_max->value==(b<7?7:b));
  assert(o.select_y_min==(a<7?7:a)-10&&o.select_y_max==(b<7?7:b)-10);
  assert(o.select_x_min==(a<7?0:2)&&o.select_x_max==(b<7?0:4));
  while(o.top_of_scrollback)o.top_of_scrollback=charmap_dispose_line(o.top_of_scrollback);
 }
 /* A trailing blank cursor row is not allocated. Do not restore the live
    cursor one row early just because the retained list ends there. */
 for(int missing=0;missing<3;missing++) {
  Object o={0};struct charmap_line *nodes[12],*p=0;
  for(int i=0;i<12-missing;i++){nodes[i]=p=charmap_newline(0,p);}
  o.top_of_scrollback=nodes[0];o.top_of_window=nodes[7];o.scrollback_pos=7;
  o.scrollback_size=12-missing;o.scrollback_max=100;
  o.saved_scrollback_pos=9;o.unrendered=1;
  assert(charmap_history_limit(&o)==9);
  charmap_scroll_up(0,&o,1);
  assert(o.unrendered&&o.scrollback_pos==8&&charmap_history_limit(&o)==9);
  charmap_scroll_up(0,&o,1);
  assert(!o.unrendered&&o.scrollback_pos==9&&charmap_history_limit(&o)==9);
  while(o.top_of_scrollback)o.top_of_scrollback=charmap_dispose_line(o.top_of_scrollback);
 }
 /* Repaint can allocate/trim while viewing history; saved offsets track it. */
 { Object o={0};struct charmap_line *nodes[12],*p=0;
  for(int i=0;i<12;i++)nodes[i]=p=charmap_newline(0,p);
  o.top_of_scrollback=nodes[0];o.top_of_window=nodes[7];o.scrollback_pos=7;
  o.scrollback_size=12;o.scrollback_max=2;o.saved_scrollback_pos=9;o.unrendered=1;
  assert(charmapcon_find_line(0,&o,4)==nodes[11]);
  assert(o.scrollback_pos==0&&o.saved_scrollback_pos==2);
  charmap_scroll_up(0,&o,1);assert(o.unrendered&&charmap_history_limit(&o)==2);
  charmap_scroll_up(0,&o,1);assert(!o.unrendered&&o.top_of_window==nodes[9]);
  while(o.top_of_scrollback)o.top_of_scrollback=charmap_dispose_line(o.top_of_scrollback);
 }
 return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';b=Path(d)/'test';p.write_text(source)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(b)],check=True)
 subprocess.run([str(b)],check=True)
print('Trim selection PASS: 144 endpoint pairs, forward/reversed/collapsed/live selections; live endpoint with 0/1/2 unallocated rows')
