#!/usr/bin/env python3
"""Actual RIS retained-buffer transaction, including allocation failure."""
from pathlib import Path
from hyperlink_fixture import hyperlink_source
import subprocess,tempfile
s=(Path(__file__).resolve().parents[1]/'charmapconclass.c').read_text()
a=s.index('struct charmapcondata\n');struct=s[a:s.index('\n};',a)+3]
a=s.index('    if (msg->Command == C_TERMINAL_RESET)');body=s[a:s.index('    if (msg->Command == C_SET_HYPERLINK)',a)]
code=r'''
#include <assert.h>
/* Search lifetime is covered by search-ui-regression.py. */
#define charmap_search_close(cl,o,redraw) ((void)0)
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
typedef void VOID;typedef int BOOL;typedef unsigned long ULONG;typedef long LONG;typedef short WORD;typedef signed char BYTE;typedef unsigned char UBYTE;typedef void*APTR;typedef void*Msg;typedef void Class;
#define TRUE 1
#define FALSE 0
#define MEMF_ANY 0
#define MEMF_CLEAR 0
#define C_TERMINAL_RESET 99
struct charmap_line {int sentinel;ULONG *underlineColors;};
'''+hyperlink_source()+struct+r'''
typedef struct {struct charmapcondata data;int hidden,balance,reset,margin,prop;} Object;
struct request {int Command;};
static int fail,freed;
#define CHAR_YMAX(o) 24
#define charmap_reset_grid(cl,o) (fail?NULL:calloc(1,sizeof(struct charmap_line)))
static void charmap_dispose_lines(struct charmap_line*p){assert(p);freed++;free(p);}
#define Console_UnRenderCursor(o) ((o)->balance--)
#define Console_RenderCursor(o) ((o)->balance++)
#define CON_IS_OBSCURED(o) ((o)->hidden)
#define DoSuperMethodA(cl,o,msg) ((o)->reset++)
#define charmapcon_clear_margins(cl,o) ((o)->margin++)
#define charmapcon_adj_prop(cl,o) ((o)->prop++)
static void run(Object*o,struct request*msg){struct charmapcondata*data=&o->data;
'''+body+r'''
}
int main(void){for(int alt=0;alt<2;alt++)for(int hidden=0;hidden<2;hidden++)for(int browsed=0;browsed<2;browsed++)for(fail=0;fail<2;fail++){
 Object o={0};struct charmapcondata*d=&o.data;d->alternate=alt;d->unrendered=browsed;o.hidden=hidden;o.balance=-browsed;freed=0;
 d->top_of_scrollback=d->top_of_window=calloc(1,sizeof(struct charmap_line));
 if(alt)d->primary_head=d->primary_window=calloc(1,sizeof(struct charmap_line));
 d->scrollback_max=alt?0:123;d->primary_max=123;d->scrollback_size=50;d->scrollback_pos=4;
 d->saved_top_of_window=d->select_line_min=d->select_line_max=d->top_of_window;
 d->saved_scrollback_pos=7;d->active_selection=d->ignore_drag=1;
 assert(con_link_open(&d->link_pool,&d->current_link,(UBYTE*)";https://reset/",14));
 con_link_fill(&d->saved_link,1,d->current_link);
 if(alt)con_link_fill(&d->primary_link,1,d->current_link);
 Object before=o;struct request msg={99};run(&o,&msg);
 if(fail){assert(!memcmp(&o,&before,sizeof(o))&&!freed);}
 else{assert(freed==1+alt&&!d->alternate&&!d->unrendered&&!o.balance&&o.reset==1&&o.prop==1&&o.margin==!hidden);
 assert(!d->primary_head&&!d->primary_window&&d->scrollback_size==25&&d->scrollback_max==123&&!d->scrollback_pos&&!d->saved_scrollback_pos);
 assert(d->top_of_scrollback==d->top_of_window&&d->top_of_window==d->saved_top_of_window&&d->top_of_window==d->select_line_min&&d->top_of_window==d->select_line_max);
 assert(!d->active_selection&&!d->ignore_drag&&!d->link_pool.records&&!d->current_link&&!d->saved_link&&!d->primary_link);}
 con_link_fill(&d->current_link,1,NULL);con_link_fill(&d->saved_link,1,NULL);con_link_fill(&d->primary_link,1,NULL);
 free(d->top_of_scrollback);free(d->primary_head);
}puts("RIS buffers PASS: 16 alternate/hidden/browsed/OOM transactions; no dangling selection; history budget retained");}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'t.c';b=Path(d)/'t';p.write_text(code)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(b)],check=True);subprocess.run([str(b)],check=True)
