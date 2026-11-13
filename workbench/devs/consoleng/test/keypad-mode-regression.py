#!/usr/bin/env python3
"""Actual keypad parser: mode only, exact consumption and no partial mutation."""
from pathlib import Path
import subprocess,tempfile
s=(Path(__file__).resolve().parents[1]/'support.c').read_text();f='static BOOL getkeypadmode('+s.split('static BOOL getkeypadmode(',1)[1].split('static BOOL getshortescape(',1)[0]
source=r'''
#include <assert.h>
typedef int BOOL;typedef signed char BYTE;typedef unsigned char UBYTE;typedef long LONG;
#define TRUE 1
#define FALSE 0
#define ESC 27
#define C_NIL 0
#define CF_APPLICATION_KEYPAD 512
typedef struct {unsigned conFlags;} Object;
#define ICU(o) (o)
'''+f+r'''
int main(void){
 for(int initial=0;initial<2;initial++)for(int final=0;final<256;final++)for(int len=0;len<4;len++) {
  Object o={5|(initial?CF_APPLICATION_KEYPAD:0)};UBYTE seq[3]={27,final,'x'},*p=seq;BYTE cmd=99;
  int expected=len>=2&&(final=='='||final=='>');
  assert(getkeypadmode(&cmd,&p,len,&o)==expected);
  if(expected)assert(cmd==C_NIL&&p==seq+2&&o.conFlags==(5|(final=='='?CF_APPLICATION_KEYPAD:0)));
  else assert(cmd==99&&p==seq&&o.conFlags==(5|(initial?CF_APPLICATION_KEYPAD:0)));
 }
 return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';b=Path(d)/'test';p.write_text(source)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(b)],check=True)
 subprocess.run([str(b)],check=True)
print('Keypad mode PASS: 2048 initial/final/length cases; unrelated flags preserved')
