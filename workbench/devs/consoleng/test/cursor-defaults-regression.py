#!/usr/bin/env python3
"""Actual library-unit defaults commands: atomic validation and bounded access."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'console.c').read_text();h=(r/'include/consoleng.h').read_text()
header=h[h.index('#define CD_GETCURSORPREFS'):h.index('#endif /* DEVICES_CONSOLENG_H */')]
a=s.index('static BOOL ConsolengValidCursorPrefs(');b=s.index('static int GM_UNIQUENAME(Init)',a);validator=s[a:b]
a=s.index('    case CD_GETDEFAULTCURSORPREFS:');b=s.index('    case CD_GETCURSORPREFS:',a);body=s[a:b]
prefix=r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
typedef uint32_t ULONG;typedef int BOOL;typedef long LONG;typedef intptr_t IPTR;typedef void Object;
#define CMD_NONSTD 9
#define CONU_LIBRARY -1
#define IOERR_BADADDRESS -5
#define IOERR_BADLENGTH -4
#define C_GET_CURSOR_PREFS 1
#define C_SET_CURSOR_PREFS 2
struct Unit {int unused;};
struct IOStdReq {void *io_Data;struct Unit *io_Unit;ULONG io_Length,io_Actual,io_Command;};
#define CopyMem(a,b,n) memcpy(b,a,n)
#define ObtainSemaphore(x) ((void)0)
#define ReleaseSemaphore(x) ((void)0)
'''
extra=r'''
static struct {struct ConsolengCursorPrefs cursorDefaults;} device;
#define ConsoleDevice (&device)
#define current device.cursorDefaults
static int request(struct IOStdReq *ioreq){LONG error=0;switch(ioreq->io_Command){
'''+body+r'''
}return error;}
int main(void){
 const ULONG sizes[]={0,20,24},versions[]={0,1,2},styles[]={0,1,6,7},intervals[]={0,49,50,500,5000,5001,UINT32_MAX},timeouts[]={0,999,1000,3600000,3600001};
 int cases=0;
 for(int a=0;a<3;a++)for(int b=0;b<3;b++)for(int c=0;c<4;c++)for(int d=0;d<7;d++)for(int e=0;e<5;e++){
  struct ConsolengCursorPrefs before={20,1,2,500,0},candidate={sizes[a],versions[b],styles[c],intervals[d],timeouts[e]};current=before;
  struct IOStdReq io={&candidate,(struct Unit*)CONU_LIBRARY,20,999,CD_SETDEFAULTCURSORPREFS};
  int valid=a==1&&b==1&&c<3&&d>=2&&d<=4&&(e==0||e==2||e==3);
  assert(request(&io)==(valid?0:CONSOLENG_ERR_BADPREFS));assert(io.io_Actual==(valid?20:0));
  assert(!memcmp(&current,valid?&candidate:&before,20));cases++;
 }
 assert(cases==1260);
 struct {struct ConsolengCursorPrefs p;ULONG guard;} out={{0},0x12345678};
 struct IOStdReq io={&out,(struct Unit*)CONU_LIBRARY,24,999,CD_GETDEFAULTCURSORPREFS};assert(!request(&io)&&io.io_Actual==20&&out.guard==0x12345678);assert(!memcmp(&out.p,&current,20));
 io.io_Length=19;io.io_Actual=999;assert(request(&io)==IOERR_BADLENGTH&&io.io_Actual==0);
 io.io_Length=20;io.io_Data=0;assert(request(&io)==IOERR_BADADDRESS&&io.io_Actual==0);
 io.io_Data=&out;io.io_Unit=NULL;assert(request(&io)==IOERR_BADADDRESS);
 struct Unit unit={0};io.io_Unit=&unit;assert(request(&io)==IOERR_BADADDRESS);
 return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';b=Path(d)/'test';p.write_text(prefix+header+validator+extra)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(b)],check=True)
 subprocess.run([str(b)],check=True)
print('Cursor defaults PASS: 1260 validation/atomicity cases, bounded snapshot and invalid buffer/unit rejection')
