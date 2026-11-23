#!/usr/bin/env python3
"""Actual mouse encoder and event routing under ASan/UBSan."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
s=(r/'mouse_report.c').read_text();s=s[s.index('static ULONG mouse_decimal'):]
c=(r/'consoletask.c').read_text();c=c[c.index('static BOOL report_mouse_event('):c.index('static ULONG pasteData(')]
prefix=r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef int BOOL;typedef uint32_t ULONG;typedef int32_t LONG;typedef unsigned char UBYTE;
#define TRUE 1
#define FALSE 0
#define IECLASS_RAWKEY 1
#define IECLASS_RAWMOUSE 2
#define RAWKEY_NM_WHEEL_UP 70
#define RAWKEY_NM_WHEEL_DOWN 71
#define RAWKEY_NM_WHEEL_LEFT 72
#define RAWKEY_NM_WHEEL_RIGHT 73
#define IECODE_NOBUTTON 255
#define IECODE_UP_PREFIX 128
#define IECODE_LBUTTON 1
#define IECODE_MBUTTON 2
#define IECODE_RBUTTON 3
#define IEQUALIFIER_LSHIFT 1
#define IEQUALIFIER_RSHIFT 2
#define IEQUALIFIER_LALT 4
#define IEQUALIFIER_RALT 8
#define IEQUALIFIER_CONTROL 16
#define IEQUALIFIER_LEFTBUTTON 32
#define IEQUALIFIER_MIDBUTTON 64
#define IEQUALIFIER_RBUTTON 128
#define WFLG_GIMMEZEROZERO 1
#define WFLG_RMBTRAP 2
#define CON_INPUTBUF_SIZE 512
struct Window {int Flags,BorderLeft,BorderTop;void *MenuStrip,*DMRequest;};
struct ConUnit {struct Window *cu_Window;LONG cu_XROrigin,cu_YROrigin,cu_XRSize,cu_YRSize,cu_XMax,cu_YMax;};
struct intConUnit {struct ConUnit unit;int mouseMode,mouseSGR,mouseUTF8,mouseLastValid,hidden;LONG mouseLastX,mouseLastY;ULONG mouseReportedButtons,numStoredChars;UBYTE inputBuf[512];};
typedef struct intConUnit Object;
struct InputEvent {int ie_Class,ie_Code,ie_Qualifier;};
struct ConsoleBase {int unused;};
#define ICU(o) (o)
#define CU(o) (&(o)->unit)
#define CON_IS_OBSCURED(o) ((o)->hidden)
#define CopyMem(a,b,n) memcpy(b,a,n)
static int answered;
static void answer_requests(Object *o,struct ConsoleBase *b){(void)o;(void)b;answered++;}
'''
main=r'''
int main(void){
 for(int sgr=0;sgr<2;sgr++)for(int utf8=0;utf8<2;utf8++)for(int button=0;button<128;button++)for(int release=0;release<2;release++)for(int pos=0;pos<20;pos++){
  LONG values[]={0,94,95,222,223,2014,2015,32767,2147483646,-1};LONG x=values[pos%10],y=values[pos<10?pos:19-pos];
  UBYTE expected[50],out[50],before[50];int n;
  if(x<0||y<0)n=0;
  else if(sgr)n=snprintf((char*)expected,sizeof(expected),"\033[<%d;%u;%u%c",button,(unsigned)x+1,(unsigned)y+1,release?'m':'M');
  else if(x<(utf8?2015:223)&&y<(utf8?2015:223)){
   n=4;expected[0]=27;expected[1]='[';expected[2]='M';expected[3]=32+(release?((button&28)|3):button);
   for(int axis=0;axis<2;axis++){int point=(axis?y:x)+33;
    if(utf8&&point>127){expected[n++]=192+point/64;expected[n++]=128+point%64;}
    else expected[n++]=point;}
  }
  else n=0;
  for(int cap=0;cap<40;cap++){
   memset(out,0xa5,sizeof(out));memcpy(before,out,sizeof(out));
   ULONG got=con_mouse_encode(sgr,utf8,button,release,x,y,out,cap);
   assert(got==(n<=cap?(ULONG)n:0));
   if(got){assert(!memcmp(out,expected,n));assert(!memcmp(out+n,before+n,sizeof(out)-n));}
   else assert(!memcmp(out,before,sizeof(out)));
  }
 }
 UBYTE buf[40];assert(!con_mouse_encode(1,0,0,0,-1,0,buf,40));assert(!con_mouse_encode(1,0,0,0,0,0,NULL,40));
 struct Window w={0};Object o={0};o.unit=(struct ConUnit){&w,10,20,8,8,79,24};o.mouseSGR=1;
 struct InputEvent e={IECLASS_RAWMOUSE,IECODE_LBUTTON,0};
 for(unsigned owner=0;owner<2;owner++)for(unsigned trapped=0;trapped<2;trapped++) {
  w.MenuStrip=owner?NULL:&w;w.DMRequest=owner?&w:NULL;w.Flags=trapped?WFLG_RMBTRAP:0;
  o.mouseMode=1003;o.mouseReportedButtons=0;o.numStoredChars=0;o.mouseLastValid=0;
  e=(struct InputEvent){IECLASS_RAWMOUSE,IECODE_RBUTTON,IEQUALIFIER_RBUTTON};
  assert(report_mouse_event(&o,&e,18,36,NULL)==(int)trapped);
  assert(!!o.numStoredChars==!!trapped);assert(o.mouseReportedButtons==(trapped?4:0));
  o.numStoredChars=0;e.ie_Code=IECODE_NOBUTTON;
  assert(report_mouse_event(&o,&e,26,36,NULL)==(int)trapped);assert(!!o.numStoredChars==!!trapped);
  o.numStoredChars=0;e.ie_Code=IECODE_RBUTTON|IECODE_UP_PREFIX;e.ie_Qualifier=0;
  assert(report_mouse_event(&o,&e,18,36,NULL)==(int)trapped);assert(!o.mouseReportedButtons);
  /* Attaching a menu after a delivered press cannot strand its release. */
  w.Flags=0;o.mouseReportedButtons=4;o.numStoredChars=0;
  assert(report_mouse_event(&o,&e,-1,36,NULL)&&o.numStoredChars&&!o.mouseReportedButtons);
 }
 w.MenuStrip=w.DMRequest=NULL;w.Flags=0;o.mouseReportedButtons=0;o.numStoredChars=0;
 e=(struct InputEvent){IECLASS_RAWMOUSE,IECODE_LBUTTON,0};

 for(int mode=0;mode<4;mode++){
  int modes[]={0,1000,1002,1003};o.mouseMode=modes[mode];o.numStoredChars=0;o.mouseLastValid=0;
  assert(report_mouse_event(&o,&e,18,36,NULL)==!!mode);
  if(mode)assert(o.numStoredChars==9&&!memcmp(o.inputBuf,"\033[<0;2;3M",9));
  o.numStoredChars=0;e.ie_Code=IECODE_LBUTTON|IECODE_UP_PREFIX;
  assert(report_mouse_event(&o,&e,18,36,NULL)==!!mode);
  if(mode)assert(o.numStoredChars==9&&!memcmp(o.inputBuf,"\033[<0;2;3m",9));
  e.ie_Code=IECODE_NOBUTTON;o.numStoredChars=0;o.mouseLastValid=0;
  assert(report_mouse_event(&o,&e,18,36,NULL)==!!mode);
  assert(o.numStoredChars==(mode==3?10:0));
  o.numStoredChars=0;e.ie_Qualifier=IEQUALIFIER_LEFTBUTTON;o.mouseLastValid=0;
  report_mouse_event(&o,&e,18,36,NULL);assert(o.numStoredChars==(mode>=2?10:0));
  o.numStoredChars=0;report_mouse_event(&o,&e,18,36,NULL);assert(!o.numStoredChars);
  e.ie_Code=IECODE_LBUTTON;e.ie_Qualifier=0;
 }
 /* An actual event past the legacy limit reaches the UTF-8 input queue. */
 o.mouseMode=1000;o.mouseSGR=0;o.mouseUTF8=1;o.unit.cu_XMax=400;o.numStoredChars=0;
 e.ie_Code=IECODE_LBUTTON;e.ie_Qualifier=0;
 assert(report_mouse_event(&o,&e,10+300*8,20+3*8,NULL));
 const UBYTE extended[]={27,'[','M',32,0xc5,0x8d,36};
 assert(o.numStoredChars==sizeof(extended)&&!memcmp(o.inputBuf,extended,sizeof(extended)));
 o.mouseSGR=1;o.mouseUTF8=0;o.unit.cu_XMax=79;
 o.mouseMode=1000;o.numStoredChars=0;e.ie_Qualifier=IEQUALIFIER_LSHIFT;
 assert(!report_mouse_event(&o,&e,18,36,NULL)&&!o.numStoredChars);
 e.ie_Qualifier=0;assert(!report_mouse_event(&o,&e,9,36,NULL));assert(!report_mouse_event(&o,&e,9999,36,NULL));
 o.hidden=1;assert(!report_mouse_event(&o,&e,18,36,NULL));o.hidden=0;
 o.numStoredChars=510;memset(o.inputBuf,0x55,512);int count=answered;
 assert(report_mouse_event(&o,&e,18,36,NULL)&&o.numStoredChars==510&&answered==count&&o.inputBuf[510]==0x55);
 o.numStoredChars=0;e.ie_Class=IECLASS_RAWKEY;e.ie_Code=RAWKEY_NM_WHEEL_UP;e.ie_Qualifier=IEQUALIFIER_CONTROL;
 assert(report_mouse_event(&o,&e,18,36,NULL));assert(o.numStoredChars==10&&!memcmp(o.inputBuf,"\033[<80;2;3M",10));
 for(int direction=0;direction<4;direction++)for(int qual=0;qual<32;qual++){
  o.numStoredChars=0;e.ie_Code=RAWKEY_NM_WHEEL_UP+direction;e.ie_Qualifier=qual;
  int expected=!(qual&(IEQUALIFIER_LSHIFT|IEQUALIFIER_RSHIFT));
  assert(report_mouse_event(&o,&e,18,36,NULL)==expected);
  if(expected){char reply[40];int button=64+direction;if(qual&(IEQUALIFIER_LALT|IEQUALIFIER_RALT))button|=8;if(qual&IEQUALIFIER_CONTROL)button|=16;int n=snprintf(reply,sizeof(reply),"\033[<%d;2;3M",button);assert(o.numStoredChars==(ULONG)n&&!memcmp(o.inputBuf,reply,n));}else assert(!o.numStoredChars);
 }
 /* Every protocol/button/edge: capture only successfully queued presses. */
 for(int protocol=0;protocol<3;protocol++)for(int key=1;key<=3;key++)for(int edge=0;edge<4;edge++){
  o.mouseMode=1000;o.mouseSGR=protocol==0;o.mouseUTF8=protocol==1;
  o.mouseReportedButtons=0;o.numStoredChars=0;
  e=(struct InputEvent){IECLASS_RAWMOUSE,key,0};
  assert(report_mouse_event(&o,&e,18,36,NULL));
  assert(o.mouseReportedButtons==(1u<<(key-1)));
  o.numStoredChars=0;e.ie_Code|=IECODE_UP_PREFIX;
  LONG px[]={-5,9999,18,18},py[]={36,36,-5,9999};
  LONG cx[]={0,79,1,1},cy[]={2,2,0,24};UBYTE expected[40];
  ULONG n=con_mouse_encode(o.mouseSGR,o.mouseUTF8,key-1,1,cx[edge],cy[edge],expected,40);
  assert(report_mouse_event(&o,&e,px[edge],py[edge],NULL));
  assert(o.numStoredChars==n&&!memcmp(expected,o.inputBuf,n)&&!o.mouseReportedButtons);
  o.numStoredChars=0;assert(!report_mouse_event(&o,&e,px[edge],py[edge],NULL));
  e.ie_Code=key;o.numStoredChars=CON_INPUTBUF_SIZE;
  report_mouse_event(&o,&e,18,36,NULL);assert(!o.mouseReportedButtons);
  o.numStoredChars=0;e.ie_Code|=IECODE_UP_PREFIX;
  assert(!report_mouse_event(&o,&e,px[edge],py[edge],NULL));
 }
 /* Suppressed releases cannot leave stale capture behind. */
 for(int suppression=0;suppression<3;suppression++){
  o.mouseMode=1000;o.hidden=0;o.numStoredChars=0;
  e=(struct InputEvent){IECLASS_RAWMOUSE,IECODE_LBUTTON,0};
  assert(report_mouse_event(&o,&e,18,36,NULL));
  e.ie_Code|=IECODE_UP_PREFIX;
  if(suppression==0)e.ie_Qualifier=IEQUALIFIER_LSHIFT;
  if(suppression==1)o.hidden=1;
  if(suppression==2)o.mouseMode=0;
  assert(!report_mouse_event(&o,&e,-5,36,NULL)&&!o.mouseReportedButtons);
 }
 return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'t.c';b=Path(d)/'t';p.write_text(prefix+s+c+main)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(b)],check=True)
 subprocess.run([str(b)],check=True)
print('Mouse PASS: atomic legacy/UTF8/SGR encoder, bounds, modifiers, click/release/drag/motion, duplicate suppression, Shift override, wheel and queue pressure')
