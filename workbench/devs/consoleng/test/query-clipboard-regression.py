#!/usr/bin/env python3
"""Actual asynchronous query clipboard lifetime, bounds and unit-generation routing."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s='\n'.join(x for x in (r/'query_clipboard.c').read_text().splitlines() if not x.startswith('#include'))
prefix=r'''
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
typedef uint32_t ULONG;typedef uint64_t UQUAD;typedef int32_t LONG;typedef int16_t WORD;typedef uint8_t UBYTE;typedef char*STRPTR;typedef int BOOL;typedef void VOID;
#define TRUE 1
#define FALSE 0
#define CON_SEARCH_PASTE_BYTES 1024
#define MEMF_PUBLIC 1
#define MEMF_CLEAR 2
#define NT_MSGPORT 1
#define NT_MESSAGE 2
#define PA_SIGNAL 1
#define SIGBREAKB_CTRL_D 3
struct Node {int ln_Type;};struct Message;struct List {struct Message*message;};
struct MsgPort {struct Node mp_Node;int mp_Flags,mp_SigBit;void*mp_SigTask;struct List mp_MsgList;};
struct Message {struct Node mn_Node;struct MsgPort*mn_ReplyPort;unsigned mn_Length;};
struct StringInfo {WORD MaxChars;};struct SGWork {STRPTR WorkBuffer;struct StringInfo*StringInfo;WORD BufferPos;};
typedef struct {UQUAD inputGeneration;BOOL utf8;} Object;
struct ConsoleBase {void*consoleTask;int copyBufferLock;ULONG copyBufferSize;const char*copyBuffer;};
#define ICU(o) (o)
#define CON_IS_UTF8(o) ((o)->utf8)
const STRPTR CONCLIP_PORTNAME="test";
static int live,fail_alloc,locks,forbidden,available,deliveries,expected_valid;static Object*expected_unit;static UQUAD expected_generation;static struct MsgPort external;
static struct Message*sent[32];static unsigned sent_count;
static void*AllocMem(size_t n,int flags){(void)flags;if(fail_alloc)return NULL;live++;return calloc(1,n);}
static void FreeMem(void*p,size_t n){(void)n;live--;free(p);}
static void NewList(struct List*l){l->message=NULL;}
static void Forbid(void){forbidden++;}static void Permit(void){forbidden--;}
static struct MsgPort*FindPort(STRPTR name){assert(forbidden&&name==CONCLIP_PORTNAME);return available?&external:NULL;}
static void PutMsg(struct MsgPort*p,struct Message*m){assert(p==&external&&forbidden);assert(sent_count<32);sent[sent_count++]=m;}
static void ReplyMsg(struct Message*m){assert(!m->mn_ReplyPort->mp_MsgList.message);m->mn_ReplyPort->mp_MsgList.message=m;}
static struct Message*GetMsg(struct MsgPort*p){struct Message*m=p->mp_MsgList.message;p->mp_MsgList.message=NULL;return m;}
static void ObtainSemaphore(int*p){(void)p;locks++;}static void ReleaseSemaphore(int*p){(void)p;locks--;}
static BOOL unit_valid(Object*o,UQUAD generation,struct ConsoleBase*b){(void)b;assert(o==expected_unit&&generation==expected_generation);return expected_valid;}
static UQUAD delivered_revision;static ULONG delivered_length;static BOOL delivered_utf8,delivered_valid;static UBYTE delivered[1024];
static void Console_QueryPaste(Object*o,UQUAD revision,const UBYTE*bytes,ULONG length,BOOL utf8,BOOL valid){assert(expected_valid&&o==expected_unit&&length<=1024);deliveries++;delivered_revision=revision;delivered_length=length;delivered_utf8=utf8;delivered_valid=valid;memcpy(delivered,bytes,length);}
'''
main=r'''
static void complete(struct ConsoleBase*b,unsigned i,const char*text,WORD length){
 struct query_clipboard_job*j=(void*)sent[i];assert(j->request.code=='V'&&j->request.sgw==&j->work&&j->work.StringInfo==&j->string&&j->string.MaxChars>=1025&&j->string.MaxChars<=1026);
 assert(j->reply.mp_SigTask==b->consoleTask&&j->reply.mp_SigBit==SIGBREAKB_CTRL_D);
 if(length>0&&length<=1024)memcpy(j->bytes,text,length);j->work.BufferPos=length;ReplyMsg(sent[i]);
}
int main(void){
 Object unit={42,TRUE};struct ConsoleBase base={(void*)123,0,3,"abc"};expected_unit=&unit;expected_generation=42;expected_valid=TRUE;
 fail_alloc=1;assert(!con_query_clipboard_request(&base,&unit,7)&&live==0);fail_alloc=0;
 assert(con_query_clipboard_request(&base,&unit,7)&&live==1);assert(!deliveries);con_query_clipboard_poll(&base,unit_valid);
 assert(deliveries==1&&delivered_revision==7&&delivered_utf8&&delivered_valid&&delivered_length==3&&!memcmp(delivered,"abc",3)&&!live);
 base.copyBufferSize=1025;assert(con_query_clipboard_request(&base,&unit,8));con_query_clipboard_poll(&base,unit_valid);assert(!delivered_valid&&!delivered_length&&!live);
 base.copyBufferSize=3;base.copyBuffer=NULL;assert(con_query_clipboard_request(&base,&unit,9));con_query_clipboard_poll(&base,unit_valid);assert(!delivered_valid&&!live);
 available=1;sent_count=0;int prior=deliveries;
 for(unsigned i=0;i<16;i++)assert(con_query_clipboard_request(&base,&unit,i));
 assert(live==16&&!con_query_clipboard_request(&base,&unit,17));con_query_clipboard_poll(&base,unit_valid);assert(deliveries==prior&&live==16);
 /* Out of order replies; captured UTF-8 mode survives a later mode change. */
 unit.utf8=FALSE;complete(&base,7,"xyz",3);con_query_clipboard_poll(&base,unit_valid);assert(delivered_revision==7&&delivered_utf8&&live==15);
 for(unsigned i=0;i<16;i++)if(i!=7)complete(&base,i,"x",1);
 con_query_clipboard_poll(&base,unit_valid);assert(!live&&!query_clipboard_jobs);
 /* Disposed/reused unit identity must never be dereferenced or delivered. */
 sent_count=0;assert(con_query_clipboard_request(&base,&unit,88));expected_valid=FALSE;expected_unit=(Object*)0x12345678;query_clipboard_jobs->unit=expected_unit;
 prior=deliveries;complete(&base,0,"x",1);con_query_clipboard_poll(&base,unit_valid);assert(deliveries==prior&&!live);
 expected_valid=TRUE;expected_unit=&unit;
 for(int n=-1;n<=1025;n+=1026){sent_count=0;assert(con_query_clipboard_request(&base,&unit,99));complete(&base,0,"",n);con_query_clipboard_poll(&base,unit_valid);assert(!delivered_valid&&!delivered_length&&!live);}

 /* Emulate the legacy provider's MaxChars-1 clipping, including multiple
  * CHRS chunks. Exactly 1024 is valid; a clipped longer value is rejected. */
 for(unsigned size=1023;size<=1027;size++){
  sent_count=0;assert(con_query_clipboard_request(&base,&unit,100+size));
  struct query_clipboard_job*j=(void*)sent[0];
  for(unsigned offset=0;offset<size;){
   unsigned chunk=size-offset;if(chunk>37)chunk=37;
   WORD n=j->string.MaxChars-1-j->work.BufferPos;if(n>(WORD)chunk)n=chunk;
   if(n>0){memset(j->bytes+j->work.BufferPos,'a',n);j->work.BufferPos+=n;}
   offset+=chunk;
  }
  ReplyMsg(sent[0]);con_query_clipboard_poll(&base,unit_valid);
  assert(delivered_valid==(size<=1024));
  assert(delivered_length==(size<=1024?size:0)&&!live);
 }
 assert(!locks&&!forbidden&&!query_clipboard_jobs);
 puts("Query clipboard PASS: async ownership, 16-job bound, out-of-order replies, stale unit rejection, captured encoding, fallback, malformed length and OOM");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'t.c';b=Path(d)/'t';p.write_text(prefix+s+main)
 subprocess.run(['cc','-Wall','-Werror','-Wno-misleading-indentation','-fsanitize=address,undefined',str(p),'-o',str(b)],check=True);subprocess.run([str(b)],check=True)
 for label,broken in [('clipboard truncation',s.replace('CON_SEARCH_PASTE_BYTES + 2','CON_SEARCH_PASTE_BYTES + 1')),('stale unit delivery',s.replace('if (valid_unit(job->unit, job->generation, ConsoleDevice))','if (valid_unit(job->unit, job->generation, ConsoleDevice) || TRUE)')),('unbounded pending jobs',s.replace('if (++count >= 16) return FALSE;','if (++count >= 99) return FALSE;'))]:
  p.write_text(prefix+broken+main);subprocess.run(['cc','-Wall','-Werror','-Wno-misleading-indentation',str(p),'-o',str(b)],check=True)
  assert subprocess.run([str(b)],capture_output=True).returncode!=0
  print('Negative control PASS:',label)
