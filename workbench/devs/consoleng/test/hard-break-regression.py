#!/usr/bin/env python3
"""Explicit LF, IND and NEL must cancel delayed automatic wrapping."""
from pathlib import Path
import subprocess,tempfile
s=(Path(__file__).resolve().parents[1]/'stdconclass.c').read_text()
s=s[s.index('    switch (msg->Command)',s.index('ReturnVoid("StdCon::DoCommand(obscured)")')):]
body=''
for begin,end in [('C_LINEFEED','C_VTAB'),('C_INDEX','C_NEXT_LINE'),('C_NEXT_LINE','C_REVERSE_IDX'),('C_REVERSE_IDX','C_ERASE_CHAR')]:
 body+=s[s.index('    case '+begin+':'):s.index('    case '+end+':',s.index('    case '+begin+':'))]
code=r'''
#include <assert.h>
#define C_LINEFEED 1
#define C_INDEX 2
#define C_NEXT_LINE 3
#define C_REVERSE_IDX 4
#define CF_WRAP_PENDING 128
#define M_LNM 1
#define D(x)
#define ICU(o) (o)
#define CU(o) (o)
#define XCP (o->cu_XCP)
#define XCCP (o->cu_XCCP)
#define CHAR_XMIN(o) 0
#define CHECK_MODE(o,m) ((o)->mode)
#define Console_UnRenderCursor(o) ((void)0)
#define Console_RenderCursor(o) ((void)0)
#define Console_Up(o,n) ((o)->y-=(n))
#define Console_Down(o,n) ((o)->y+=(n))
#define Console_Left(o,n) ((o)->cu_XCP-=(n))
typedef struct {int cu_XCP,cu_XCCP,y,conFlags,mode;} Object;
static void run(Object *o,int cmd){switch(cmd){
'''+body+r'''
}}
int main(void){for(int c=1;c<=4;c++)for(int m=0;m<2;m++){
 Object o={7,7,2,CF_WRAP_PENDING|16,m};run(&o,c);
 assert(o.y==(c==C_REVERSE_IDX?1:3) && o.conFlags==16);
 assert(o.cu_XCP==((c==C_NEXT_LINE||(c==C_LINEFEED&&m))?0:7));
}return 0;}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'t.c';b=Path(d)/'t';p.write_text(code)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(b)],check=True)
 subprocess.run([str(b)],check=True)
print('Hard breaks PASS: LF/IND/NEL/RI cancel pending wrap; unrelated flags and explicit column semantics preserved')
