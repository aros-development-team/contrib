#!/usr/bin/env python3
"""Exercise real URL worker ownership with simulated DOS scheduling/failures."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
s=(r/'url_action.c').read_text()
start=s.index('static BOOL url_scheme')
code=s[start:s.index('\n}',start)+2]
code+=s[s.index('static BOOL url_arguments'):]
prefix=r'''
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
typedef unsigned char UBYTE;typedef uint32_t ULONG;typedef int32_t LONG;typedef uintptr_t IPTR,BPTR;typedef int BOOL;typedef const char *CONST_STRPTR;
#define TRUE 1
#define FALSE 0
#define BNULL 0
#define MEMF_PUBLIC 0
#define CON_URL_MAX 1023
struct Library{int dummy;};struct DosLibrary{int dummy;};struct Task{void *tc_UserData;};
enum {TAG_DONE,NP_Entry,NP_Name,NP_StackSize,NP_UserData,NP_CopyVars,NP_Cli,NP_Input,NP_Output,NP_Error,NP_CloseInput,NP_CloseOutput,NP_CloseError,NP_CurrentDir,NP_HomeDir,NP_Path,NP_Seglist,NP_FreeSeglist,NP_Arguments};
static int fail,worker,allocs,libs,loads,unloads,commands;
static struct Task task;
static LONG (*entry)(void);
static char received[2050];
static void *AllocVec(ULONG size,ULONG flags){if(fail==1)return 0;allocs++;return malloc(size);}
static void FreeVec(void *p){assert(p);allocs--;free(p);}
static void *OpenLibrary(CONST_STRPTR name,ULONG version){assert(!strcmp(name,"dos.library"));if((!worker&&fail==2)||(worker&&fail==4))return 0;libs++;return (void*)1;}
static void CloseLibrary(struct Library *p){assert(p);libs--;}
static struct Task *FindTask(void *p){assert(worker);return &task;}
static BPTR load(CONST_STRPTR path){assert(worker);assert(!strcmp(path,"C:OpenURL"));loads++;return fail==5?0:123;}
static void unload(BPTR seg){assert(worker&&seg==123);unloads++;}
#define LoadSeg(p) ((void)DOSBase,load(p))
#define UnLoadSeg(p) ((void)DOSBase,unload(p))
static IPTR tag(IPTR *t,IPTR wanted){for(;*t;t+=2)if(*t==wanted)return t[1];return 0;}
static void *create(IPTR *tags){
 if(!worker){
  /* Caller must not cause implicit OpenNIL or inherited path I/O. */
  IPTR required[]={NP_Input,NP_Output,NP_Error,NP_CurrentDir,NP_HomeDir,NP_Path,NP_CopyVars,NP_Cli,0};
  for(int i=0;required[i];i++){int seen=0;for(IPTR*t=tags;*t;t+=2)if(t[0]==required[i]){assert(t[1]==0);seen=1;}assert(seen);}
  if(fail==3)return 0;
  entry=(LONG(*)(void))tag(tags,NP_Entry);task.tc_UserData=(void*)tag(tags,NP_UserData);assert(entry&&task.tc_UserData);return (void*)1;
 }
 assert(tag(tags,NP_Seglist)==123&&tag(tags,NP_FreeSeglist));
 if(fail==6)return 0;
 strcpy(received,(char*)tag(tags,NP_Arguments));commands++;return (void*)2;
}
#define CreateNewProcTags(...) ((void)DOSBase,create((IPTR[]){__VA_ARGS__}))
'''
main=r'''
int main(void){
 for(fail=0;fail<=6;fail++){
  worker=allocs=libs=loads=unloads=commands=0;entry=0;task.tc_UserData=0;
  BOOL started=con_open_url((UBYTE*)"https://example.org/a*\"b&x=2");
  assert(started==(fail==0||fail>=4));assert(loads==0&&libs==0);
  if(started){assert(allocs==1);worker=1;assert(entry()==0);}
  assert(allocs==0&&libs==0);
  assert(commands==(fail==0));assert(unloads==(fail==6));
  if(!fail)assert(!strcmp(received,"\"https://example.org/a***\"b&x=2\"\n"));
 }
 fail=0;worker=0;assert(!con_open_url((UBYTE*)"javascript:bad"));assert(!allocs&&!libs);
 return 0;
}
'''
assert 'options noexpunge' in (r/'consoleng.conf').read_text(), 'Worker entry requires resident device lifetime'
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';b=Path(d)/'test';p.write_text(prefix+code+main)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(b)],check=True)
 subprocess.run([str(b)],check=True)
print('URL launcher PASS: no caller file I/O, worker/command args, six failure paths, allocation/library/segment ownership; native runtime pending')
