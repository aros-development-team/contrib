#!/usr/bin/env python3
"""Actual input-unit pin across a concurrent close/list removal attempt."""
from pathlib import Path
import tempfile,subprocess,resource
r=Path(__file__).resolve().parents[1];s=(r/'cdinputhandler.c').read_text();a=s.index('static Object *obtainconunit(',s.index('/***********************'));b=s.index('/****************\n** initCDIH()',a);body=s[a:b]
close=(r/'console.c').read_text();close=close[close.index('static int GM_UNIQUENAME(Close)'):close.index('AROS_LH1(void, beginio')]
assert close.index('ObtainSemaphore(&ConsoleDevice->consoleTaskLock)') < close.index('ObtainSemaphore(&ConsoleDevice->unitListLock)') < close.index('DisposeObject(') < close.index('ReleaseSemaphore(&ConsoleDevice->consoleTaskLock)')
code=r'''
#define _XOPEN_SOURCE 700
#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <errno.h>
typedef void VOID;typedef void *APTR;typedef unsigned long ULONG;
#define CF_OBSCURED 8
#define D(x)
#define ReturnPtr(name,type,value) return value
struct Window {int id;};
struct Object {struct Object *next;struct Window *cu_Window;int conFlags;};typedef struct Object Object;
struct IntuitionBase {struct Window *ActiveWindow;};
struct ConsoleBase {struct IntuitionBase *cb_IntuitionBase;pthread_rwlock_t unitListLock;struct {Object *mlh_Head;} unitList;};
#define CU(o) (o)
#define ICU(o) (o)
static void ObtainSemaphoreShared(pthread_rwlock_t *p){assert(!pthread_rwlock_rdlock(p));}
static void ReleaseSemaphore(pthread_rwlock_t *p){assert(!pthread_rwlock_unlock(p));}
static ULONG LockIBase(ULONG n){return n;}
static void UnlockIBase(ULONG n){(void)n;}
static Object *NextObject(Object **p){Object *o=*p;if(o)*p=o->next;return o;}
'''+body+r'''
static pthread_barrier_t ready,done;static struct ConsoleBase device;static Object unit;static struct Window window;
static void *dispatch(void *arg){(void)arg;Object *o=obtainconunit(&window,&device);assert(o==&unit);pthread_barrier_wait(&ready);pthread_barrier_wait(&done);assert(o->cu_Window==&window);releaseconunit(o,&device);return NULL;}
int main(void){
 struct IntuitionBase intuition={&window};device.cb_IntuitionBase=&intuition;pthread_rwlock_init(&device.unitListLock,NULL);
 pthread_barrier_init(&ready,NULL,2);pthread_barrier_init(&done,NULL,2);
 for(int i=0;i<1000;i++){
  unit=(Object){NULL,&window,0};device.unitList.mlh_Head=&unit;pthread_t thread;assert(!pthread_create(&thread,NULL,dispatch,NULL));
  pthread_barrier_wait(&ready);assert(pthread_rwlock_trywrlock(&device.unitListLock)==EBUSY);pthread_barrier_wait(&done);pthread_join(thread,NULL);
  assert(!pthread_rwlock_trywrlock(&device.unitListLock));device.unitList.mlh_Head=NULL;ReleaseSemaphore(&device.unitListLock);
  assert(!obtainconunit(&window,&device));assert(!pthread_rwlock_trywrlock(&device.unitListLock));ReleaseSemaphore(&device.unitListLock);
 }
 unit.conFlags=CF_OBSCURED;device.unitList.mlh_Head=&unit;assert(!obtainconunit(NULL,&device));assert(!pthread_rwlock_trywrlock(&device.unitListLock));ReleaseSemaphore(&device.unitListLock);
 pthread_barrier_destroy(&ready);pthread_barrier_destroy(&done);pthread_rwlock_destroy(&device.unitListLock);return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(code)
 subprocess.run(['cc','-pthread','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
 # The pre-fix unpin-before-queue behavior must fail the same concurrency check.
 negative=code.replace('    if (!o)\n        ReleaseSemaphore', '    ReleaseSemaphore', 1)
 assert negative!=code
 (p/'negative.c').write_text(negative)
 subprocess.run(['cc','-pthread','-Wall','-Wextra','-Werror',str(p/'negative.c'),'-o',str(p/'negative')],check=True)
 def no_core():resource.setrlimit(resource.RLIMIT_CORE,(0,0))
 result=subprocess.run([str(p/'negative')],capture_output=True,timeout=10,preexec_fn=no_core)
 assert result.returncode!=0 and b'EBUSY' in result.stderr
 print('NEGATIVE CONTROL: premature unit unlock rejected:',result.stderr.decode().strip())
print('PASS: 1000 concurrent input pins exclude list removal; missing/hidden units unlock; close lock order')
