#!/usr/bin/env python3
"""Actual decoder: exhaustive encodings, shortest forms, scalars and legacy bytes."""
from pathlib import Path
import subprocess,tempfile
s=(Path(__file__).resolve().parents[1]/'support.c').read_text();a=s.index('ULONG con_utf8_decode(');b=s.index('\nULONG con_utf8_encode(',a);decoder=s[a:b]
code=r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
typedef uint8_t UBYTE;typedef uint32_t ULONG;typedef int32_t LONG;typedef int16_t WORD;
'''+decoder+r'''
static void check(const UBYTE*b,int n,ULONG expected,int used){const UBYTE*p=b;LONG left=n;assert(con_utf8_decode(&p,&left)==expected);assert(p==b+used&&left==n-used);}
int main(void){unsigned cases=0;
for(unsigned width=2;width<=4;width++){
 unsigned max=width==2?0x800:width==3?0x10000:0x200000,min=width==2?0x80:width==3?0x800:0x10000;
 for(unsigned cp=0;cp<max;cp++){
  UBYTE b[5]={0};unsigned v=cp;
  for(unsigned i=width-1;i>0;i--){b[i]=0x80|(v&63);v>>=6;}b[0]=(width==2?0xc0:width==3?0xe0:0xf0)|v;b[width]='Q';
  ULONG expected=cp>=min&&cp<=0x10ffff&&!(cp>=0xd800&&cp<=0xdfff)?cp:'?';
  check(b,width+1,expected,width);cases++;
 }
}
for(unsigned byte=0;byte<256;byte++)if(byte<0xc0||byte>=0xf8){UBYTE b[]={byte,'Q'};check(b,2,byte,1);cases++;}
UBYTE seq[3][4]={{0xc2,0xa3},{0xe4,0xb8,0xad},{0xf0,0x9f,0x98,0x80}};
for(unsigned k=0;k<3;k++){
 for(unsigned len=1;len<k+2;len++){check(seq[k],len,'?',len);cases++;}
 for(unsigned pos=1;pos<k+2;pos++)for(unsigned byte=0;byte<256;byte++)if((byte&0xc0)!=0x80){
  UBYTE b[4];for(unsigned i=0;i<4;i++)b[i]=seq[k][i];b[pos]=byte;check(b,k+2,b[0],1);cases++;
 }
}
printf("UTF8 scalar PASS cases=%u\n",cases);}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'t.c';exe=Path(d)/'t'
 for negative in [0,1]:
  body=code
  if negative:
   a=body.index('    if ((need == 2 && cp < 0x80)');b=body.index('    return cp;',a);body=body[:a]+body[b:]
  p.write_text(body);subprocess.run(['cc','-Wall','-Werror','-O1','-fsanitize=address,undefined',str(p),'-o',str(exe)],check=True)
  result=subprocess.run([str(exe)],capture_output=True);assert bool(result.returncode)==bool(negative),result.stderr.decode()
  if not negative:print(result.stdout.decode(),end='')
print('Missing scalar validation negative control rejected')
