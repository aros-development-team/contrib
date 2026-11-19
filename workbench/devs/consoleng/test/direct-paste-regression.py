#!/usr/bin/env python3
"""Actual direct clipboard injection/drain with allocator size accounting."""
from pathlib import Path
import tempfile,subprocess
r=Path(__file__).resolve().parents[1];s=(r/'consoletask.c').read_text()
queue=s[s.index('static VOID con_queue_data('):s.index('/* Protos */')]
drain=s[s.index('static ULONG pasteData('):s.index('VOID consoleTaskEntry(')]
snip=(r/'setconsnip.c').read_text();snip=snip.split('AROS_LIBFUNC_INIT',1)[1].split('AROS_LIBFUNC_EXIT',1)[0]
snip='static LONG set_snip(struct ConsoleBase *ConsoleDevice, void *data){\n'+snip+'}\n'
dispose=(r/'consoleclass.c').read_text();dispose=dispose[dispose.index('static VOID console_dispose('):dispose.index('/********* Console class dispatcher')].replace('ConsoleDevice','cl->base')
code=r'''
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
typedef void VOID;typedef int BOOL;typedef int32_t LONG;typedef uint32_t ULONG;typedef unsigned char UBYTE;typedef void *APTR;
#define TRUE 1
#define FALSE 0
#define CF_BRACKETED_PASTE 16
#define MEMF_ANY 0
#define MEMF_PUBLIC 0
#define SIGBREAKF_CTRL_D 8
#define CON_INPUTBUF_SIZE 512
#define MIN(a,b) ((a)<(b)?(a):(b))
#define D(x)
#define CopyMem(a,b,n) memcpy(b,a,n)
struct List;struct Node {struct Node *next,*prev;struct List *owner;};
struct List {struct Node *head,*tail;};
struct ConUnit {int unused;};
struct intConUnit {struct ConUnit unit;ULONG conFlags,pasteBufferPos,numStoredChars;struct List pasteData;UBYTE inputBuf[512];void *setFont;};
struct intPasteData {struct Node node;struct intConUnit *unit;char *pasteBuffer;ULONG pasteBufferSize,pasteAllocationSize;};
struct ConsoleBase {int consoleTaskLock,consoleTask,copyBufferLock;UBYTE *copyBuffer;ULONG copyBufferSize;};
#define ICU(p) ((struct intConUnit *)(p))
static int held,allocs,failat,live,consume=1;static UBYTE output[200000];static ULONG used;
static void ObtainSemaphore(int *p){(void)p;assert(!held);held=1;}
static void ReleaseSemaphore(int *p){(void)p;assert(held);held=0;}
static void Signal(int t,int s){(void)t;(void)s;assert(!held);}
static void *AllocMem(size_t n,int flags){(void)flags;assert(n);if(allocs++==failat)return NULL;size_t *p=malloc(n+sizeof(size_t));assert(p);*p=n;live++;return p+1;}
static void FreeMem(void *ptr,size_t n){if(!ptr){assert(!n);return;}size_t *p=(size_t*)ptr-1;assert(*p==n);live--;free(p);}
static void AddTail(struct List *l,struct Node *n){n->owner=l;n->prev=l->tail;n->next=NULL;if(l->tail)l->tail->next=n;else l->head=n;l->tail=n;}
static void *GetHead(struct List *l){return l->head;}
static void Remove(struct Node *n){struct List *l=n->owner;if(n->prev)n->prev->next=n->next;else l->head=n->next;if(n->next)n->next->prev=n->prev;else l->tail=n->prev;}
static void answer_requests(APTR unit,struct ConsoleBase *base){(void)base;struct intConUnit *u=unit;if(consume){assert(used+u->numStoredChars<=sizeof(output));memcpy(output+used,u->inputBuf,u->numStoredChars);used+=u->numStoredChars;u->numStoredChars=0;}}
'''+queue+drain+snip+r'''
#define console_title_stack(cl,o,cmd) ((void)0)
typedef struct intConUnit Object;typedef void *Msg;typedef struct {struct ConsoleBase *base;} Class;
static void *RemHead(struct List *l){struct Node *n=l->head;if(n)Remove(n);return n;}
static void console_detach_font_window(struct ConsoleBase *b,Object *o){(void)b;(void)o;}
static void ConsolengCloseFont(struct ConsoleBase *b,void *f){(void)b;(void)f;}
static int disposed;
static void DoSuperMethodA(Class *c,Object *o,Msg m){(void)c;(void)o;(void)m;disposed++;}
'''+dispose+r'''
int main(void){
 UBYTE data[65537],want[65548];ULONG sizes[]={0,1,511,512,513,4096,65536};
 for(int mode=0;mode<2;mode++)for(int clipboard=0;clipboard<2;clipboard++)for(int which=0;which<7;which++)for(int failure=-1;failure<2;failure++){
  struct intConUnit u={0};struct ConsoleBase b={0};ULONG n=sizes[which],expected=0;u.conFlags=mode?16:0;allocs=live=used=0;failat=failure;
  for(ULONG i=0;i<n;i++)data[i]=(UBYTE)(i%256);data[n]=0;
  if(clipboard&&mode){memcpy(want,"\033[200~",6);expected=6;}
  for(ULONG i=0;i<n;i++)if(!(clipboard&&mode)||(data[i]!=27&&data[i]!=3))want[expected++]=data[i];
  if(clipboard&&mode){memcpy(want+expected,"\033[201~",6);expected+=6;}
  if(clipboard)con_inject_paste(&b,&u.unit,data,n);else con_inject(&b,&u.unit,data,n);
  u.conFlags^=16; /* queued framing must not change halfway through delivery */
  if(failure>=0){assert(!u.pasteData.head&&!live);continue;}
  while(u.pasteData.head)assert(pasteData(&u,&b));
  assert(!live&&!u.pasteBufferPos&&used==expected&&!memcmp(output,want,used));
 }
 struct intConUnit u={0};struct ConsoleBase b={0};failat=-1;allocs=live=used=0;u.conFlags=16;
 con_inject(&b,&u.unit,(UBYTE*)"\033[1;2R",-1);con_inject_paste(&b,&u.unit,(UBYTE*)"A\033B\3C",-1);
 while(u.pasteData.head)assert(pasteData(&u,&b));
 assert(used==21&&!memcmp(output,"\033[1;2R\033[200~ABC\033[201~",21)&&!live);
 con_inject_paste(&b,&u.unit,NULL,20);con_inject_paste(&b,&u.unit,(UBYTE*)"x",0x7fffffff);assert(!u.pasteData.head&&!live);
 u.numStoredChars=512;memset(u.inputBuf,'Q',512);consume=0;used=0;
 con_inject_paste(&b,&u.unit,(UBYTE*)"Z",1);assert(live==2);
 assert(!pasteData(&u,&b)&&!u.pasteBufferPos&&u.numStoredChars==512&&live==2);
 consume=1;assert(!pasteData(&u,&b)&&!u.numStoredChars&&used==512);
 assert(pasteData(&u,&b)==13&&!live&&used==525&&!memcmp(output+512,"\033[200~Z\033[201~",13));
 assert(set_snip(&b,"abc")==1&&b.copyBufferSize==3&&live==1);
 assert(!set_snip(&b,NULL)&&!b.copyBuffer&&!b.copyBufferSize&&!live);
 assert(!set_snip(&b,NULL)&&!b.copyBuffer&&!live);
 assert(set_snip(&b,"xyz")==1);assert(!set_snip(&b,"")&&!b.copyBuffer&&!live);
 failat=allocs;assert(!set_snip(&b,"failure")&&!b.copyBuffer&&!b.copyBufferSize&&!live);
 failat=-1;used=0;con_inject_paste(&b,&u.unit,(UBYTE*)"first",5);con_inject_paste(&b,&u.unit,(UBYTE*)"second",6);
 u.pasteBufferPos=3;Class cl={&b};console_dispose(&cl,&u,NULL);
 assert(!live&&!u.pasteData.head&&!u.pasteData.tail&&!u.pasteBufferPos&&disposed==1);
 return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(code)
 subprocess.run(['cc','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS: actual injection/drain, 84 mode/length/allocation cases, filtered-size frees, FIFO, replies and overflow guard')
