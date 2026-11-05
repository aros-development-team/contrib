#!/usr/bin/env python3
"""Exercise short ESC controls without consuming unsupported or partial input."""
from pathlib import Path
import subprocess,tempfile
s=(Path(__file__).resolve().parents[1]/'support.c').read_text()
f='static BOOL getshortescape('+s.split('static BOOL getshortescape(',1)[1].split('/* ANSI aliases',1)[0]
code=r'''
#include <assert.h>
typedef signed char BYTE;typedef unsigned char UBYTE;typedef long LONG;typedef int BOOL;
#define TRUE 1
#define FALSE 0
#define ESC 27
#define C_INDEX 1
#define C_NEXT_LINE 2
#define C_H_TAB_SET 3
#define C_REVERSE_IDX 4
#define C_SAVE_CURSOR 5
#define C_RESTORE_CURSOR 6
#define C_TERMINAL_RESET 7
#define C_SCREEN_ALIGNMENT 8
#define C_NIL 9
#define C_PRIMARY_ATTRIBUTES 10
'''+f+r'''
int main(void){
 const char codes[]="DEHM78c";
 for(int c=0;c<256;c++)for(int n=0;n<=3;n++)for(int esc=0;esc<2;esc++){
  UBYTE input[]={esc?27:'X',c,'Z'},*p=input;BYTE cmd=99;int want=0;
  if(n>=2&&esc)for(int i=0;i<7;i++)if(c==codes[i])want=i+1;
  if(n>=2&&esc&&c=='Z')want=C_PRIMARY_ATTRIBUTES;
  if(n>=2&&esc&&c=='\\')want=C_NIL;
  if(n>=3&&esc&&c=='#')want=C_NIL;
  assert(getshortescape(&cmd,&p,n)==!!want);
  assert(cmd==(want?want:99)&&p==input+(want?(c=='#'?3:2):0));
 }
 UBYTE alignment[]={27,'#','8'},*p=alignment;BYTE command=0;assert(getshortescape(&command,&p,3)&&command==C_SCREEN_ALIGNMENT&&p==alignment+3);
 return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'t.c';b=Path(d)/'t';p.write_text(code)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(b)],check=True)
 subprocess.run([str(b)],check=True)
print('Short escapes PASS: 2048 code/length/prefix cases; IND/NEL/HTS/RI/DECSC/DECRC/DECID and unchanged rejected input')
