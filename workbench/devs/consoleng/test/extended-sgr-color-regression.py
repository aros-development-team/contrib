#!/usr/bin/env python3
"""Actual semicolon color consumption, range validation and trailing parameters."""
from pathlib import Path
import subprocess,tempfile
s=(Path(__file__).resolve().parents[1]/'consoleclass.c').read_text()
a=s.index('                /* Consume extended colors');b=s.index('                /* Bright ANSI:',a)
code=r'''
#include <assert.h>
#include <stdio.h>
typedef unsigned long ULONG;typedef long IPTR;typedef int BOOL;typedef unsigned char UBYTE;
#define TRUE 1
#define FALSE 0
#define CU(o) (o)
#define ICU(o) (o)
#define CONPEN_XTERM(n) (0x100+(n))
#define CONPEN_RGB(r,g,b) (0x1000000|((r)<<16)|((g)<<8)|(b))
typedef struct {UBYTE cu_FgPen,cu_BgPen;ULONG underlineColor;} Object;
static IPTR key,seen[16];static int calls,ns;
static void Console_GetColorPen(Object *o,IPTR value,UBYTE *out){(void)o;key=value;calls++;*out=123;}
static void run(Object *o,const IPTR *v,UBYTE n){struct {const IPTR *Params;UBYTE NumParams;} request={v,n},*msg=&request;calls=ns=0;key=-1;
for(UBYTE i=0;i<n;i++){IPTR param=v[i];
'''+s[a:b]+r'''
seen[ns++]=param;}}
int main(void){IPTR values[]={-1,0,1,254,255,256,65535};int cases=0;
for(int target=38;target<=58;target+=10){
 for(int a=0;a<7;a++){
  IPTR p[]={target,5,values[a],7};Object o={10,11};run(&o,p,4);int valid=values[a]>=0&&values[a]<=255;
  assert(calls==(valid&&target!=58)&&ns==1&&seen[0]==7);if(valid){if(target==58)assert(o.underlineColor==0x101+values[a]);else assert(key==0x100+values[a]);}
  assert(o.cu_FgPen==(valid&&target==38?123:10)&&o.cu_BgPen==(valid&&target==48?123:11));cases++;
 }
 for(int a=0;a<7;a++)for(int b=0;b<7;b++)for(int c=0;c<7;c++){
  IPTR p[]={target,2,values[a],values[b],values[c],7};Object o={10,11};run(&o,p,6);
  int consumed=2,valid=1;for(int j=2;j<5;j++){consumed++;if(p[j]<0||p[j]>255){valid=0;break;}}
  assert(calls==(valid&&target!=58)&&ns==6-consumed);for(int j=0;j<ns;j++)assert(seen[j]==p[j+consumed]);
  if(valid){if(target==58)assert(o.underlineColor==0x1000001+p[2]*65536+p[3]*256+p[4]);else assert(key==0x1000000+p[2]*65536+p[3]*256+p[4]);}
  cases++;
 }
 IPTR p[]={target,2,1,4,31};for(int n=1;n<5;n++){Object o={10,11};run(&o,p,n);assert(!calls&&!ns&&o.cu_FgPen==10&&o.cu_BgPen==11);cases++;}
 IPTR bad[]={target,3,7};Object o={10,11};run(&o,bad,3);assert(!calls&&ns==1&&seen[0]==7);cases++;
}
printf("Extended SGR colors PASS: %d range/iterator/truncation cases\n",cases);}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'t.c';b=Path(d)/'t'
 for negative in [0,1,2]:
  body=code.replace('index >= 0 && index <= 255','TRUE') if negative==1 else code
  if negative==2:body=body.replace('CONPEN_RGB(rgb[0], rgb[1], rgb[2])','CONPEN_RGB(rgb[2], rgb[1], rgb[0])')
  p.write_text(body);subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(b)],check=True)
  result=subprocess.run([str(b)],capture_output=True);assert bool(result.returncode)==bool(negative),result.stderr.decode()
  if not negative:print(result.stdout.decode(),end='')
print('Out-of-range and RGB order negative controls rejected')
