#!/usr/bin/env python3
"""Execute the real preview apply function with each setter failing once."""
from pathlib import Path
import tempfile,subprocess
r=Path(__file__).resolve().parents[1]
h=(r/'include/consoleng.h').read_text().split('#define CD_GETCURSORPREFS')[1].split('#endif')[0]
h='#define CD_GETCURSORPREFS'+h
s=(r/'prefs/main.c').read_text();apply=s[s.index('static int apply('):s.index('static void populate(')]
settings=(r/'prefs/settings.h').read_text().replace('#include "../binding_prefs.h"',(r/'binding_prefs.h').read_text()).replace('#include "semantic_text.h"',(r/'prefs/semantic_text.h').read_text().replace('#include "../semantic_prefs.h"',(r/'semantic_prefs.h').read_text()))
prefix='''#include <assert.h>
#include <stdint.h>
typedef uint32_t ULONG;typedef uint16_t UWORD;typedef int BOOL;typedef void VOID;
#define TRUE 1
#define FALSE 0
#define CMD_NONSTD 9
#define FindTask(x) ((void*)0x1234)
'''
mock=r'''
struct IOStdReq {int unused;};
static struct ConsoleSettings current;
static unsigned fail,failed;
static int exchange(struct IOStdReq*io,UWORD cmd,void*p,ULONG n){
 (void)io;
 if(cmd==fail&&!failed++){return 0;}
 #define FIELD(g,s,field) if(cmd==g||cmd==s){assert(n==sizeof(current.field));if(cmd==s)memcpy(&current.field,p,n);else memcpy(p,&current.field,n);return 1;}
 FIELD(CD_GETCURSORPREFS,CD_SETCURSORPREFS,cursor)
 FIELD(CD_GETHISTORYPREFS,CD_SETHISTORYPREFS,history)
 FIELD(CD_GETFONTPREFS,CD_SETFONTPREFS,font)
 FIELD(CD_GETCOLORPREFS,CD_SETCOLORPREFS,colors)
 FIELD(CD_GETSEMANTICPREFS,CD_SETSEMANTICPREFS,semantic)
 FIELD(CD_GETBINDINGPREFS,CD_SETBINDINGPREFS,bindings)
 #define TOGGLE(g,s,field) if(cmd==g||cmd==s){ULONG*words=p;assert(n==12);if(cmd==s)current.field=words[2];else {words[0]=12;words[1]=1;words[2]=current.field;}return 1;}
 TOGGLE(CD_GETEMOJIPREFS,CD_SETEMOJIPREFS,emoji)
 TOGGLE(CD_GETSCROLLPREFS,CD_SETSCROLLPREFS,wheelLines)
 TOGGLE(CD_GETBELLPREFS,CD_SETBELLPREFS,bell)
 TOGGLE(CD_GETCLIPBOARDPREFS,CD_SETCLIPBOARDPREFS,oscCopy)
 if(cmd==CD_GETCURSORCOLORPREFS||cmd==CD_SETCURSORCOLORPREFS){struct ConsolengCursorColorPrefs*c=p;assert(n==sizeof(*c));if(cmd==CD_SETCURSORCOLORPREFS){current.cursorCustom=c->ccp_Enabled;current.cursorRGB=c->ccp_RGB;}else *c=(struct ConsolengCursorColorPrefs){sizeof(*c),1,current.cursorCustom,current.cursorRGB};return 1;}
 assert(!"unexpected command");return 0;
}
'''
main=r'''
int main(void){
 struct ConsoleSettings old={{20,1,2,500,0},{12,1,1000},{140,1,0,{0}},{84,1,0,{0}},1,1,1,0,0};
 old.semantic=con_semantic_defaults();
 con_binding_prefs_defaults(&old.bindings);struct ConsoleSettings next=old;
 next.cursor.cp_Style=6;next.history.hp_Lines=2000;next.font.fp_Pixels=16;strcpy(next.font.fp_Name,"fixed.font");
 next.colors.cp_Mask=1;next.colors.cp_RGB[0]=0xff0088;next.emoji=0;next.bell=0;next.oscCopy=0;next.cursorCustom=1;next.cursorRGB=0x123456;
 next.wheelLines=7;next.semantic.sp_Chars[0]=';';
 next.bindings.bp_Entries[0]=(struct ConsolengKeyBinding){'x',5,1};
 unsigned commands[]={CD_SETFONTPREFS,CD_SETCOLORPREFS,CD_SETCURSORPREFS,CD_SETHISTORYPREFS,CD_SETEMOJIPREFS,CD_SETBELLPREFS,CD_SETCLIPBOARDPREFS,CD_SETCURSORCOLORPREFS,CD_SETBINDINGPREFS,CD_SETSCROLLPREFS,CD_SETSEMANTICPREFS};
 for(unsigned i=0;i<sizeof(commands)/sizeof(*commands);i++){
  current=old;fail=commands[i];failed=0;assert(!apply(NULL,0,&next));assert(!memcmp(&current,&old,sizeof(old)));
 }
 current=old;fail=failed=0;assert(apply(NULL,0,&next));assert(!memcmp(&current,&next,sizeof(next)));
 puts("Preferences rollback PASS: all eleven setter failure positions restore appearance and bindings; successful apply complete");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(prefix+h+settings+mock+apply+main)
 subprocess.run(['cc','-Wall','-Werror','-Wno-unused-function','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
 broken=apply.replace('exchange(io,CD_SETBINDINGPREFS,(void*)&p->bindings,sizeof(p->bindings))','1')
 (p/'test.c').write_text(prefix+h+settings+mock+broken+main)
 subprocess.run(['cc','-Wall','-Werror','-Wno-unused-function',str(p/'test.c'),'-o',str(p/'test')],check=True)
 assert subprocess.run([str(p/'test')],capture_output=True).returncode!=0
 print('Negative control PASS: preview must actually apply shortcut bindings')
