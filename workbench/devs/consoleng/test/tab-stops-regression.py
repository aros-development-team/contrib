#!/usr/bin/env python3
"""Exercise actual tab storage/dispatch with full-width and legacy tables."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'stdconclass.c').read_text();h=(r/'console_gcc.h').read_text()
enum='enum'+h.split('enum',1)[1].split('};',1)[0]+'};'
a=s.index('static void stdcon_tab_import(');helper=s[a:s.index('static WORD stdcon_relative_coordinate(',a)]
a=s.index('    if (msg->Command == C_HTAB');dispatch=s[a:s.index('    if (msg->Command == C_SAVE_CURSOR',a)]
code=r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
typedef unsigned short UWORD;typedef unsigned char UBYTE;typedef intptr_t IPTR;typedef signed char BYTE;
#define MAXTABS 80
#define STDCON_TAB_COLUMNS 32768
#define CF_WRAP_PENDING 128
struct stdcondata {UBYTE tabBits[4096];UWORD tabMirror[MAXTABS];};
typedef struct {int x,xc,xmax,hidden,draws;unsigned conFlags;UWORD cu_TabStops[MAXTABS];} Object;
struct Msg {BYTE Command;IPTR*Params;};
#define CU(o) (o)
#define ICU(o) (o)
#define XCP (o->x)
#define XCCP (o->xc)
#define CHAR_XMIN(o) 0
#define CHAR_XMAX(o) (o->xmax)
#define CON_IS_OBSCURED(o) (o->hidden)
#define Console_UnRenderCursor(o) ((o)->draws++)
#define Console_RenderCursor(o) ((o)->draws++)
'''+enum+helper+'static void run(struct stdcondata*data,Object*o,struct Msg*msg){IPTR*params=msg->Params;'+dispatch+'}\n'+r'''
static void init(struct stdcondata*d,Object*o,int max){memset(o,0,sizeof(*o));o->xmax=max;memset(d->tabBits,1,sizeof(d->tabBits));stdcon_tab_export(d,o->cu_TabStops);}
static void cmd(struct stdcondata*d,Object*o,int c,IPTR p){struct Msg m={c,&p};run(d,o,&m);}
int main(void){struct stdcondata d;Object o;IPTR counts[]={0,1,2,32768,65535,INTPTR_MAX};int widths[]={1,7,8,9,79,80,81,631,632,633,1000,32768};
for(unsigned w=0;w<12;w++)for(int hidden=0;hidden<2;hidden++)for(int back=0;back<2;back++)for(unsigned n=0;n<6;n++){
 init(&d,&o,widths[w]-1);o.hidden=hidden;o.x=o.xc=widths[w]/2;o.conFlags=129;int want=o.x;IPTR remaining=counts[n]?counts[n]:1;
 while(remaining>0&&(back?want>0:want<o.xmax)){want+=back?-1:1;if(want%8==0)remaining--;}
 cmd(&d,&o,back?C_CURSOR_BACKTAB:C_CURSOR_HTAB,counts[n]);assert(o.x==want&&o.xc==want&&o.conFlags==1&&o.draws==(hidden?0:2));
}
init(&d,&o,32767);cmd(&d,&o,C_CLEAR_TABS,3);for(int i=0;i<4096;i++)assert(!d.tabBits[i]);
/* More stops than fit in public table; high-coordinate stop remains usable. */
for(int i=1;i<=200;i++){o.x=i;cmd(&d,&o,C_H_TAB_SET,0);}o.x=30000;cmd(&d,&o,C_H_TAB_SET,0);assert(o.cu_TabStops[79]==65535);
o.x=200;cmd(&d,&o,C_HTAB,0);assert(o.x==30000);cmd(&d,&o,C_CLEAR_TABS,0);o.x=200;cmd(&d,&o,C_HTAB,0);assert(o.x==32767);
/* Unsupported clear modes are ignored. CTC 0/2/5 use same storage. */
o.x=100;cmd(&d,&o,C_CLEAR_TABS,1);assert(d.tabBits[100>>3]&(1<<(100&7)));cmd(&d,&o,C_CURSOR_TAB_CTRL,2);assert(!(d.tabBits[100>>3]&(1<<(100&7))));cmd(&d,&o,C_CURSOR_TAB_CTRL,0);assert(d.tabBits[100>>3]&(1<<(100&7)));cmd(&d,&o,C_CURSOR_TAB_CTRL,5);o.x=20;cmd(&d,&o,C_CURSOR_BACKTAB,65535);assert(o.x==0);
/* Legacy direct table edits remain authoritative and are bounded even without sentinel. */
for(int i=0;i<80;i++){o.cu_TabStops[i]=1000+i;}o.x=0;cmd(&d,&o,C_HTAB,0);assert(o.x==1000);o.x=1079;cmd(&d,&o,C_HTAB,0);assert(o.x==32767);
o.cu_TabStops[0]=60000;o.cu_TabStops[1]=5;o.cu_TabStops[2]=65535;o.x=0;cmd(&d,&o,C_HTAB,0);assert(o.x==5);
return 0;}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)
 for negative in (False,True):
  candidate=code.replace('count = command == C_HTAB ? 1 : params[0];','count = command == C_HTAB ? 1 : (short)params[0];') if negative else code
  (p/'test.c').write_text(candidate)
  subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
  result=subprocess.run([str(p/'test')],capture_output=True);assert bool(result.returncode)==negative,result.stderr.decode()
print('PASS: 288 tab navigation cases; 201 custom stops; HTS/TBC/CTC; bounded legacy import; narrowing negative control')
