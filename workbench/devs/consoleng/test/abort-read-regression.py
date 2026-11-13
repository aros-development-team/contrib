#!/usr/bin/env python3
"""Actual AbortIO ownership check: queued, foreign, quick and completed reads."""
from pathlib import Path
import tempfile,subprocess,resource
r=Path(__file__).resolve().parents[1];s=(r/'console.c').read_text();a=s.index('    LONG ret = -1;',s.index('AROS_LH1(LONG, abortio,'));b=s.index('    AROS_LIBFUNC_EXIT',a);body=s[a:b]
submit=s[s.index('    if (!done_quick)'):s.index('    ReturnVoid("BeginIO")')]
assert submit.index('ObtainSemaphore')<submit.index('PutMsg(')<submit.index('ReleaseSemaphore')
code=r'''
#include <assert.h>
#include <string.h>
typedef int LONG;
#define IOF_QUICK 1
#define NT_MESSAGE 5
#define NT_REPLYMSG 7
#define IOERR_ABORTED -2
struct Node {struct Node *ln_Succ,*ln_Pred;int ln_Type;};
struct List {struct Node root;};
struct Message {struct Node mn_Node;};
struct IORequest {struct Message io_Message;int io_Flags,io_Error;unsigned io_Actual;};
#define IOStdReq IORequest
struct MsgPort {struct List mp_MsgList;};
struct ConsoleBase {int consoleTaskLock;struct MsgPort *commandPort;struct List readRequests;};
static int replies,removals;
static void init(struct List*l){l->root.ln_Succ=l->root.ln_Pred=&l->root;}
static void add(struct List*l,struct Node*n){n->ln_Pred=l->root.ln_Pred;n->ln_Succ=&l->root;n->ln_Pred->ln_Succ=n;l->root.ln_Pred=n;}
#define ForeachNode(l,n) for(n=(l)->root.ln_Succ;n!=&(l)->root;n=n->ln_Succ)
static void Remove(struct Node*n){assert(n->ln_Pred&&n->ln_Succ);n->ln_Pred->ln_Succ=n->ln_Succ;n->ln_Succ->ln_Pred=n->ln_Pred;removals++;}
static void ReplyMsg(struct Message*m){m->mn_Node.ln_Type=NT_REPLYMSG;replies++;}
static void ObtainSemaphore(int*l){assert(!*l);*l=1;}
static void ReleaseSemaphore(int*l){assert(*l);*l=0;}
static LONG abort_read(struct IORequest*ioreq,struct ConsoleBase*ConsoleDevice){
'''+body+r'''
}
int main(void){
 struct MsgPort port;struct ConsoleBase base={0,&port,{{0}}};struct List foreign;
 init(&port.mp_MsgList);init(&base.readRequests);
 struct IORequest completedQuick={0};completedQuick.io_Flags=IOF_QUICK;completedQuick.io_Message.mn_Node.ln_Type=NT_MESSAGE;
 assert(abort_read(&completedQuick,&base)==-1);
 for(int queue=0;queue<2;queue++)for(int position=0;position<4;position++)for(int cycle=0;cycle<1000;cycle++){
  init(&port.mp_MsgList);init(&base.readRequests);init(&foreign);struct List*l=queue?&base.readRequests:&port.mp_MsgList;
  struct IORequest req[4]={0};for(int i=0;i<4;i++){req[i].io_Message.mn_Node.ln_Type=NT_MESSAGE;req[i].io_Actual=999;add(l,&req[i].io_Message.mn_Node);}
  replies=removals=0;assert(!abort_read(&req[position],&base));assert(replies==1&&removals==1&&req[position].io_Actual==0&&req[position].io_Error==IOERR_ABORTED);
  assert(abort_read(&req[position],&base)==-1&&replies==1&&removals==1);
  struct Node*n=l->root.ln_Succ;for(int i=0;i<4;i++)if(i!=position){assert(n==&req[i].io_Message.mn_Node);n=n->ln_Succ;}assert(n==&l->root);
  struct IORequest outsider={0};outsider.io_Message.mn_Node.ln_Type=NT_MESSAGE;outsider.io_Actual=17;add(&foreign,&outsider.io_Message.mn_Node);
  assert(abort_read(&outsider,&base)==-1&&outsider.io_Actual==17&&foreign.root.ln_Succ==&outsider.io_Message.mn_Node);
  outsider.io_Flags=IOF_QUICK;assert(abort_read(&outsider,&base)==-1&&replies==1&&removals==1);
  struct IORequest quick={0};quick.io_Flags=IOF_QUICK;quick.io_Message.mn_Node.ln_Type=NT_MESSAGE;assert(abort_read(&quick,&base)==-1);
  assert(!base.consoleTaskLock);
 }
 return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(code)
 subprocess.run(['cc','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
 # Former node-type-only logic wrongly removes an unqueued quick completion.
 old="""    LONG ret=-1;ObtainSemaphore(&ConsoleDevice->consoleTaskLock);
    if(ioreq->io_Message.mn_Node.ln_Type!=NT_REPLYMSG){
      ioreq->io_Error=IOERR_ABORTED;Remove(&ioreq->io_Message.mn_Node);
      ReplyMsg(&ioreq->io_Message);ret=0;
    }
    ReleaseSemaphore(&ConsoleDevice->consoleTaskLock);return ret;
"""
 (p/'negative.c').write_text(code.replace(body,old))
 subprocess.run(['cc','-Wall','-Wextra','-Werror',str(p/'negative.c'),'-o',str(p/'negative')],check=True)
 def no_core():resource.setrlimit(resource.RLIMIT_CORE,(0,0))
 negative=subprocess.run([str(p/'negative')],capture_output=True,timeout=10,preexec_fn=no_core)
 assert negative.returncode!=0 and b'ln_Pred&&n->ln_Succ' in negative.stderr
 print('NEGATIVE CONTROL: old node-type-only abort attempts to unlink an unqueued quick request')
print('PASS: 8000 queued cancellations preserve sibling FIFO; repeated/completed/foreign/quick requests unchanged')
