from pathlib import Path
import tempfile,subprocess
r=Path(__file__).resolve().parents[1]
def function(file,start,end):
 s=(r/file).read_text();a=s.rindex(start);return s[a:s.index(end,a)]
query=function('support.c','static BOOL getmoderequest(','/* DECSCUSR has')
state=function('consoleclass.c','static ULONG console_mode_state(','static VOID console_docommand(')
source=(r/'support.c').read_text();assert source.index('found = getmoderequest(')<source.index('found = getdecprivatecommand(')
s=(r/'charmapconclass.c').read_text();assert s.index('if (msg->Command == C_GET_MODE_STATE)')<s.index('ULONG old_scrollback_size',s.index('static VOID charmapcon_docommand'))
code=r'''
#define CF_ORIGIN_MODE 2048
#define CF_ALTERNATE_SCROLL 4096
#define CF_COLOR_SCHEME_NOTIFY 8192
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef int BOOL;typedef signed char BYTE;typedef unsigned char UBYTE;typedef uint32_t ULONG;typedef int32_t LONG;typedef intptr_t IPTR;
#define TRUE 1
#define FALSE 0
#define C_NIL 0
#define C_GET_MODE_STATE 100
#define CF_APPLICATION_CURSOR 1
#define CF_FOCUS_REPORTING 2
#define CF_BRACKETED_PASTE 4
#define CF_INSERT_MODE 8
#define M_LNM 0
#define PMB_AWM 1
typedef struct {ULONG conFlags,mouseMode,mouseSGR,mouseUTF8,modes;} Object;
#define CU(o) (o)
#define ICU(o) (o)
#define CHECK_MODE(o,m) ((o)->modes&(1U<<(m)))
struct ConsoleBase {int unused;};
'''+state+r'''
static char output[64];static int calls;
static void Console_DoCommand(Object*o,int c,int n,IPTR*p){assert(c==100&&n==3);*(ULONG*)p[2]=console_mode_state(o,p[0],p[1]);}
static void con_inject(struct ConsoleBase*b,Object*o,const UBYTE*s,LONG n){(void)b;(void)o;assert(n>0&&n<64);memcpy(output,s,n);output[n]=0;calls++;}
'''+query+r'''
int main(void){Object o={0};struct ConsoleBase base={0};ULONG modes[]={0,1,3,4,6,7,12,20,25,1000,1002,1003,1004,1005,1006,1007,1049,2004,2026,2031,65535};
 for(int priv=0;priv<2;priv++)for(unsigned m=0;m<sizeof(modes)/sizeof(*modes);m++)for(int on=0;on<2;on++){
  o=(Object){on?(15|CF_ORIGIN_MODE|CF_ALTERNATE_SCROLL|CF_COLOR_SCHEME_NOTIFY):0,on?modes[m]:0,on,on,on?3:0};ULONG value=console_mode_state(&o,priv,modes[m]);int supported=priv?(modes[m]==1||modes[m]==6||modes[m]==7||(modes[m]>=1000&&modes[m]<=1004&&modes[m]!=1001)||modes[m]==1005||modes[m]==1006||modes[m]==1007||modes[m]==2004||modes[m]==2031):(modes[m]==20||modes[m]==4);
  assert(value==(supported?(on?1:2):0));char text[32],expected[40];snprintf(text,sizeof(text),"%s%u$p",priv?"?":"",modes[m]);snprintf(expected,sizeof(expected),"\033[%s%u;%u$y",priv?"?":"",modes[m],value);
  for(unsigned n=0;n<strlen(text);n++){UBYTE*p=(UBYTE*)text,count=77;BYTE cmd=99;calls=0;assert(!getmoderequest(&cmd,&p,&count,n,&o,&base));assert(p==(UBYTE*)text&&cmd==99&&count==77&&!calls);}
  UBYTE*p=(UBYTE*)text,count=77;BYTE cmd=99;calls=0;assert(getmoderequest(&cmd,&p,&count,strlen(text),&o,&base));assert(calls==1&&!strcmp(output,expected)&&cmd==0&&!count&&!*p);
 }
 const char*bad[]={"?1;7$p","?1$p",">1$p","?1 p","?1$q","?1$","?65536$p","?999999999999999999999999$p"};
 for(unsigned k=0;k<sizeof(bad)/sizeof(*bad);k++){UBYTE*p=(UBYTE*)bad[k],count=0;BYTE cmd=0;calls=0;BOOL ok=getmoderequest(&cmd,&p,&count,strlen(bad[k]),&o,&base);if(k==1)assert(ok&&calls==1);else assert(!calls);}
 return 0;}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(code)
 subprocess.run(['cc','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
 # Removing the new report must fail the independent expected-mode table.
 missing=code.replace('    case 2031: enabled = !!(ICU(o)->conFlags & CF_COLOR_SCHEME_NOTIFY); break;', '')
 assert missing != code
 (p/'test.c').write_text(missing)
 subprocess.run(['cc','-Wall','-Wextra','-Werror',str(p/'test.c'),'-o',str(p/'negative')],check=True)
 assert subprocess.run([str(p/'negative')],capture_output=True).returncode != 0

print('PASS:84 mode/state cases, all partial prefixes, malformed/overflow rejection and exact unframed replies')
