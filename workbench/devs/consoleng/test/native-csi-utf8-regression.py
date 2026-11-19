#!/usr/bin/env python3
"""Standalone native CSI must not split valid UTF-8 continuation bytes."""
from pathlib import Path
import subprocess,tempfile
s=(Path(__file__).resolve().parents[1]/'support.c').read_text()
def extract(name):
 a=s.rindex('static '+name);start=s.index('{',a);depth=1;b=start+1
 while depth:
  if s[b]=='{':depth+=1
  elif s[b]=='}':depth-=1
  b+=1
 return s[a:b]+'\n'
f=extract('WORD utf8_seqlen(')+extract('BOOL check_special(')+extract('LONG con_text_span(')
source=r'''
#include <assert.h>
typedef unsigned char UBYTE;typedef unsigned char *STRPTR;typedef long LONG;typedef short WORD;typedef int BOOL;
#define TRUE 1
#define FALSE 0
#define CSI 0x9b
#define ESC 27
#define NIL 0
#define BELL 7
#define BACKSPACE 8
#define HTAB 9
#define LINEFEED 10
#define VTAB 11
#define FORMFEED 12
#define CARRIAGE_RETURN 13
#define SHIFT_OUT 14
#define SHIFT_IN 15
#define INDEX 0x84
#define NEXT_LINE 0x85
#define H_TAB_SET 0x88
#define REVERSE_INDEX 0x8d
'''+f+r'''
int main(void){
 for(int utf8=0;utf8<2;utf8++){
  UBYTE tab[]={11},span[]={'a',11,'b'};
  assert(check_special(tab,1,utf8));assert(con_text_span(span,3,utf8)==1);
  UBYTE ri[]={0x8d};assert(check_special(ri,1,utf8)==!utf8);
  UBYTE nel[]={0x85},embedded[]={'a',0x85,'b'};
  assert(check_special(nel,1,utf8)==!utf8);
  assert(con_text_span(embedded,3,utf8)==(utf8?3:1));
 }
 assert(check_special((STRPTR)"\x9b" "P",2,1));
 for(unsigned cp=0x80;cp<=0x10ffff;cp++){
  if(cp>=0xd800&&cp<=0xdfff)continue;
  UBYTE b[8]={'x'};int n;
  if(cp<0x800){b[1]=0xc0|(cp>>6);b[2]=0x80|(cp&63);n=2;}
  else if(cp<0x10000){b[1]=0xe0|(cp>>12);b[2]=0x80|((cp>>6)&63);b[3]=0x80|(cp&63);n=3;}
  else {b[1]=0xf0|(cp>>18);b[2]=0x80|((cp>>12)&63);b[3]=0x80|((cp>>6)&63);b[4]=0x80|(cp&63);n=4;}
  b[n+1]=CSI;b[n+2]='K';
  assert(con_text_span(b,n+3,1)==n+1);
  for(int tail=1;tail<=n;tail++)assert(con_text_span(b,tail+1,1)==tail+1);
 }
 UBYTE classic[]={'a',CSI,'P'};assert(con_text_span(classic,3,0)==1);
 return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';b=Path(d)/'test';p.write_text(source)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(b)],check=True)
 subprocess.run([str(b)],check=True)
print('Native CSI UTF8 PASS: every non-ASCII Unicode scalar, truncated tails and standalone CSI')
