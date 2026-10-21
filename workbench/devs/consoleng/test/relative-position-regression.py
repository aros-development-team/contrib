#!/usr/bin/env python3
"""Actual relative dispatch and hidden controls: no wrapping or painting."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'stdconclass.c').read_text();h=(r/'console_gcc.h').read_text()
enum='enum'+h.split('enum',1)[1].split('};',1)[0]+'};'
a=s.index('static WORD stdcon_relative_coordinate(');helpers=s[a:s.index('/* Saved cursor and logical rendition',a)]
a=s.index('    if ((msg->Command >= C_CURSOR_UP');dispatch=s[a:s.index('    if (msg->Command == C_HTAB',a)]
a=s.index('        case C_FORMFEED:',s.index('static VOID stdcon_docommand('));hidden=s[a:s.index('        case C_ASCII:',a)]
code=r'''
#include <assert.h>
#include <stdint.h>
typedef short WORD;typedef int BOOL;typedef signed char BYTE;typedef long LONG;typedef intptr_t IPTR;typedef void VOID;
#define TRUE 1
#define FALSE 0
#define CF_WRAP_PENDING 128
#define M_LNM 1
typedef struct {WORD x,xc,y,yc,xmax,ymax;int hidden,draws,scrolls,mode;unsigned conFlags;} Object;
struct Msg {BYTE Command;IPTR*Params;};
#define ICU(o) (o)
#define XCP (o->x)
#define XCCP (o->xc)
#define YCP (o->y)
#define YCCP (o->yc)
#define CHAR_XMIN(o) 0
#define CHAR_YMIN(o) 0
#define CON_SCROLL_TOP(o) 0
#define CON_SCROLL_BOTTOM(o) CHAR_YMAX(o)
#define CHAR_XMAX(o) (o->xmax)
#define CHAR_YMAX(o) (o->ymax)
#define CON_IS_OBSCURED(o) (o->hidden)
#define CHECK_MODE(o,m) (o->mode)
#define Console_UnRenderCursor(o) ((o)->draws++)
#define Console_RenderCursor(o) ((o)->draws++)
static void down(Object*o,int n){assert(n==1);if(o->y==o->ymax)o->scrolls++;else o->y++;o->yc=o->y;}
static void up(Object*o,int n){assert(n==1);if(!o->y)o->scrolls++;else o->y--;o->yc=o->y;}
#define Console_Down(o,n) down(o,n)
#define Console_Up(o,n) up(o,n)
'''+enum+helpers+'static void run(Object*o,struct Msg*msg){IPTR*params=msg->Params;'+dispatch+'}\n'+'static void hidden_control(Object*o,struct Msg*msg){switch(msg->Command){'+hidden+'}}\n'+r'''
int main(void){IPTR counts[]={0,1,3,32768,65535,INTPTR_MAX};int commands[]={C_CURSOR_UP,C_CURSOR_DOWN,C_CURSOR_FORWARD,C_CURSOR_BACKWARD,C_CURSOR_NEXT_LINE,C_CURSOR_PREV_LINE,C_BACKSPACE,C_CARRIAGE_RETURN};
 for(int w=1;w<=31;w+=3)for(int h=1;h<=19;h+=3)for(int hidden=0;hidden<2;hidden++)for(int c=0;c<8;c++)for(int n=0;n<6;n++){
  Object o={.x=w/2,.xc=w/2,.y=h/2,.yc=h/2,.xmax=w-1,.ymax=h-1,.hidden=hidden,.conFlags=129};IPTR p=counts[n];struct Msg msg={commands[c],&p};int x=o.x,y=o.y;IPTR count=p?p:1;
  if(c==0||c==5)y=count>y?0:y-count;if(c==1||c==4)y=count>h-1-y?h-1:y+count;
  if(c==2)x=count>w-1-x?w-1:x+count;if(c==3)x=count>x?0:x-count;if(c==4||c==5||c==7)x=0;if(c==6&&x)x--;
  run(&o,&msg);assert(o.x==x&&o.xc==x&&o.y==y&&o.yc==y&&o.conFlags==1&&!o.scrolls&&o.draws==(hidden?0:2));
 }
 int controls[]={C_FORMFEED,C_LINEFEED,C_INDEX,C_NEXT_LINE,C_REVERSE_IDX,C_VTAB};
 for(int mode=0;mode<2;mode++)for(int y=0;y<3;y++)for(int c=0;c<6;c++){
  Object o={.x=3,.xc=3,.y=y,.yc=y,.xmax=9,.ymax=2,.hidden=1,.mode=mode,.conFlags=129};struct Msg msg={controls[c],0};hidden_control(&o,&msg);
  int x=(c==0||c==3||(c==1&&mode))?0:3;int want=c==0?0:c<4?(y<2?y+1:2):(y?y-1:0);
  assert(o.x==x&&o.xc==x&&o.y==want&&o.yc==want&&!o.draws&&o.conFlags==1);assert(o.scrolls==((c>=1&&c<=3&&y==2)||(c>=4&&y==0)));
 }return 0;}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)
 for negative in (False,True):
  candidate=code.replace('    if (count > available) count = available;','    if (count > available) count = (WORD)count;') if negative else code
  (p/'test.c').write_text(candidate)
  subprocess.run(['cc','-Wall','-Werror','-Wno-misleading-indentation','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
  result=subprocess.run([str(p/'test')],capture_output=True);assert bool(result.returncode)==negative,result.stderr.decode()
print('PASS: 7392 relative cursor cases, 36 hidden controls; no-scroll/no-paint and large-count negative control')
