#!/usr/bin/env python3
"""Actual synchronization buffer and timer ownership with fault injection."""
from pathlib import Path
import subprocess,tempfile
s=(Path(__file__).resolve().parents[1]/'sync_output.c').read_text();s=s[s.index('#define CON_SYNC_MAX'):]
prefix=r'''
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
typedef uint8_t UBYTE;typedef uint32_t ULONG;typedef int32_t LONG;typedef int BOOL;typedef void VOID;typedef char*STRPTR;typedef const char*CONST_STRPTR;
#define TRUE 1
#define FALSE 0
#define MEMF_PUBLIC 1
#define MEMF_CLEAR 2
#define NT_MSGPORT 4
#define PA_SIGNAL 0
#define SIGBREAKB_CTRL_D 13
#define TR_ADDREQUEST 1
#define UNIT_MICROHZ 0
#define IOERR_ABORTED -2
#define IOERR_BADLENGTH -4
struct Library {int dummy;};
struct MsgPort {struct{int ln_Type;}mp_Node;int mp_Flags,mp_SigBit;void*mp_SigTask;int mp_MsgList;};
struct IORequest {struct{struct MsgPort*mn_ReplyPort;unsigned mn_Length;}io_Message;int io_Command;int done;};
struct timerequest {struct IORequest tr_node;struct{ULONG tv_secs,tv_micro;}tr_time;};
struct ConsolengSync;
struct ConUnit {struct ConsolengSync*sync;ULONG pendingCSILen;UBYTE output[3*1024*1024];ULONG outputLength;};
typedef struct ConUnit Object;
struct ConsoleBase {void*consoleTask;struct Library*cb_IntuitionBase;int unitListLock;struct{Object*mlh_Head;}unitList;};
#define ICU(u) (u)
#define CU(u) (u)
#define NewList(l) ((void)0)
#define ObtainSemaphoreShared(l) ((void)0)
#define ReleaseSemaphore(l) ((void)0)
static Object*NextObject(Object**p){Object*o=*p;*p=NULL;return o;}
static int allocations,fail_alloc,fail_open,parse_fail,opens,arms,waits,aborts;
static void*AllocMem(ULONG size,int flags){if(fail_alloc){fail_alloc--;return NULL;}void*p=calloc(1,size);assert(p);allocations++;return p;}
static void FreeMem(void*p,ULONG size){assert(p&&allocations>0);allocations--;free(p);}
#define CopyMem(a,b,n) memcpy(b,a,n)
static int OpenDevice(CONST_STRPTR n,int unit,struct IORequest*r,int flags){if(fail_open)return 1;opens++;return 0;}
static void CloseDevice(struct IORequest*r){assert(opens>0);opens--;}
static int CheckIO(struct IORequest*r){return r->done;}
static void AbortIO(struct IORequest*r){aborts++;r->done=1;}
static void WaitIO(struct IORequest*r){assert(r->done);waits++;}
static void SendIO(struct IORequest*r){assert(((struct timerequest*)r)->tr_time.tv_micro==150000);arms++;r->done=0;}
ULONG con_parse_output(struct ConUnit*u,STRPTR bytes,ULONG n,struct ConsoleBase*b,LONG*error){*error=0;if(parse_fail){*error=IOERR_ABORTED;return 0;}assert(u->outputLength+n<=sizeof(u->output));if(n)memcpy(u->output+u->outputLength,bytes,n);u->outputLength+=n;return n;}
'''
test=r'''
static struct ConUnit unit;
int main(void){struct ConsoleBase base={.unitList={&unit}};LONG error;
 for(unsigned split=0;split<=8;split++){
  memset(&unit,0,sizeof(unit));con_sync_set(&unit,&base,TRUE);assert(con_sync_buffering(&unit)&&opens==1);
  assert(writeToConsole(&unit,"hello",5,&base,&error)==5&&!error&&!unit.outputLength);
  assert(writeToConsole(&unit,"\x1b[?2026l",split,&base,&error)==split&&!error);
  if(split<8)assert(!unit.outputLength&&con_sync_active(&unit));
  assert(writeToConsole(&unit,"\x1b[?2026l"+split,8-split,&base,&error)==8-split&&!error);
  assert(!con_sync_active(&unit)&&unit.outputLength==13&&!memcmp(unit.output,"hello\x1b[?2026l",13));
  con_sync_dispose(&unit);assert(!allocations&&!opens);
 }
 memset(&unit,0,sizeof(unit));con_sync_set(&unit,&base,TRUE);int before=arms;
 assert(writeToConsole(&unit,"A\x1b[?2026hB",10,&base,&error)==10&&arms==before+1&&!unit.outputLength);
 unit.sync->timer.tr_node.done=1;con_sync_poll(&base);assert(!con_sync_active(&unit)&&unit.outputLength==10);con_sync_dispose(&unit);
 memset(&unit,0,sizeof(unit));con_sync_set(&unit,&base,TRUE);fail_alloc=1;
 assert(writeToConsole(&unit,"fallback",8,&base,&error)==8&&!error&&!con_sync_active(&unit)&&unit.outputLength==8);con_sync_dispose(&unit);
 memset(&unit,0,sizeof(unit));con_sync_set(&unit,&base,TRUE);
 UBYTE*large=malloc(CON_SYNC_MAX+1);memset(large,'x',CON_SYNC_MAX+1);
 assert(writeToConsole(&unit,(STRPTR)large,CON_SYNC_MAX+1,&base,&error)==CON_SYNC_MAX+1&&!error);
 assert(!con_sync_active(&unit)&&unit.outputLength==CON_SYNC_MAX+1&&!memcmp(unit.output,large,CON_SYNC_MAX+1)&&unit.sync->capacity==CON_SYNC_MAX);free(large);con_sync_dispose(&unit);
 memset(&unit,0,sizeof(unit));con_sync_set(&unit,&base,TRUE);assert(writeToConsole(&unit,"retry",5,&base,&error)==5);parse_fail=1;
 assert(writeToConsole(&unit,"\x1b[?2026l",8,&base,&error)==8&&error==IOERR_ABORTED&&!unit.outputLength&&unit.sync->length==13);
 parse_fail=0;unit.sync->timer.tr_node.done=1;con_sync_poll(&base);assert(unit.outputLength==13&&!memcmp(unit.output,"retry\x1b[?2026l",13));con_sync_dispose(&unit);
 memset(&unit,0,sizeof(unit));con_sync_set(&unit,&base,TRUE);assert(writeToConsole(&unit,NULL,0x80000000UL,&base,&error)==0&&error==IOERR_BADLENGTH);before=aborts;con_sync_dispose(&unit);assert(aborts==before+1&&!allocations&&!opens);
 fail_alloc=1;con_sync_set(&unit,&base,TRUE);assert(!unit.sync&&!allocations);fail_open=1;con_sync_set(&unit,&base,TRUE);assert(!unit.sync&&!allocations);fail_open=0;
 printf("Sync output PASS: end-marker splits, extension, timeout, 2MiB cap, allocation/parser failures, retry, close and timer cleanup\n");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(prefix+s+test)
 subprocess.run(['cc','-Wall','-Werror','-Wno-unused-variable','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
 for name,old,new in [
  ('missing timeout flush','sync_flush(CU(o), base);','(void)0;'),
  ('early buffer cap','s->length == CON_SYNC_MAX','s->length == CON_SYNC_MAX / 2'),
  ('missing close cancellation','    sync_cancel(s);\n    CloseDevice','    /* omitted */\n    CloseDevice')]:
  assert old in s
  (p/'test.c').write_text(prefix+s.replace(old,new)+test)
  subprocess.run(['cc','-Wall','-Werror','-Wno-unused-variable',str(p/'test.c'),'-o',str(p/'test')],check=True)
  result=subprocess.run([str(p/'test')],capture_output=True)
  assert result.returncode, name+' escaped detection'
  print('Negative control rejected:',name)
