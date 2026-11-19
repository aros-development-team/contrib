#!/usr/bin/env python3
"""Actual read-only paste-mode command: validation, bounds and flag isolation."""
from pathlib import Path
import subprocess,tempfile,re
r=Path(__file__).resolve().parents[1];s=(r/'console.c').read_text();h=(r/'include/consoleng.h').read_text()
assert '#define CD_GETPASTEMODE (CMD_NONSTD + 25)' in h
body=s[s.index('    case CD_GETPASTEMODE:'):s.index('    case CD_GETCOLORPREFS:')]
code=r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
typedef uint32_t ULONG;
struct Unit {ULONG conFlags;};
struct IO {void *io_Data;struct Unit *io_Unit;ULONG io_Actual,io_Length;};
struct Base {int consoleTaskLock;};
#define CD_GETPASTEMODE 34
#define CONU_LIBRARY -1
#define IOERR_BADADDRESS 1
#define IOERR_BADLENGTH 2
#define CF_BRACKETED_PASTE 16
#define ICU(p) (p)
#define CopyMem(a,b,n) memcpy(b,a,n)
static int held;
static void ObtainSemaphore(int *p){(void)p;assert(!held);held=1;}
static void ReleaseSemaphore(int *p){(void)p;assert(held);held=0;}
static int run(struct IO *ioreq,struct Base *ConsoleDevice){int error=0;switch(CD_GETPASTEMODE){
'''+body+r'''
}return error;}
int main(void){
 struct Base base={0};
 for(unsigned flags=0;flags<256;flags++)for(unsigned len=0;len<12;len++)for(int kind=0;kind<4;kind++){
  unsigned char out[16];memset(out,0xa5,sizeof(out));struct Unit unit={flags};
  struct IO io={kind==3?0:out+1,kind==1?0:kind==2?(struct Unit*)CONU_LIBRARY:&unit,99,len};
  int error=run(&io,&base);assert(!held&&unit.conFlags==flags);
  if(kind){assert(error==IOERR_BADADDRESS&&!io.io_Actual);for(int i=0;i<16;i++)assert(out[i]==0xa5);}
  else if(len<4){assert(error==IOERR_BADLENGTH&&!io.io_Actual);for(int i=0;i<16;i++)assert(out[i]==0xa5);}
  else {ULONG got;memcpy(&got,out+1,4);assert(!error&&io.io_Actual==4&&got==!!(flags&16));assert(out[0]==0xa5);for(int i=5;i<16;i++)assert(out[i]==0xa5);}
 }
 return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(code)
 subprocess.run(['cc','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS: paste-mode query, 12288 flags/length/address cases, exact unaligned copy and unchanged state')
