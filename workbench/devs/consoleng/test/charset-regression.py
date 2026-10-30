#!/usr/bin/env python3
"""Actual DEC designation, fragmentation and bounded Unicode output mapping."""
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
static ULONG got[4000];static int ngot,calls,inputUtf8;
ULONG con_utf8_decode(const UBYTE **sp,LONG *left);
static void Console_DoCommand(Object*o,BYTE c,UBYTE n,IPTR*p){assert(c==C_ASCII_STRING&&n==2&&!!CON_IS_UTF8(o)==inputUtf8);const UBYTE*s=(const UBYTE*)p[0];LONG left=p[1];while(left){assert(ngot<4000);got[ngot++]=CON_TEXT_IS_UTF8(o)?con_utf8_decode(&s,&left):(--left,*s++);}calls++;o->conFlags|=128;}
'''+functions+'\nstatic const ULONG expected[32]={'+expected+'};\n'+r'''
int main(void){
 for(ULONG cp=0;cp<=0x10ffff;cp++)assert(con_charset_map(cp)==(cp>=95&&cp<=126?expected[cp-95]:cp));
 for(int slot=0;slot<4;slot++)for(int original=0;original<16;original++)for(int c=0;c<256;c++){
  Object o={.charsets=original};UBYTE b[]={27,'('+slot,c},*p=b;BYTE command=99;
  assert(csi_incomplete(b,1)&&csi_incomplete(b,2)&&!csi_incomplete(b,3));
  assert(getcharset(&command,&p,3,&o)&&command==C_NIL&&p==b+(c<32||c>126?2:3));
  int want=c=='0'?(original|(1<<slot)):c=='B'?(original&~(1<<slot)):original;assert(o.charsets==want);
 }
 UBYTE bytes[1024];for(int i=0;i<1024;i++)bytes[i]=95+i%32;
 for(int utf8=0;utf8<2;utf8++)for(int slot=0;slot<4;slot++){
  inputUtf8=utf8;Object o={.charsets=1<<slot,.activeCharset=slot,.conFlags=8|(utf8?4:0)};
  IPTR p[]={(IPTR)bytes,sizeof(bytes)};ngot=calls=0;con_emit_command(&o,C_ASCII_STRING,2,p);
  assert(ngot==1024&&calls>1&&!o.renderUTF8&&o.conFlags==(8|128|(utf8?4:0)));
  for(int i=0;i<1024;i++)assert(got[i]==expected[i%32]);
  UBYTE mixed[]={0xc2,0xa3,'q',0xe2,0x94,0x82};p[0]=(IPTR)mixed;p[1]=sizeof(mixed);ngot=0;
  con_emit_command(&o,C_ASCII_STRING,2,p);if(utf8){assert(ngot==3&&got[0]==163&&got[1]==0x2500&&got[2]==0x2502);}else{assert(ngot==6&&got[0]==194&&got[1]==163&&got[2]==0x2500&&got[3]==226&&got[4]==148&&got[5]==130);}
  IPTR single='q';ngot=0;con_emit_command(&o,C_ASCII,1,&single);assert(ngot==1&&got[0]==0x2500);
 }
 puts("Charset PASS: all Unicode scalar positions, 16384 designations, fragment prefixes, chunked UTF8/Latin1 output and preserved mode flags");}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'t.c';b=Path(d)/'t';p.write_text(code)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(b)],check=True);subprocess.run([str(b)],check=True)
