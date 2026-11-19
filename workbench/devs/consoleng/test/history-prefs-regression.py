#!/usr/bin/env python3
"""Exercise actual history device dispatch and charmap command branch."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
s=(r/'console.c').read_text();c=(r/'charmapconclass.c').read_text();h=(r/'include/consoleng.h').read_text()
h=h[h.index('#define CD_GETCURSORPREFS'):h.index('#endif /* DEVICES_CONSOLENG_H */')]
body=s[s.index('    case CD_GETHISTORYPREFS:'):s.index('    case CD_GETDEFAULTCURSORPREFS:')]
a=c.index('    if (msg->Command == C_GET_HISTORY_PREFS')
b=c.index('{',a);depth=1;b+=1
while depth:
 depth+=(c[b]=='{')-(c[b]=='}');b+=1
branch=c[a:b]
prefix=r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
typedef uint32_t ULONG;typedef intptr_t IPTR;typedef int BOOL;typedef void *APTR;
#define TRUE 1
#define FALSE 0
#define CMD_NONSTD 9
#define CONU_LIBRARY -1
#define IOERR_BADADDRESS -5
#define IOERR_BADLENGTH -4
#define IOERR_NOCMD -3
#define C_GET_HISTORY_PREFS 1
#define C_SET_HISTORY_PREFS 2
#define CopyMem(a,b,n) memcpy(b,a,n)
#define ObtainSemaphore(x) ((void)0)
#define ReleaseSemaphore(x) ((void)0)
struct Unit {int supported,alternate;ULONG primary_max,scrollback_max,viewport;};
typedef struct Unit Object;
struct P_Console_DoCommand {int Command;};
struct IOStdReq {APTR io_Data;struct Unit *io_Unit;ULONG io_Length,io_Actual,io_Command;};
'''
extra=r'''
static struct {struct ConsolengHistoryPrefs historyDefaults;} device,*ConsoleDevice=&device;
static void Console_DoCommand(Object *data,int cmd,int n,IPTR *params){
 assert(n==2);if(!data->supported)return;
 struct P_Console_DoCommand m={cmd},*msg=&m;
'''+branch+r'''
}
static int request(struct IOStdReq *ioreq){int error=0;switch(ioreq->io_Command){
'''+body+r'''
}return error;}
int main(void){
 struct Unit unit={1,0,42,1000,17};
 struct ConsolengHistoryPrefs p={12,1,5000};
 struct IOStdReq io={&p,&unit,12,999,CD_SETHISTORYPREFS};
 assert(!request(&io)&&io.io_Actual==12&&unit.scrollback_max==5000&&unit.viewport==17);
 unit.alternate=1;unit.scrollback_max=0;p.hp_Lines=81;
 assert(!request(&io)&&unit.primary_max==81&&unit.scrollback_max==0&&unit.viewport==17);
 io.io_Command=CD_GETHISTORYPREFS;memset(&p,0,sizeof(p));assert(!request(&io)&&p.hp_Lines==81&&p.hp_Size==12&&p.hp_Version==1);
 ULONG sizes[]={0,11,12,16},versions[]={0,1,2},lines[]={0,1,1000,CONSOLENG_HISTORY_MAX_LINES,CONSOLENG_HISTORY_MAX_LINES+1,UINT32_MAX};
 for(unsigned a=0;a<4;a++)for(unsigned b=0;b<3;b++)for(unsigned c=0;c<6;c++){
  p=(struct ConsolengHistoryPrefs){sizes[a],versions[b],lines[c]};
  unit.primary_max=81;io.io_Command=CD_SETHISTORYPREFS;
  int valid=a==2&&b==1&&c<4;
  assert(request(&io)==(valid?0:CONSOLENG_ERR_BADPREFS));assert(io.io_Actual==(valid?12:0));
  assert(unit.primary_max==(valid?lines[c]:81)&&unit.scrollback_max==0&&unit.viewport==17);
 }
 p=(struct ConsolengHistoryPrefs){12,1,4096};io.io_Unit=(struct Unit*)CONU_LIBRARY;io.io_Command=CD_SETDEFAULTHISTORYPREFS;
 assert(!request(&io)&&ConsoleDevice->historyDefaults.hp_Lines==4096);
 p.hp_Lines=0;io.io_Command=CD_GETDEFAULTHISTORYPREFS;assert(!request(&io)&&p.hp_Lines==4096);
 io.io_Unit=&unit;assert(request(&io)==IOERR_BADADDRESS&&io.io_Actual==0);
 io.io_Command=CD_GETHISTORYPREFS;io.io_Length=11;assert(request(&io)==IOERR_BADLENGTH);
 io.io_Length=12;io.io_Data=0;assert(request(&io)==IOERR_BADADDRESS);
 io.io_Data=&p;unit.supported=0;assert(request(&io)==IOERR_NOCMD&&io.io_Actual==0);
 unit.supported=1;
 struct {struct ConsolengHistoryPrefs p;ULONG guard;} out={{0},0x12345678};
 io.io_Data=&out;io.io_Length=sizeof(out);assert(!request(&io)&&io.io_Actual==12&&out.guard==0x12345678);
 return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';exe=Path(d)/'test';p.write_text(prefix+h+extra)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
print('History prefs PASS: 72 schema/range cases, atomic rejection, primary/alternate isolation, unchanged viewport, defaults, bounded GET and unsupported units')
