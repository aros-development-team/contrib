#!/usr/bin/env python3
"""First retained raster batch cannot alias an unknown style sentinel."""
from pathlib import Path
import subprocess, tempfile
s=(Path(__file__).resolve().parents[1]/'charmapconclass.c').read_text()
a=s.index('    UBYTE flags = 0;\n    BOOL styleSet = FALSE;')
initial=s[a:s.index('\n\n',a)]
a=s.index('            if (!styleSet || decoration != previousDecoration ||')
b=s.index('            flags = drawFlags;',a)+len('            flags = drawFlags;')
block=s[a:b]
code=r'''
#include <assert.h>
#include <stdio.h>
typedef unsigned char UBYTE;typedef int BOOL;typedef unsigned long ULONG;
#define FALSE 0
#define TRUE 1
#define CON_TXTFLAGS_MASK 7
#define CON_TXTFLAGS_UNDERLINED 4
static int rasterStyle,calls;
#define SetSoftStyle(rp,f,m) (rasterStyle=((f)&(m)),calls++)
static void test(int previous,int first,int next,int d1,int d2,int c1,int c2){
'''+initial+r'''
 rasterStyle=previous;calls=0;UBYTE drawFlags=first,decoration=d1;ULONG underlineColor=c1;
'''+block+r'''
 assert(rasterStyle==(first&((d1||c1)?3:7)));assert(calls==1);
 drawFlags=next;decoration=d2;underlineColor=c2;
'''+block+r'''
 assert(rasterStyle==(next&((d2||c2)?3:7)));assert(calls==(1+((first&7)!=(next&7)||d1!=d2||c1!=c2)));
}
int main(void){int cases=0;
 for(int p=0;p<8;p++)for(int f=0;f<256;f++)for(int n=0;n<256;n++){
 for(int d1=0;d1<4;d1++)for(int d2=0;d2<4;d2++){for(int c1=0;c1<2;c1++)for(int c2=0;c2<2;c2++){test(p,f,n,d1,d2,c1,c2);cases++;}}}
 printf("REFRESHSTYLE PASS %d initial/style-transition cases\n",cases);
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';exe=Path(d)/'test';p.write_text(code)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
 # Restore the old sentinel and omit first-run initialization.
 p.write_text(code.replace('UBYTE flags = 0;', 'UBYTE flags = 255;').replace('!styleSet || ', '').replace('decoration != previousDecoration || ', ''))
 subprocess.run(['cc',str(p),'-o',str(exe)],check=True)
 result=subprocess.run([str(exe)],capture_output=True)
 assert result.returncode!=0,'old sentinel negative control escaped'
 print('REFRESHSTYLE old-sentinel negative control rejected')
