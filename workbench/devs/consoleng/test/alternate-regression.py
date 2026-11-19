#!/usr/bin/env python3
"""Actual alternate screen ownership, idempotence, OOM and cursor restoration."""
from pathlib import Path
from hyperlink_fixture import hyperlink_source
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'charmapconclass.c').read_text()
fields='struct charmapcondata\n{'+s.split('struct charmapcondata\n{',1)[1].split('\n};',1)[0]+'\n};\n'
dispatch=s[s.index('    if (msg->Command == C_SET_HYPERLINK)'):s.index('    /* Budget edits')]
f='static VOID charmap_alternate_screen('+s.split('static VOID charmap_alternate_screen(',1)[1].split('/* Limit a retained/drawn span',1)[0]
dispose='static VOID charmapcon_dispose('+s.split('static VOID charmapcon_dispose(',1)[1].split('/*********  CharMapCon::DoCommand()',1)[0]
source=r'''
#include <stdlib.h>
#include <assert.h>
/* Search lifetime is covered by search-ui-regression.py. */
#define charmap_search_close(cl,o,redraw) ((void)0)
#define CHARMAP_ROLES(o) 0x0102
typedef void *Msg;typedef void VOID;typedef void Class;typedef void *APTR;typedef int BOOL;typedef short WORD;typedef signed char BYTE;typedef unsigned char UBYTE;typedef unsigned long ULONG;typedef long LONG;
#define TRUE 1
#define FALSE 0
#define MEMF_ANY 0
#define MEMF_CLEAR 1
#define CF_WRAP_PENDING 128
#define CF_ORIGIN_MODE 2048
#define CON_SCROLL_TOP(o) 0
#define CON_SCROLL_BOTTOM(o) CHAR_YMAX(o)
struct charmap_line {struct charmap_line *next,*prev;int text;unsigned long long *roles;ULONG *underlineColors;};
'''+(r/'hyperlink.h').read_text().replace('#include <exec/types.h>','')+fields+r'''
typedef struct {struct charmapcondata data;unsigned long fgColor,bgColor,savedFgColor,savedBgColor;int keyboardAlternate,keyboardSwitches,x,y,xc,yc,xmax,ymax,cu_FgPen,cu_BgPen,cu_TxFlags,savedX,savedY,savedFgPen,savedBgPen,savedTxFlags,underlineColor,savedUnderlineColor,underlineStyle,savedUnderlineStyle,savedWrapPending,savedOrigin,charsets,savedCharsets,activeCharset,savedCursor,conFlags,hidden,draws;} Object;
#define CON_IS_OBSCURED(o) ((o)->hidden)
#define INST_DATA(cl,o) (&(o)->data)
#define XCP (o->x)
#define YCP (o->y)
#define XCCP (o->xc)
#define YCCP (o->yc)
#define CU(o) (o)
#define ICU(o) (o)
#define con_keyboard_switch(o,set) ((o)->keyboardAlternate=!!(set),(o)->keyboardSwitches++)
#define Console_GetColorPen(o,n,p) (*(p)=(n))
#define CHAR_XMIN(o) 0
#define CHAR_YMIN(o) 0
#define CHAR_XMAX(o) ((o)->xmax)
#define CHAR_YMAX(o) ((o)->ymax)
#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))
static int fail,allocs,frees;
static void *AllocMem(size_t n,int flags){if(fail)return 0;allocs++;return calloc(1,n);}
#define AllocMem AllocMem
#define FreeMem(p,n) (free(p),frees++)
static void charmap_dispose_lines(struct charmap_line *p){while(p){struct charmap_line *n=p->next;free(p);frees++;p=n;}}
#define Console_UnRenderCursor(o) ((void)0)
#define Console_RenderCursor(o) ((void)0)
#define charmapcon_clear_selection(cl,o) ((o)->data.active_selection=0)
#define charmapcon_refresh(cl,o,n) ((o)->draws++)
#define charmapcon_adj_prop(cl,o) ((void)0)
#define charmapcon_free_prop(cl,o) ((void)0)
#define CloseLibrary(lib) ((void)0)
#define charmap_reflow_primary(cl,o,x,y,p) ((void)0)
#define DoSuperMethodA(cl,o,msg) ((void)0)
'''+hyperlink_source()+f+dispose+r'''
#define C_SET_HYPERLINK 1
#define C_SAVE_CURSOR 2
#define C_RESTORE_CURSOR 3
static void link_command(Object *o,int command,const char *payload){
 struct {int Command;} message={command},*msg=&message;
 unsigned long params[2]={(unsigned long)payload,payload?strlen(payload):0};
 struct charmapcondata *data=&o->data;
'''+dispatch+r'''
}

int main(void){
 for(int origin=0;origin<2;origin++)for(int hidden=0;hidden<2;hidden++)for(int shrink=0;shrink<2;shrink++)for(int repeat=1;repeat<5;repeat++){
  Object o={0};o.charsets=9;o.activeCharset=1;o.hidden=hidden;o.x=12;o.y=9;o.xmax=79;o.ymax=24;o.cu_FgPen=2;o.cu_BgPen=4;o.fgColor=2;o.bgColor=4;o.cu_TxFlags=1;o.conFlags=CF_WRAP_PENDING|(origin?CF_ORIGIN_MODE:0);
  struct charmap_line *primary=AllocMem(sizeof(*primary),1);primary->text=1234;
  o.data.top_of_scrollback=o.data.top_of_window=primary;o.data.scrollback_size=50;o.data.scrollback_pos=20;o.data.scrollback_max=1000;
  link_command(&o,C_SET_HYPERLINK,"id=primary;https://primary/a;b");
  struct con_link *original=o.data.current_link;assert(original);
  link_command(&o,C_SAVE_CURSOR,NULL);o.savedCursor=TRUE;
  link_command(&o,C_SET_HYPERLINK,";https://temporary/");
  link_command(&o,C_RESTORE_CURSOR,NULL);assert(o.data.current_link==original);
  fail=1;charmap_alternate_screen(0,&o,1);assert(!o.data.alternate&&o.data.top_of_window==primary&&o.conFlags==(CF_WRAP_PENDING|(origin?CF_ORIGIN_MODE:0)));fail=0;
  for(int i=0;i<repeat;i++)charmap_alternate_screen(0,&o,1);
  assert(o.keyboardAlternate&&o.keyboardSwitches==1);
  assert(o.data.alternate&&o.data.top_of_window!=primary&&o.data.primary_head==primary);
  assert(o.data.scrollback_size==1&&o.data.scrollback_max==0&&o.data.scrollback_pos==0);
  assert(o.x==12&&o.y==9&&!(o.conFlags&CF_WRAP_PENDING));
  assert(o.data.primary_link==original&&o.data.saved_link==original);
  link_command(&o,C_SET_HYPERLINK,"id=alt;https://alternate/");
  link_command(&o,C_SAVE_CURSOR,NULL);
  assert(o.data.saved_link!=original);
  o.conFlags ^= CF_ORIGIN_MODE;
  o.charsets=6;
  o.data.top_of_window->text=9876;o.x=2;o.y=3;o.savedX=1;o.savedY=1;o.cu_FgPen=7;o.fgColor=7;o.bgColor=8;
  if(shrink){o.xmax=5;o.ymax=4;}
  for(int i=0;i<repeat;i++)charmap_alternate_screen(0,&o,0);
  assert(!o.keyboardAlternate&&o.keyboardSwitches==2);
  assert(!o.data.alternate&&o.data.top_of_window==primary&&primary->text==1234);
  assert(!!(o.conFlags&CF_ORIGIN_MODE)==origin&&o.savedOrigin==origin);
  assert(o.charsets==9&&o.savedCharsets==9&&o.activeCharset==1);
  assert(o.data.primary_head==0&&o.data.scrollback_size==50&&o.data.scrollback_pos==20&&o.data.scrollback_max==1000);
  assert(o.x==(shrink?5:12)&&o.y==(shrink?4:9)&&o.savedX==12&&o.savedY==9);
  assert(o.cu_FgPen==2&&o.cu_BgPen==4&&o.cu_TxFlags==1&&o.savedWrapPending);
  assert(o.fgColor==2&&o.bgColor==4&&o.savedFgColor==2&&o.savedBgColor==4);
  assert(o.draws==(hidden?0:2));
  assert(o.data.current_link==original&&o.data.saved_link==original&&!o.data.primary_link);
  assert(o.data.link_pool.records==1);
  charmapcon_dispose(0,&o,0);
  assert(!o.data.link_pool.records);
 }
 /* Closing while alternate is active owns and frees both buffers. */
 { Object o={0};o.xmax=79;o.ymax=24;
   o.data.top_of_scrollback=o.data.top_of_window=AllocMem(sizeof(struct charmap_line),1);
   o.data.scrollback_size=1;
   link_command(&o,C_SET_HYPERLINK,";https://close-primary/");
   charmap_alternate_screen(0,&o,1);
   link_command(&o,C_SET_HYPERLINK,";https://close-alt/");
   link_command(&o,C_SAVE_CURSOR,NULL);
   assert(o.data.link_pool.records==2);
   charmapcon_dispose(0,&o,0);
   assert(!o.data.link_pool.records);
 }
 assert(allocs==frees);return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';b=Path(d)/'test'
 for negative in (False,True):
  candidate=source.replace('ICU(o)->fgColor = data->primary_fgColor;', '/* former missing logical restoration */') if negative else source
  p.write_text(candidate)
  subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(b)],check=True)
  result=subprocess.run([str(b)],capture_output=True)
  assert bool(result.returncode)==negative,result.stderr.decode()
print('Alternate PASS: buffers, OOM, repeated modes, logical rendition, resize clamp; missing-role negative control')
