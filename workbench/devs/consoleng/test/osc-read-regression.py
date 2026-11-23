#!/usr/bin/env python3
"""Exercise actual asynchronous OSC52 read code with explicit provider ownership."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
s='\n'.join(x for x in (r/'read_clipboard.c').read_text().splitlines() if not x.startswith('#include'))
u=(r/'osc_clipboard.c').read_text();u=u[u.index('BOOL con_osc_clipboard_utf8('):u.index('VOID con_osc_clipboard_apply(')]
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
#define CU(o) (o)
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
typedef struct {UQUAD inputGeneration;BOOL utf8;ULONG oscCopyDisabled;} Object;
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

static unsigned delivered_length;static UBYTE delivered[1500000];
static void con_inject(struct ConsoleBase*b,Object*o,const UBYTE*p,ULONG n){(void)b;assert(expected_valid&&o==expected_unit&&n<=sizeof(delivered));deliveries++;delivered_length=n;memcpy(delivered,p,n);}
/* Copyright (C) 2026, The AROS Development Team. All rights reserved. */
#ifndef DOS_CONCLIP_H
#define DOS_CONCLIP_H
/* Optional ConClip text snapshot protocol. The initial fields match the legacy
 * edit-hook message; old servers reply without modifying status. */
struct SGWork;
#define CONCLIP_READ_TEXT 0x100
#define CONCLIP_WRITE_TEXT 0x101
#define CONCLIP_WRITE_MAX (1024UL * 1024UL)
#define CONCLIP_TEXT_VERSION 1
#define CONCLIP_TEXT_UNSUPPORTED 0
#define CONCLIP_TEXT_OK 1
#define CONCLIP_TEXT_ERROR 2
struct ConClipTextMessage {
    struct Message msg;
    struct SGWork *sgw; /* extended requests: unused, must be NULL */
    WORD code;
    ULONG version, status;
    UBYTE *data;
    ULONG length, capacity;
};
/* READ: Concatenates all FTXT/CHRS chunks in stream order. A malformed/short
 * later chunk fails the whole snapshot without transferring a partial prefix.
 * On OK, data belongs to the caller: FreeMem(data, capacity). Capacity includes
 * twelve spare bytes for terminal framing. ERROR never transfers ownership. */
/* WRITE: caller retains data until reply; length <= capacity and WRITE_MAX.
 * Zero length writes an empty FTXT/CHRS. Status OK means all chunk writes and
 * pops succeeded; ERROR may have modified clipboard contents (no rollback).
 * Old servers reply UNSUPPORTED. Existing struct layout/version are unchanged. */
#endif
'''
main=r'''

static void finish(unsigned index,const UBYTE *data,ULONG n,ULONG status){
 struct ConClipTextMessage*q=(void*)sent[index];assert(q->code==CONCLIP_READ_TEXT&&q->version==1&&!q->sgw);
 q->status=status;if(status==CONCLIP_TEXT_OK){q->data=AllocMem(n+12,0);q->capacity=n+12;q->length=n;if(n)memcpy(q->data,data,n);}ReplyMsg(sent[index]);
}
int main(void){
 Object unit={42,TRUE,0};struct ConsoleBase base={(void*)123,0,3,"abc"};expected_unit=&unit;expected_generation=42;expected_valid=TRUE;
 assert(!con_osc_read_request(&base,&unit,'c',0)&&!live);unit.oscCopyDisabled=2;
 fail_alloc=1;assert(!con_osc_read_request(&base,&unit,'c',0)&&!live);fail_alloc=0;
 for(int st=0;st<2;st++)for(int target=0;target<3;target++){
  UBYTE c="cps"[target];assert(con_osc_read_request(&base,&unit,c,st));int prior=deliveries;
  con_osc_read_poll(&base,unit_valid);assert(deliveries==prior+1&&!live);
  UBYTE want[]="\033]52;c;YWJj\007";want[5]=c;assert(delivered_length==(st?13:12));assert(!memcmp(delivered,want,11));assert(st?delivered[11]==27&&delivered[12]=='\\':delivered[11]==7);
 }
 available=1;sent_count=0;int prior=deliveries;
 for(unsigned i=0;i<16;i++)assert(con_osc_read_request(&base,&unit,'c',0));
 assert(live==16&&!con_osc_read_request(&base,&unit,'c',0));con_osc_read_poll(&base,unit_valid);assert(deliveries==prior);
 finish(7,(UBYTE*)"ab",2,CONCLIP_TEXT_OK);con_osc_read_poll(&base,unit_valid);assert(live==15&&delivered_length==12&&!memcmp(delivered,"\033]52;c;YWI=\007",12));
 for(unsigned i=0;i<16;i++)if(i!=7)finish(i,NULL,0,CONCLIP_TEXT_UNSUPPORTED);
 con_osc_read_poll(&base,unit_valid);assert(!live&&!osc_read_jobs);
 /* Revoking then re-enabling cannot revive outstanding requests. */
 sent_count=0;prior=deliveries;assert(con_osc_read_request(&base,&unit,'c',0));con_osc_read_cancel(&unit);unit.oscCopyDisabled=0;unit.oscCopyDisabled=2;
 finish(0,(UBYTE*)"x",1,CONCLIP_TEXT_OK);con_osc_read_poll(&base,unit_valid);assert(!live&&deliveries==prior);
 /* Generation validation must precede even reading permission bits. */
 sent_count=0;assert(con_osc_read_request(&base,&unit,'c',0));expected_valid=FALSE;expected_unit=(Object*)0x12345678;osc_read_jobs->unit=expected_unit;
 finish(0,(UBYTE*)"x",1,CONCLIP_TEXT_OK);con_osc_read_poll(&base,unit_valid);assert(!live&&deliveries==prior);expected_valid=TRUE;expected_unit=&unit;
 for(int mode=0;mode<5;mode++){
  sent_count=0;assert(con_osc_read_request(&base,&unit,'c',0));finish(0,(UBYTE*)"\xff",1,CONCLIP_TEXT_OK);
  struct ConClipTextMessage*q=(void*)sent[0];
  if(mode==1)q->length=q->capacity+1;
  if(mode==2)q->length=CONCLIP_WRITE_MAX+1;
  if(mode==3){q->data[0]='a';fail_alloc=1;}
  if(mode==4){q->data[0]='a';unit.oscCopyDisabled=0;}
  con_osc_read_poll(&base,unit_valid);fail_alloc=0;unit.oscCopyDisabled=2;assert(!live&&deliveries==prior);
 }
 sent_count=0;assert(con_osc_read_request(&base,&unit,'c',1));finish(0,NULL,0,CONCLIP_TEXT_OK);con_osc_read_poll(&base,unit_valid);assert(!live&&delivered_length==9&&!memcmp(delivered,"\033]52;c;\033\\",9));

 UBYTE*large=malloc(CONCLIP_WRITE_MAX);assert(large);memset(large,'a',CONCLIP_WRITE_MAX);
 for(unsigned size=1;size<=CONCLIP_WRITE_MAX;size=size==1?2:size==2?3:size==3?CONCLIP_WRITE_MAX:CONCLIP_WRITE_MAX+1){
  sent_count=0;assert(con_osc_read_request(&base,&unit,'c',0));finish(0,large,size,CONCLIP_TEXT_OK);con_osc_read_poll(&base,unit_valid);
  assert(!live&&delivered_length==8+4*((size+2)/3));
  for(unsigned i=0;i<size/3;i++)assert(!memcmp(delivered+7+4*i,"YWFh",4));
  if(size%3)assert(!memcmp(delivered+7+4*(size/3),size%3==1?"YQ==":"YWE=",4));
 }
 free(large);
 assert(!locks&&!forbidden);puts("OSC52 read PASS: permissions, framing, exact base64, local/system routing, async bound, revocation, stale identities, invalid UTF8/length, unsupported provider and OOM");
}

'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'t.c';b=Path(d)/'t'
 def run(source,sanitize=False):
  p.write_text(prefix+'\n'+u+source+main)
  subprocess.run(['cc','-Wall','-Werror','-Wno-misleading-indentation']+(['-fsanitize=address,undefined'] if sanitize else [])+[str(p),'-o',str(b)],check=True)
  return subprocess.run([str(b)],capture_output=True)
 result=run(s,True);print(result.stdout.decode(),end='');assert result.returncode==0,result.stderr.decode()
 for label,broken in [('stale identity',s.replace('valid_unit(j->unit,j->generation,base)', '(valid_unit(j->unit,j->generation,base)||TRUE)')),('revoked request',s.replace('if (j->valid &&', 'if (')),('unbounded queue',s.replace('++count >= 16','++count >= 99'))]:
  assert run(broken).returncode!=0,label
  print('Negative control rejected:',label)
