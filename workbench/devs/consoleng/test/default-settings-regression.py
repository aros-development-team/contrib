#!/usr/bin/env python3
"""Actual combined-defaults dispatch: reject atomically and hold publication lock."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'console.c').read_text();h=(r/'include/consoleng.h').read_text()
h=h[h.index('#define CD_GETCURSORPREFS'):h.index('#endif /* DEVICES_CONSOLENG_H */')]
a=s.index('static BOOL ConsolengValidCursorPrefs(');validator=s[a:s.index('static int GM_UNIQUENAME(Init)',a)]
body=s[s.index('    case CD_GETDEFAULTPREFS:'):s.index('    case CD_GETHISTORYPREFS:')]
prefix=r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
typedef uint32_t ULONG;typedef int BOOL;
#define CMD_NONSTD 9
#define CONU_LIBRARY -1
#define IOERR_BADADDRESS -5
#define IOERR_BADLENGTH -4
#define CopyMem(a,b,n) memcpy(b,a,n)
static int locked,acquires;
#define ObtainSemaphore(x) do{assert(!locked);locked=1;acquires++;}while(0)
#define ReleaseSemaphore(x) do{assert(locked);locked=0;}while(0)
struct Unit {int unused;};
struct IOStdReq {void *io_Data;struct Unit *io_Unit;ULONG io_Length,io_Actual,io_Command;};
'''
extra=r'''
static struct {struct ConsolengCursorPrefs cursorDefaults;struct ConsolengHistoryPrefs historyDefaults;} state,*ConsoleDevice=&state;
static int request(struct IOStdReq *ioreq){int error=0;switch(ioreq->io_Command){
'''+body+r'''
}return error;}
int main(void){
 struct ConsolengDefaultPrefs initial={40,1,{20,1,0,500,0},{12,1,1000}},candidate={40,1,{20,1,5,350,8000},{12,1,4096}};
 struct IOStdReq io={&candidate,(struct Unit*)CONU_LIBRARY,40,999,CD_SETDEFAULTPREFS};
 state.cursorDefaults=initial.dp_Cursor;state.historyDefaults=initial.dp_History;
 assert(!request(&io)&&io.io_Actual==40&&acquires==1&&!locked);
 assert(!memcmp(&state.cursorDefaults,&candidate.dp_Cursor,20)&&!memcmp(&state.historyDefaults,&candidate.dp_History,12));
 for(int field=0;field<10;field++){
  struct ConsolengDefaultPrefs p=candidate;((ULONG*)&p)[field]=0xffffffffU;io.io_Data=&p;acquires=0;
  assert(request(&io)==CONSOLENG_ERR_BADPREFS&&io.io_Actual==0&&!acquires);
  assert(!memcmp(&state.cursorDefaults,&candidate.dp_Cursor,20)&&!memcmp(&state.historyDefaults,&candidate.dp_History,12));
 }
 struct {struct ConsolengDefaultPrefs p;ULONG guard;} out={{0},0x12345678};
 io.io_Data=&out;io.io_Length=sizeof(out);io.io_Command=CD_GETDEFAULTPREFS;acquires=0;
 assert(!request(&io)&&io.io_Actual==40&&acquires==1&&!locked&&out.guard==0x12345678);
 assert(!memcmp(&out.p,&candidate,40));
 io.io_Length=39;assert(request(&io)==IOERR_BADLENGTH&&io.io_Actual==0);
 io.io_Length=40;io.io_Data=0;assert(request(&io)==IOERR_BADADDRESS);
 io.io_Data=&out;io.io_Unit=0;assert(request(&io)==IOERR_BADADDRESS);
 return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';exe=Path(d)/'test';p.write_text(prefix+h+validator+extra)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
print('Default settings PASS: one publication lock, coherent snapshot, rejection of each malformed field without mutation, bounded GET and invalid buffer/unit')
