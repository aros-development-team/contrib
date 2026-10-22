#!/usr/bin/env python3
"""CMD_WRITE excludes the console task while parser/render state is in use."""
from pathlib import Path
import subprocess
import tempfile

source = (Path(__file__).resolve().parents[1] / "console.c").read_text()
begin = source.index("    case CMD_WRITE:", source.index("AROS_LH1(void, beginio"))
body = source[begin:source.index("    case CMD_READ:", begin)]
prefix = r'''
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint32_t ULONG; typedef int32_t LONG; typedef char *STRPTR;
struct Unit {int unused;}; struct ConUnit {int unused;};
struct ConsoleBase {pthread_mutex_t consoleTaskLock;};
struct IOStdReq {struct Unit *io_Unit; void *io_Data; ULONG io_Length,io_Actual;};
#define CMD_WRITE 3
#define CONU_LIBRARY -1
#define IOERR_BADADDRESS -5
#define IOERR_BADLENGTH -4
#define D(x)
#define DEBUG 0
static pthread_mutex_t gate=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t changed=PTHREAD_COND_INITIALIZER;
static int rendering,attempted,excluded,acquires,releases;
static void ObtainSemaphore(pthread_mutex_t *p){assert(!pthread_mutex_lock(p));acquires++;}
static void ReleaseSemaphore(pthread_mutex_t *p){releases++;assert(!pthread_mutex_unlock(p));}
static ULONG writeToConsole(struct ConUnit *u,void *p,ULONG n,struct ConsoleBase *b,LONG *error){
 assert(u&&p&&n);
 pthread_mutex_lock(&gate);rendering=1;pthread_cond_broadcast(&changed);
 while(!attempted)pthread_cond_wait(&changed,&gate);
 pthread_mutex_unlock(&gate);
 *error=0;return n;
}
static void *refresh(void *arg){
 struct ConsoleBase *b=arg;
 pthread_mutex_lock(&gate);
 while(!rendering)pthread_cond_wait(&changed,&gate);
 int rc=pthread_mutex_trylock(&b->consoleTaskLock);
 excluded=(rc==EBUSY);
 if(!rc)pthread_mutex_unlock(&b->consoleTaskLock);
 attempted=1;pthread_cond_broadcast(&changed);pthread_mutex_unlock(&gate);
 pthread_mutex_lock(&b->consoleTaskLock);pthread_mutex_unlock(&b->consoleTaskLock);
 return NULL;
}
static LONG command(struct ConsoleBase *ConsoleDevice,struct IOStdReq *ioreq){
 LONG error=0;switch(CMD_WRITE){
'''
suffix = r'''
 }return error;
}
int main(void){
 struct ConsoleBase b={PTHREAD_MUTEX_INITIALIZER};struct Unit u={0};
 for(int i=0;i<100;i++){
  rendering=attempted=excluded=acquires=releases=0;
  struct IOStdReq io={&u,"abc",3,0};pthread_t task;
  assert(!pthread_create(&task,NULL,refresh,&b));
  assert(command(&b,&io)==0&&io.io_Actual==3);
  assert(!pthread_join(task,NULL));assert(excluded&&acquires==1&&releases==1);
 }
 struct IOStdReq bad={NULL,"abc",3,99};
 acquires=releases=0;
 assert(command(&b,&bad)==IOERR_BADADDRESS&&bad.io_Actual==0&&!acquires&&!releases);
 bad.io_Unit=&u;bad.io_Length=0x80000000u;
 assert(command(&b,&bad)==IOERR_BADLENGTH&&!acquires&&!releases);
 puts("CMD_WRITE lock PASS: 100 overlapping refresh attempts, balanced ownership, invalid requests");
}
'''
with tempfile.TemporaryDirectory() as directory:
    path = Path(directory) / "test.c"
    binary = Path(directory) / "test"
    for negative in (False, True):
        candidate = body
        if negative:
            candidate = candidate.replace(
                "ObtainSemaphore(&ConsoleDevice->consoleTaskLock);", "")
            candidate = candidate.replace(
                "ReleaseSemaphore(&ConsoleDevice->consoleTaskLock);", "")
        path.write_text(prefix + candidate + suffix)
        subprocess.run(["cc", "-Wall", "-Werror", "-Wno-unused-function",
                        "-pthread", str(path), "-o", str(binary)], check=True)
        result = subprocess.run([str(binary)], capture_output=True, text=True, timeout=15)
        assert (result.returncode == 0) != negative, result.stderr
        print("Negative control PASS: unlocked write permits refresh" if negative
              else result.stdout.strip())
