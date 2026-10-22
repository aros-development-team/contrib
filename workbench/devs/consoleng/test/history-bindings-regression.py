#!/usr/bin/env python3
"""Execute production history-action bounds independently of raster rendering."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'charmapconclass.c').read_text()
f=s[s.index('static BOOL charmap_history_action('):s.index('/* Cell editing may cut')]
h=(r/'include/consoleng.h').read_text();h=h[h.index('#define CONSOLENG_BIND_LINEUP'):h.index('struct ConsolengKeyBinding')]
prefix=r'''
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
typedef uint32_t ULONG;typedef int BOOL;typedef void Class;
#define TRUE 1
#define FALSE 0
struct charmapcondata {int alternate,hidden;ULONG scrollback_pos,limit,rows;};
typedef struct charmapcondata Object;
#define INST_DATA(c,o) (o)
#define CON_IS_OBSCURED(o) ((o)->hidden)
#define CHAR_YMAX(o) ((o)->rows-1)
static unsigned moves,adjustments;
static BOOL charmap_history_clear_action(Class*c,Object*o){return TRUE;}
static BOOL charmap_selection_clear_action(Class*c,Object*o){return TRUE;}
static ULONG charmap_history_limit(Object*o){return o->limit;}
static void charmapcon_scroll_to(Class*c,Object*o,ULONG y){assert(y<=o->limit);o->scrollback_pos=y;moves++;}
static void charmapcon_adj_prop(Class*c,Object*o){adjustments++;}
'''
main=r'''
int main(void){unsigned checks=0;
 for(unsigned limit=0;limit<120;limit++)for(unsigned pos=0;pos<=limit;pos++)for(unsigned rows=1;rows<=50;rows+=7)for(unsigned action=4;action<=18;action++){
  Object o={0,0,pos,limit,rows};moves=adjustments=0;
  unsigned expect=pos,step=(action==7||action==8)?rows:(action==17||action==18)?rows/2:1;
  if(action==5||action==7||action==17)expect=pos<step?0:pos-step;
  if(action==6||action==8||action==18)expect=limit-pos<step?limit:pos+step;
  if(action==9)expect=0;if(action==10)expect=limit;
  int valid=(action>=5&&action<=10)||action==17||action==18;
  if(action==13)continue;
  assert(charmap_history_action(0,&o,action)==valid);
  assert(o.scrollback_pos==expect&&moves==(unsigned)valid&&adjustments==(unsigned)valid);
  for(int hidden=0;hidden<2;hidden++) {o=(Object){!hidden,hidden,pos,limit,rows};moves=adjustments=0;
   assert(charmap_history_action(0,&o,action)==valid);assert(o.scrollback_pos==pos&&!moves&&!adjustments);}
  checks++;
 }
 printf("HISTORY_BINDINGS_PASS %u bounds/action cases plus alternate/hidden isolation\n",checks);
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';b=Path(d)/'test';p.write_text(prefix+h+f+main)
 subprocess.run(['cc','-Wall','-Werror','-Wno-misleading-indentation','-fsanitize=address,undefined',str(p),'-o',str(b)],check=True);subprocess.run([str(b)],check=True)
 broken=f.replace('pos = 0; break;', 'pos = limit; break;');assert broken!=f
 p.write_text(prefix+h+broken+main);subprocess.run(['cc','-Wall','-Werror','-Wno-misleading-indentation',str(p),'-o',str(b)],check=True)
 assert subprocess.run([str(b)],capture_output=True).returncode!=0
 print('Negative control PASS: wrong history endpoint rejected')
 broken=f.replace('step = ((ULONG)CHAR_YMAX(o) + 1) / 2;', 'step = ((ULONG)CHAR_YMAX(o) + 1) / 2 + 1;');assert broken!=f
 p.write_text(prefix+h+broken+main);subprocess.run(['cc','-Wall','-Werror','-Wno-misleading-indentation',str(p),'-o',str(b)],check=True)
 assert subprocess.run([str(b)],capture_output=True).returncode!=0
 print('Negative control PASS: rounded-up half page rejected')
