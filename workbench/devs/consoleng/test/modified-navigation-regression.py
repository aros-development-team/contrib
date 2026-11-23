#!/usr/bin/env python3
"""Execute native navigation encoder with exhaustive qualifier and event guards."""
from pathlib import Path
import subprocess, tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'consoletask.c').read_text()
f=s[s.index('static LONG con_modified_navigation('):s.index('static BOOL checkconunit(')]
prefix='''#include <assert.h>
#include <string.h>
#include <stdio.h>
typedef unsigned char UBYTE; typedef unsigned long ULONG; typedef long LONG; typedef int BOOL;
struct InputEvent {int ie_Class,ie_Code; ULONG ie_Qualifier;};
#define IECLASS_RAWKEY 1
#define IECODE_UP_PREFIX 128
'''
quals=['LSHIFT','RSHIFT','LALT','RALT','CONTROL','LCOMMAND','RCOMMAND']
for i,q in enumerate(quals): prefix+=f'#define IEQUALIFIER_{q} {1<<i}\n'
keys=['UP','DOWN','RIGHT','LEFT','HOME','END','INSERT','DELETE','PAGEUP','PAGEDOWN']+[f'F{i}' for i in range(1,13)]
for i,k in enumerate(keys): prefix+=f'#define RAWKEY_{k} {i+20}\n'
main=r'''
int main(void){unsigned count=0;
 for(int utf=0;utf<2;utf++)for(int cls=0;cls<2;cls++)for(int key=0;key<256;key++)for(int q=0;q<256;q++){
 UBYTE b[10],before[10];memset(b,0xa5,10);memcpy(before,b,10);
 struct InputEvent e={cls,key,q};int m=1+!!(q&3)+2*!!(q&12)+4*!!(q&16);
 int expected=utf&&cls==1&&key>=20&&key<42&&!(q&96)&&m>1;
 int n=con_modified_navigation(b,2,&e,utf);
 if(expected){char want[10];int k=key-20;const char *bases[]={"1","1","1","1","1","1","2","3","5","6","1","1","1","1","15","17","18","19","20","21","23","24"};int size=snprintf(want,sizeof(want),"\033[%s;%d%c",bases[k],m,"ABCDHF~~~~PQRS~~~~~~~~"[k]);assert(n==size&&!memcmp(b,want,size)&&!memcmp(b+size,before+size,10-size));}
 else assert(n==2&&!memcmp(b,before,10));
 count++;
 }
 printf("Modified navigation/function keys PASS: %u qualifier/event/mode combinations\n",count);
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'probe.c';exe=Path(d)/'probe';p.write_text(prefix+f+main)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
 # Reject a missing-control-bit mutation, proving exact modifiers are checked.
 p.write_text(prefix+f.replace('modifier += 4','modifier += 0')+main)
 subprocess.run(['cc','-Wall','-Werror',str(p),'-o',str(exe)],check=True)
 assert subprocess.run([str(exe)],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL).returncode!=0

 # Two-digit function-key parameters must not lose their leading digit.
 p.write_text(prefix+f.replace("if (tens) bytes[out++] = tens;", "(void)tens;")+main)
 subprocess.run(['cc','-Wall','-Werror',str(p),'-o',str(exe)],check=True)
 assert subprocess.run([str(exe)],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL).returncode!=0
 print('Missing Control modifier and truncated function parameter negative controls rejected')
