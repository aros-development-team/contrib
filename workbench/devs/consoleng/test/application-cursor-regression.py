#!/usr/bin/env python3
"""Actual application-key translator: exact cursor sequences and preservation."""
from pathlib import Path
import subprocess,tempfile
s=(Path(__file__).resolve().parents[1]/'consoletask.c').read_text();a=s.index('static LONG con_application_cursor(');start=s.index('{',a);depth=1;b=start+1
while depth:
 if s[b]=='{':depth+=1
 elif s[b]=='}':depth-=1
 b+=1
source=r'''
#include <assert.h>
#include <string.h>
typedef unsigned char UBYTE;typedef long LONG;typedef unsigned long ULONG;
#define CF_APPLICATION_CURSOR 256
'''+s[a:b]+r'''
int main(void){
 for(int mode=0;mode<2;mode++)for(int seven=0;seven<2;seven++)for(int key=0;key<256;key++) {
  UBYTE b[8]={seven?27:0x9b,seven?'[':key,key,0x55,0x66};UBYTE original[8];memcpy(original,b,8);
  int n=seven?3:2;int expected=mode&&key&&strchr("ABCDHF",key);
  int actual=con_application_cursor(b,n,mode?CF_APPLICATION_CURSOR:0);
  if(expected)assert(actual==3&&b[0]==27&&b[1]=='O'&&b[2]==key&&b[3]==0x55);
  else assert(actual==n&&!memcmp(b,original,8));
 }
 UBYTE modified[]={27,'[','1',';','5','D'};UBYTE copy[6];memcpy(copy,modified,6);
 assert(con_application_cursor(modified,6,CF_APPLICATION_CURSOR)==6&&!memcmp(copy,modified,6));
 UBYTE empty[3]={0};assert(con_application_cursor(empty,0,CF_APPLICATION_CURSOR)==0);
 return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';b=Path(d)/'test';p.write_text(source)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(b)],check=True)
 subprocess.run([str(b)],check=True)
print('Application cursor PASS: 1024 mode/prefix/final combinations; modified and empty input preserved')
