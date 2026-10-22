#!/usr/bin/env python3
"""Actual OSC state machine: bounded font sizes, fragmentation and overflow discard."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'support.c').read_text();a=s.index('static LONG consume_osc(');b=s.index('/* DEC Special Graphics',a);code=s[a:b]
prefix=r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
typedef uint8_t UBYTE;typedef int8_t BYTE;typedef int16_t WORD;typedef uint32_t ULONG;typedef int32_t LONG;typedef intptr_t IPTR;typedef int BOOL;
#define FALSE 0
#define TRUE 1
#define ESC 27
#define CON_OSCBUF_SIZE 2048
#define OSC_IDLE 0
#define OSC_COLLECT 1
#define OSC_OVERFLOW 2
#define OSC_GOT_ESC 3
#define OSC_OVERFLOW_ESC 4
#define C_SET_CURSOR_SHAPE 5
#define C_SET_OSC_CLIPBOARD 6
#define C_SET_HYPERLINK 4
#define C_NIL 0
#define C_SET_TITLE 1
#define C_SET_FONT 2
#define C_SET_COLOR 3
struct intConUnit {UBYTE oscBuf[CON_OSCBUF_SIZE];uint16_t oscLen;UBYTE oscState;};
static void con_osc_clipboard_byte(struct intConUnit *u,UBYTE b){(void)u;(void)b;}
typedef struct intConUnit Object;
#define ICU(o) (o)
#define CON_IS_UTF8(o) 1
'''
main=r'''
/* OSC ends at ESC; leave the entire following escape sequence untouched. */
static void fragmented(Object *o,UBYTE *seq,int len,int cut,int st,BYTE *cmd,UBYTE *count,IPTR *p){
 int end=len-(st?2:0);
 LONG used=consume_osc(o,seq,cut,cmd,count,p);
 assert(used==(cut<end?cut:end));
 if(o->oscState!=OSC_IDLE){
  assert(*cmd==C_NIL);
  used+=consume_osc(o,seq+cut,len-cut,cmd,count,p);
 }
 assert(used==end&&o->oscState==OSC_IDLE);
 if(st)assert(seq[end]==27&&seq[end+1]=='\\');
}

static void check(const char *body,int valid,IPTR size){
 char seq[600];size_t n=strlen(body);assert(n<500);
 for(int st=0;st<2;st++){
  memcpy(seq,body,n);seq[n]=st?27:7;seq[n+1]=st?'\\':0;int len=n+(st?2:1);
  for(int cut=0;cut<=len;cut++){
   Object o={0};o.oscState=OSC_COLLECT;BYTE cmd=-1;UBYTE count=0;IPTR p[3]={0};
   fragmented(&o,(UBYTE*)seq,len,cut,st,&cmd,&count,p);
   assert(cmd==(valid?C_SET_FONT:C_NIL));assert(o.oscState==OSC_IDLE&&o.oscLen==0);
   if(valid){assert(count==2&&p[1]==size&&!strcmp((char*)p[0],"DejaVuSansMono"));}
  }
 }
}
/* Ignored C0 bytes must not enter OSC strings, even across writes. */
static void controls(void){
 const char *body="50;DejaVuSansMono,24";
 for(unsigned ctrl=0;ctrl<32;ctrl++){
  if(ctrl==7||ctrl==24||ctrl==26||ctrl==27)continue;
  for(size_t pos=0;pos<=strlen(body);pos++){
   UBYTE seq[100];size_t n=strlen(body);
   memcpy(seq,body,pos);seq[pos]=ctrl;memcpy(seq+pos+1,body+pos,n-pos);seq[n+1]=7;
   for(size_t cut=0;cut<=n+2;cut++){
    Object o={0};o.oscState=OSC_COLLECT;BYTE cmd=-1;UBYTE count=0;IPTR p[3]={0};
    assert(consume_osc(&o,seq,cut,&cmd,&count,p)==(LONG)cut);
    if(cut<n+2){assert(cmd==C_NIL);assert(consume_osc(&o,seq+cut,n+2-cut,&cmd,&count,p)==(LONG)(n+2-cut));}
    assert(cmd==C_SET_FONT&&count==2&&p[1]==24);
    assert(!strcmp((char*)p[0],"DejaVuSansMono"));
    assert(o.oscState==OSC_IDLE&&o.oscLen==0);
   }
  }
 }
 /* Ignored bytes do not exhaust the bounded payload or revive an overflow. */
 for(int overflow=0;overflow<2;overflow++){
  Object o={0};o.oscState=OSC_COLLECT;BYTE cmd;UBYTE count=0;IPTR p[3]={0};
  UBYTE ignored[800]={0};
  if(overflow){memset(ignored,'x',sizeof(ignored));}
  assert(consume_osc(&o,ignored,sizeof(ignored),&cmd,&count,p)==sizeof(ignored));
  memset(ignored,0,sizeof(ignored));
  consume_osc(&o,ignored,sizeof(ignored),&cmd,&count,p);
  const char *tail="50;DejaVuSansMono,24\7";
  consume_osc(&o,(UBYTE*)tail,strlen(tail),&cmd,&count,p);
  assert(cmd==(overflow?C_NIL:C_SET_FONT));
 }
}
static void links(void){
 const char *bodies[]={"8;;https://example.org/a;b", "8;id=one:foo=bar;mailto:a@example.org", "8;;", "8;id=x;"};
 for(unsigned k=0;k<4;k++)for(int st=0;st<2;st++){
  UBYTE seq[100];size_t n=strlen(bodies[k]);memcpy(seq,bodies[k],n);seq[n]=st?27:7;seq[n+1]='\\';n+=st?2:1;
  for(size_t cut=0;cut<=n;cut++){
   Object o={0};o.oscState=OSC_COLLECT;BYTE cmd=-1;UBYTE count=0;IPTR p[3]={0};
   fragmented(&o,seq,n,cut,st,&cmd,&count,p);
   assert(cmd==C_SET_HYPERLINK&&count==2&&p[1]==(IPTR)strlen(bodies[k])-2);
   assert(!strcmp((char*)p[0],bodies[k]+2));
  }
 }
 for(int overflow=0;overflow<2;overflow++){
  UBYTE seq[2100];memset(seq,'x',sizeof(seq));memcpy(seq,"8;;",3);
  size_t n=overflow?2090:1300;seq[n++]=7;
  Object o={0};o.oscState=OSC_COLLECT;BYTE cmd;UBYTE count;IPTR p[3];
  for(size_t i=0;i<n;i++){consume_osc(&o,seq+i,1,&cmd,&count,p);if(i+1<n)assert(cmd==C_NIL);}
  assert(cmd==C_SET_HYPERLINK&&count==2);
  if(overflow)assert(!p[0]&&!p[1]);else assert(p[0]&&p[1]==1298);
 }
}
static void control_ends(void){
 for(int control=0x18;control<=0x1A;control+=2)for(int overflow=0;overflow<2;overflow++){
  UBYTE seq[800];size_t n=overflow?600:21;memset(seq,'a',n);
  const char *body="50;DejaVuSansMono,24";
  if(!overflow){n=strlen(body);memcpy(seq,body,n);}else memcpy(seq,"50;",3);
  seq[n++]=control;seq[n]='X';
  for(size_t cut=0;cut<n;cut++){
   Object o={0};o.oscState=OSC_COLLECT;BYTE cmd=-1;UBYTE count=0;IPTR p[3]={0};
   assert(consume_osc(&o,seq,cut,&cmd,&count,p)==(LONG)cut&&cmd==C_NIL);
   assert(consume_osc(&o,seq+cut,n+1-cut,&cmd,&count,p)==(LONG)(n-cut));
   assert(o.oscState==OSC_IDLE&&!o.oscLen);
   assert(cmd==(overflow?C_NIL:C_SET_FONT));
   if(!overflow)assert(count==2&&p[1]==24&&!strcmp((char*)p[0],"DejaVuSansMono"));
  }
 }
 for(int control=0x18;control<=0x1A;control+=2){
  Object o={0};o.oscState=OSC_COLLECT;BYTE cmd;UBYTE count;IPTR p[3];
  UBYTE seq[]={'4',';','1',';','?',control,'X'};
  assert(consume_osc(&o,seq,sizeof(seq),&cmd,&count,p)==6);
  assert(cmd==C_SET_COLOR&&count==3&&p[2]==TRUE);
  const char *link="8;;https://example.org/";o.oscState=OSC_COLLECT;
  consume_osc(&o,(UBYTE*)link,strlen(link),&cmd,&count,p);
  UBYTE stop=control;consume_osc(&o,&stop,1,&cmd,&count,p);
  assert(cmd==C_SET_HYPERLINK&&!strcmp((char*)p[0],";https://example.org/"));
 }
}
static void cursor_shapes(void){
 for(int shape=0;shape<10;shape++)for(int st=0;st<2;st++)for(int extra=0;extra<2;extra++){
  UBYTE seq[100];int n=snprintf((char*)seq,sizeof(seq),"50;CursorShape=%d%s",shape,extra?";ignored":"");
  seq[n++]=st?27:7;if(st)seq[n++]='\\';
  for(int cut=0;cut<=n;cut++){
   Object o={0};o.oscState=OSC_COLLECT;BYTE cmd=-1;UBYTE count=0;IPTR p[3]={0};
   fragmented(&o,seq,n,cut,st,&cmd,&count,p);
   assert(cmd==(shape<3?C_SET_CURSOR_SHAPE:C_NIL));
   if(shape<3)assert(count==1&&p[0]==shape);
  }
 }
}

static void escape_reentry(void){
 for(int overflow=0;overflow<2;overflow++)for(int next=0;next<256;next++){
  UBYTE seq[700];int n=overflow?600:7;memcpy(seq,"2;Title",7);
  if(overflow){memset(seq+7,'a',n-7);}seq[n]=27;seq[n+1]=next;seq[n+2]='X';
  for(int cut=0;cut<=n+1;cut++){
   Object o={0};o.oscState=OSC_COLLECT;BYTE cmd=-1;UBYTE count=0;IPTR p[3]={0};
   LONG used=consume_osc(&o,seq,cut,&cmd,&count,p);
   if(cut<=n){assert(used==cut&&cmd==C_NIL);used+=consume_osc(&o,seq+cut,n+3-cut,&cmd,&count,p);}
   assert(used==n&&o.oscState==OSC_IDLE&&seq[used]==27&&seq[used+1]==next);
   assert(cmd==(overflow?C_NIL:C_SET_TITLE));
   if(!overflow)assert(count==1&&!strcmp((char*)p[0],"Title"));
  }
 }
}
int main(void){
 escape_reentry();
 cursor_shapes();
 control_ends();
 links();
 controls();
 check("50;DejaVuSansMono",1,0);check("50;DejaVuSansMono,0",1,0);
 check("50;DejaVuSansMono,1",1,1);check("50;DejaVuSansMono,24",1,24);check("50;DejaVuSansMono,65535",1,65535);
 const char *bad[]={"50;,24","50;DejaVuSansMono,","50;DejaVuSansMono,-1","50;DejaVuSansMono,+1","50;DejaVuSansMono,24x","50;DejaVuSansMono,65536","50;DejaVuSansMono,999999999999999999999999999999999999999999999999999999999"};
 for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);i++)check(bad[i],0,0);
 char huge[400];memset(huge,'a',sizeof(huge));memcpy(huge,"50;",3);huge[399]=0;check(huge,0,0);
 /* Oversized title and color OSC must also remain discarded with either terminator. */
 memcpy(huge,"0; ",3);check(huge,0,0);memcpy(huge,"4;1",3);check(huge,0,0);
 return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';exe=Path(d)/'test';p.write_text(prefix+code+main)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
 broken=code.replace('b == 0x07 || b == 0x18 || b == 0x1A','b == 0x07')
 p.write_text(prefix+broken+main)
 subprocess.run(['cc','-Wall','-Werror',str(p),'-o',str(exe)],check=True)
 result=subprocess.run([str(exe)],capture_output=True)
 assert result.returncode != 0, 'missing CAN/SUB termination was not rejected'
 broken=code.replace('            used--;','            /* deliberately swallow ESC */')
 p.write_text(prefix+broken+main)
 subprocess.run(['cc','-Wall','-Werror',str(p),'-o',str(exe)],check=True)
 assert subprocess.run([str(exe)],capture_output=True).returncode!=0, 'swallowed ESC was not rejected'
print('OSC PASS: 156416 ESC handoff cases, font bounds, OSC8 labels, ignored C0, CAN/SUB termination and following-byte preservation; negative control rejects old behavior')
