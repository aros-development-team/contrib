from pathlib import Path
import tempfile,subprocess
r=Path(__file__).resolve().parents[1];s=(r/'consoletask.c').read_text();f=s[s.index('static BOOL report_alternate_scroll('):s.index('/* Classic raw-event consumers')]
code=r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
typedef int BOOL;typedef uint32_t ULONG;typedef intptr_t IPTR;typedef unsigned char UBYTE;
#define TRUE 1
#define FALSE 0
#define CF_ALTERNATE_SCROLL 4096
#define IECLASS_RAWKEY 1
#define RAWKEY_NM_WHEEL_UP 70
#define RAWKEY_NM_WHEEL_DOWN 71
#define RAWKEY_NM_WHEEL_LEFT 72
#define RAWKEY_NM_WHEEL_RIGHT 73
#define IEQUALIFIER_LSHIFT 1
#define IEQUALIFIER_RSHIFT 2
#define C_GET_MODE_STATE 7
#define CON_INPUTBUF_SIZE 32
struct intConUnit{ULONG conFlags,mouseMode,numStoredChars,wheelLines;UBYTE inputBuf[32];};
typedef struct{struct intConUnit icu;void*cu_Window;int hidden,alternate;}Object;
struct ConsoleBase{int unused;};struct InputEvent{int ie_Class,ie_Code,ie_Qualifier;};
#define ICU(o) (&(o)->icu)
#define CU(o) (o)
#define CON_IS_OBSCURED(o) ((o)->hidden)
static int calls,queries;
static void Console_DoCommand(Object*o,int cmd,int n,IPTR*p){assert(cmd==7&&n==3&&p[0]==1&&p[1]==1049);queries++;*(ULONG*)p[2]=o->alternate;}
static void answer_requests(Object*o,struct ConsoleBase*b){calls++;}
'''+f+r'''
int main(void){unsigned cases=0;
for(int speed=0;speed<4;speed++)for(int flags=0;flags<64;flags++)for(int alt=0;alt<3;alt++)for(int code=69;code<75;code++)for(int used=0;used<=32;used++){
 Object o={0};o.icu.wheelLines=(unsigned[]){0,1,3,100}[speed];o.icu.conFlags=flags&1?4096:0;o.icu.mouseMode=flags&2?1000:0;o.hidden=!!(flags&4);o.cu_Window=flags&8?0:(void*)1;o.alternate=alt;o.icu.numStoredChars=used;memset(o.icu.inputBuf,0xa5,32);
 struct InputEvent e={flags&16?2:1,code,flags&32?1:0};calls=queries=0;
 int expected=!(flags&62)&&(flags&1)&&alt==1&&(code>=70&&code<=73);
 int handled=report_alternate_scroll(&o,&e,0);assert(handled==expected);
 unsigned queued=expected?o.icu.wheelLines:0;if(queued>(32-used)/3)queued=(32-used)/3;assert(calls==!!queued&&o.icu.numStoredChars==used+3*queued);
 for(int i=0;i<32;i++)assert(o.icu.inputBuf[i]==(queued&&i>=used&&i<used+3*queued?((i-used)%3==0?27:(i-used)%3==1?'O':"ABDC"[code-70]):0xa5));
 cases++;
}
printf("Alternate scroll PASS: %u event/mode/capacity cases\n",cases);}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'t.c';b=Path(d)/'t';p.write_text(code);subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(b)],check=True);subprocess.run([str(b)],check=True)
 p.write_text(code.replace('if (alternate != 1) return FALSE;',''))
 subprocess.run(['cc','-Wall','-Werror',str(p),'-o',str(b)],check=True);assert subprocess.run([str(b)],capture_output=True).returncode
 print('Primary-screen leak negative control rejected')
