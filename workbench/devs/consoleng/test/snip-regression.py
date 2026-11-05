#!/usr/bin/env python3
"""Exercise production clipboard bodies with deterministic allocation failures."""
from pathlib import Path
import subprocess, tempfile
root=Path(__file__).resolve().parents[1]
def body(name):
 s=(root/name).read_text()
 return s.split('AROS_LIBFUNC_INIT',1)[1].split('AROS_LIBFUNC_EXIT',1)[0]
code=r'''
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
typedef unsigned long ULONG;typedef long LONG;typedef void *APTR;
#define MEMF_PUBLIC 0
#define MEMF_CLEAR 1
struct Base {void *copyBuffer;ULONG copyBufferSize;int copyBufferLock;};
static int fail,live,locked;
static void *AllocMem(ULONG n,int flags){(void)flags;assert(n);if(fail)return NULL;void*p=malloc(n);assert(p);live++;return p;}
static void FreeMem(void*p,ULONG n){assert(!!p==!!n);if(p){live--;free(p);}}
static void *AllocVec(ULONG n,int flags){(void)flags;if(fail)return NULL;return calloc(1,n);}
#define CopyMem(s,d,n) memcpy(d,s,n)
static void ObtainSemaphore(int*p){assert(!locked);locked=1;(void)p;}
static void ReleaseSemaphore(int*p){assert(locked);locked=0;(void)p;}
static LONG put(struct Base*ConsoleDevice,APTR data){
'''+body('setconsnip.c')+r'''
}
static APTR get(struct Base*ConsoleDevice){
'''+body('getconsnip.c')+r'''
}
int main(void){struct Base b={0};char payload[65537];int cases=0;
assert(!get(&b));assert(!put(&b,NULL));assert(!put(&b,""));
for(unsigned n=1;n<=65536;n*=2){
 memset(payload,'A'+n%26,n);payload[n]=0;
 assert(put(&b,payload)==1 && live==1 && !locked);
 char *snapshot=get(&b);assert(snapshot && !strcmp(snapshot,payload));
 void*old=b.copyBuffer;ULONG oldsize=b.copyBufferSize;
 fail=1;assert(!put(&b,"replacement"));assert(b.copyBuffer==old && b.copyBufferSize==oldsize && live==1 && !locked);
 assert(!get(&b));assert(b.copyBuffer==old);fail=0;
 char *copy=get(&b);assert(copy && !strcmp(copy,snapshot));free(copy);
 assert(put(&b,snapshot)==1);free(snapshot);assert(live==1);
 assert(!put(&b,n&1?NULL:""));assert(!get(&b)&&!live&&!locked);cases++;
}
assert(put(&b,"UTF-8: \xc3\xa9 \xe2\x82\xac \xf0\x9f\x98\x80"));
char *s=get(&b);assert(!strcmp(s,"UTF-8: \xc3\xa9 \xe2\x82\xac \xf0\x9f\x98\x80"));free(s);
fail=1;assert(!put(&b,NULL));assert(!live&&!locked);
printf("SNIP PASS %d sizes, allocation failure retention, copies and clear\n",cases);
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';e=Path(d)/'test';p.write_text(code)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(e)],check=True)
 subprocess.run([str(e)],check=True)
 old=body('setconsnip.c').replace('    ULONG size =', '    FreeMem(ConsoleDevice->copyBuffer, ConsoleDevice->copyBufferSize);\n    ConsoleDevice->copyBuffer = NULL;\n    ConsoleDevice->copyBufferSize = 0;\n    ULONG size =', 1)
 p.write_text(code.replace(body('setconsnip.c'),old))
 subprocess.run(['cc',str(p),'-o',str(e)],check=True)
 assert subprocess.run([str(e)],capture_output=True).returncode!=0,'old destructive allocation escaped'
 print('SNIP old destructive allocation rejected')
