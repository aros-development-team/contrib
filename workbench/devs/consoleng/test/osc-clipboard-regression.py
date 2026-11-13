#!/usr/bin/env python3
"""Actual OSC collector, strict codec and permission dispatch with fault injection."""
from pathlib import Path
import base64, random, subprocess, tempfile
r=Path(__file__).resolve().parents[1]
s='\n'.join(x for x in (r/'osc_clipboard.c').read_text().splitlines() if not x.startswith('#include'))
a=(r/'support.c').read_text();parser=a[a.index('static LONG consume_osc('):a.index('/* DEC Special Graphics')]
prefix=r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef uint8_t UBYTE;typedef int8_t BYTE;typedef int16_t WORD;typedef uint32_t ULONG;typedef int32_t LONG;typedef intptr_t IPTR;typedef int BOOL;typedef void VOID;typedef void*APTR;
#define TRUE 1
#define FALSE 0
#define MEMF_PUBLIC 1
#define MEMF_CLEAR 2
#define CONCLIP_WRITE_MAX (1024UL*1024UL)
#define CON_OSCBUF_SIZE 2048
#define OSC_IDLE 0
#define OSC_COLLECT 1
#define OSC_OVERFLOW 2
#define OSC_GOT_ESC 3
#define OSC_OVERFLOW_ESC 4
#define ESC 27
#define C_NIL 0
#define C_SET_TITLE 1
#define C_SET_FONT 2
#define C_SET_COLOR 3
#define C_SET_HYPERLINK 4
#define C_SET_CURSOR_SHAPE 5
#define C_SET_OSC_CLIPBOARD 6
struct intConUnit{UBYTE oscBuf[2048];uint16_t oscLen;UBYTE oscState;UBYTE*oscClipboard;ULONG oscClipboardLength,oscClipboardCapacity;BOOL oscClipboardInvalid,oscCopyDisabled;};
typedef struct intConUnit Object;
#define ICU(o) (o)
#define CON_IS_UTF8(o) 1
struct Hook{int dummy;};struct ConsoleBase{int sniphooks,copyBufferLock;void*copyBuffer;ULONG copyBufferSize;};
static int live,allocations,fail_at,queued,hooks,provider=1;static UBYTE saved[CONCLIP_WRITE_MAX];static ULONG saved_length;
static void*AllocMem(size_t n,int flags){(void)flags;if(++allocations==fail_at)return NULL;size_t*p=calloc(1,n+sizeof(size_t));assert(p);*p=n;live++;return p+1;}
static void FreeMem(void*p,size_t n){if(!p){assert(!n);return;}size_t*h=(size_t*)p-1;assert(*h==n);free(h);live--;}
static void*AllocVec(size_t n,int flags){return AllocMem(n,flags);}static void FreeVec(void*p){FreeMem(p,((size_t*)p)[-1]);}
#define CopyMem(a,b,n) memcpy(b,a,n)
#define IsListEmpty(p) (!*(p))
#define ObtainSemaphore(p) assert(!(*(p))++)
#define ReleaseSemaphore(p) assert((*(p))--==1)
#define ForeachNode(list,h) for((h)=(void*)1;(h);(h)=NULL)
#define CALLHOOKPKT(h,o,d) (hooks++,assert((d)->size==base->copyBufferSize))
static LONG con_copy_clipboard_request(struct ConsoleBase*b,const UBYTE*p,ULONG n){(void)b;if(!provider)return 0;if(provider<0)return -1;assert(n<=sizeof(saved));if(n)memcpy(saved,p,n);saved_length=n;queued++;return 1;}
'''
prefix += r'''
static unsigned read_requests;static UBYTE read_target;static BOOL read_st;
static BOOL con_osc_read_request(struct ConsoleBase*b,Object*o,UBYTE target,BOOL st){(void)b;(void)o;read_requests++;read_target=target;read_st=st;return TRUE;}
static void con_osc_read_cancel(Object*o){(void)o;}
'''
unit_dispatch=(r/'stdconclass.c').read_text();a=unit_dispatch.index('    if (msg->Command == C_GET_CLIPBOARD_PREFS');unit_dispatch=unit_dispatch[a:unit_dispatch.index('    if (msg->Command == C_GET_BELL_PREFS',a)]
policy=r'''
#define C_GET_CLIPBOARD_PREFS 1
#define C_SET_CLIPBOARD_PREFS 2
struct ConsolengClipboardPrefs{ULONG cbp_Size,cbp_Version,cbp_Enabled;};
'''+(r/'clipboard_prefs.h').read_text()+r'''
static void policy(Object *o,int command,struct ConsolengClipboardPrefs*p){struct{int Command;} m={command},*msg=&m;IPTR params[]={(IPTR)p,0};
'''+unit_dispatch+r'''
}
'''
main=r'''
static unsigned cases;
static void run(const UBYTE*body,ULONG n,const UBYTE*expected,ULONG expected_length,int valid,int fragment,int route){
 struct intConUnit u={0};struct ConsoleBase b={0};u.oscState=OSC_COLLECT;b.sniphooks=route==2;provider=route==1?0:1;
 queued=hooks=0;saved_length=777;BYTE cmd=C_NIL;UBYTE count=0;IPTR p[3]={0};
 const UBYTE prefix[]="52;";assert(consume_osc(&u,(UBYTE*)prefix,3,&cmd,&count,p)==3);
 for(ULONG i=0;i<n;){ULONG chunk=fragment?1:n-i;assert(consume_osc(&u,(UBYTE*)body+i,chunk,&cmd,&count,p)==chunk);assert(cmd==C_NIL);i+=chunk;}
 UBYTE end=7;assert(consume_osc(&u,&end,1,&cmd,&count,p)==1&&cmd==C_SET_OSC_CLIPBOARD&&count==1&&p[0]==0);
 con_osc_clipboard_apply(&b,&u);con_osc_clipboard_reset(&u);
 int system=(!n||body[0]=='c'||body[0]==';')&&!route;
 if(valid){if(system){assert(queued==1&&saved_length==expected_length&&(!expected_length||!memcmp(saved,expected,expected_length)));}else{assert(!queued&&b.copyBufferSize==expected_length&&(!expected_length||!memcmp(b.copyBuffer,expected,expected_length)));}}
 else assert(!queued&&!b.copyBuffer&&!hooks&&saved_length==777);
 FreeMem(b.copyBuffer,b.copyBufferSize);assert(!live);cases++;
}
int main(void){
'''
def arr(b):return '(const UBYTE[]){'+(','.join(str(x) for x in b) or '0')+'}'
fixtures=[]
for data in [b'',b'A',b'AB',b'ABC',b'a\x00b','Ω☕😀'.encode(),bytes(range(128))]:
 for target in [b'',b'c',b'p',b's',b'cp',b'pc']:
  fixtures.append((target+b';'+base64.b64encode(data),data,True))
for raw in [b'c;?',b'x;QQ==',b'c;Q',b'c;QQ',b'c;QQ=',b'c;QR==',b'c;QUJ=',b'c;QQ===',b'c;QQ==QQ==',b'c;Q Q=',b'c;QQ-_',b'c;QQ==;AAAA',b'c',b'c'*256+b';QQ==']:
 fixtures.append((raw,b'',False))
for invalid in [b'\xc0\x80',b'\xc1\xbf',b'\xed\xa0\x80',b'\xf4\x90\x80\x80',b'\xe0\x80\x80',b'\xf0\x80\x80\x80',b'\xe2\x82',b'\x80',b'\xff']:
 fixtures.append((b'c;'+base64.b64encode(invalid),b'',False))
rng=random.Random(52)
for i in range(100):
 data=''.join(chr(rng.choice([rng.randrange(0,0xd800),rng.randrange(0xe000,0x110000)])) for _ in range(i)).encode()
 fixtures.append((b'c;'+base64.b64encode(data),data,True))
for body,expected,valid in fixtures:
 main+='for(int fragment=0;fragment<2;fragment++)for(int route=0;route<3;route++)run(%s,%d,%s,%d,%d,fragment,route);\n'%(arr(body),len(body),arr(expected),len(expected),valid)
main+=r'''
/* Disabled collection cannot allocate or publish. Re-enabling cannot revive
 * a partial transfer discarded by the policy setter. A fresh OSC may copy. */
for(int active=0;active<2;active++){
 Object u={0};struct ConsoleBase b={0};struct ConsolengClipboardPrefs permission={12,1,0};
 if(active){const char*prefix="c;QQ";for(unsigned i=0;i<strlen(prefix);i++)con_osc_clipboard_byte(&u,prefix[i]);}
 policy(&u,C_SET_CLIPBOARD_PREFS,&permission);assert(u.oscCopyDisabled&&!u.oscClipboard&&u.oscClipboardInvalid);
 int oldalloc=allocations;for(int i=0;i<10000;i++)con_osc_clipboard_byte(&u,'A');assert(allocations==oldalloc);
 permission.cbp_Enabled=1;policy(&u,C_SET_CLIPBOARD_PREFS,&permission);assert(!u.oscCopyDisabled&&u.oscClipboardInvalid);
 queued=0;con_osc_clipboard_apply(&b,&u);assert(!queued&&!b.copyBuffer);con_osc_clipboard_reset(&u);
 const char*full="c;QQ==";for(unsigned i=0;i<strlen(full);i++)con_osc_clipboard_byte(&u,full[i]);provider=1;con_osc_clipboard_apply(&b,&u);assert(queued==1&&saved_length==1&&saved[0]=='A');
 permission.cbp_Enabled=0;policy(&u,C_GET_CLIPBOARD_PREFS,&permission);assert(permission.cbp_Enabled==1);con_osc_clipboard_reset(&u);assert(!live);
}

/* Queries preserve their terminator and do not enter the copy decoder. */
for(int enabled=0;enabled<2;enabled++)for(int st=0;st<2;st++)for(int target=0;target<3;target++){
 Object q={0};struct ConsoleBase b={0};q.oscCopyDisabled=enabled?3:0;q.oscState=OSC_COLLECT;
 char raw[]="52;c;?";raw[3]="cps"[target];BYTE cmd=C_NIL;UBYTE count=0;IPTR params[3]={0};
 assert(consume_osc(&q,(UBYTE*)raw,6,&cmd,&count,params)==6);
 UBYTE term=st?27:7;assert(consume_osc(&q,&term,1,&cmd,&count,params)==(st?0:1)&&cmd==C_SET_OSC_CLIPBOARD&&count==1&&params[0]==st);
 unsigned prior=read_requests;assert(con_osc_clipboard_read(&b,&q,params[0]));assert(read_requests==prior+enabled);
 if(enabled){assert(read_target=="cps"[target]&&read_st==st);}con_osc_clipboard_reset(&q);assert(!live);
}
/* Exact decoded cap, over-cap padding, raw collector overflow and OOM discard. */
ULONG n=(CONCLIP_WRITE_MAX+2)/3*4;UBYTE*large=malloc(n+8),*expected=malloc(CONCLIP_WRITE_MAX);assert(large&&expected);memset(expected,0,CONCLIP_WRITE_MAX);memcpy(large,"c;",2);memset(large+2,'A',n);large[n]='=';large[n+1]='=';
run(large,n+2,expected,CONCLIP_WRITE_MAX,1,0,0);
large[n]='A';large[n+1]='=';run(large,n+2,expected,0,0,0,0);free(large);free(expected);
for(int fail=1;fail<=5;fail++){
 struct intConUnit u={0};allocations=0;fail_at=fail;
 for(int i=0;i<8192;i++)con_osc_clipboard_byte(&u,'A');
 assert(u.oscClipboardInvalid);con_osc_clipboard_reset(&u);assert(!live);
}fail_at=0;
struct intConUnit u={0};for(ULONG i=0;i<OSC_CLIPBOARD_MAX+1;i++)con_osc_clipboard_byte(&u,'A');assert(u.oscClipboardInvalid);con_osc_clipboard_reset(&u);assert(!live);
/* Local replacement allocation failure preserves the previous snip. */
struct ConsoleBase b={0};b.copyBuffer=AllocMem(3,0);memcpy(b.copyBuffer,"old",3);b.copyBufferSize=3;const char *local="p;bmV3";for(unsigned i=0;i<strlen(local);i++)con_osc_clipboard_byte(&u,local[i]);fail_at=allocations+1;con_osc_clipboard_apply(&b,&u);assert(b.copyBufferSize==3&&!memcmp(b.copyBuffer,"old",3));fail_at=0;con_osc_clipboard_reset(&u);FreeMem(b.copyBuffer,3);assert(!live);
printf("OSC52 PASS: %u fragmented/route/encoding cases; 1 MiB boundary, queries denied, collector/OOM cleanup and atomic local failure\n",cases);
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'t.c';e=Path(d)/'t';p.write_text(prefix+s+parser+policy+main)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(e)],check=True);subprocess.run([str(e)],check=True)
 for name,broken in [('noncanonical Base64',s.replace('(b & 15)','0').replace('(c & 3)','0')),('invalid UTF8',s.replace('!con_osc_clipboard_utf8(unit->oscClipboard, length)','(!con_osc_clipboard_utf8(unit->oscClipboard, length) && 0)'))]:
  p.write_text(prefix+broken+parser+policy+main);subprocess.run(['cc','-Wall','-Werror',str(p),'-o',str(e)],check=True)
  assert subprocess.run([str(e)],capture_output=True).returncode!=0,name
  print('Negative control rejected:',name)
