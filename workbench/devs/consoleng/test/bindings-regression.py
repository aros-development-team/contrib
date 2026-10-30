#!/usr/bin/env python3
"""Execute the production logical binding validator, matcher and release state."""
from pathlib import Path
import subprocess, tempfile
root=Path(__file__).resolve().parents[1]
header=(root/'include/consoleng.h').read_text().split('/* Version 1 logical-key bindings.')[1].split('#define CD_GETINTERACTIONPROFILE')[0].split('#endif')[0]
header=header[header.index('#define CD_GETBINDINGPREFS'):]
source=(root/'bindings.c').read_text();source=(root/'binding_prefs.h').read_text()+source[source.index('BOOL con_bindings_valid'):]
stub=r'''
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
typedef uint32_t ULONG; typedef uint8_t UBYTE; typedef int BOOL; typedef void VOID;
#define TRUE 1
#define FALSE 0
#define CMD_NONSTD 9
#define IECLASS_RAWKEY 1
#define IECODE_UP_PREFIX 128
#define IEQUALIFIER_LSHIFT 1
#define IEQUALIFIER_RSHIFT 2
#define IEQUALIFIER_LALT 4
#define IEQUALIFIER_RALT 8
#define IEQUALIFIER_CONTROL 16
#define IEQUALIFIER_LCOMMAND 32
#define IEQUALIFIER_RCOMMAND 64
#define IEQUALIFIER_REPEAT 128
#define IEQUALIFIER_CAPSLOCK 256
#define IEQUALIFIER_NUMERICPAD 512
struct InputEvent {unsigned ie_Class,ie_Code,ie_Qualifier;};
struct ConsoleBase {int unused;};
static unsigned mapped='c', maplen=1;
static int MapRawKey(struct InputEvent *e, UBYTE *b, int n, void *k) {
 assert(!(e->ie_Qualifier & ~IEQUALIFIER_NUMERICPAD));
 b[0]=mapped; return maplen;
}
'''
test=r'''
int main(void) {
 struct ConsolengBindingPrefs p,q; con_bindings_defaults(&p); assert(con_bindings_valid(&p));
 unsigned checks=0;
 for(unsigned key=0;key<256;key++) for(unsigned qual=0;qual<1024;qual++) {
  struct InputEvent e={1,33,qual}; mapped=key;
  unsigned mods=((qual&3)?1:0)|((qual&12)?2:0)|((qual&16)?4:0)|((qual&32)?8:0)|((qual&64)?16:0);
  unsigned folded=key>='A'&&key<='Z'?key+32:key, expect=0;
  if(mods==16) {if(folded=='c')expect=1;if(folded=='v')expect=2;if(folded=='a')expect=3;}
  if(mods==5&&folded=='f')expect=4;
  assert(con_binding_action(&p,&e,0)==expect);checks++;
  e.ie_Code|=128;assert(con_binding_action(&p,&e,0)==0);checks++;
 }
 /* Every defined special key is independent of native text mapping. */
 for(unsigned raw=0;raw<128;raw++) {
  struct ConsolengBindingPrefs special={sizeof(special),2,1,{{CONSOLENG_BIND_RAW+raw,5,1}}};
  if(!con_binding_special(CONSOLENG_BIND_RAW+raw)){assert(!con_bindings_valid(&special));continue;}
  assert(con_bindings_valid(&special));special.bp_Version=1;assert(!con_bindings_valid(&special));special.bp_Version=2;
  for(unsigned len=0;len<3;len++)for(unsigned actual=0;actual<128;actual++)for(unsigned qual=0;qual<128;qual++) {
   struct InputEvent ev={1,actual,qual};maplen=len;mapped='c';
   unsigned mods=((qual&3)?1:0)|((qual&12)?2:0)|((qual&16)?4:0)|((qual&32)?8:0)|((qual&64)?16:0);
   assert(con_binding_action(&special,&ev,0)==(actual==raw&&mods==5));checks++;
  }
 }
 for(unsigned action=5;action<=10;action++) {
  struct ConsolengBindingPrefs history={sizeof(history),3,1,{{CONSOLENG_BIND_RAW+0x50,0,action}}};
  struct InputEvent ev={1,0x50,0};assert(con_bindings_valid(&history));
  assert(con_binding_action(&history,&ev,0)==action);
  history.bp_Version=2;assert(!con_bindings_valid(&history));
 }
 {struct ConsolengBindingPrefs reverse={sizeof(reverse),4,1,{{CONSOLENG_BIND_RAW+0x51,0,CONSOLENG_BIND_SEARCH_BACKWARD}}};
 struct InputEvent ev={1,0x51,0};assert(con_bindings_valid(&reverse));assert(con_binding_action(&reverse,&ev,0)==CONSOLENG_BIND_SEARCH_BACKWARD);
 for(unsigned v=1;v<4;v++){reverse.bp_Version=v;assert(!con_bindings_valid(&reverse));}}
 for(unsigned code=0;code<256;code++) {
  char hex[3];snprintf(hex,sizeof(hex),"%02x",code);
  int valid=code<=0x40||code==0x4a||(code>=0x5a&&code<=0x5e)||con_binding_special(CONSOLENG_BIND_RAW|code);
  struct ConsolengBindingPrefs physical={sizeof(physical),5,1,{{CONSOLENG_BIND_RAW|code,0,CONSOLENG_BIND_SEARCH}}};
  assert(!!con_bindings_valid(&physical)==valid);
  assert(con_binding_hex_key(hex)==(valid?(CONSOLENG_BIND_RAW|code):(ULONG)~(ULONG)0));
  if(valid){struct InputEvent ev={1,code,0};maplen=0;assert(con_binding_action(&physical,&ev,0)==CONSOLENG_BIND_SEARCH);}
  if(!con_binding_special(CONSOLENG_BIND_RAW|code)){physical.bp_Version=4;assert(!con_bindings_valid(&physical));}
 }
 const char *badhex[]={"","0","000","gg","-1"," 1","1 ","80","ff","60","63","7a"};
 for(unsigned i=0;i<sizeof(badhex)/sizeof(*badhex);i++)assert(con_binding_hex_key(badhex[i])==(ULONG)~(ULONG)0);
 maplen=1;
 struct ConsolengBindingPrefs priority={sizeof(priority),2,2,{{'c',5,2},{CONSOLENG_BIND_RAW+0x50,5,1}}};
 struct InputEvent collision={1,0x50,IEQUALIFIER_LSHIFT|IEQUALIFIER_CONTROL};mapped='c';
 assert(con_binding_action(&priority,&collision,0)==1);
 struct ConsolengBindingPrefs clear={sizeof(clear),7,1,{{CONSOLENG_BIND_RAW+0x52,0,CONSOLENG_BIND_CLEARHISTORY}}};
 assert(con_bindings_valid(&clear));clear.bp_Version=6;assert(!con_bindings_valid(&clear));
 clear.bp_Version=7;struct InputEvent clearEvent={1,0x52,0};assert(con_binding_action(&clear,&clearEvent,0)==CONSOLENG_BIND_CLEARHISTORY);
 clearEvent.ie_Qualifier=IEQUALIFIER_REPEAT;assert(!con_binding_repeat(13,13,&clearEvent));
 struct InputEvent e={1,33,0};
 q=p;q.bp_Entries[0]=(struct ConsolengKeyBinding){'x',0,1};mapped='x';
 assert(con_binding_action(&q,&e,0)==1);assert(!con_binding_action(&p,&e,0));
 maplen=0;assert(!con_binding_action(&q,&e,0));maplen=2;assert(!con_binding_action(&q,&e,0));maplen=1;
 e.ie_Class=2;assert(!con_binding_action(&q,&e,0));e.ie_Class=1;
 for(unsigned field=0;field<9;field++) {
  q=p;
  switch(field){case 0:q.bp_Size--;break;case 1:q.bp_Version=20;break;case 2:q.bp_Count=17;break;
   case 3:q.bp_Entries[1]=q.bp_Entries[0];break;case 4:q.bp_Entries[0].kb_Key=127;break;
   case 5:q.bp_Entries[0].kb_Key='C';break;case 6:q.bp_Entries[0].kb_Modifiers=32;break;
   case 7:q.bp_Entries[0].kb_Action=5;break;case 8:q.bp_Entries[15].kb_Key='x';break;}
  assert(!con_bindings_valid(&q));
 }
 memset(&q,0,sizeof(q));q.bp_Size=sizeof(q);q.bp_Version=1;assert(con_bindings_valid(&q));
 assert(!con_binding_action(&q,&e,0));
 for(unsigned key=0;key<128;key++) {
  UBYTE held[16]={0};e=(struct InputEvent){1,key,0};assert(!con_binding_release(held,&e));
  con_binding_hold(held,&e);e.ie_Qualifier=IEQUALIFIER_REPEAT;assert(con_binding_release(held,&e));
  e.ie_Code|=128;e.ie_Qualifier=0;assert(con_binding_release(held,&e));assert(!con_binding_release(held,&e));
  e.ie_Code=key;con_binding_hold(held,&e);assert(!con_binding_release(held,&e));
  e.ie_Code|=128;assert(!con_binding_release(held,&e));
 }
 for(unsigned action=14;action<=16;action++) {
  struct ConsolengBindingPrefs zoom={sizeof(zoom),8,1,{{CONSOLENG_BIND_RAW+0x52,0,action}}};
  assert(con_bindings_valid(&zoom));zoom.bp_Version=7;assert(!con_bindings_valid(&zoom));
  struct InputEvent ev={1,0x52,IEQUALIFIER_REPEAT};assert(!!con_binding_repeat(action,action,&ev)==(action!=16));
 }
 for(unsigned action=17;action<=18;action++){
  struct ConsolengBindingPrefs half={sizeof(half),9,1,{{CONSOLENG_BIND_RAW+0x54,0,action}}};
  assert(con_bindings_valid(&half));half.bp_Version=8;assert(!con_bindings_valid(&half));
 }
 for(unsigned action=59;action<=64;action++) {
  struct ConsolengBindingPrefs search={sizeof(search),17,1,{{CONSOLENG_BIND_RAW+0x52,32,action}}};
  assert(con_bindings_valid(&search));search.bp_Version=16;assert(!con_bindings_valid(&search));
 }
 for(unsigned action=53;action<=58;action++) {
  struct ConsolengBindingPrefs find={sizeof(find),16,1,{{CONSOLENG_BIND_RAW+0x52,32,action}}};
  assert(con_bindings_valid(&find));find.bp_Version=15;assert(!con_bindings_valid(&find));
 }
 for(unsigned action=44;action<=52;action++) {
  struct ConsolengBindingPrefs view={sizeof(view),15,1,{{CONSOLENG_BIND_RAW+0x52,32,action}}};
  assert(con_bindings_valid(&view));view.bp_Version=14;assert(!con_bindings_valid(&view));
 }
 for(unsigned action=26;action<=43;action++) {
  struct ConsolengBindingPrefs motion={sizeof(motion),14,1,{{CONSOLENG_BIND_RAW+0x52,32,action}}};
  assert(con_bindings_valid(&motion));motion.bp_Version=13;assert(!con_bindings_valid(&motion));
 }
 for(unsigned action=22;action<=25;action++) {
  struct ConsolengBindingPrefs selection={sizeof(selection),13,1,{{CONSOLENG_BIND_RAW+0x52,32,action}}};
  assert(con_bindings_valid(&selection));selection.bp_Version=12;assert(!con_bindings_valid(&selection));
  struct InputEvent repeat={1,0x52,IEQUALIFIER_REPEAT};assert(!con_binding_repeat(action,action,&repeat));
 }
 struct ConsolengBindingPrefs clearSelection={sizeof(clearSelection),10,1,{{CONSOLENG_BIND_RAW+0x52,0,CONSOLENG_BIND_CLEARSELECTION}}};
 assert(con_bindings_valid(&clearSelection));clearSelection.bp_Version=9;assert(!con_bindings_valid(&clearSelection));
 for(unsigned action=20;action<=21;action++) {
  struct ConsolengBindingPrefs nav={sizeof(nav),11,1,{{CONSOLENG_BIND_RAW+0x52,4,action}}};
  assert(con_bindings_valid(&nav));nav.bp_Version=10;assert(!con_bindings_valid(&nav));
 }
 unsigned rules[243],states[32];
 for(unsigned i=0;i<243;i++){unsigned n=i;rules[i]=0;for(unsigned d=0;d<5;d++){rules[i]|=(n%3)<<(5+2*d);n/=3;}}
 for(unsigned state=0;state<32;state++){states[state]=0;for(unsigned d=0;d<5;d++)if(state&(1<<d))states[state]|=1U<<(5+2*d);}
 unsigned contextChecks=0;
 for(unsigned i=0;i<243;i++){
  struct ConsolengBindingPrefs policy={sizeof(policy),19,1,{{CONSOLENG_BIND_RAW+0x50,rules[i],1}}};
  assert(con_bindings_valid(&policy));
  for(unsigned state=0;state<32;state++){
   unsigned n=i;int expect=1;
   for(unsigned d=0;d<5;d++){unsigned condition=n%3;n/=3;if(condition && ((condition==1)!=!!(state&(1<<d))))expect=0;}
   struct InputEvent ev={1,0x50,0};assert(con_binding_action_in_context(&policy,&ev,0,states[state])==(unsigned)expect);contextChecks++;
  }
  for(unsigned j=0;j<243;j++){
   policy.bp_Count=2;policy.bp_Entries[1]=(struct ConsolengKeyBinding){CONSOLENG_BIND_RAW+0x50,rules[j],2};
   int overlap=0;for(unsigned state=0;state<32;state++){
    unsigned a=i,b=j;int match=1;for(unsigned d=0;d<5;d++){unsigned x=a%3,y=b%3;a/=3;b/=3;
     if((x&&((x==1)!=!!(state&(1<<d))))||(y&&((y==1)!=!!(state&(1<<d)))))match=0;}
    overlap|=match;
   }
   assert(!!con_bindings_valid(&policy)==!overlap);contextChecks++;
  }
 }
 for(unsigned d=0;d<5;d++){
  struct ConsolengBindingPrefs bad={sizeof(bad),19,1,{{CONSOLENG_BIND_RAW+0x50,3U<<(5+2*d),1}}};assert(!con_bindings_valid(&bad));
 }
 printf("BINDING_CONTEXT_PASS %u matching/overlap cases\n",contextChecks);
 unsigned modeChecks=0;
 for(unsigned a=0;a<3;a++)for(unsigned b=0;b<3;b++)for(unsigned mods=0;mods<32;mods++) {
  unsigned masks[]={0,CONSOLENG_BIND_NAVONLY,CONSOLENG_BIND_NOTNAV};
  struct ConsolengBindingPrefs policy={sizeof(policy),12,2,{{CONSOLENG_BIND_RAW+0x50,mods|masks[a],CONSOLENG_BIND_COPY},{CONSOLENG_BIND_RAW+0x50,mods|masks[b],CONSOLENG_BIND_PASTE}}};
  assert(!!con_bindings_valid(&policy)==(a&&b&&a!=b));modeChecks++;
 }
 for(unsigned keykind=0;keykind<2;keykind++)for(unsigned scope=0;scope<3;scope++)for(unsigned nav=0;nav<2;nav++) {
  unsigned masks[]={0,CONSOLENG_BIND_NAVONLY,CONSOLENG_BIND_NOTNAV};
  struct ConsolengBindingPrefs policy={sizeof(policy),12,1,{{keykind?CONSOLENG_BIND_RAW+0x50:'x',4|masks[scope],CONSOLENG_BIND_COPY}}};
  assert(con_bindings_valid(&policy));struct InputEvent ev={1,0x50,IEQUALIFIER_CONTROL};mapped='x';maplen=1;
  assert(con_binding_action_in_mode(&policy,&ev,0,nav)==((!scope||(scope==1?nav:!nav))?CONSOLENG_BIND_COPY:0));
  if(!nav)assert(con_binding_action(&policy,&ev,0)==con_binding_action_in_mode(&policy,&ev,0,FALSE));
  policy.bp_Version=11;assert(!!con_bindings_valid(&policy)==!scope);
  policy.bp_Version=12;policy.bp_Entries[0].kb_Modifiers=96;assert(!con_bindings_valid(&policy));modeChecks++;
 }
 {struct ConsolengBindingPrefs dual={sizeof(dual),12,2,{{CONSOLENG_BIND_RAW+0x50,32,1},{CONSOLENG_BIND_RAW+0x50,64,2}}};struct InputEvent ev={1,0x50,0};
 assert(con_binding_action_in_mode(&dual,&ev,0,TRUE)==1);assert(con_binding_action(&dual,&ev,0)==2);}
 {struct ConsolengBindingPrefs precedence={sizeof(precedence),12,2,{{CONSOLENG_BIND_RAW+0x50,32,1},{'x',0,2}}};struct InputEvent ev={1,0x50,0};mapped='x';maplen=1;
 assert(con_binding_action_in_mode(&precedence,&ev,0,TRUE)==1);assert(con_binding_action(&precedence,&ev,0)==2);}
 printf("BINDING_MODES_PASS %u mode/overlap cases\n",modeChecks);
 unsigned repeatChecks=0;
 for(unsigned original=0;original<=21;original++)for(unsigned current=0;current<=21;current++)
 for(unsigned cls=1;cls<=2;cls++)for(unsigned up=0;up<2;up++)for(unsigned repeat=0;repeat<2;repeat++) {
  struct InputEvent ev={cls,33|(up?128:0),repeat?IEQUALIFIER_REPEAT:0};
  assert(!!con_binding_repeat(original,current,&ev)==(cls==1&&!up&&repeat&&original==current&&((current>=5&&current<=10)||current==14||current==15||current==17||current==18)));repeatChecks++;
 }
 printf("BINDINGS_REPEAT_PASS %u original/current/event combinations\n",repeatChecks);
 printf("BINDINGS_PASS %u mapping/release checks, validation, isolation and held-key cases\n",checks);
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(stub+header+source+test)
 subprocess.run(['cc','-std=c99','-Wall','-Werror','-Wno-unused-parameter',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
mutations=[
 ('physical binding version gate', 'p->bp_Version >= 5 && con_binding_physical', 'FALSE && con_binding_physical'),
 ('repeat policy change', 'original == current &&', 'TRUE &&'),
 ('repeat clipboard action', 'current >= CONSOLENG_BIND_LINEUP', 'current >= CONSOLENG_BIND_COPY'),
 ('special key without text', 'if (p->bp_Version >= 2)', 'if (FALSE)'),
 ('duplicate chord', 'b->kb_Key == p->bp_Entries[j].kb_Key &&', '0 &&'),
 ('unconsumed release', 'return consumed;', 'return consumed && FALSE;'),
 ('extra modifier', '(binding & 31UL)==modifiers','(binding & modifiers)==(binding & 31UL)'),
 ('required context','if((binding & yes) && !(context & yes))','if(FALSE && (binding & yes) && !(context & yes))'),
 ('excluded context','if((binding & (yes<<1)) && (context & yes))','if(FALSE && (binding & (yes<<1)) && (context & yes))'),
 ('ASCII case folding', "key += 'a'-'A';", 'key += 0;'),
]
for label,old,new in mutations:
 assert old in source
 with tempfile.TemporaryDirectory() as d:
  p=Path(d);(p/'test.c').write_text(stub+header+source.replace(old,new)+test)
  subprocess.run(['cc','-std=c99','-Wall','-Werror','-Wno-unused-parameter',str(p/'test.c'),'-o',str(p/'test')],check=True)
  assert subprocess.run([str(p/'test')],capture_output=True).returncode!=0,label
  print('Negative control PASS:',label)
