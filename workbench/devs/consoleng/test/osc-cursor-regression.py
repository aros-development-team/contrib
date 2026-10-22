#!/usr/bin/env python3
"""Actual cursor dispatch: shape-only updates preserve blink and preferences."""
from pathlib import Path
import subprocess,tempfile
s=(Path(__file__).resolve().parents[1]/'stdconclass.c').read_text()
a=s.index('    if (msg->Command == C_SET_CURSOR_STYLE ||');body=s[a:s.index('    if (msg->Command == C_SET_OSC_CLIPBOARD',a)]
code=r'''
#include <assert.h>
#include <string.h>
#include <stdio.h>
typedef int BOOL;typedef unsigned long ULONG;typedef unsigned char UBYTE;
#define C_SET_CURSOR_STYLE 1
#define C_SET_CURSOR_SHAPE 2
struct state {int cursorstyle,resetCursorStyle,interval,timeout,visible;};
struct request {int Command;};
static int draws;
#define Console_UnRenderCursor(o) (draws++)
#define Console_RenderCursor(o) (draws++)
static void run(struct state *data,int command,long value){struct request m={command},*msg=&m;long params[]={value};
'''+body+r'''
}
int main(void){
 for(int old=0;old<=6;old++)for(long shape=-1;shape<=7;shape++)for(int command=1;command<=2;command++){
  struct state state={old,5,350,8000,0},expected=state;draws=0;
  int valid=shape>=0&&shape<=(command==2?2:6);
  if(valid){static const int shapes[]={2,6,4};expected.cursorstyle=command==2?shapes[shape]-(old&1):shape;}
  run(&state,command,shape);
  assert(!memcmp(&state,&expected,sizeof(state))&&draws==(valid?2:0));
 }
 puts("OSC cursor dispatch PASS:126 old-style/selector/command cases, blink and preference preservation");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'t.c';b=Path(d)/'t';p.write_text(code)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(b)],check=True);subprocess.run([str(b)],check=True)
 p.write_text(code.replace('shapes[style] - (data->cursorstyle & 1)','shapes[style]'))
 subprocess.run(['cc','-Wall','-Werror',str(p),'-o',str(b)],check=True)
 assert subprocess.run([str(b)],capture_output=True).returncode!=0
 print('Negative control PASS: loss of blink state rejected')
