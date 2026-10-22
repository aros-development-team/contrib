#!/usr/bin/env python3
"""Glyph drawing runs with DOS process state and waits for owned request completion."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
s=(r/'consolengglyph.c').read_text();body=s[s.index('struct glyph_draw_request {'):]
prefix=r'''
#include <assert.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
typedef intptr_t IPTR;typedef unsigned long ULONG;typedef long LONG;typedef int BOOL;
#define FALSE 0
#define BNULL 0
#define NT_PROCESS 13
#define CONSOLENG_GLYPH_NOMEM 3
#define TAG_DONE 0
enum{NP_Entry=1,NP_Name,NP_StackSize,NP_UserData,NP_CopyVars,NP_Cli,NP_Input,NP_Output,NP_Error,NP_CloseInput,NP_CloseOutput,NP_CloseError,NP_CurrentDir,NP_HomeDir,NP_Path};
struct Task{struct{int ln_Type;}tc_Node;void*tc_UserData;};struct Process{struct Task pr_Task;};
struct MsgPort{int reply;};struct Message{struct MsgPort*mn_ReplyPort;unsigned mn_Length;};
struct Library{int n;};struct DosLibrary{int n;};struct ConsoleBase{int n;};struct RastPort{int n;};
static struct Process parent,worker;static struct Task*current;static struct Library lib;static struct MsgPort port;
static int fail,opened,closed,ports,deleted,spawned,drawn,replied,schedule,wakes;static LONG(*entry)(void);
static struct Task*FindTask(void*p){return current;}
static struct Library*OpenLibrary(const char*n,int v){opened++;return fail==1?NULL:&lib;}
static void CloseLibrary(struct Library*l){assert(l==&lib);closed++;}
static struct MsgPort*CreateMsgPort(void){ports++;port.reply=0;return fail==2?NULL:&port;}
static void DeleteMsgPort(struct MsgPort*p){assert(p==&port);deleted++;}
static void ReplyMsg(struct Message*m){assert(current==&worker.pr_Task);m->mn_ReplyPort->reply=1;replied++;}
static void*GetMsg(struct MsgPort*p){if(!p->reply)return NULL;p->reply=0;return p;}
static void WaitPort(struct MsgPort*p){assert(entry&&p==&port);if(schedule==2&&!wakes++){return;}current=&worker.pr_Task;entry();current=&parent.pr_Task;}
static struct Process*CreateNewProcTags(IPTR tag,...){
 spawned++;va_list ap;va_start(ap,tag);unsigned seen=0;
 while(tag){IPTR v=va_arg(ap,IPTR);seen|=1u<<tag;
 if(tag==NP_Entry)entry=(LONG(*)(void))v;else if(tag==NP_UserData)worker.pr_Task.tc_UserData=(void*)v;
 else if(tag==NP_StackSize)assert(v>=65536);else if(tag!=NP_Name)assert(v==0);
 tag=va_arg(ap,IPTR);}
 va_end(ap);assert(seen==((1u<<(NP_Path+1))-2));worker.pr_Task.tc_Node.ln_Type=NT_PROCESS;if(fail!=3&&schedule==1){current=&worker.pr_Task;entry();current=&parent.pr_Task;}return fail==3?NULL:&worker;}
static LONG ConsolengGlyphDrawProcess(struct Library*g,struct ConsoleBase*d,struct RastPort*r,ULONG cp,LONG x,LONG y,BOOL emoji){assert(current->tc_Node.ln_Type==NT_PROCESS);assert(g==&lib&&d&&r);drawn++;return cp+x+y+emoji;}
'''
main=r'''
int main(void){struct ConsoleBase d={0};struct RastPort rp={0};
 for(schedule=0;schedule<3;schedule++)for(int process=0;process<2;process++)for(fail=0;fail<4;fail++)for(int i=0;i<50;i++){
 wakes=opened=closed=ports=deleted=spawned=drawn=replied=0;current=&parent.pr_Task;parent.pr_Task.tc_Node.ln_Type=process?NT_PROCESS:1;
 LONG got=ConsolengGlyphDraw(&lib,&d,&rp,0x2600+i,2,3,i%2);
 if(process){assert(got==0x2600+i+5+i%2&&drawn==1&&!opened&&!spawned);}
 else if(fail){assert(got==CONSOLENG_GLYPH_NOMEM&&!drawn&&!replied);assert(closed==(fail!=1)&&deleted==(fail==3));}
 else{assert(got==0x2600+i+5+i%2&&drawn==1&&replied==1&&closed==1&&deleted==1&&spawned==1);}
 }
 puts("Glyph worker PASS: process fast path, deferred task execution, 1200 immediate/deferred/spurious-wakeup and failure cases");}
'''
with tempfile.TemporaryDirectory() as td:
 for name,old,new in [('production',None,None),('no process dispatch','if (FindTask(NULL)->tc_Node.ln_Type == NT_PROCESS)','if (1)'),('no completion wait','while (!GetMsg(port)) WaitPort(port);','(void)GetMsg; (void)WaitPort;')]:
  text=body
  if old:assert old in text;text=text.replace(old,new)
  p=Path(td)/'test.c';b=Path(td)/'test';p.write_text(prefix+text+main)
  subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(b)],check=True)
  result=subprocess.run([str(b)],capture_output=True,text=True)
  assert (result.returncode==0)==(old is None),(name,result.stderr)
  print(result.stdout.strip() if old is None else 'Negative control PASS: '+name)
