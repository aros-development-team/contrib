#!/usr/bin/env python3
"""Actual palette allocation and retained-color remapping, including dormant buffers."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];std=(r/'stdconclass.c').read_text();ch=(r/'charmapconclass.c').read_text()
a=std.index('static VOID stdcon_setslot(');slot=std[a:std.index('static VOID stdcon_colorreply(',a)]
a=ch.index('static VOID charmap_remap_colors(');remap=ch[a:ch.index('/* Cursor coordinates',a)]
a=ch.index('static VOID charmapcon_docommand(');dispatch=ch[a:ch.index('    // This is a bit of a hack:',a)]+'    viewport_restores++;\n}\n'
# Reset has a separate state/native regression; keep this palette extraction bounded.
a=dispatch.index('    if (msg->Command == C_TERMINAL_RESET)');b=dispatch.index('    /* Budget edits',a)
dispatch=dispatch[:a]+dispatch[b:]
prefix=r'''
#include <assert.h>
/* Search lifetime is covered by search-ui-regression.py. */
#define charmap_search_close(cl,o,redraw) ((void)0)
#define CHARMAP_ROLES(o) 0x0102
#include <stdint.h>
#include <string.h>
typedef intptr_t IPTR;typedef void *APTR;typedef void VOID;typedef uint32_t ULONG;typedef int32_t LONG;typedef unsigned char UBYTE;typedef signed char BYTE;typedef short WORD;typedef int BOOL;typedef void Class;typedef void *Msg;
#define FALSE 0
#define TRUE 1
struct Library {int unused;};
struct stdcondata {struct Library *scd_GfxBase;void *cm;};
static int refs[256],allocation,releases,attempts;
static LONG stdcon_bestpen(struct stdcondata *d,int r,int g,int b){attempts++;if(allocation>=0)refs[allocation]++;return allocation;}
static void release(struct Library *gfx,void *cm,int pen){assert(refs[pen]>0);refs[pen]--;releases++;}
#define ReleasePen(cm,p) release(GfxBase,cm,p)
struct charmap_line {struct charmap_line *next;ULONG size;BYTE *fgpen,*bgpen;unsigned long long *roles;ULONG *underlineColors;};
struct charmapcondata {struct charmap_line *top_of_scrollback,*primary_head;BOOL alternate;BYTE primary_fg,primary_bg;ULONG primary_fgColor,primary_bgColor;ULONG scrollback_pos,saved_scrollback_pos,primary_max,scrollback_max;};
typedef struct {struct charmapcondata data;struct {BYTE cu_FgPen,cu_BgPen;} cu;struct {BYTE savedFgPen,savedBgPen;BOOL savedCursor,emojiDisabled;ULONG fgColor,bgColor,savedFgColor,savedBgColor,extendedPaletteRevision;} icu;int hidden;} Object;
struct P_Console_DoCommand {int Command;IPTR *Params;};
struct ConsolengHistoryPrefs {ULONG hp_Size,hp_Version,hp_Lines;};
#define CONSOLENG_HISTORY_PREFS_VERSION 1
#define C_SET_CURSOR_SHAPE 92
#define C_GET_CURSOR_COLOR_PREFS 93
#define C_SET_CURSOR_COLOR_PREFS 94
#define C_NIL 0
#define C_GET_MODE_STATE 8
#define C_GET_HISTORY_PREFS 1
#define C_SET_HISTORY_PREFS 2
#define C_GET_CURSOR_PREFS 3
#define C_SET_CURSOR_PREFS 4
#define C_SET_COLOR 5
#define C_GET_COLOR_PREFS 6
#define C_GET_EMOJI_PREFS 90
#define C_SET_EMOJI_PREFS 91
#define C_SET_COLOR_PREFS 7
#define EnterFunc(x) ((void)0)
static int viewport_restores;

#define INST_DATA(cl,o) (&(o)->data)
#define CU(o) (&(o)->cu)
#define ICU(o) (&(o)->icu)
#define CON_IS_OBSCURED(o) ((o)->hidden)
#define CHARMAP_COLORPENS 34
static UBYTE before[34],after[34];
#define Console_GetColorPen(o,n,p) (*(p)=after[n])
static int phase,repaints,supers,extendedChange;
static void charmapcon_colorpens(Object *o,UBYTE *p){memcpy(p,phase?after:before,34);}
static void DoSuperMethodA(Class *cl,Object *o,Msg msg){phase=1;supers++;o->cu.cu_FgPen=after[0];o->icu.extendedPaletteRevision+=extendedChange;}
static void charmapcon_refresh(Class *cl,Object *o,LONG off){assert(off==0);repaints++;}
'''
main=r'''
int main(void){
 struct stdcondata d={0};ULONG rgb=0x01112233;LONG pen=3;refs[3]=1;
 allocation=-1;stdcon_setslot(&d,&rgb,&pen,0x01445566);assert(rgb==0x01112233&&pen==3&&refs[3]==1&&releases==0&&attempts==1);
 allocation=7;stdcon_setslot(&d,&rgb,&pen,0x01445566);assert(rgb==0x01445566&&pen==7&&!refs[3]&&refs[7]==1&&releases==1);
 attempts=releases=0;stdcon_setslot(&d,&rgb,&pen,rgb);assert(!attempts&&!releases&&refs[7]==1);
 allocation=7;stdcon_setslot(&d,&rgb,&pen,0x01778899);assert(pen==7&&refs[7]==1&&attempts==1&&releases==1);
 stdcon_setslot(&d,&rgb,&pen,0);assert(!rgb&&pen==-1&&!refs[7]&&releases==2);
 allocation=-1;stdcon_setslot(&d,&rgb,&pen,0x01000000);assert(!rgb&&pen==-1);
 for(int alt=0;alt<2;alt++)for(int hidden=0;hidden<2;hidden++)for(int saved=0;saved<2;saved++)for(int changed=0;changed<2;changed++){
  Object o={0};o.hidden=hidden;o.icu.savedCursor=saved;o.icu.savedFgPen=(BYTE)200;o.icu.savedBgPen=(BYTE)250;o.icu.fgColor=0;o.icu.bgColor=1;o.icu.savedFgColor=0;o.icu.savedBgColor=2;
  o.cu.cu_FgPen=(BYTE)200;o.cu.cu_BgPen=40;
  BYTE fg[3]={(BYTE)200,7,(BYTE)250},bg[3]={40,(BYTE)200,7},pf[2]={(BYTE)200,(BYTE)250},pb[2]={40,7};
  struct charmap_line empty={0},primary={&empty,2,pf,pb},active={&empty,3,fg,bg};
  o.data.top_of_scrollback=&active;o.data.primary_head=&primary;o.data.alternate=alt;o.data.primary_fg=(BYTE)200;o.data.primary_bg=40;o.data.primary_fgColor=0;o.data.primary_bgColor=1;
  o.data.scrollback_pos=17;o.data.saved_scrollback_pos=31;
  for(int i=0;i<34;i++)before[i]=after[i]=i+50;
  before[0]=after[0]=200;before[1]=after[1]=40;before[2]=after[2]=250;
  if(changed){after[0]=250;after[1]=30;after[2]=5;}
  phase=repaints=supers=0;struct P_Console_DoCommand msg={C_SET_COLOR,0};viewport_restores=0;charmapcon_docommand(0,&o,&msg);assert(!viewport_restores);
  assert(supers==1&&repaints==(changed&&!hidden));assert(o.data.scrollback_pos==17&&o.data.saved_scrollback_pos==31);
  assert((UBYTE)fg[0]==(changed?250:200)&&fg[1]==7&&(UBYTE)fg[2]==(changed?5:250));
  assert(bg[0]==(changed?30:40)&&(UBYTE)bg[1]==(changed?250:200)&&bg[2]==7);
  assert((UBYTE)pf[0]==(alt&&changed?250:200)&&(UBYTE)pf[1]==(alt&&changed?5:250));
  assert(pb[0]==(alt&&changed?30:40)&&pb[1]==7);
  assert((UBYTE)o.data.primary_fg==(alt&&changed?250:200)&&o.data.primary_bg==(alt&&changed?30:40));
  assert((UBYTE)o.icu.savedFgPen==(saved&&changed?250:200)&&(UBYTE)o.icu.savedBgPen==(saved&&changed?5:250));
  assert((UBYTE)o.cu.cu_FgPen==(changed?250:200)&&o.cu.cu_BgPen==(changed?30:40));
 }
 for(int hidden=0;hidden<2;hidden++){
  Object o={0};o.hidden=hidden;o.data.scrollback_pos=17;
  for(int i=0;i<34;i++)before[i]=after[i]=i+50;
  extendedChange=1;phase=repaints=0;struct P_Console_DoCommand msg={C_SET_COLOR,0};
  charmapcon_setcolor(0,&o,&msg);assert(repaints==!hidden&&o.data.scrollback_pos==17);
 }
 extendedChange=0;
 Object query={0};query.data.scrollback_pos=23;query.data.saved_scrollback_pos=41;
 for(int alt=0;alt<2;alt++){
  ULONG value=99;IPTR params[3]={1,1049,(IPTR)&value};struct P_Console_DoCommand msg={C_GET_MODE_STATE,params};query.data.alternate=alt;viewport_restores=supers=0;
  charmapcon_docommand(0,&query,&msg);assert(value==(alt?1U:2U)&&!viewport_restores&&!supers&&query.data.scrollback_pos==23&&query.data.saved_scrollback_pos==41);
  msg.Command=C_NIL;charmapcon_docommand(0,&query,&msg);assert(!viewport_restores&&!supers);
 }
 return 0;
}
'''
source=prefix+slot+remap+dispatch+main
with tempfile.TemporaryDirectory() as d:
 for name,code,success in [
  ('current',source,True),
  ('missing_extended',source.replace('changed = revision != ICU(o)->extendedPaletteRevision;', 'changed = FALSE; (void)revision;'),False),
  ('missing_primary',source.replace('charmap_remap_colors(data->primary_head, map, after);',''),False),
  ('lost_override',source.replace('if (replacement == -1)\n            return;','if (replacement == -1) { *rgbslot=0; *penslot=-1; return; }'),False),
  ('viewport_restore',source.replace('        charmapcon_setcolor(cl, o, msg);\n        return;','        charmapcon_setcolor(cl, o, msg);\n        viewport_restores++; return;'),False)]:
  p=Path(d)/(name+'.c');exe=Path(d)/name;p.write_text(code)
  subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(exe)],check=True)
  result=subprocess.run([str(exe)],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
  assert (result.returncode==0)==success,(name,result.stderr.decode())
  print(name, 'PASS' if success else 'negative control rejected')
print('Color state PASS: allocation/refcounts; 16 primary/alternate/hidden/saved-cursor cases; one-step unsigned remap; actual early dispatch preserves viewport; four negative controls plus extended-only repaint')
