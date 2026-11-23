#!/usr/bin/env python3
"""Window resize/refresh targets must not follow keyboard focus."""
from pathlib import Path
import subprocess,tempfile
s=(Path(__file__).resolve().parents[1]/'cdinputhandler.c').read_text()
a=s.index('static struct Window *event_window(');f=s[a:s.index('static Object *obtainconunit(',a)]
assert 'unit = obtainconunit(event_window(ie), ConsoleDevice);' in s
code=r'''
#include <assert.h>
#include <stddef.h>
#define IECLASS_ACTIVEWINDOW 1
#define IECLASS_INACTIVEWINDOW 2
#define IECLASS_SIZEWINDOW 3
#define IECLASS_REFRESHWINDOW 4
struct Window {int unused;};
struct InputEvent {int ie_Class;void *ie_EventAddress;};
'''+f+r'''
int main(void){struct Window target={0};
 for(int cls=0;cls<64;cls++)for(int addr=0;addr<2;addr++){
  struct InputEvent e={cls,addr?&target:NULL};
  assert(event_window(&e)==((cls>=1&&cls<=4&&addr)?&target:NULL));
 }return 0;}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)
 for old in (False,True):
  src=code.replace('    case IECLASS_SIZEWINDOW:\n    case IECLASS_REFRESHWINDOW:\n','') if old else code
  (p/'test.c').write_text(src)
  subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
  result=subprocess.run([str(p/'test')],capture_output=True)
  assert bool(result.returncode)==old,result.stderr.decode()
print('PASS: 128 window-event routing cases; former active-window resize routing rejected')
