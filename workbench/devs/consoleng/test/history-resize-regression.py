#!/usr/bin/env python3
"""Actual resize dispatch must restore live state before cursor/geometry adjustment."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'charmapconclass.c').read_text()
restore='static BOOL charmap_restore_live('+s.split('static BOOL charmap_restore_live(',1)[1].split('/* Keyboard clearing preserves',1)[0]
resize='static VOID charmapcon_newwindowsize('+s.split('static VOID charmapcon_newwindowsize(',1)[1].split('static VOID charmapcon_handlemouse(',1)[0]
source=r'''
#include <assert.h>
/* Search lifetime is covered by search-ui-regression.py. */
#define charmap_search_close(cl,o,redraw) ((void)0)
typedef unsigned long ULONG;typedef int BOOL;typedef short WORD;typedef long LONG;typedef void VOID;typedef void Class;typedef void *Msg;
#define TRUE 1
#define FALSE 0
struct charmap_line {int unused;ULONG *underlineColors;};
struct charmapcondata {struct charmap_line *top_of_window,*saved_top_of_window;unsigned long scrollback_pos,saved_scrollback_pos;int select_y_min,select_y_max,unrendered;};
struct P_Console_NewWindowSize {int new_max;};
typedef struct {struct charmapcondata data;int y,max,refresh,prop,cursor,scroll,hidden,conFlags;} Object;
static struct charmap_line live,history;
#define INST_DATA(cl,o) (&(o)->data)
#define YCP (o->y)
#define XCP 0
#define CHAR_XMAX(o) 79
#define ICU(o) (o)
#define CF_WRAP_PENDING 128
#define charmap_reflow_primary(cl,o,x,y,p) ((void)(x),(void)(y),(void)(p),FALSE)
#define CHAR_YMAX(o) ((o)->max)
#define D(x) ((void)0)
#define CON_IS_OBSCURED(o) ((o)->hidden)
static void DoSuperMethodA(Class *cl,Object *o,Msg msg) {
 assert(o->data.top_of_window==&live&&!o->data.unrendered);
 o->max=((struct P_Console_NewWindowSize*)msg)->new_max;if(o->y>o->max)o->y=o->max;
}
#define charmapcon_update_text_width(cl,o) ((void)0)
static void charmap_scroll_up(Class *cl,Object *o,int n) {assert(o->data.top_of_window==&live);o->data.scrollback_pos+=n;o->scroll+=n;}
#define charmapcon_refresh(cl,o,n) ((o)->refresh++)
#define charmapcon_adj_prop(cl,o) ((o)->prop++)
#define Console_RenderCursor(o) ((o)->cursor++)
'''+restore+resize+r'''
int main(void) {
 for(int hidden=0;hidden<2;hidden++)for(int browsing=0;browsing<2;browsing++)for(int oldy=0;oldy<30;oldy++)for(int height=1;height<35;height++) {
  Object o={0};o.hidden=hidden;o.y=oldy;o.max=29;
  o.data.top_of_window=browsing?&history:&live;o.data.saved_top_of_window=&live;
  o.data.scrollback_pos=browsing?10:100;o.data.saved_scrollback_pos=100;o.data.unrendered=browsing;
  o.data.select_y_min=2;o.data.select_y_max=4;
  struct P_Console_NewWindowSize msg={height-1};
  charmapcon_newwindowsize(0,&o,&msg);
  assert(!o.data.unrendered&&o.data.top_of_window==&live);
  int movement=oldy>height-1?oldy-height+1:0;
  assert(o.scroll==movement&&o.data.scrollback_pos==100+movement);
  assert(o.data.select_y_min==2-(browsing?90:0));
  assert(o.data.select_y_max==4-(browsing?90:0));
  assert(o.refresh==!hidden&&o.prop==!hidden&&o.cursor==(hidden?0:browsing));
  assert(!charmap_restore_live(&o.data));
 }
 return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';b=Path(d)/'test';p.write_text(source)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(b)],check=True)
 subprocess.run([str(b)],check=True)
print('History resize PASS: 4080 visible/hidden live/history, cursor and shrink/grow combinations')
