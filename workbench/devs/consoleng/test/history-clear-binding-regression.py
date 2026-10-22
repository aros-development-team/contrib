#!/usr/bin/env python3
"""Execute production clear action, live restoration and list disposal."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1];s=(root/'charmapconclass.c').read_text()
clear=s[s.index('static VOID charmap_clear_history('):s.index('static VOID charmap_erase_display(')]
restore=s[s.index('static BOOL charmap_restore_live('):s.index('/* Allocate before touching primary state')]
prefix=r'''
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
typedef void VOID;typedef void Class;typedef int BOOL;typedef long LONG;
#define TRUE 1
#define FALSE 0
struct charmap_line {struct charmap_line *next,*prev;unsigned value;};
struct charmapcondata {
 struct charmap_line *top_of_window,*top_of_scrollback,*saved_top_of_window,*select_line_min,*select_line_max;
 unsigned scrollback_size,scrollback_pos,saved_scrollback_pos;
 int select_x_min,select_x_max,select_y_min,select_y_max,active_selection,ignore_drag,alternate,unrendered;
};
typedef struct {struct charmapcondata data;int hidden,search,cursor_x,cursor_y;} Object;
#define INST_DATA(c,o) (&(o)->data)
#define CON_IS_OBSCURED(o) ((o)->hidden)
static unsigned freed,closed,refreshed,adjusted,hidden_cursor,shown_cursor,selection;
static Object *owner;
static void charmap_search_close(Class*c,Object*o,BOOL redraw){assert(!redraw);o->search=0;closed++;}
static struct charmap_line *charmap_dispose_line(struct charmap_line*p){assert(!owner->search);struct charmap_line*n=p->next;free(p);freed++;return n;}
static void charmapcon_clear_selection(Class*c,Object*o){selection++;}
static void charmapcon_adj_prop(Class*c,Object*o){adjusted++;}
static void charmapcon_refresh(Class*c,Object*o,LONG off){assert(!off);refreshed++;}
#define Console_UnRenderCursor(o) (hidden_cursor++)
#define Console_RenderCursor(o) (shown_cursor++)
'''
main=r'''
int main(void){unsigned cases=0;
 for(unsigned history=0;history<21;history++)for(int browsing=0;browsing<2;browsing++)for(int mode=0;mode<3;mode++)for(int selected=0;selected<2;selected++){
  Object o={0};owner=&o;o.hidden=mode==1;o.data.alternate=mode==2;o.search=1;o.cursor_x=4;o.cursor_y=2;
  struct charmapcondata*d=&o.data;struct charmap_line*tail=NULL,*live=NULL;
  for(unsigned i=0;i<history+3;i++){
   struct charmap_line*p=calloc(1,sizeof(*p));assert(p);p->value=i;p->prev=tail;if(tail)tail->next=p;else d->top_of_scrollback=p;tail=p;
   if(i==history)live=p;
  }
  d->top_of_window=browsing?d->top_of_scrollback:live;d->saved_top_of_window=live;
  d->scrollback_size=history+3;d->scrollback_pos=browsing?0:history;d->saved_scrollback_pos=history;d->unrendered=browsing;
  d->active_selection=1;d->select_line_min=d->select_line_max=selected?d->top_of_scrollback:live;
  struct charmapcondata before=*d;freed=closed=refreshed=adjusted=hidden_cursor=shown_cursor=selection=0;
  assert(charmap_history_clear_action(NULL,&o));
  if(mode){assert(!memcmp(&before,d,sizeof(before)));assert(o.search&&!freed&&!closed&&!refreshed&&!adjusted&&!hidden_cursor&&!shown_cursor);}
  else{
   assert(!o.search&&closed==1&&freed==history&&refreshed==1&&adjusted==1&&hidden_cursor==1&&shown_cursor==1);
   assert(d->top_of_window==live&&d->top_of_scrollback==live&&!live->prev&&!d->scrollback_pos&&!d->unrendered);
   assert(d->scrollback_size==3&&live->value==history&&live->next->value==history+1&&live->next->next->value==history+2);
   assert(o.cursor_x==4&&o.cursor_y==2);
   assert(d->active_selection==!(selected&&history));
   assert(charmap_history_clear_action(NULL,&o));assert(freed==history);
  }
  struct charmap_line*p=d->top_of_scrollback;while(p){struct charmap_line*n=p->next;free(p);p=n;}
  cases++;
 }
 printf("History clear binding PASS: %u live/history/hidden/alternate/selection cases; cursor and visible cells preserved\n",cases);
}
'''
with tempfile.TemporaryDirectory() as temp:
    p=Path(temp);src=p/'test.c';binary=p/'test'
    src.write_text(prefix+clear+restore+main)
    subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(src),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
    broken=restore.replace('    charmap_restore_live(data);','    (void)data;')
    assert broken!=restore
    src.write_text(prefix+clear+broken+main)
    subprocess.run(['cc','-Wall','-Werror','-Wno-unused-function',str(src),'-o',str(binary)],check=True)
    assert subprocess.run([str(binary)],capture_output=True).returncode!=0
    print('Negative control PASS: restore live screen before clearing history')
