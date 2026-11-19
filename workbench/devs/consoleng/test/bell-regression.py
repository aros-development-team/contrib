#!/usr/bin/env python3
"""Native BEL schedules a pointer-free signal and flashes only after unlock."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
s=(r/'stdconclass.c').read_text();a=s.index('    if (msg->Command == C_BELL)');b=s.index('    if (msg->Command == C_SCREEN_ALIGNMENT)',a);request=s[a:b]
assert a<s.index('    if (CON_IS_OBSCURED(o))',a)
s=(r/'consoletask.c').read_text();a=s.index('        ReleaseSemaphore(&ConsoleDevice->consoleTaskLock);\n\n        /* Native visual bell');b=s.index('    } /* forever */',a);delivery=s[a:b]
assert 'SIGBREAKF_CTRL_D |\n        SIGBREAKF_CTRL_E' in s
code=r'''
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#define C_BELL 7
#define C_NIL 0
#define SIGBREAKF_CTRL_E 16
struct Unit {int bellDisabled;};static struct Unit unit;
#define ICU(o) (&unit)
struct Task {int x;};struct Base {struct Task *consoleTask;int consoleTaskLock;};
static unsigned pending;static int flashes,locked;
static void Signal(struct Task *t,unsigned mask){assert(t);pending|=mask;}
static void DisplayBeep(void *screen){assert(!locked);assert(!screen);flashes++;}
static void ReleaseSemaphore(int *lock){assert(locked);locked=0;}
static void request(struct Base *ConsoleDevice,int command){struct {int Command;} m={command},*msg=&m;
'''+request+r'''
}
static void wake(struct Base *ConsoleDevice,unsigned wakeupsig){locked=1;
'''+delivery+r'''
}
int main(void){struct Task t;struct Base b={&t,0};int cases=0;
 for(unsigned n=0;n<1024;n++){
 pending=0;flashes=0;locked=1;
 for(unsigned i=0;i<n;i++)request(&b,C_BELL);
 assert(locked && flashes==0 && pending==(n?16:0));
 unsigned got=pending;pending=0;wake(&b,got);
 assert(flashes==!!n && !locked);wake(&b,0);assert(flashes==!!n);cases++;
 }
 pending=0;request(&b,C_NIL);assert(!pending);
 unit.bellDisabled=1;request(&b,C_BELL);assert(!pending);unit.bellDisabled=0;
 b.consoleTask=NULL;request(&b,C_BELL);assert(!pending);
 printf("BELL PASS %d coalescing/deferred-lock cases\n",cases);
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';exe=Path(d)/'test';p.write_text(code)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
 p.write_text(code.replace('Signal(ConsoleDevice->consoleTask, SIGBREAKF_CTRL_E);','DisplayBeep(NULL);'))
 subprocess.run(['cc',str(p),'-o',str(exe)],check=True)
 assert subprocess.run([str(exe)],capture_output=True).returncode!=0,'inline-lock negative control escaped'
 print('BELL inline locked call rejected')
