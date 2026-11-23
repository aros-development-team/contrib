#!/usr/bin/env python3
"""Production async copy ownership, stopped provider, bounds, race and failures."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
s='\n'.join(x for x in (r/'copy_clipboard.c').read_text().splitlines() if not x.startswith('#include'))
selection=(r/'charmapconclass.c').read_text().split('static VOID charmapcon_copy',1)[1].split('static struct charmap_line *charmap_row_at',1)[0]
assert 'WaitPort' not in selection and 'if (!buf) return;' in selection
assert selection.index('if (!buf) return;') < selection.index('FreeMem((APTR) ConsoleDevice->copyBuffer')
assert 'con_copy_clipboard_poll();' in (r/'consoletask.c').read_text()
prefix=r'''
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
typedef uint32_t ULONG;typedef int32_t LONG;typedef int16_t WORD;typedef uint8_t UBYTE;typedef char*STRPTR;typedef int BOOL;typedef void VOID;
#define MEMF_PUBLIC 1
#define MEMF_CLEAR 2
#define NT_MSGPORT 1
#define NT_MESSAGE 2
#define PA_SIGNAL 1
#define SIGBREAKB_CTRL_D 3
#define EO_BIGCHANGE 9
#define CONCLIP_WRITE_TEXT 0x101
#define CONCLIP_TEXT_VERSION 1
#define CONCLIP_TEXT_UNSUPPORTED 0
#define CONCLIP_TEXT_OK 1
#define CONCLIP_TEXT_ERROR 2
#define CONCLIP_WRITE_MAX (1024UL*1024UL)
struct Node{int ln_Type;};struct Message;struct List{struct Message*m;};
struct MsgPort{struct Node mp_Node;int mp_Flags,mp_SigBit;void*mp_SigTask;struct List mp_MsgList;};
struct Message{struct Node mn_Node;struct MsgPort*mn_ReplyPort;unsigned mn_Length;};
struct SGWork{STRPTR WorkBuffer;WORD NumChars,Code,EditOp;};
struct ConClipTextMessage{struct Message msg;struct SGWork*sgw;WORD code;ULONG version,status;UBYTE*data;ULONG length,capacity;};struct ConsoleBase{void*consoleTask;};
const STRPTR CONCLIP_PORTNAME="test";
static int live,fail,forbidden,lookups,available=1,vanish;static struct MsgPort external;
static struct Message*sent[32];static unsigned sent_count;
static void*AllocMem(size_t n,int f){(void)f;if(fail)return NULL;size_t*p=calloc(1,n+sizeof(size_t));assert(p);*p=n;live++;return p+1;}
static void FreeMem(void*p,size_t n){size_t*h=(size_t*)p-1;assert(*h==n);live--;free(h);}
static void NewList(struct List*l){l->m=NULL;}
static void Forbid(void){assert(!forbidden);forbidden=1;}static void Permit(void){assert(forbidden);forbidden=0;}
static struct MsgPort*FindPort(STRPTR n){assert(forbidden&&n==CONCLIP_PORTNAME);lookups++;return available&&!(vanish&&lookups%2==0)?&external:NULL;}
static void PutMsg(struct MsgPort*p,struct Message*m){assert(p==&external&&forbidden&&sent_count<32);sent[sent_count++]=m;}
static void ReplyMsg(struct Message*m){assert(!m->mn_ReplyPort->mp_MsgList.m);m->mn_ReplyPort->mp_MsgList.m=m;}
static struct Message*GetMsg(struct MsgPort*p){struct Message*m=p->mp_MsgList.m;p->mp_MsgList.m=NULL;return m;}
'''
main=r'''
int main(void){struct ConsoleBase b={(void*)123};UBYTE bytes[CONCLIP_WRITE_MAX];memset(bytes,'x',sizeof(bytes));
assert(con_copy_clipboard_request(&b,NULL,1)==-1);
assert(con_copy_clipboard_request(&b,bytes,CONCLIP_WRITE_MAX+1)==-1);assert(!live&&!sent_count);
for(int status=0;status<3;status++){
 assert(con_copy_clipboard_request(&b,NULL,0)==1);struct copy_clipboard_job*j=(void*)sent[0];assert(j->request.length==0&&j->request.capacity==0);j->request.status=status;ReplyMsg(sent[0]);con_copy_clipboard_poll();assert(!live&&sent_count==1);sent_count=0;
}
fail=1;assert(con_copy_clipboard_request(&b,bytes,2)==-1&&!live);fail=0;
available=0;assert(con_copy_clipboard_request(&b,bytes,32768)==0);available=1;
lookups=0;vanish=1;assert(con_copy_clipboard_request(&b,bytes,3)==0&&!live&&!sent_count);vanish=0;
for(int i=0;i<16;i++)assert(con_copy_clipboard_request(&b,bytes,i?CONCLIP_WRITE_MAX:1)==1);
assert(live==16&&sent_count==16);assert(con_copy_clipboard_request(&b,bytes,1)==-1);
memset(bytes,'z',sizeof(bytes));
for(int i=0;i<16;i++){struct copy_clipboard_job*j=(void*)sent[i];ULONG length=i?CONCLIP_WRITE_MAX:1;assert(j->request.code==CONCLIP_WRITE_TEXT&&!j->request.sgw&&j->request.length==length&&j->request.capacity==length&&j->request.version==1&&j->request.status==0);assert(j->request.data==j->bytes&&j->bytes[0]=='x'&&j->bytes[length-1]=='x');assert(j->reply.mp_SigTask==b.consoleTask&&j->reply.mp_SigBit==SIGBREAKB_CTRL_D);}
for(int i=0;i<1000;i++){con_copy_clipboard_poll();}assert(live==16);
((struct copy_clipboard_job*)sent[7])->request.status=CONCLIP_TEXT_OK;ReplyMsg(sent[7]);con_copy_clipboard_poll();assert(live==15);
for(int i=15;i>=0;i--)if(i!=7){((struct copy_clipboard_job*)sent[i])->request.status=CONCLIP_TEXT_OK;ReplyMsg(sent[i]);con_copy_clipboard_poll();}
assert(!live&&!copy_clipboard_jobs&&!forbidden);
/* Only UNSUPPORTED with a representable WORD retries; ERROR never retries. */
for(unsigned n=32767;n<=32768;n++)for(int status=0;status<=2;status++){
 sent_count=0;assert(con_copy_clipboard_request(&b,bytes,n)==1);struct copy_clipboard_job*j=(void*)sent[0];j->request.status=status;ReplyMsg(sent[0]);con_copy_clipboard_poll();
 if(!status&&n==32767){assert(live==1&&sent_count==2&&j->request.code=='C'&&j->request.sgw==&j->work&&j->work.NumChars==32767);ReplyMsg(sent[1]);con_copy_clipboard_poll();}else assert(sent_count==1);
 assert(!live&&!copy_clipboard_jobs);
}
sent_count=0;assert(con_copy_clipboard_request(&b,bytes,1)==1);available=0;ReplyMsg(sent[0]);con_copy_clipboard_poll();assert(!live&&sent_count==1);
puts("COPY ASYNC PASS: owned 1 MiB snapshots, 16-job bound, legacy 32767 boundary, status-aware fallback, missing ports, OOM and late replies");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'t.c';e=Path(d)/'t';p.write_text(prefix+s+main)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(e)],check=True);subprocess.run([str(e)],check=True)
 for name,broken in [('unbounded jobs',s.replace('++count >= 16','++count >= 99')),('signed WORD fallback overflow',s.replace('job->request.length <= 32767','job->request.length <= 65535'))]:
  p.write_text(prefix+broken+main);subprocess.run(['cc',str(p),'-o',str(e)],check=True)
  assert subprocess.run([str(e)],capture_output=True).returncode!=0,name
  print('Negative control rejected:',name)
