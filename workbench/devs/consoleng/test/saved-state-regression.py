#!/usr/bin/env python3
"""Execute actual saved-cursor dispatch, hidden state, and logical rendition."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'stdconclass.c').read_text()
a=s.index('static void stdcon_saved_cursor(');helper=s[a:s.index('/* DECSTBM ignores',a)]
a=s.index('    if (msg->Command == C_SAVE_CURSOR');dispatch=s[a:s.index('    if (msg->Command == C_DEVICE_STATUS_REPORT',a)]
code=r'''
#define CF_ORIGIN_MODE 2048
#include <assert.h>
#include <stdint.h>
#include <string.h>
typedef short WORD;typedef signed char BYTE;
#define TRUE 1
#define C_SAVE_CURSOR 1
#define C_RESTORE_CURSOR 2
#define CF_WRAP_PENDING 128
typedef struct {WORD x,xc,y,yc,xmax,ymax,savedX,savedY;unsigned fgColor,bgColor,savedFgColor,savedBgColor,conFlags;int cu_FgPen,cu_BgPen,savedFgPen,savedBgPen,underlineColor,savedUnderlineColor,underlineStyle,savedUnderlineStyle,cu_TxFlags,savedTxFlags,savedWrapPending,savedOrigin,charsets,savedCharsets,activeCharset,savedCursor,hidden,draws;} Object;
struct Msg {BYTE Command;};
#define CU(o) (o)
#define ICU(o) (o)
#define XCP (o->x)
#define XCCP (o->xc)
#define YCP (o->y)
#define YCCP (o->yc)
#define CHAR_XMIN(o) 0
#define CHAR_YMIN(o) 0
#define CHAR_XMAX(o) (o->xmax)
#define CON_SCROLL_TOP(o) 0
#define CON_SCROLL_BOTTOM(o) CHAR_YMAX(o)
#define CHAR_YMAX(o) (o->ymax)
#define CON_IS_OBSCURED(o) (o->hidden)
#define Console_UnRenderCursor(o) ((o)->draws++)
#define Console_RenderCursor(o) ((o)->draws++)
static int epoch;
#define Console_GetColorPen(o,color,out) (*(out)=(int)((color)%100)+epoch)
'''+helper+'static void run(Object*o,struct Msg*msg){'+dispatch+'}\n'+r'''
int main(void){int positions[]={-1,0,1,79,32767};
for(int saved=0;saved<2;saved++)for(int hidden=0;hidden<2;hidden++)for(int savehidden=0;savehidden<2;savehidden++)for(int x=0;x<5;x++)for(int y=0;y<5;y++){
 Object o={.x=positions[x],.xc=positions[x],.y=positions[y],.yc=positions[y],.xmax=30,.ymax=10,.fgColor=0x123456,.bgColor=0xabcdef,.cu_FgPen=3,.cu_BgPen=4,.underlineColor=0x1012346,.underlineStyle=3,.cu_TxFlags=5,.hidden=savehidden,.conFlags=129};
 struct Msg save={C_SAVE_CURSOR},restore={C_RESTORE_CURSOR};if(saved){run(&o,&save);assert(o.savedCursor&&o.draws==0&&o.conFlags==129);}
 o.hidden=hidden;o.x=o.xc=4;o.y=o.yc=3;o.fgColor=11;o.bgColor=22;o.cu_FgPen=11;o.cu_BgPen=22;o.cu_TxFlags=0;o.underlineStyle=1;o.underlineColor=0;epoch=7;run(&o,&restore);
 int wantx=saved?positions[x]:0,wanty=saved?positions[y]:0;if(wantx<0)wantx=0;if(wantx>30)wantx=30;if(wanty<0)wanty=0;if(wanty>10)wanty=10;
 assert(o.x==wantx&&o.xc==wantx&&o.y==wanty&&o.yc==wanty&&o.conFlags==1&&o.draws==(hidden?0:2));
 assert(o.underlineStyle==(saved?3:1)&&o.underlineColor==(saved?0x1012346:0));
 assert(o.fgColor==(saved?0x123456:11)&&o.bgColor==(saved?0xabcdef:22)&&o.cu_TxFlags==(saved?5:0));
 assert(o.cu_FgPen==(saved?0x123456%100+7:11)&&o.cu_BgPen==(saved?0xabcdef%100+7:22));
 /* Save the restored state again, then resolve pens after a palette epoch change. */
 run(&o,&save);o.fgColor=0;o.bgColor=0;epoch=13;run(&o,&restore);assert(o.underlineStyle==(saved?3:1)&&o.underlineColor==(saved?0x1012346:0));
 assert(o.fgColor==o.savedFgColor&&o.bgColor==o.savedBgColor&&o.cu_FgPen==(int)(o.savedFgColor%100)+13);
}
for(int hidden=0;hidden<2;hidden++)for(int delta=-1;delta<=1;delta++){
 Object o={.x=7,.xc=7,.y=1,.yc=1,.xmax=7,.ymax=5,.conFlags=129,.hidden=hidden};struct Msg save={C_SAVE_CURSOR},restore={C_RESTORE_CURSOR};run(&o,&save);assert(o.savedWrapPending);o.xmax+=delta;o.x=0;o.conFlags=1;run(&o,&restore);assert(!!(o.conFlags&128)==(delta==0));
}return 0;}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)
 for negative in (0,1,2):
  candidate=code.replace('ICU(o)->fgColor = ICU(o)->savedFgColor;','/* old missing logical foreground restoration */') if negative==1 else code
  if negative==2:candidate=code.replace('ICU(o)->conFlags |= CF_WRAP_PENDING;','/* missing saved pending wrap */')
  (p/'test.c').write_text(candidate)
  subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
  result=subprocess.run([str(p/'test')],capture_output=True);assert bool(result.returncode)==bool(negative),result.stderr.decode()
print('PASS: 200 saved/unsaved visible/hidden clipping and rendition cases; repeated-save/palette epoch; missing-role and missing-wrap negative controls; six edge/resize cases')
