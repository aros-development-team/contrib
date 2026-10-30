#!/usr/bin/env python3
"""Actual REP history and bounded Unicode emission, including encoding changes."""
from pathlib import Path
import subprocess,tempfile
s=(Path(__file__).resolve().parents[1]/'support.c').read_text()
def fn(name):
 a=s.index(name);a=s.rfind('\n',0,a)+1;b=s.index('{',a);d=1;c=b+1
 while d:d+=(s[c]=='{')-(s[c]=='}');c+=1
 return s[a:c]
functions='\n'.join(fn(n) for n in ['ULONG con_utf8_decode(', 'ULONG con_utf8_encode(', 'static ULONG con_charset_map(', 'static VOID con_repeat_char(', 'static VOID con_remember_text(', 'static VOID con_emit_command(', 'static BOOL getcharset(', 'static BOOL csi_incomplete('])
# Human-readable terminal symbols, independent of production numeric table.
expected=', '.join(str(ord(c)) for c in ' ◆▒␉␌␍␊°±␤␋┘┐┌└┼⎺⎻─⎼⎽├┤┴┬│≤≥π≠£·')
code=r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef unsigned char UBYTE;typedef signed char BYTE;typedef unsigned short UWORD;typedef short WORD;typedef unsigned long ULONG;typedef long LONG;typedef intptr_t IPTR;typedef int BOOL;typedef void VOID;
#define TRUE 1
#define FALSE 0
#define ESC 27
#define CSI 155
#define CF_UTF8 4
#define C_ASCII 0
#define C_ASCII_STRING 1
#define C_NIL 2
#define C_REPEAT_CHAR 3
struct intConUnit {UBYTE charsets,activeCharset;ULONG conFlags,lastPrintable;BOOL renderUTF8,hasLastPrintable;};typedef struct intConUnit Object;
#define ICU(o) (o)
#define CON_IS_UTF8(o) ((o)->conFlags&CF_UTF8)
#define CON_TEXT_IS_UTF8(o) (CON_IS_UTF8(o)||(o)->renderUTF8)
static ULONG got[70000];static int ngot,calls,inputUtf8;
ULONG con_utf8_decode(const UBYTE **sp,LONG *left);
static void Console_DoCommand(Object*o,BYTE c,UBYTE n,IPTR*p){assert(c==C_ASCII_STRING&&n==2&&!!CON_IS_UTF8(o)==inputUtf8);const UBYTE*s=(const UBYTE*)p[0];LONG left=p[1];while(left){assert(ngot<70000);got[ngot++]=CON_TEXT_IS_UTF8(o)?con_utf8_decode(&s,&left):(--left,*s++);}calls++;o->conFlags|=128;}
'''+functions+'\nstatic const ULONG expected[32]={'+expected+'};\n'+r'''
int main(void){ (void)getcharset;(void)csi_incomplete;(void)expected;
 IPTR counts[]={-1,0,1,2,63,64,65,65535,INTPTR_MAX};ULONG cps[]={'q',0xe9,0x3c0,0x1f642,0x301};unsigned tests=0;
 for(int utf8=0;utf8<2;utf8++)for(int charset=0;charset<2;charset++)for(int before=0;before<2;before++)for(int c=0;c<5;c++)for(int n=0;n<9;n++){
  inputUtf8=utf8;Object o={.charsets=charset,.conFlags=utf8?4:0,.renderUTF8=before,.hasLastPrintable=1,.lastPrintable=cps[c]};ngot=calls=0;
  con_repeat_char(&o,counts[n]);int want=counts[n]<=0?1:counts[n]>65535?65535:counts[n];
  assert(ngot==want&&calls==(want+63)/64&&o.renderUTF8==before&&o.lastPrintable==cps[c]&&o.hasLastPrintable);
  ULONG cp=charset&&c==0?0x2500:cps[c];for(int i=0;i<ngot;i++)assert(got[i]==cp);tests++;
 }
 Object o={0};inputUtf8=0;ngot=calls=0;con_repeat_char(&o,65535);assert(!calls&&!ngot&&!o.hasLastPrintable);
 UBYTE text[]={ 'a',0xcf,0x80 };IPTR p[]={(IPTR)text,3};o.conFlags=4;inputUtf8=1;con_emit_command(&o,C_ASCII_STRING,2,p);assert(o.lastPrintable==0x3c0);
 p[1]=0;con_emit_command(&o,C_ASCII_STRING,2,p);assert(o.lastPrintable==0x3c0);
 o.conFlags=0;inputUtf8=0;ngot=calls=0;con_repeat_char(&o,3);assert(ngot==3&&got[0]==0x3c0&&got[2]==0x3c0);
 UBYTE q='q';p[0]=(IPTR)&q;p[1]=1;o.charsets=1;con_emit_command(&o,C_ASCII_STRING,2,p);assert(o.lastPrintable=='q');
 o.charsets=0;ngot=0;con_repeat_char(&o,2);assert(ngot==2&&got[0]=='q'&&got[1]=='q');
 printf("REP PASS: %u count/encoding/charset/wide/combining combinations, no preceding input, empty span and original-codepoint cache\n",tests);}

'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'t.c';b=Path(d)/'t';p.write_text(code)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(b)],check=True);subprocess.run([str(b)],check=True)
