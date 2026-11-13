#!/usr/bin/env python3
"""Production select-all anchors, viewport coordinates, empty/hidden and repeat."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'charmapconclass.c').read_text();s=s[s.index('static BOOL charmapcon_select_all('):s.index('static struct charmap_line *charmap_row_at')]
prefix=r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
typedef uint32_t ULONG;typedef int32_t LONG;typedef int BOOL;
#define TRUE 1
#define FALSE 0
struct charmap_line{struct charmap_line*next;ULONG size,columns;};
struct charmapcondata{struct charmap_line*top_of_scrollback,*select_line_min,*select_line_max;ULONG scrollback_pos;LONG select_x_min,select_x_max,select_y_min,select_y_max;BOOL active_selection,ignore_drag;};
typedef struct charmapcondata Class;typedef struct {int hidden;} Object;
#define INST_DATA(c,o) (c)
#define CON_IS_OBSCURED(o) ((o)->hidden)
#define CHAR_YMIN(o) 0
#define CHAR_YMAX(o) 30
static int closes,clears,refreshes;
static ULONG charmap_copy_columns(struct charmap_line*l){return l->columns;}
static void charmap_search_close(Class*c,Object*o,int restore){(void)c;(void)o;assert(restore);closes++;}
static void charmapcon_clear_selection(Class*c,Object*o){(void)o;clears++;c->select_x_max=c->select_x_min;c->select_y_max=c->select_y_min;}
static void charmapcon_refresh_lines(Class*c,Object*o,int a,int b){(void)c;assert(!o->hidden&&a==0&&b==30);refreshes++;}
'''
main=r'''
int main(void){Object o={0};Class c={0};
assert(charmapcon_select_all(&c,&o)&&closes==1&&clears==1&&!refreshes);
struct charmap_line tail={0,80,0},last={&tail,80,5},mid={&last,80,0},head={&mid,80,80};c.top_of_scrollback=&head;
for(ULONG pos=0;pos<10000;pos+=7){c.scrollback_pos=pos;c.active_selection=c.ignore_drag=1;assert(charmapcon_select_all(&c,&o));assert(c.select_line_min==&head&&c.select_line_max==&last&&c.select_x_min==0&&c.select_x_max==80&&c.select_y_min==-(LONG)pos&&c.select_y_max==2-(LONG)pos&&!c.active_selection&&!c.ignore_drag);}
int before=closes;o.hidden=1;assert(!charmapcon_select_all(&c,&o)&&closes==before);o.hidden=0;
head.columns=0;last.columns=0;before=refreshes;assert(charmapcon_select_all(&c,&o)&&refreshes==before&&c.select_x_max==c.select_x_min&&c.select_y_max==c.select_y_min);
puts("SELECT ALL PASS: retained history, interior blanks, trailing unused rows, viewport offsets, hidden/empty and repeated selection");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'t.c';e=Path(d)/'t';p.write_text(prefix+s+main)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(e)],check=True);subprocess.run([str(e)],check=True)
 p.write_text(prefix+s.replace('-(LONG)data->scrollback_pos','0')+main);subprocess.run(['cc',str(p),'-o',str(e)],check=True);assert subprocess.run([str(e)],capture_output=True).returncode!=0
 print('Negative control rejected: absolute selection coordinates')
