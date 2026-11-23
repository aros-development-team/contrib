#!/usr/bin/env python3
"""Execute actual absolute-position dispatch, wide clipping and CSI parser."""
from pathlib import Path
import tempfile,subprocess
r=Path(__file__).resolve().parents[1];s=(r/'stdconclass.c').read_text()
a=s.index('static WORD stdcon_absolute_coordinate(');helpers=s[a:s.index('/* Cursor-motion CSI',a)]
a=s.index('    if (msg->Command == C_CURSOR_POS');dispatch=s[a:s.index('    if ((msg->Command >= C_CURSOR_UP',a)]
header=(r/'console_gcc.h').read_text();enum='enum'+header.split('enum',1)[1].split('};',1)[0]+'};'
support=(r/'support.c').read_text();a=support.rindex('static BOOL getabsolutecommand(');parser=support[a:support.index('/* Validate the full ANSI',a)]
code=r'''
#define CF_ORIGIN_MODE 2048
#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
#include <assert.h>
#include <string.h>
typedef int16_t WORD;typedef int8_t BYTE;typedef uint8_t UBYTE;typedef uint16_t UWORD;typedef uint32_t ULONG;typedef int32_t LONG;typedef intptr_t IPTR;typedef int BOOL;typedef void VOID;
#define MIN(a,b) ((a)<(b)?(a):(b))
#define TRUE 1
#define FALSE 0
#define CF_WRAP_PENDING 128
#define MAX_COMMAND_PARAMS 16
#define FIRST_CSI_CMD 0x40
#define D(x) ((void)0)
struct ConsoleBase {int unused;};
typedef struct {WORD x,xc,y,yc,xmin,xmax,ymin,ymax;ULONG conFlags;int hidden,draws;} Object;
struct P_Console_DoCommand {BYTE Command;IPTR *Params;};
#define ICU(o) (o)
#define XCP (o->x)
#define XCCP (o->xc)
#define YCP (o->y)
#define YCCP (o->yc)
#define CHAR_XMIN(o) (o->xmin)
#define CHAR_XMAX(o) (o->xmax)
#define CHAR_YMIN(o) (o->ymin)
#define CON_SCROLL_TOP(o) 0
#define CON_SCROLL_BOTTOM(o) CHAR_YMAX(o)
#define CHAR_YMAX(o) (o->ymax)
#define CON_IS_OBSCURED(o) (o->hidden)
#define Console_UnRenderCursor(o) ((o)->draws++)
#define Console_RenderCursor(o) ((o)->draws++)
#define Console_GetDefaultParams(o,c,p) ((p)[0]=1,(p)[1]=1)
'''+enum+helpers+'static void run(Object*o,struct P_Console_DoCommand*msg){IPTR*params=msg->Params;'+dispatch+'}\n'+parser+r'''
int main(void){
 IPTR values[]={-1,0,1,2,127,32767,32768,65535,INTPTR_MAX};int commands[]={C_CURSOR_POS,C_CURSOR_COLUMN,C_CURSOR_ROW};
 for(int w=1;w<128;w+=7)for(int h=1;h<64;h+=5)for(int hidden=0;hidden<2;hidden++)for(int c=0;c<3;c++)for(unsigned n=0;n<sizeof(values)/sizeof(*values);n++){
  Object o={.x=0,.xc=0,.y=0,.yc=0,.xmax=w-1,.ymax=h-1,.conFlags=129,.hidden=hidden};IPTR p[2]={values[n],values[n]};struct P_Console_DoCommand msg={commands[c],p};
  run(&o,&msg);IPTR want=values[n]>0?values[n]-1:0;
  assert(o.x==o.xc&&o.y==o.yc&&o.conFlags==1&&o.draws==(hidden?0:2));
  assert(o.x==(commands[c]==C_CURSOR_ROW?0:want>=w?w-1:want));assert(o.y==(commands[c]==C_CURSOR_COLUMN?0:want>=h?h-1:want));
 }
 const char*bad[]={"1;2;3H","?5H","1:2H","1;2G","1;2d"};
 for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);i++){UBYTE*p=(UBYTE*)bad[i],count=99;BYTE command=99;IPTR v[2]={55,66};assert(!getabsolutecommand(&command,&p,&count,strlen(bad[i]),v));assert(p==(UBYTE*)bad[i]&&count==99&&command==99&&v[0]==55&&v[1]==66);}
 const char*seq[]={"H","0;0H","65535;65535H","32768;32768H","5;6H","65535G","0G","10d","65535d","0d","10`","65535`",";f","5;f",";6f","000f","999999999999999999H"};
 for(unsigned i=0;i<sizeof(seq)/sizeof(*seq);i++){
  for(unsigned n=0;n<strlen(seq[i]);n++){UBYTE*p=(UBYTE*)seq[i],count=0;BYTE command=-1;IPTR v[16]={0};assert(!getabsolutecommand(&command,&p,&count,n,v));}
  UBYTE*p=(UBYTE*)seq[i],count=0;BYTE command=-1;IPTR v[16]={0};if(!getabsolutecommand(&command,&p,&count,strlen(seq[i]),v)){fprintf(stderr,"parse failed: %s\n",seq[i]);return 1;}assert(!*p);
  char final=seq[i][strlen(seq[i])-1];assert(command==(final=='d'?C_CURSOR_ROW:final=='G'||final=='`'?C_CURSOR_COLUMN:C_CURSOR_POS));
 }
 return 0;}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)
 for negative in (False,True):
  src=code.replace('parameter > 0 ? parameter - 1 : 0','parameter > 0 ? (WORD)parameter - 1 : 0') if negative else code
  (p/'test.c').write_text(src)
  subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
  result=subprocess.run([str(p/'test')],capture_output=True)
  assert bool(result.returncode)==negative,result.stderr.decode()
print('PASS: 13338 visible/hidden clipping cases, 17 CSI sequences, all partial prefixes and malformed input isolation; signed-WORD negative control rejected')
