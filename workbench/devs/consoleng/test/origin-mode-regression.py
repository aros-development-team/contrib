#!/usr/bin/env python3
"""Actual DECOM state, addressing, margins and saved cursor helpers."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'stdconclass.c').read_text();h=(r/'console_gcc.h').read_text()
def fn(name):
 a=s.index(name);a=s.rfind('\n',0,a)+1;b=s.index('{',a);depth=1;c=b+1
 while depth:depth+=(s[c]=='{')-(s[c]=='}');c+=1
 return s[a:c]
helpers='\n'.join(fn(n) for n in ['static WORD stdcon_absolute_coordinate(','static VOID stdcon_absolute_position(','static void stdcon_saved_cursor(','static BOOL stdcon_scroll_region(','static VOID stdcon_origin_mode('])
enum='enum'+h.split('enum',1)[1].split('};',1)[0]+'};\n'
macros='\n'.join(x for x in h.splitlines() if x.startswith('#define CON_SCROLL_'))
code=r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
typedef short WORD;typedef signed char BYTE;typedef intptr_t IPTR;typedef int BOOL;typedef void VOID;
#define TRUE 1
#define FALSE 0
#define CF_ORIGIN_MODE 2048
#define CF_WRAP_PENDING 128
#define MIN(a,b) ((a)<(b)?(a):(b))
typedef struct {int x,xc,y,yc,xmax,ymax,scrollTop,scrollBottom,conFlags,savedOrigin,charsets,savedCharsets,activeCharset,savedCursor,savedWrapPending,savedX,savedY,fgColor,bgColor,savedFgColor,savedBgColor,cu_FgPen,cu_BgPen,savedFgPen,savedBgPen,underlineColor,savedUnderlineColor,underlineStyle,savedUnderlineStyle,cu_TxFlags,savedTxFlags;} Object;
#define ICU(o) (o)
#define CU(o) (o)
#define XCP (o->x)
#define XCCP (o->xc)
#define YCP (o->y)
#define YCCP (o->yc)
#define CHAR_XMIN(o) 0
#define CHAR_YMIN(o) 0
#define CHAR_XMAX(o) (o->xmax)
#define CHAR_YMAX(o) (o->ymax)
#define CON_IS_OBSCURED(o) 1
#define Console_UnRenderCursor(o) ((void)0)
#define Console_RenderCursor(o) ((void)0)
#define Console_GetColorPen(o,c,out) (*(out)=(c))
'''+enum+macros+'\n'+helpers+r'''
int main(void){unsigned cases=0;IPTR values[]={0,1,2,3,65535,INTPTR_MAX};
for(int h=2;h<=24;h++)for(int top=0;top<h-1;top++)for(int bottom=top+1;bottom<h;bottom++)for(int enabled=0;enabled<2;enabled++){
 Object o={.xmax=79,.ymax=h-1,.scrollTop=top,.scrollBottom=bottom+1,.conFlags=129};
 stdcon_origin_mode(&o,enabled);assert(o.x==0&&o.y==(enabled?top:0));assert(o.conFlags==(1|(enabled?CF_ORIGIN_MODE:0)));
 for(int n=0;n<6;n++){
  IPTR p[]={values[n],5};stdcon_absolute_position(&o,C_CURSOR_POS,p);int relative=values[n]>0?values[n]-1:0;
  /* Avoid narrowing the oracle's untrusted count before clipping. */
  IPTR row=values[n]>0?values[n]-1:0;int max=enabled?bottom-top:h-1;if(row>max)row=max;
  (void)relative;assert(o.x==4&&o.y==(enabled?top:0)+row);cases++;
  stdcon_saved_cursor(&o,C_SAVE_CURSOR);int savedY=o.y;
  stdcon_origin_mode(&o,!enabled);stdcon_saved_cursor(&o,C_RESTORE_CURSOR);
  assert(!!(o.conFlags&CF_ORIGIN_MODE)==enabled&&o.x==4&&o.y==savedY);
 }
 IPTR region[]={top+1,bottom+1};assert(stdcon_scroll_region(&o,region));assert(o.y==(enabled?top:0)&&o.x==0);
}
printf("Origin PASS: %u address/save/restore cases plus margin/mode homing\n",cases);}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'t.c';e=Path(d)/'t';p.write_text(code)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(e)],check=True);subprocess.run([str(e)],check=True)
