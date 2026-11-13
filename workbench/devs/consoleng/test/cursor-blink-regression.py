#!/usr/bin/env python3
"""Actual cursor state methods: timer, nesting, focus, obscuring and overwrite."""
from pathlib import Path
import subprocess,tempfile
s=(Path(__file__).resolve().parents[1]/'stdconclass.c').read_text()
def function(name):
 a=s.index(name);a=s.rfind('\n',0,a)+1;b=s.index('{',a);d=1;c=b+1
 while d:d+=(s[c]=='{')-(s[c]=='}');c+=1
 return s[a:c]
f=function('static UBYTE stdcon_blink_style(')+'\n'+'\n'.join(function(n) for n in ['static VOID stdcon_rendercursor(', 'static VOID stdcon_unrendercursor(', 'static VOID stdcon_setactive(', 'static void stdcon_overwritecursor('])
prefix=r'''
#include <assert.h>
#include <stdint.h>
typedef unsigned char UBYTE;typedef void VOID;typedef void Class;typedef int BOOL;typedef uint32_t ULONG;
#define TRUE 1
#define FALSE 0
#define IECLASS_RAWKEY 4
#define IECLASS_TIMER 1
#define IECLASS_ACTIVEWINDOW 2
#define IECLASS_INACTIVEWINDOW 3
struct stdcondata {int cursorvisible,rendercursorcount,windowactive,cursorstyle,cursorpainted,cursorTimerReady;ULONG cursorStamp,cursorInterval,cursorTimeout,cursorStart;int draws;};
typedef struct {struct stdcondata data;int hidden;} Object;
struct InputEvent {int ie_Class;struct {ULONG tv_secs,tv_micro;} ie_TimeStamp;};
struct P_Console_HandleGadgets {struct InputEvent *Event;};
struct P_Console_RenderCursor {int unused;};struct P_Console_UnRenderCursor {int unused;};
#define INST_DATA(cl,o) (&(o)->data)
#define CON_IS_OBSCURED(o) ((o)->hidden)
static void stdcon_drawcursor(Object *o,struct stdcondata *d){assert(!o->hidden);d->cursorpainted=!d->cursorpainted;d->draws++;}
'''
main=r'''
static void event(Object *o,int type,ULONG ms){struct InputEvent e={type,{ms/1000,(ms%1000)*1000}};struct P_Console_HandleGadgets m={&e};stdcon_setactive(0,o,&m);}
int main(void){
 for(int style=0;style<=6;style++){
  assert(stdcon_blink_style(style,1)==(style?((style-1)/2)*2+1:1));
  assert(stdcon_blink_style(style,0)==(style?((style-1)/2)*2+2:2));
  Object o={0};o.data.cursorInterval=500;o.data.cursorstyle=style;o.data.cursorvisible=1;o.data.windowactive=1;
  stdcon_rendercursor(0,&o,0);assert(o.data.cursorpainted&&o.data.draws==1);
  event(&o,IECLASS_TIMER,100);event(&o,IECLASS_TIMER,599);assert(o.data.draws==1);
  event(&o,IECLASS_TIMER,600);assert(o.data.cursorpainted==!(style&1));
  if(style&1){
   event(&o,IECLASS_RAWKEY,601);assert(o.data.cursorpainted&&!o.data.cursorTimerReady);
   event(&o,IECLASS_TIMER,700);event(&o,IECLASS_TIMER,1199);assert(o.data.cursorpainted);
   event(&o,IECLASS_TIMER,1200);assert(!o.data.cursorpainted);
   /* Unrender while already blink-hidden must not invert a ghost cursor. */
   int draws=o.data.draws;stdcon_unrendercursor(0,&o,0);assert(o.data.draws==draws);
   stdcon_rendercursor(0,&o,0);assert(o.data.cursorpainted&&o.data.draws==draws+1);
   event(&o,IECLASS_TIMER,1000);event(&o,IECLASS_TIMER,1500);assert(!o.data.cursorpainted);
   event(&o,IECLASS_INACTIVEWINDOW,1501);assert(o.data.cursorpainted&&!o.data.windowactive);
   draws=o.data.draws;event(&o,IECLASS_TIMER,2500);assert(o.data.draws==draws);
   event(&o,IECLASS_ACTIVEWINDOW,2501);assert(o.data.windowactive&&o.data.cursorpainted);
   /* Millisecond counter rollover and repeated equal timestamps. */
   event(&o,IECLASS_TIMER,UINT32_MAX-200);event(&o,IECLASS_TIMER,299);assert(!o.data.cursorpainted);
   draws=o.data.draws;event(&o,IECLASS_TIMER,299);assert(o.data.draws==draws);
  }
  stdcon_unrendercursor(0,&o,0);assert(!o.data.cursorpainted);
  o.hidden=1;stdcon_rendercursor(0,&o,0);int draws=o.data.draws;
  event(&o,IECLASS_TIMER,10000);assert(o.data.draws==draws);
  o.hidden=0;stdcon_unrendercursor(0,&o,0);stdcon_rendercursor(0,&o,0);
  assert(o.data.cursorpainted);
  stdcon_overwritecursor(&o.data);assert(!o.data.cursorpainted&&o.data.rendercursorcount==0);
  stdcon_rendercursor(0,&o,0);assert(o.data.cursorpainted);
  stdcon_unrendercursor(0,&o,0);stdcon_unrendercursor(0,&o,0);
  draws=o.data.draws;stdcon_rendercursor(0,&o,0);assert(o.data.draws==draws);
  stdcon_rendercursor(0,&o,0);assert(o.data.cursorpainted);
  o.data.cursorvisible=0;draws=o.data.draws;event(&o,IECLASS_TIMER,20000);assert(o.data.draws==draws);
 }
 /* Configurable phase and idle stop; input restarts the idle lifetime. */
 Object timed={0};timed.data.cursorstyle=1;timed.data.cursorInterval=200;timed.data.cursorTimeout=1000;timed.data.cursorvisible=timed.data.windowactive=1;
 stdcon_rendercursor(0,&timed,0);event(&timed,IECLASS_TIMER,100);
 event(&timed,IECLASS_TIMER,299);assert(timed.data.cursorpainted);
 event(&timed,IECLASS_TIMER,300);assert(!timed.data.cursorpainted);
 event(&timed,IECLASS_TIMER,1100);assert(timed.data.cursorpainted);
 int draws=timed.data.draws;event(&timed,IECLASS_TIMER,10000);assert(timed.data.draws==draws);
 event(&timed,IECLASS_RAWKEY,10001);event(&timed,IECLASS_TIMER,10002);event(&timed,IECLASS_TIMER,10202);assert(!timed.data.cursorpainted);
 return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';b=Path(d)/'test'
 for negative in [False,True]:
  body=f.replace('if (data->cursorpainted && (data->rendercursorcount == 0)','if (data->cursorvisible && (data->rendercursorcount == 0)') if negative else f
  p.write_text(prefix+body+main)
  subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(b)],check=True)
  result=subprocess.run([str(b)],capture_output=True)
  assert (result.returncode!=0)==negative,result.stderr.decode()
print('Cursor blink PASS: seven styles, nesting, off-phase redraw, rollover, focus, obscured, overwrite, raw input, DEC12 shape mapping and negative control')
