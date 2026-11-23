#!/usr/bin/env python3
"""Replay a queued event after the same address becomes a different unit."""
from pathlib import Path
import subprocess,tempfile,resource
r=Path(__file__).resolve().parents[1]
s=(r/'consoletask.c').read_text();a=s.index('static BOOL checkconunit(',s.index('/********** checkconunit()'));b=s.index('/******** answer_read_request()',a)
body=s[a:b]
assert 'checkconunit(cdihmsg->unit, cdihmsg->unitGeneration, ConsoleDevice)' in s
assert 'message->unitGeneration = ICU(unit)->inputGeneration' in (r/'cdinputhandler.c').read_text()
source=(r/'console.c').read_text();assert source.index('ICU(ioreq->io_Unit)->inputGeneration =') < source.index('add_msg.MethodID = OM_ADDTAIL')
code=r'''
#include <assert.h>
#include <stdint.h>
typedef int BOOL;typedef uint64_t UQUAD;
#define TRUE 1
#define FALSE 0
#define ICU(o) (o)
typedef struct Object {struct Object *next;UQUAD inputGeneration;} Object;
struct ConsoleBase {int unitListLock;struct {Object *mlh_Head;} unitList;};
static void ObtainSemaphoreShared(int *lock){assert(!*lock);*lock=1;}
static void ReleaseSemaphore(int *lock){assert(*lock);*lock=0;}
static Object *NextObject(Object **p){Object *o=*p;if(o)*p=o->next;return o;}
'''+body+r'''
int main(void){
 Object unit={0,1},other={0,7};struct ConsoleBase base={0,{&unit}};
 for(UQUAD generation=1;generation<100001;generation++){
  unit.inputGeneration=generation;assert(checkconunit(&unit,generation,&base));
  base.unitList.mlh_Head=0;assert(!checkconunit(&unit,generation,&base));
  unit.inputGeneration=generation+1;base.unitList.mlh_Head=&unit;
  assert(!checkconunit(&unit,generation,&base));
  assert(checkconunit(&unit,generation+1,&base));
  assert(!checkconunit(&other,7,&base));assert(!base.unitListLock);
 }
 return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(code)
 subprocess.run(['cc','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
 negative=code.replace('o == unit && ICU(o)->inputGeneration == generation','o == unit')
 (p/'negative.c').write_text(negative)
 subprocess.run(['cc','-Wall','-Wextra','-Wno-unused-parameter',str(p/'negative.c'),'-o',str(p/'negative')],check=True)
 def no_core():resource.setrlimit(resource.RLIMIT_CORE,(0,0))
 n=subprocess.run([str(p/'negative')],capture_output=True,timeout=10,preexec_fn=no_core)
 assert n.returncode!=0 and b'!checkconunit' in n.stderr
 print('NEGATIVE CONTROL: pointer-only identity accepts stale generation and is rejected')
print('PASS: 100000 same-address replacements reject stale events and accept current events')
