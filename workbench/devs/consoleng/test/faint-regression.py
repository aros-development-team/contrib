#!/usr/bin/env python3
"""Actual faint policy: RGB channels, indexed semantics and rendition isolation."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'stdconclass.c').read_text();a=s.index('UBYTE ConsoleNGFaintPen(');helper=s[a:s.index('static void setstyle(',a)]
s=(r/'consoleclass.c').read_text();a=s.index('                case 2:\n');b=s.index('                case 3:\n',a);cases=s[a:b];a=s.index('                case 22:\n');b=s.index('                case 23:\n',a);cases+=s[a:b]
code=r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
typedef uint8_t UBYTE;typedef uint32_t ULONG;
#define CON_TXTFLAGS_FAINT 128
#define CON_TXTFLAGS_BOLD 2
#define CON_TXTFLAGS_CONCEALED 16
#define CONPEN_ISXTERM(n) ((n)>=256&&(n)<=511)
#define CONPEN_XTERMIDX(n) ((n)-256)
#define CONPEN_XTERM(n) (256+(n))
#define CONPEN_ISBRIGHT(n) ((n)>=512&&(n)<=519)
#define CONPEN_BRIGHTIDX(n) ((n)-512)
#define CONPEN_RGB(r,g,b) (0x1000000u|((r)<<16)|((g)<<8)|(b))
struct Library{int unused;};struct Screen{struct{void*ColorMap;}ViewPort;};struct Window{struct Screen*WScreen;};
typedef struct{struct Window*cu_Window;UBYTE cu_TxFlags,cu_FgPen;}Object;
#define CU(o) (o)
static ULONG red,green,blue,last;static int gets,looks;
static void GetRGB32(void*cm,UBYTE pen,int count,ULONG*rgb){assert(pen==33&&count==1);gets++;rgb[0]=red*0x1010101u;rgb[1]=green*0x1010101u;rgb[2]=blue*0x1010101u;}
static void Console_GetColorPen(Object*o,ULONG key,UBYTE*out){assert(out!=&o->cu_FgPen);last=key;looks++;*out=77;}
'''+helper+'\nstatic void sgr(Object*o,int code){switch(code){'+cases+'default:break;}}\n'+r'''
int main(void){struct Screen screen={0};struct Window window={&screen};Object o={&window,0,33};int count=0;
const ULONG keys[]={8,1,0x1234567,256,263,264,271,272,298,511,512,513,514,515,516,517,518,519};
for(unsigned flags=0;flags<256;flags++)for(unsigned value=0;value<256;value++)for(unsigned i=0;i<18;i++){
 ULONG key=keys[i];red=value;green=value^0x55;blue=value^0xaa;gets=looks=0;last=0;
 UBYTE pen=ConsoleNGFaintPen(&o,0,flags,33,key);
 int active=(flags&128)&&!(flags&16);int indexed=key>=256&&key<=511;
 if(indexed&&(key>=272||(flags&2)))active=0;
 assert(pen==(active?77:33)&&o.cu_FgPen==33&&looks==active);
 if(active){if(key>=512&&key<=519){assert(!gets&&last==key-512+256);}else if(indexed&&key>=264){assert(!gets&&last==key-8);}else{assert(gets==1&&last==CONPEN_RGB(red*66/100,green*66/100,blue*66/100));}}
 else assert(!gets);
 count++;
}
for(int flags=0;flags<256;flags++){o.cu_TxFlags=flags;sgr(&o,2);assert(o.cu_TxFlags==(flags|128));sgr(&o,22);assert(o.cu_TxFlags==(flags&~130));}
o.cu_Window=0;assert(ConsoleNGFaintPen(&o,0,128,33,8)==33);
printf("Faint PASS:%d policy/channel cases,SGR2/22 independence,logical pen unchanged\n",count);
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';b=Path(d)/'test';p.write_text(code)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(b)],check=True);subprocess.run([str(b)],check=True)
 p.write_text(code.replace('index >= 16 ||',''))
 subprocess.run(['cc','-Wall','-Werror',str(p),'-o',str(b)],check=True)
 assert subprocess.run([str(b)],capture_output=True).returncode!=0
 print('Faint extended-index negative control rejected')

 p.write_text(code.replace('CONPEN_XTERM(CONPEN_BRIGHTIDX(key))', 'CONPEN_XTERM(CONPEN_BRIGHTIDX(key) + 8)'))
 subprocess.run(['cc','-Wall','-Werror',str(p),'-o',str(b)],check=True)
 assert subprocess.run([str(b)],capture_output=True).returncode!=0
 print('Named bright negative control rejected')
