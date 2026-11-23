#!/usr/bin/env python3
"""Actual DECOM state, addressing, margins and saved cursor helpers."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'stdconclass.c').read_text();h=(r/'console_gcc.h').read_text()
def fn(name):
 a=s.index(name);a=s.rfind('\n',0,a)+1;b=s.index('{',a);depth=1;c=b+1
 while depth:depth+=(s[c]=='{')-(s[c]=='}');c+=1
 return s[a:c]
helpers='\n'.join(fn(n) for n in ['static void stdcon_tab_export(', 'static VOID stdcon_reset_state('])
enum='enum'+h.split('enum',1)[1].split('};',1)[0]+'};\n'
macros='\n'.join(x for x in h.splitlines() if x.startswith('#define CON_SCROLL_'))
code=r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef unsigned short UWORD;typedef unsigned char UBYTE;typedef void VOID;
#define FALSE 0
#define TRUE 1
#define MAXTABS 80
#define STDCON_TAB_COLUMNS 32768
#define CONUNIT_PEN_DEFFG 20
#define CONUNIT_PEN_DEFBG 21
#define PMB_AWM 1
#define PMB_ASM 2
#define M_LNM 4
#define SET_MODE(o,n) ((o)->modes|=(n))
#define CLEAR_MODE(o,n) ((o)->modes&=~(n))
struct stdcondata {UBYTE tabBits[4096];UWORD tabMirror[MAXTABS];int cursorstyle,resetCursorStyle,cursorTimerReady,cursorvisible,rendercursorcount;};
typedef struct {int keyboardAlternate,keyboardFlags,conFlags,mouseReportedButtons,mouseMode,mouseSGR,mouseUTF8,mouseLastValid,scrollTop,scrollBottom,savedCursor,savedOrigin,charsets,savedCharsets,activeCharset,savedWrapPending,modes,underlineColor,savedUnderlineColor,underlineStyle,savedUnderlineStyle,cu_TxFlags,cu_FgPen,cu_BgPen,x,xc,y,yc;UWORD cu_TabStops[MAXTABS];} Object;
#define ICU(o) (o)
#define con_keyboard_dispose(o) ((o)->keyboardFlags=0)
#define CU(o) (o)
#define XCP (o->x)
#define XCCP (o->xc)
#define YCP (o->y)
#define YCCP (o->yc)
#define CHAR_XMIN(o) 0
#define CHAR_YMIN(o) 0
#define Console_GetColorPen(o,c,out) (*(out)=(c))
'''+'\n'.join(x for x in h.splitlines() if x.startswith('#define CF_'))+'\n'+helpers+r'''
int main(void){for(int flags=0;flags<4096;flags++)for(int visible=0;visible<2;visible++){
 Object o;memset(&o,0xff,sizeof(o));o.conFlags=flags;
 struct stdcondata d;memset(&d,0,sizeof(d));d.cursorvisible=visible;d.rendercursorcount=visible?1:2;d.resetCursorStyle=4;d.cursorstyle=1;
 stdcon_reset_state(&d,&o);
 assert(o.conFlags==(flags&(CF_UTF8|CF_OBSCURED|CF_DELAYEDDISPOSE|CF_DISPOSE)));
 assert(!o.keyboardFlags&&!o.keyboardAlternate);
 assert(!o.mouseReportedButtons&&!o.mouseMode&&!o.mouseSGR&&!o.mouseUTF8&&!o.mouseLastValid&&!o.scrollTop&&!o.scrollBottom);
 assert(!o.charsets&&!o.savedCharsets&&!o.activeCharset);
 assert(!o.underlineStyle&&!o.savedUnderlineStyle&&!o.underlineColor&&!o.savedUnderlineColor);
 assert(!o.savedCursor&&!o.savedOrigin&&!o.savedWrapPending&&!o.x&&!o.xc&&!o.y&&!o.yc&&!o.cu_TxFlags);
 assert((o.modes&7)==3&&o.cu_FgPen==20&&o.cu_BgPen==21);
 assert(d.cursorvisible&&d.rendercursorcount==1&&d.cursorstyle==4&&!d.cursorTimerReady);
 for(int i=0;i<4096;i++)assert(d.tabBits[i]==1);
 for(int i=0;i<79;i++)assert(o.cu_TabStops[i]==i*8);
 assert(o.cu_TabStops[79]==65535);
 stdcon_reset_state(&d,&o);assert(d.rendercursorcount==1);
}puts("RIS state PASS: 8192 mode/visibility combinations, idempotence, tabs and preference style");}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'t.c';e=Path(d)/'t';p.write_text(code)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(e)],check=True);subprocess.run([str(e)],check=True)
