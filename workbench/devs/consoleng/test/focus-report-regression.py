#!/usr/bin/env python3
"""Compile actual focus reporting and check mode, routing and atomic queue bounds."""
from pathlib import Path
import subprocess, tempfile
root=Path(__file__).resolve().parents[1]
source=(root/'consoletask.c').read_text()
start=source.index('static void report_focus_event(');end=source.index('{',start)+1;depth=1
while depth:
 depth+=(source[end]=='{')-(source[end]=='}');end+=1
function=source[start:end]
# Integration gate: raw-event consumers must suppress the secondary focus packet.
assert 'if (!rawEvent)\n                        report_focus_event(cdihmsg->unit, &cdihmsg->ie, ConsoleDevice);' in source
prefix=r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
typedef unsigned char UBYTE;
#define CF_FOCUS_REPORTING 32
#define IECLASS_ACTIVEWINDOW 11
#define IECLASS_INACTIVEWINDOW 12
#define CON_INPUTBUF_SIZE 512
struct intConUnit {uint32_t conFlags,numStoredChars;UBYTE inputBuf[512];};
typedef struct intConUnit Object;
struct InputEvent {int ie_Class;};
struct ConsoleBase {int unused;};
#define ICU(o) (o)
#define CopyMem(a,b,n) memcpy(b,a,n)
static int answered;
static void answer_requests(Object *o,struct ConsoleBase *b){(void)o;(void)b;answered++;}
'''
main=r'''
int main(void){
 for(int flags=0;flags<64;flags++)for(int cls=0;cls<16;cls++)for(int used=0;used<=512;used++){
  Object o;memset(&o,0xa5,sizeof(o));o.conFlags=flags;o.numStoredChars=used;
  Object before=o;struct InputEvent e={cls};answered=0;
  report_focus_event(&o,&e,0);
  int expect=(flags&CF_FOCUS_REPORTING)&&(cls==11||cls==12)&&used<=509;
  assert(answered==expect);assert(o.conFlags==before.conFlags);
  assert(o.numStoredChars==(uint32_t)(used+3*expect));
  if(expect){assert(!memcmp(o.inputBuf,before.inputBuf,used));assert(!memcmp(o.inputBuf+used,cls==11?"\033[I":"\033[O",3));assert(!memcmp(o.inputBuf+used+3,before.inputBuf+used+3,509-used));}
  else assert(!memcmp(o.inputBuf,before.inputBuf,512));
 }
 return 0;
}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp);(p/'test.c').write_text(prefix+function+main)
 subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS: focus mode/events, 525312 queue cases, atomic reports and raw-event precedence')
