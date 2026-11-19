#!/usr/bin/env python3
"""Actual font profile dispatch: reject atomically and preserve bounded snapshots."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'console.c').read_text();h=(r/'include/consoleng.h').read_text()
h=h[h.index('#define CD_GETCURSORPREFS'):h.index('#endif /* DEVICES_CONSOLENG_H */')]
a=s.index('static BOOL ConsolengValidCursorPrefs(');validator=s[a:s.index('static int GM_UNIQUENAME(Init)',a)]
body=s[s.index('    case CD_GETDEFAULTPROFILE:'):s.index('    case CD_GETDEFAULTPREFS:')]
a=s.index('static BOOL ConsolengValidFontPrefs(');validator=s[a:s.index('static BOOL ConsolengFontAvailable(',a)]+validator
prefix=r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
typedef uint32_t ULONG;typedef unsigned char UBYTE;typedef int BOOL;
#define TRUE 1
#define FALSE 0
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
static struct {struct ConsolengCursorPrefs cursorDefaults;struct ConsolengHistoryPrefs historyDefaults;struct ConsolengFontPrefs fontDefaults;} state,*ConsoleDevice=&state;
static int ConsolengFontAvailable(const struct ConsolengFontPrefs *p){return strcmp(p->fp_Name,"missing.font")!=0;}
static int request(struct IOStdReq *ioreq){int error=0;switch(ioreq->io_Command){
'''+body+r'''
}return error;}
int main(void){
 struct ConsolengDefaultProfile initial={180,1,{20,1,0,500,0},{12,1,1000},{140,1,0,{0}}},candidate={180,1,{20,1,5,350,8000},{12,1,4096},{140,1,16,"fixed.font"}};
 struct IOStdReq io={&candidate,(struct Unit*)CONU_LIBRARY,180,999,CD_SETDEFAULTPROFILE};
 state.cursorDefaults=initial.dp_Cursor;state.historyDefaults=initial.dp_History;
 assert(!request(&io)&&io.io_Actual==180&&acquires==1&&!locked);
 assert(!memcmp(&state.cursorDefaults,&candidate.dp_Cursor,20)&&!memcmp(&state.historyDefaults,&candidate.dp_History,12));
 for(int field=0;field<13;field++){
  struct ConsolengDefaultProfile p=candidate;((ULONG*)&p)[field]=0xffffffffU;io.io_Data=&p;acquires=0;
  assert(request(&io)==CONSOLENG_ERR_BADPREFS&&io.io_Actual==0&&!acquires);
  assert(!memcmp(&state.cursorDefaults,&candidate.dp_Cursor,20)&&!memcmp(&state.historyDefaults,&candidate.dp_History,12));
 }
 struct ConsolengDefaultProfile bad=candidate;memset(bad.dp_Font.fp_Name,'x',128);io.io_Data=&bad;
 assert(request(&io)==CONSOLENG_ERR_BADPREFS&&io.io_Actual==0);
 bad=candidate;strcpy(bad.dp_Font.fp_Name,"missing.font");
 assert(request(&io)==CONSOLENG_ERR_BADPREFS&&io.io_Actual==0);
 assert(!memcmp(&state.fontDefaults,&candidate.dp_Font,sizeof(candidate.dp_Font)));
 struct {struct ConsolengDefaultProfile p;ULONG guard;} out={{0},0x12345678};
 io.io_Data=&out;io.io_Length=sizeof(out);io.io_Command=CD_GETDEFAULTPROFILE;acquires=0;
 assert(!request(&io)&&io.io_Actual==180&&acquires==1&&!locked&&out.guard==0x12345678);
 assert(!memcmp(&out.p,&candidate,sizeof(candidate)));
 io.io_Length=179;assert(request(&io)==IOERR_BADLENGTH&&io.io_Actual==0);
 io.io_Length=180;io.io_Data=0;assert(request(&io)==IOERR_BADADDRESS);
 io.io_Data=&out;io.io_Unit=0;assert(request(&io)==IOERR_BADADDRESS);
 return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';exe=Path(d)/'test';p.write_text(prefix+h+validator+extra)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
print('Font profile PASS: one publication lock, coherent snapshot, rejection of each malformed field without mutation, bounded GET and invalid buffer/unit')
