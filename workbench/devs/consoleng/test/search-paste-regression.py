#!/usr/bin/env python3
"""Actual bounded query paste against Python's Unicode decoder and text oracle."""
from pathlib import Path
import random,struct,subprocess,tempfile
r=Path(__file__).resolve().parents[1]
s=(r/'search.c').read_text();f=s[s.index('BOOL con_search_append('):]
prefix=r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint32_t ULONG;typedef uint8_t UBYTE;typedef int BOOL;
#define TRUE 1
#define FALSE 0
#define CON_SEARCH_MAX 256
#define CON_SEARCH_PASTE_BYTES (4 * CON_SEARCH_MAX)
'''
main=r'''
int main(int argc,char**argv){
 assert(argc==2);FILE*in=fopen(argv[1],"rb");assert(in);ULONG h[5],q[256],before[256],expected[256];UBYTE bytes[2048];unsigned cases=0;
 while(fread(h,sizeof(h),1,in)==1){
  ULONG size=h[0],length=h[1],utf8=h[2],valid=h[3],wanted=h[4];assert(size<=sizeof(bytes)&&wanted<=256);
  assert(fread(bytes,1,size,in)==size);assert(fread(expected,sizeof(*expected),wanted,in)==wanted);
  for(unsigned i=0;i<256;i++)q[i]=before[i]=0x10000+i;
  BOOL got=con_search_append(q,&length,bytes,size,utf8);assert(got==(BOOL)valid);
  assert(length==wanted&&!memcmp(q,expected,wanted*sizeof(*q)));
  assert(!memcmp(q+wanted,before+wanted,(256-wanted)*sizeof(*q)));cases++;
 }
 assert(feof(in));fclose(in);
 ULONG n=0;assert(con_search_append(q,&n,NULL,0,1)&&n==0);
 assert(!con_search_append(q,&n,NULL,1,1)&&n==0);
 assert(!con_search_append(NULL,&n,bytes,0,1));assert(!con_search_append(q,NULL,bytes,0,1));
 n=257;assert(!con_search_append(q,&n,bytes,0,1)&&n==257);
 printf("Search paste PASS:%u independent Unicode/Latin-1/control/capacity cases, atomic failure and null bounds\n",cases);
}
'''
rng=random.Random(185);cases=[]
def add(data,old=0,utf8=True):
 before=list(range(0x10000,0x10000+old));valid=True
 try:
  if len(data)>1024:raise ValueError()
  text=data.decode('utf-8' if utf8 else 'latin1')
  text=text.replace('\r\n','\n').replace('\r','\n').replace('\t',' ')
  appended=[ord(x) for x in text if (ord(x)>=32 or x=='\n') and not 127<=ord(x)<=159]
  if old+len(appended)>256:raise ValueError()
  expected=before+appended
 except (ValueError,UnicodeError):valid=False;expected=before
 cases.append((data,old,int(utf8),int(valid),expected))
for _ in range(10000):
 data=rng.randbytes(rng.randrange(81));add(data,rng.randrange(257),bool(rng.randrange(2)))
chars=[0,9,10,13,27,31,32,65,127,128,159,160,0xe9,0x3a9,0x4e2d,0x1f600,0x10ffff]
for _ in range(1000):add(''.join(chr(rng.choice(chars)) for _ in range(rng.randrange(257))).encode(),rng.randrange(33))
for data in [b'abc\xe2\x82',b'abc\xc0\xaf',b'abc\xe0\x80\xaf',b'abc\xed\xa0\x80',b'abc\xf0\x80\x80\xaf',b'abc\xf4\x90\x80\x80',b'a'*256,b'a'*257,b'\r\n\r\n\t\x00\x1b',b'\0'*1024,b'a'*1025,chr(0x1f600).encode()*256,bytes(range(256))]:
 for old in [0,1,255,256]:
  for utf8 in [False,True]:add(data,old,utf8)
with tempfile.TemporaryDirectory() as d:
 d=Path(d);datafile=d/'cases.bin'
 with datafile.open('wb') as out:
  for data,old,utf8,valid,expected in cases:
   out.write(struct.pack('<5I',len(data),old,utf8,valid,len(expected)));out.write(data)
   if expected:out.write(struct.pack('<'+'I'*len(expected),*expected))
 p=d/'t.c';b=d/'t';p.write_text(prefix+f+main)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(b)],check=True);subprocess.run([str(b),str(datafile)],check=True)
 for label,broken in [('noncanonical UTF-8',f.replace('cp < minimum ||','minimum == 0 ||')),('partial failed append',f.replace('if (extra > size - at) return FALSE;','if (extra > size - at) { query[0] = 0; return FALSE; }'))]:
  p.write_text(prefix+broken+main);subprocess.run(['cc','-Wall','-Werror',str(p),'-o',str(b)],check=True)
  assert subprocess.run([str(b),str(datafile)],capture_output=True).returncode!=0
  print('Negative control PASS:',label)
