#!/usr/bin/env python3
"""Retained blank tails use stored rendition without adding reflow cells."""
from pathlib import Path
import subprocess,tempfile
s=(Path(__file__).resolve().parents[1]/'charmapconclass.c').read_text()
a=s.index('static UBYTE charmap_blank_tail_pen(');b=s.index('static VOID charmap_block_span(',a)
f=s[a:b]
assert 'UBYTE tail = charmap_blank_tail_pen(line, CU(o)->cu_BgPen);' in s
assert 'SetAPen(rp, tail);' in s
src=r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
typedef uint8_t UBYTE;typedef int8_t BYTE;typedef uint32_t ULONG;
#define CON_TXTFLAGS_REVERSED 1
struct charmap_line {ULONG size,*text;BYTE *fgpen,*bgpen,*flags;ULONG *underlineColors;};
'''+f+r'''
int main(void){ULONG text[2]={'X',' '};BYTE fg[2]={3,0},bg[2]={4,0},flags[2]={0,0};struct charmap_line l={2,text,fg,bg,flags};unsigned cases=0;
for(int pen=0;pen<256;pen++)for(int rev=0;rev<2;rev++)for(int space=0;space<2;space++){
 fg[1]=(BYTE)(255-pen);bg[1]=(BYTE)pen;flags[1]=rev;text[1]=space?' ':'X';
 assert(charmap_blank_tail_pen(&l,42)==(space?(rev?255-pen:pen):42));assert(l.size==2);cases++;
}
assert(charmap_blank_tail_pen(0,42)==42);l.size=0;assert(charmap_blank_tail_pen(&l,42)==42);
printf("Blank-tail PASS: %u pen/rendition/text combinations, no size mutation\n",cases);
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'t.c';e=Path(d)/'t';p.write_text(src)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(e)],check=True);subprocess.run([str(e)],check=True)
