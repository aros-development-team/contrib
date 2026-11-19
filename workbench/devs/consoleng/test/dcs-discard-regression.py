#!/usr/bin/env python3
"""Exercise the production bounded DCS consumer across every split point."""
from pathlib import Path
import subprocess, tempfile
r = Path(__file__).resolve().parents[1]
s = (r / 'support.c').read_text()
a = s.index('static LONG discard_prefix(')
f = s[a:s.index('/* Accumulate an OSC string', a)]
code = r'''
#include <assert.h>
#include <string.h>
#include <stdio.h>
typedef long LONG; typedef unsigned char UBYTE; typedef int BOOL;
typedef struct { int dcsDiscard, utf8; } Object;
#define ICU(x) (x)
#define CON_IS_UTF8(x) ((x)->utf8)
#define FALSE 0
#define ESC 27
''' + f + r'''
int main(void) {

 for(int utf=0;utf<2;utf++)for(int byte=0;byte<256;byte++)for(int n=0;n<3;n++){
  UBYTE input[2]={27,byte};int yes=byte=='P'||byte=='X'||byte=='^'||byte=='_';
  assert(discard_prefix(input,n,utf)==(n==2&&yes?2:0));
  input[0]=byte;input[1]='x';yes=byte==0x90||byte==0x98||byte==0x9e||byte==0x9f;
  assert(discard_prefix(input,n,utf)==(n&&!utf&&yes?1:0));
 }
 unsigned char payload[8192]; memset(payload, 'x', sizeof(payload));
 for(int utf=0;utf<2;utf++) for(int end=0;end<5;end++) {
  unsigned char endings[]={27,24,26,156,7};
  payload[4096]=endings[end];
  int terminates=end<3 || (end==3 && !utf);
  long expected=terminates ? 4096+(end!=0) : 8192;
  for(long split=0;split<=8192;split++) {
   Object o={1,utf}; long used=consume_dcs(&o,payload,split);
   if(o.dcsDiscard) used+=consume_dcs(&o,payload+split,8192-split);
   assert(used==expected); assert(o.dcsDiscard==!terminates);
  }
  Object o={1,utf};long used=0;
  while(o.dcsDiscard && used<8192) {
   long n=consume_dcs(&o,payload+used,1);used+=n;
  }
  assert(used==expected);assert(o.dcsDiscard==!terminates);
 }
 Object o={1,1}; unsigned char encoded[]={0xc2,0x9c,'x'};
 assert(consume_dcs(&o,encoded,3)==3 && o.dcsDiscard);
 assert(consume_dcs(&o,encoded,0)==0 && o.dcsDiscard);
 puts("DCS discard PASS: 81930 split cases, bytewise, ESC handoff, CAN/SUB, C1/UTF8, BEL payload, bounded memory");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';b=Path(d)/'test'
 for mutation in [False,True]:
  p.write_text(code.replace('b == 0x18 ||', 'b == 0x07 || b == 0x18 ||') if mutation else code)
  subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(b)],check=True)
  result=subprocess.run([str(b)],capture_output=True)
  assert bool(result.returncode)==mutation,result.stderr.decode()
  if not mutation:print(result.stdout.decode(),end='')
print('Incorrect BEL termination negative control rejected')
