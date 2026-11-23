#!/usr/bin/env python3
"""Actual colon-SGR parser: atomic groups, partial input and capacity."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'support.c').read_text();a=s.index('static BOOL getcolonsgr(');f=s[a:s.index('static BOOL string2command(',a)]
code=r'''
#include <assert.h>
#include <string.h>
#include <stdio.h>
typedef int BOOL;typedef signed char BYTE;typedef unsigned char UBYTE;typedef unsigned long ULONG;typedef long LONG;typedef long IPTR;
#define TRUE 1
#define FALSE 0
#define MAX_COMMAND_PARAMS 16
#define C_SELECT_GRAPHIC_RENDITION 7
#define C_NIL 0
'''+f+r'''
static void test(const char *seq,const IPTR *want,int n){for(unsigned len=0;len<=strlen(seq);len++){UBYTE *p=(UBYTE*)seq,count=99;BYTE cmd=99;IPTR values[16];for(int i=0;i<16;i++)values[i]=777;int yes=len==strlen(seq);assert(getcolonsgr(&cmd,&p,&count,len,values,TRUE)==yes);assert(p==(UBYTE*)seq+(yes?len:0));assert(count==(yes?n:99));assert(cmd==(yes?(n?7:0):99));for(int i=0;i<16;i++)assert(values[i]==(yes&&i<n?want[i]:777));}}
int main(void){IPTR defaults[]={1,0,31},zeros[]={0,0,0},tail[]={1,0};test("1;;31m",defaults,3);test(";;m",zeros,3);test("1;m",tail,2);
const char *native[]={"1;;31m",";;m","1;m"};for(int k=0;k<3;k++){UBYTE*p=(UBYTE*)native[k],n=99;BYTE c=99;IPTR v[16];assert(!getcolonsgr(&c,&p,&n,strlen(native[k]),v,FALSE)&&n==99&&c==99&&p==(UBYTE*)native[k]);}
IPTR off[]={24},one[]={4},two[]={21},mixed[]={1,21,38,2,12,34,56,0},ignored[]={1,-5,31},curly[]={-3},dotted[]={-4},dashed[]={-5};
test("4:0m",off,1);test("4:1m",one,1);test("4:2m",two,1);test("004:0002m",two,1);test("4:m",off,1);test("1;4:2;38;2;12;34;56;m",mixed,8);test("1;4:5;31m",ignored,3);test("4:99m",one,1);test("4:2:9m",one,1);test("4::m",one,1);test("4:9999999999999999999m",one,1);test("4:3m",curly,1);test("4:4m",dotted,1);test("4:5m",dashed,1);test("4:5:9m",one,1);IPTR ulrgb[]={58,2,12,34,56},ulidx[]={58,5,255};test("58:2::12:34:56m",ulrgb,5);test("58:5:255m",ulidx,3);IPTR rgb[]={38,2,12,34,56},idx[]={48,5,255};test("38:2::12:34:56m",rgb,5);test("38:2:0:12:34:56m",rgb,5);test("38:2:12:34:56m",rgb,5);test("48:5:255m",idx,3);
const char *badcolor[]={"38:2:1:12:34:56m","38:2::12::56m","38:2:256:0:0m","48:5:256m","48:5:m","38:2:0:1:2:3:4m","38::1:2:3m"};for(unsigned k=0;k<7;k++)test(badcolor[k],0,0);test("999999999999999999999:2m",0,0);
const char *plain[]={"4m","21m","?4:2m","4:2 q",">4:2m"};for(int k=0;k<5;k++){UBYTE*p=(UBYTE*)plain[k],count=99;BYTE cmd=99;IPTR v[16];assert(!getcolonsgr(&cmd,&p,&count,strlen(plain[k]),v,TRUE)&&p==(UBYTE*)plain[k]&&count==99&&cmd==99);}
char seq[200];strcpy(seq,"4:2");IPTR expected[16];expected[0]=21;for(int i=1;i<16;i++){strcat(seq,";1");expected[i]=1;}strcat(seq,"m");test(seq,expected,16);seq[strlen(seq)-1]=0;strcat(seq,";1m");test(seq,0,0);
strcpy(seq,"4:2");expected[0]=21;for(int i=1;i<11;i++){strcat(seq,";1");expected[i]=1;}strcat(seq,";38:2:12:34:56m");expected[11]=38;expected[12]=2;expected[13]=12;expected[14]=34;expected[15]=56;test(seq,expected,16);
strcpy(seq,"4:2");for(int i=1;i<12;i++)strcat(seq,";1");strcat(seq,";38:2:12:34:56m");test(seq,0,0);
puts("Colon SGR PASS: selectors, ordered colors, unsupported groups, fragmentation, saturation, capacity");}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';b=Path(d)/'test'
 for negative in [0,1,2,3]:
  body=code.replace("style == 1 ? 4 : 21", "style == 1 ? 4 : 2") if negative==1 else code
  if negative==2:body=body.replace("parts[rgb+c]", "parts[rgb+2-c]")
  if negative==3:body=body.replace("if (i != next || style > 5) style = 1;", "if (i != next || style > 5) style = 3;")
  p.write_text(body)
  subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(b)],check=True)
  result=subprocess.run([str(b)],capture_output=True);assert bool(result.returncode)==bool(negative),result.stderr.decode()
  if not negative:print(result.stdout.decode(),end='')
print('Faint-confusion, RGB-channel and selector-fallback negative controls rejected')
