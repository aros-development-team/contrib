#!/usr/bin/env python3
"""Actual CMD_READ dispatch: zero reads and invalid destinations never queue."""
from pathlib import Path
import tempfile,subprocess,resource
s=(Path(__file__).resolve().parents[1]/'console.c').read_text();a=s.index('    case CMD_READ:',s.index('AROS_LH1(void, beginio'));b=s.index('    case ',a+len('    case CMD_READ:'));body=s[a:b]
assert body.count('case ')==1 and 'done_quick = FALSE' in body
code=r'''
#include <assert.h>
#include <stdint.h>
typedef uint32_t ULONG;typedef int BOOL;
#define TRUE 1
#define FALSE 0
#define DEBUG 0
#define D(x)
#define CMD_READ 2
#define CONU_LIBRARY -1
#define IOERR_BADADDRESS -5
struct Unit {int x;};struct IOStdReq {struct Unit*io_Unit;void*io_Data;ULONG io_Length,io_Actual;int io_Error;};
static int begin(struct IOStdReq*ioreq){int error=0;BOOL done_quick=TRUE;switch(CMD_READ){
'''+body+r'''
}if(done_quick)ioreq->io_Error=error;return !done_quick;}
int main(void){struct Unit valid;char buffer[4];ULONG lengths[]={0,1,512,65536,0xffffffffU};
 struct IOStdReq empty={&valid,0,0,999,17};assert(!begin(&empty)&&empty.io_Actual==0&&empty.io_Error==0);
 for(int unit=0;unit<3;unit++)for(int data=0;data<2;data++)for(int n=0;n<5;n++){
  struct IOStdReq req={unit==0?0:unit==1?(struct Unit*)CONU_LIBRARY:&valid,data?buffer:0,lengths[n],999,17};
  int invalid=unit!=2||(lengths[n]&&!data);int queued=begin(&req);
  assert(queued==(!invalid&&lengths[n]!=0));assert(!req.io_Actual);
  assert(req.io_Error==(invalid?IOERR_BADADDRESS:0));
 }
 return 0;}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(code)
 subprocess.run(['cc','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
 old="    case CMD_READ: ioreq->io_Actual=0;ioreq->io_Error=0;done_quick=FALSE;break;\n"
 (p/'negative.c').write_text(code.replace(body,old))
 subprocess.run(['cc','-Wall','-Wextra','-Werror',str(p/'negative.c'),'-o',str(p/'negative')],check=True)
 def no_core():resource.setrlimit(resource.RLIMIT_CORE,(0,0))
 negative=subprocess.run([str(p/'negative')],capture_output=True,timeout=10,preexec_fn=no_core)
 assert negative.returncode!=0 and b'!begin(&empty)' in negative.stderr
 print('NEGATIVE CONTROL: former dispatch queues a zero-length read and fails immediate-completion assertion')
print('PASS: 30 read unit/buffer/length cases; zero-length immediate, invalid address rejected, unsigned capacities preserved')
