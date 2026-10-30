#!/usr/bin/env python3
"""Execute actual saved-history removal under ASan/UBSan."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'charmapconclass.c').read_text()
f='static VOID charmap_clear_history('+s.split('static VOID charmap_clear_history(',1)[1].split('static VOID charmap_erase_display(',1)[0]
prefix=r'''
/* This fixture has no plain wide grid; standard-width-regression and native edits cover it. */
#define stdcon_width_edit(d,o,x,y,n,op) ((void)(x),(void)(y),(void)(n),(void)(op))
#define CHAR_XMAX(o) 9
#define CHAR_YMAX(o) 2
#include <assert.h>
#include <stdlib.h>
typedef void VOID;typedef void Class;
#define FALSE 0
#define TRUE 1
typedef int BOOL;
struct charmap_line {struct charmap_line *next,*prev;int value;unsigned long *underlineColors;};
struct charmapcondata {
 struct charmap_line *top_of_window,*top_of_scrollback,*saved_top_of_window,*select_line_min,*select_line_max;
 unsigned scrollback_size,scrollback_pos,saved_scrollback_pos;
 int select_x_min,select_x_max,select_y_min,select_y_max,active_selection,ignore_drag;
};
typedef struct {struct charmapcondata data;int hidden;} Object;
static int freed,cleared;
#define INST_DATA(cl,o) (&(o)->data)
#define CON_IS_OBSCURED(o) ((o)->hidden)
#define charmapcon_clear_selection(cl,o) (cleared++)
static struct charmap_line *charmap_dispose_line(struct charmap_line *p) {
 struct charmap_line *next=p->next;free(p);freed++;return next;
}
'''
raster=(r/'stdconclass.c').read_text().split('    case C_ERASE_IN_DISPLAY:',1)[1].split('    case C_INSERT_CHAR:',1)[0]
prefix += r"""
typedef short WORD;typedef unsigned char UBYTE;typedef long LONG;typedef long IPTR;
static struct {int cu_BgPen,cu_XROrigin,cu_YROrigin,cu_XRExtant,cu_YRExtant;} geometry={6,0,0,79,47};
struct rp {UBYTE FgPen;};static int draws;
#define CU(o) (&geometry)
#define XCP 1
#define YCP 1
#define CHAR_YMIN(o) 0
#define GFX_X(o,x) ((x)*8)
#define GFX_Y(o,y) ((y)*16)
#define Console_UnRenderCursor(o) ((void)0)
#define Console_RenderCursor(o) ((void)0)
#define SetAPen(rp,p) ((rp)->FgPen=(p))
#define SetDrMd(rp,m) ((void)0)
#define JAM2 1
#define RectFill(rp,a,b,c,d) (draws++)
"""
f += 'static void paint(Object *o, IPTR *params, struct rp *rp) {switch(0){case 0:'+raster+'}}\n'
main=r'''
int main(void) {
 for(int selection=0;selection<4;selection++)for(int hidden=0;hidden<2;hidden++)for(int history=0;history<21;history++)for(int visible=1;visible<6;visible++) {
  Object o={0};o.hidden=hidden;struct charmapcondata *d=&o.data;struct charmap_line *prev=0;
  for(int i=0;i<history+visible;i++) {
   struct charmap_line *p=calloc(1,sizeof *p);assert(p);p->prev=prev;p->value=i;
   if(prev)prev->next=p;else d->top_of_scrollback=p;
   if(i==history)d->top_of_window=p;
   prev=p;
  }
  d->scrollback_size=history+visible;d->scrollback_pos=history;d->saved_scrollback_pos=history;
  d->saved_top_of_window=d->top_of_window;d->select_line_min=d->select_line_max=d->top_of_scrollback;
  if(selection==1)d->select_line_min=d->select_line_max=d->top_of_window;
  if(selection==2)d->select_line_max=d->top_of_window;
  if(selection==3)d->select_line_min=d->top_of_window;
  d->select_x_max=1;d->active_selection=1;freed=cleared=0;
  struct charmap_line *live=d->top_of_window;
  charmap_clear_history(0,&o);
  assert(freed==history&&d->scrollback_size==visible&&d->scrollback_pos==0);
  assert(d->top_of_scrollback==live&&d->top_of_window==live&&live->prev==0);
  assert(d->saved_top_of_window==live&&d->saved_scrollback_pos==0);
  if(history&&selection!=1){assert(!d->active_selection&&d->select_line_min==live&&d->select_line_max==live);assert(cleared==!hidden);}
  else assert(cleared==0&&d->active_selection==1);
  int n=0;for(struct charmap_line *p=live;p;p=p->next){assert(p->value==history+n);n++;}assert(n==visible);
  charmap_clear_history(0,&o);assert(freed==history);
  while(live)live=charmap_dispose_line(live);
 }
 Object empty={0};charmap_clear_history(0,&empty);
 for(IPTR mode=0;mode<4;mode++) {
  struct rp rp={9};draws=0;paint(&empty,&mode,&rp);
  assert(draws==(mode==3?0:mode==2?1:2)&&rp.FgPen==9);
 }
 return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';b=Path(d)/'test';p.write_text(prefix+f+main)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(b)],check=True)
 subprocess.run([str(b)],check=True)
print('History PASS: 840 list/visibility/selection cases, preserved viewport, safe selection reset, repeated clear and empty buffer')
