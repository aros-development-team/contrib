#!/usr/bin/env python3
"""Execute the real split-write block across signed-WORD length boundaries."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'support.c').read_text()
a=s.index('    /* Complete an UTF-8 sequence split across CMD_WRITEs */');b=s.index('    /* Interpret string into a command',a);block=s[a:b]
code=r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
typedef int BOOL;typedef uint8_t UBYTE;typedef int16_t WORD;typedef uint32_t ULONG;typedef intptr_t IPTR;typedef struct Unit Object;
struct Unit {UBYTE utf8Stash[4];UBYTE utf8StashLen;};
#define ICU(x) (x)
#define CON_IS_UTF8(x) 1
#define MIN(a,b) ((a)<(b)?(a):(b))
#define C_ASCII_STRING 1
#define ReturnInt(name,type,value) return value
static unsigned emits,emitted;static UBYTE result[4];
static WORD utf8_seqlen(UBYTE lead){return lead<0xe0?2:lead<0xf0?3:4;}
static void Console_UnRenderCursor(Object*u){(void)u;}
static void Console_RenderCursor(Object*u){(void)u;}
static void con_emit_command(Object*u,int cmd,int n,IPTR*p){(void)u;assert(cmd==1&&n==2&&p[1]>=1&&p[1]<=4);memcpy(result,(void*)p[0],p[1]);emitted=p[1];emits++;}
static void *bounded_copy(void*d,const void*s,size_t n){assert(n<=3);return memcpy(d,s,n);}
#define memcpy bounded_copy
static ULONG run(struct Unit*unit,UBYTE*write_str,ULONG towrite){ULONG orig_towrite=towrite;
'''+block+r'''
return orig_towrite-towrite;}
#undef memcpy
int main(void){ULONG sizes[]={1,2,3,32767,32768,65535,65536,65537,1048576};unsigned cases=0;
UBYTE sequences[3][4]={{0xc2,0xa3},{0xe4,0xb8,0xad},{0xf0,0x9f,0x98,0x80}};
UBYTE *buf=calloc(1048576,1);assert(buf);
for(unsigned k=0;k<3;k++)for(unsigned prefix=1;prefix<k+2;prefix++)for(unsigned j=0;j<sizeof(sizes)/sizeof(*sizes);j++){
 struct Unit u={{0},0};unsigned n=k+2,need=n-prefix,take=sizes[j]<need?sizes[j]:need;
 memcpy(u.utf8Stash,sequences[k],prefix);u.utf8StashLen=prefix;memcpy(buf,sequences[k]+prefix,need);emits=0;
 assert(run(&u,buf,sizes[j])==take);assert(emits==(take==need));
 if(emits){assert(u.utf8StashLen==0);assert(!memcmp(result,sequences[k],n));}
 else{assert(u.utf8StashLen==prefix+take);assert(!memcmp(u.utf8Stash,sequences[k],prefix+take));}
 cases++;
}
unsigned interruptions=0;
for(unsigned k=0;k<3;k++)for(unsigned prefix=1;prefix<k+2;prefix++)for(unsigned extra=0;extra<k+2-prefix;extra++)for(unsigned byte=0;byte<256;byte++){
 if((byte&0xc0)==0x80)continue;
 struct Unit u={{0},0};memcpy(u.utf8Stash,sequences[k],prefix);u.utf8StashLen=prefix;
 memcpy(buf,sequences[k]+prefix,extra);buf[extra]=byte;buf[extra+1]='Q';emits=emitted=0;
 assert(run(&u,buf,extra+2)==extra);assert(emits==1&&u.utf8StashLen==0);
 assert(emitted==1&&result[0]=='?');assert(buf[extra]==byte);interruptions++;
}
printf("UTF8 interrupted stash PASS cases=%u\n",interruptions);
free(buf);printf("UTF8 stash length PASS cases=%u\n",cases);}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'t.c';exe=Path(d)/'t'
 for negative in [0,1,2]:
  body=code.replace('ULONG take = MIN((ULONG)(need - ICU(unit)->utf8StashLen), towrite);','WORD take = MIN(need - ICU(unit)->utf8StashLen, (WORD) towrite);') if negative==1 else code
  if negative==2:body=body.replace('while (valid < take && (write_str[valid] & 0xc0) == 0x80) valid++;','valid = take;')
  p.write_text(body);subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(exe)],check=True)
  result=subprocess.run([str(exe)],capture_output=True);assert bool(result.returncode)==bool(negative),result.stderr.decode()
  if not negative:print(result.stdout.decode(),end='')
print('Signed-WORD truncation and swallowed-byte negative controls rejected')
