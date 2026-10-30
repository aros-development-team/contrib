#!/usr/bin/env python3
"""Actual DEC private parser: incomplete sequences cannot switch buffers."""
from pathlib import Path
import subprocess,tempfile
s=(Path(__file__).resolve().parents[1]/'support.c').read_text()
a=s.rindex('static BOOL getdecprivatecommand(');start=s.index('{',a);depth=1;b=start+1
while depth:
 if s[b]=='{':depth+=1
 elif s[b]=='}':depth-=1
 b+=1
f=s[a:b]
source=r'''
#include <assert.h>
#include <string.h>
typedef int BOOL;typedef signed char BYTE;typedef unsigned char UBYTE;typedef unsigned long ULONG;typedef long LONG;typedef long IPTR;
#define TRUE 1
#define FALSE 0
#define C_SET_ORIGIN_MODE 99
#define C_SET_SCROLL_REGION 100
#define C_ERASE_IN_DISPLAY 101
#define C_NIL 0
#define C_CURSOR_VISIBLE 1
#define C_CURSOR_INVISIBLE 2
#define C_ALTERNATE_SCREEN 3
#define C_SET_AUTOWRAP_MODE 4
#define C_RESET_AUTOWRAP_MODE 5
#define C_SET_CURSOR_BLINK 6
#define C_SET_SYNC_OUTPUT 7
#define CF_APPLICATION_CURSOR 8
#define CF_ALTERNATE_SCROLL 16
#define CF_BRACKETED_PASTE 1
#define CF_FOCUS_REPORTING 2
#define CF_COLOR_SCHEME_NOTIFY 4
typedef struct {int conFlags,calls,last,mouseReportedButtons,mouseMode,mouseSGR,mouseUTF8,mouseLastValid;} Object;
#define ICU(o) (o)
static void Console_DoCommand(Object *o,int cmd,int n,IPTR *p){if(cmd==100){assert(n==2&&p[0]==0&&p[1]==0);o->calls++;o->last=100;return;}if(cmd==101){assert(n==1&&p[0]==2&&o->last==100);o->calls++;o->last=101;return;}assert((cmd==C_SET_ORIGIN_MODE&&n==1)||(cmd==C_ALTERNATE_SCREEN&&n==1)||((cmd==6||cmd==7)&&n==1)||((cmd==4||cmd==5)&&n==0));o->calls++;o->last=p?*p:cmd;}
'''+f+r'''
int main(void){
 for(int set=0;set<2;set++)for(int length=0;length<=6;length++){
  Object sync={0};UBYTE *seq=(UBYTE*)(set?"?2026h":"?2026l"),*p=seq;BYTE cmd=99;
  int done=getdecprivatecommand(&cmd,&p,length,&sync);
  assert(done==(length==6));assert(sync.calls==(done?1:0));
  assert(sync.last==(done?set:0));assert(p==seq+(done?6:0));
 }

 for(int set=0;set<2;set++)for(int length=0;length<=3;length++){
  Object columns={0};UBYTE *seq=(UBYTE*)(set?"?3h":"?3l"),*p=seq;BYTE cmd=99;
  int done=getdecprivatecommand(&cmd,&p,length,&columns);
  assert(done==(length==3));assert(columns.calls==(done?2:0));
  assert(columns.last==(done?101:0));assert(p==seq+(done?3:0));
 }

 Object wheel={0};BYTE wheelcmd;UBYTE *wp=(UBYTE*)"?1007h";assert(getdecprivatecommand(&wheelcmd,&wp,6,&wheel));assert(wheel.conFlags==16);wp=(UBYTE*)"?1007l";assert(getdecprivatecommand(&wheelcmd,&wp,6,&wheel));assert(!wheel.conFlags);
 Object origin={0};BYTE oc;UBYTE *op=(UBYTE*)"?6h";
 assert(getdecprivatecommand(&oc,&op,3,&origin)&&origin.calls==1&&origin.last==1);
 op=(UBYTE*)"?6l";assert(getdecprivatecommand(&oc,&op,3,&origin)&&origin.calls==2&&origin.last==0);
 Object mouse={0};BYTE mc;UBYTE *mp;
 mp=(UBYTE*)"?1000;1006h";assert(getdecprivatecommand(&mc,&mp,11,&mouse)&&mouse.mouseMode==1000&&mouse.mouseSGR);
 mouse.mouseReportedButtons=7;
 mp=(UBYTE*)"?1002h";assert(getdecprivatecommand(&mc,&mp,6,&mouse)&&mouse.mouseMode==1002&&!mouse.mouseReportedButtons);
 mouse.mouseReportedButtons=7;
 mp=(UBYTE*)"?1000l";assert(getdecprivatecommand(&mc,&mp,6,&mouse)&&mouse.mouseMode==1002&&mouse.mouseReportedButtons==7);
 mp=(UBYTE*)"?1003h";assert(getdecprivatecommand(&mc,&mp,6,&mouse)&&mouse.mouseMode==1003&&!mouse.mouseReportedButtons);
 mp=(UBYTE*)"?1005h";assert(getdecprivatecommand(&mc,&mp,6,&mouse)&&mouse.mouseUTF8&&!mouse.mouseSGR);
 mp=(UBYTE*)"?1006h";assert(getdecprivatecommand(&mc,&mp,6,&mouse)&&!mouse.mouseUTF8&&mouse.mouseSGR);
 mp=(UBYTE*)"?1006;1005h";assert(getdecprivatecommand(&mc,&mp,11,&mouse)&&mouse.mouseUTF8&&!mouse.mouseSGR);
 mp=(UBYTE*)"?1005l";assert(getdecprivatecommand(&mc,&mp,6,&mouse)&&!mouse.mouseUTF8&&!mouse.mouseSGR);
 mp=(UBYTE*)"?1003;1006l";assert(getdecprivatecommand(&mc,&mp,11,&mouse)&&!mouse.mouseMode&&!mouse.mouseSGR);
 Object wrap={0};UBYTE *w=(UBYTE*)"?7;1h";BYTE wc;
 assert(getdecprivatecommand(&wc,&w,5,&wrap)&&wrap.last==4&&(wrap.conFlags&CF_APPLICATION_CURSOR));
 w=(UBYTE*)"?7l";assert(getdecprivatecommand(&wc,&w,3,&wrap)&&wrap.last==5);
 w=(UBYTE*)"?12h";assert(getdecprivatecommand(&wc,&w,4,&wrap)&&wrap.last==1);
 w=(UBYTE*)"?12l";assert(getdecprivatecommand(&wc,&w,4,&wrap)&&wrap.last==0);
 const char *cases[]={"?1049h","?1049l","?1004;1049h","?1049;2004h"};
 for(int c=0;c<4;c++)for(int n=1;n<strlen(cases[c]);n++){
  Object modes={0};UBYTE *m=(UBYTE*)"?1h";BYTE modecmd;
 assert(getdecprivatecommand(&modecmd,&m,3,&modes)&&(modes.conFlags&CF_APPLICATION_CURSOR));
 m=(UBYTE*)"?1l";assert(getdecprivatecommand(&modecmd,&m,3,&modes)&&!modes.conFlags);
 Object o={0};UBYTE *p=(UBYTE*)cases[c];BYTE cmd=99;
  assert(!getdecprivatecommand(&cmd,&p,n,&o));assert(o.calls==0&&o.conFlags==0&&p==(UBYTE*)cases[c]);
 }
 for(int c=0;c<4;c++){
  Object modes={0};UBYTE *m=(UBYTE*)"?1h";BYTE modecmd;
 assert(getdecprivatecommand(&modecmd,&m,3,&modes)&&(modes.conFlags&CF_APPLICATION_CURSOR));
 m=(UBYTE*)"?1l";assert(getdecprivatecommand(&modecmd,&m,3,&modes)&&!modes.conFlags);
 Object o={0};UBYTE *p=(UBYTE*)cases[c];BYTE cmd=99;
  assert(getdecprivatecommand(&cmd,&p,strlen(cases[c]),&o));
  assert(o.calls==1&&o.last==(c!=1)&&*p==0&&cmd==C_NIL);
 }
 Object modes={0};UBYTE *m=(UBYTE*)"?1h";BYTE modecmd;
 assert(getdecprivatecommand(&modecmd,&m,3,&modes)&&(modes.conFlags&CF_APPLICATION_CURSOR));
 m=(UBYTE*)"?1l";assert(getdecprivatecommand(&modecmd,&m,3,&modes)&&!modes.conFlags);
 Object o={0};UBYTE *p=(UBYTE*)"?1049z";BYTE cmd;
 assert(getdecprivatecommand(&cmd,&p,6,&o)&&o.calls==0);
 return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';b=Path(d)/'test';p.write_text(source)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(b)],check=True)
 subprocess.run([str(b)],check=True)
 p.write_text(source.replace('            case 3:', '            case 65534:',1))
 subprocess.run(['cc','-Wall','-Werror',str(p),'-o',str(b)],check=True)
 assert subprocess.run([str(b)],capture_output=True).returncode!=0
print('Alternate parser PASS: split sequences, combined modes, set/reset, DECCOLM reset ordering and negative control, ignored final')
