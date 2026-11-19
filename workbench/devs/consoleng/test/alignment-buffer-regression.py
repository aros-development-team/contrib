#!/usr/bin/env python3
"""Actual DECALN screen splice preserves history/tail and is atomic on OOM."""
from pathlib import Path
from hyperlink_fixture import hyperlink_source
import subprocess,tempfile
s=(Path(__file__).resolve().parents[1]/'charmapconclass.c').read_text()
a=s.index('struct charmapcondata\n');fields=s[a:s.index('\n};',a)+3]
a=s.index('static BOOL charmap_alignment(');f=s[a:s.index('static VOID charmapcon_docommand(',a)]
code=r'''
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
typedef void VOID;typedef int BOOL;typedef unsigned long ULONG;typedef long LONG;typedef short WORD;typedef signed char BYTE;typedef unsigned char UBYTE;typedef void*APTR;typedef void Class;
#define FALSE 0
#define TRUE 1
struct charmap_line {struct charmap_line*next,*prev;ULONG size,text[5];BYTE flags[5];ULONG *underlineColors;};
'''+hyperlink_source()+fields+r'''
typedef struct {struct charmapcondata data;int hidden;} Object;
#define INST_DATA(cl,o) (&(o)->data)
#define CHAR_YMAX(o) 3
#define CON_IS_OBSCURED(o) ((o)->hidden)
#define charmapcon_clear_selection(cl,o) ((void)0)
static int fail,freed;
static struct charmap_line*rows(int n){struct charmap_line*h=NULL,*tail=NULL;while(n--){struct charmap_line*p=calloc(1,sizeof(*p));p->size=5;for(int x=0;x<5;x++){p->text[x]='X';p->flags[x]=32;}p->prev=tail;if(tail)tail->next=p;else h=p;tail=p;}return h;}
static struct charmap_line*charmap_reset_grid(Class*c,Object*o){return fail?NULL:rows(4);}
static void charmap_dispose_lines(struct charmap_line*p){while(p){struct charmap_line*n=p->next;free(p);freed++;p=n;}}
'''+f+r'''
int main(void){int tests=0;for(int history=0;history<4;history++)for(int visible=0;visible<7;visible++)for(int hidden=0;hidden<2;hidden++)for(fail=0;fail<2;fail++){
 if(!visible&&history)continue;
 Object o={0};o.hidden=hidden;struct charmapcondata*d=&o.data;
 d->top_of_scrollback=rows(history+visible);struct charmap_line*first=d->top_of_scrollback,*before=NULL;
 for(int n=0;n<history;n++){before=first;first=first->next;}
 d->top_of_window=d->saved_top_of_window=first;d->scrollback_size=history+visible;d->scrollback_pos=d->saved_scrollback_pos=history;
 d->select_line_min=d->select_line_max=first;d->active_selection=1;d->scrollback_max=1000;
 struct charmap_line*after=first;for(int n=0;n<4&&after;n++)after=after->next;
 Object old=o;freed=0;BOOL result=charmap_alignment(NULL,&o);assert(result==!fail);
 if(fail)assert(!memcmp(&old,&o,sizeof(o))&&!freed);
 else{assert(freed==(visible<4?visible:4)&&d->scrollback_size==(ULONG)(history+(visible<4?4:visible))&&d->scrollback_max==1000&&d->scrollback_pos==(ULONG)history);
  struct charmap_line*p=d->top_of_window;assert(p&&p->prev==before&&(!before||before->next==p));
  assert(d->saved_top_of_window==p&&d->select_line_min==p&&d->select_line_max==p&&!d->active_selection);
  for(int y=0;y<4;y++){for(int x=0;x<5;x++)assert(p->text[x]=='E'&&!p->flags[x]);if(y==3)assert(p->next==after&&(!after||after->prev==p));p=p->next;}
 }
 charmap_dispose_lines(d->top_of_scrollback);tests++;
}printf("DECALN buffers PASS: %d history/short-screen/tail/hidden/OOM cases\n",tests);}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'t.c';b=Path(d)/'t';p.write_text(code)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(b)],check=True);subprocess.run([str(b)],check=True)
