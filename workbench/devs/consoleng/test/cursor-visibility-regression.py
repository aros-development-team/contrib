#!/usr/bin/env python3
"""Cursor visibility state must be independent of hidden tab rendering."""
from pathlib import Path
import subprocess,tempfile
s=(Path(__file__).resolve().parents[1]/'stdconclass.c').read_text()
a=s.index('    if (msg->Command == C_CURSOR_VISIBLE ||');b=s.index('    if (msg->Command == C_SET_SYNC_OUTPUT)',a)
assert a<s.index('    if (CON_IS_OBSCURED(o))',a)
code=r'''
#include <assert.h>
#include <stdio.h>
typedef int BOOL;
#define TRUE 1
#define FALSE 0
#define C_CURSOR_VISIBLE 1
#define C_CURSOR_INVISIBLE 2
struct state {int cursorvisible,rendercursorcount,hidden,painted,toggles;};
static void render(struct state *o){o->rendercursorcount++;if(o->cursorvisible&&o->rendercursorcount==1&&!o->hidden){o->painted=!o->painted;o->toggles++;}}
static void unrender(struct state *o){o->rendercursorcount--;if(o->painted&&o->rendercursorcount==0&&!o->hidden){o->painted=!o->painted;o->toggles++;}}
#define Console_RenderCursor(o) render(o)
#define Console_UnRenderCursor(o) unrender(o)
static void run(struct state *o,int command){struct state *data=o;struct {int Command;} m={command},*msg=&m;
'''+s[a:b]+r'''
}
int main(void){int count=0;for(int hidden=0;hidden<2;hidden++)for(int old=0;old<2;old++)for(int next=0;next<2;next++)for(int nesting=-2;nesting<4;nesting++){
 struct state o={old,nesting,hidden,0,0};if(!hidden&&old&&nesting==1)o.painted=1;
 run(&o,next?1:2);assert(o.cursorvisible==next&&o.rendercursorcount==nesting);
 if(hidden)assert(!o.painted&&!o.toggles);
 int changes=o.toggles;run(&o,next?1:2);assert(o.toggles==changes&&o.rendercursorcount==nesting);
 count++;
}printf("Cursor visibility PASS: %d state/hidden/nesting cases and idempotent repeats\n",count);}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'t.c';b=Path(d)/'t'
 for negative in [False,True]:
  p.write_text(code.replace('BOOL visible =','if(o->hidden){return;} BOOL visible =') if negative else code)
  subprocess.run(['cc','-Wall','-Werror',str(p),'-o',str(b)],check=True)
  result=subprocess.run([str(b)],capture_output=True);assert bool(result.returncode)==negative,result.stderr.decode()
  if not negative:print(result.stdout.decode(),end='')
print('Hidden-state bypass negative control rejected')
