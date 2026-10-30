#!/usr/bin/env python3
"""Exercise production wheel dispatch and clamped viewport movement."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'console.c').read_text();c=(r/'charmapconclass.c').read_text()
h=(r/'include/consoleng.h').read_text();h=h[h.index('#define CD_GETSCROLLPREFS'):h.index('/* Combined future-console defaults, including wheel distance. */')]
dispatch=s[s.index('    case CD_GETSCROLLPREFS:'):s.index('    case CD_GETBINDINGPREFS:')]
wheel=c[c.index('static VOID charmap_wheel_action('):c.index('static VOID charmapcon_handlegadgets(')]
prefix=r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
typedef uint32_t ULONG;typedef int BOOL;typedef void VOID;typedef void Class;
#define TRUE 1
#define FALSE 0
#define CMD_NONSTD 9
#define CONU_LIBRARY -1
#define IOERR_BADADDRESS -5
#define IOERR_BADLENGTH -4
#define CONSOLENG_ERR_BADPREFS -20
struct SignalSemaphore {int held;};
static void ObtainSemaphore(struct SignalSemaphore*l){assert(!l->held);l->held++;}
static void ReleaseSemaphore(struct SignalSemaphore*l){assert(l->held==1);l->held--;}
#define CopyMem(a,b,n) memcpy(b,a,n)
struct Unit {ULONG wheelLines;};
struct charmapcondata {BOOL alternate;ULONG scrollback_pos,limit;};
typedef struct {struct Unit unit;struct charmapcondata data;BOOL hidden;ULONG cursor;} Object;
#define ICU(o) ((struct Unit*)(o))
#define INST_DATA(c,o) (&(o)->data)
#define CON_IS_OBSCURED(o) ((o)->hidden)
struct IOStdReq {void*io_Data;struct Unit*io_Unit;ULONG io_Length,io_Actual,io_Command;};
static struct {struct SignalSemaphore unitListLock,consoleTaskLock;ULONG wheelLinesDefault;} device,*ConsoleDevice=&device;
static unsigned closed,moved,adjusted;
static void charmap_search_close(Class*c,Object*o,BOOL redraw){assert(redraw);closed++;}
static ULONG charmap_history_limit(struct charmapcondata*d){return d->limit;}
static void charmapcon_scroll_to(Class*c,Object*o,ULONG pos){assert(pos<=o->data.limit);o->data.scrollback_pos=pos;moved++;}
static void charmapcon_adj_prop(Class*c,Object*o){adjusted++;}
'''
main=r'''
static int request(struct IOStdReq*ioreq){int error=0;switch(ioreq->io_Command){
'''+dispatch+r'''
}return error;}
int main(void){
 Object o={0};struct ConsolengScrollPrefs p={12,1,3};struct IOStdReq io={&p,&o.unit,12,999,CD_SETSCROLLPREFS};
 for(unsigned def=0;def<2;def++)for(unsigned size=0;size<4;size++)for(unsigned version=0;version<3;version++)for(unsigned speed=0;speed<6;speed++){
  ULONG sizes[]={0,11,12,16},speeds[]={0,1,3,100,101,UINT32_MAX};
  p=(struct ConsolengScrollPrefs){sizes[size],version,speeds[speed]};
  o.unit.wheelLines=7;device.wheelLinesDefault=9;io.io_Unit=def?(struct Unit*)CONU_LIBRARY:&o.unit;io.io_Command=def?CD_SETDEFAULTSCROLLPREFS:CD_SETSCROLLPREFS;
  BOOL ok=size==2&&version==1&&speed<4;assert(request(&io)==(ok?0:CONSOLENG_ERR_BADPREFS));
  assert(io.io_Actual==(ok?12:0));assert(o.unit.wheelLines==(ok&&!def?speeds[speed]:7));assert(device.wheelLinesDefault==(ok&&def?speeds[speed]:9));
 }
 p=(struct ConsolengScrollPrefs){12,1,3};io.io_Command=CD_SETSCROLLPREFS;io.io_Unit=&o.unit;
 io.io_Length=11;assert(request(&io)==IOERR_BADLENGTH&&!io.io_Actual);io.io_Length=12;
 io.io_Data=NULL;assert(request(&io)==IOERR_BADADDRESS);io.io_Data=&p;
 io.io_Unit=NULL;assert(request(&io)==IOERR_BADADDRESS);io.io_Unit=(struct Unit*)CONU_LIBRARY;assert(request(&io)==IOERR_BADADDRESS);
 io.io_Command=CD_GETDEFAULTSCROLLPREFS;io.io_Unit=&o.unit;assert(request(&io)==IOERR_BADADDRESS);
 struct {struct ConsolengScrollPrefs p;ULONG guard;} out={{0},0xabcdef};io.io_Data=&out;io.io_Length=sizeof(out);
 io.io_Unit=(struct Unit*)CONU_LIBRARY;device.wheelLinesDefault=31;assert(!request(&io)&&io.io_Actual==12&&out.p.sp_Lines==31&&out.guard==0xabcdef);
 io.io_Command=CD_GETSCROLLPREFS;io.io_Unit=&o.unit;o.unit.wheelLines=17;assert(!request(&io)&&out.p.sp_Lines==17&&out.p.sp_Size==12&&out.p.sp_Version==1);
 unsigned cases=0;
 for(unsigned limit=0;limit<130;limit++)for(unsigned pos=0;pos<=limit;pos++)for(unsigned speed=0;speed<=100;speed+=10)for(unsigned down=0;down<2;down++)for(unsigned mode=0;mode<3;mode++){
  o=(Object){{speed},{mode==1,pos,limit},mode==2,42};closed=moved=adjusted=0;
  unsigned expected=pos;if(speed&&!mode)expected=down?(speed>limit-pos?limit:pos+speed):(speed>pos?0:pos-speed);
  charmap_wheel_action(NULL,&o,down);assert(o.data.scrollback_pos==expected&&o.cursor==42);
  assert(closed==(speed&&!mode)&&moved==(expected!=pos)&&adjusted==moved);cases++;
 }
 printf("Wheel PASS: 144 schema/isolation cases; %u viewport cases; no per-line redraw loop\n",cases);
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'t.c';b=Path(d)/'t';code=prefix+h+wheel+main;p.write_text(code)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(b)],check=True);subprocess.run([str(b)],check=True)
 for label,old,new in [('alternate isolation','data->alternate || ','FALSE || '),('zero disabled','if(!lines || ','if(FALSE || '),('range rejection','prefs.sp_Lines>CONSOLENG_SCROLL_MAX_LINES','FALSE')]:
  broken=code.replace(old,new);assert broken!=code;p.write_text(broken)
  subprocess.run(['cc','-Wall','-Werror',str(p),'-o',str(b)],check=True);assert subprocess.run([str(b)],capture_output=True).returncode
  print('Negative control PASS:',label)
