#!/usr/bin/env python3
"""Production ANSI save parser: incomplete input, native and private isolation."""
from pathlib import Path
import subprocess,tempfile
s=(Path(__file__).resolve().parents[1]/'support.c').read_text()
f='static BOOL getansisavedcursor('+s.split('static BOOL getansisavedcursor(',1)[1].split('/* Window title and geometry',1)[0]
code=r'''
#include <assert.h>
#include <string.h>
typedef signed char BYTE;typedef unsigned char UBYTE;typedef long LONG;typedef int BOOL;
#define TRUE 1
#define FALSE 0
#define ESC 27
#define C_SAVE_CURSOR 1
#define C_RESTORE_CURSOR 2
'''+f+r'''
int main(void){
const char *good[]={"\033[s","\033[u","\033[0s","\033[99999u","\033[1;2s","\033[1:2u"};
for(int k=0;k<6;k++)for(unsigned n=0;n<=strlen(good[k]);n++){
 UBYTE *p=(void*)good[k],count=99;BYTE c=99;int yes=n==strlen(good[k]);
 assert(getansisavedcursor(&c,&p,&count,n)==yes);
 assert(p==(UBYTE*)good[k]+(yes?n:0)&&count==(yes?0:99)&&c==(yes?(k%2?2:1):99));
}
for(int c=0;c<256;c++){
 UBYTE b[]={27,'[',c,'u','X'},*p=b,n=99;BYTE command=99;
 int yes=(c>='0'&&c<='9')||c==';'||c==':'||c=='s'||c=='u';
 assert(getansisavedcursor(&command,&p,&n,5)==yes);
 if(!yes)assert(p==b&&command==99&&n==99);
 else assert(p==b+((c=='s'||c=='u')?3:4)&&n==0);
 b[0]=0x9b;p=b;n=99;command=99;
 assert(!getansisavedcursor(&command,&p,&n,5)&&p==b&&n==99&&command==99);
}
return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'t.c';b=Path(d)/'t';p.write_text(code)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(b)],check=True)
 subprocess.run([str(b)],check=True)
print('ANSI saved cursor parser PASS: fragments, numeric parameters, all 256 prefix bytes and native CSI isolation')
