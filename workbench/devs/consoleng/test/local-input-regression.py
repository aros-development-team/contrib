#!/usr/bin/env python3
"""Actual private dispatch and task gate: consumed keys cannot reach readers."""
from pathlib import Path
import subprocess,tempfile,re
r=Path(__file__).resolve().parents[1]
s=(r/'consoletask.c').read_text();h=(r/'consoleif.h').read_text(encoding='latin1')
helper=s[s.index('static BOOL con_filter_local_input'):s.index('/* Queue one complete injection.')]
macro=h[h.index('#define Console_FilterInput'):h.index('#define Console_Copy')]
message=h[h.index('struct P_Console_FilterInput\n'):h.index('struct P_Console_GetDefaultParams')]
condition=re.search(r'if \((checkconunit\(cdihmsg->unit, cdihmsg->unitGeneration, ConsoleDevice\).*?)\)\s*\{',s,re.S).group(1)
assert 'con_filter_local_input' in condition
assert s.index('!con_filter_local_input(cdihmsg') < s.index('BOOL rawEvent = report_raw_event') < s.index('RawKeyConvert(&cdihmsg->ie')
assert s.index('FreeMem(cdihmsg, sizeof(struct cdihMessage))') > s.index('!con_filter_local_input(cdihmsg')
assert 'M_Console_GetColorPen = M_Console_Paste + 5,' in h
assert h.index('M_Console_FilterInput') > h.index('M_Console_GetColorPen =')
std=(r/'stdconclass.c').read_text()
assert re.search(r'case M_Console_FilterInput:.*?retval = FALSE;\s*break;',std,re.S)
prefix=r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
typedef int BOOL;typedef uint32_t ULONG;typedef uint64_t UQUAD;typedef uint8_t UBYTE;typedef void *Msg;
#define IECLASS_RAWKEY 1
#define M_Console_FilterInput 123
struct ConUnit {unsigned id;};
typedef struct ConUnit Object;
struct InputEvent {unsigned ie_Class,ie_Code,ie_Qualifier;};
struct input_message {struct ConUnit *unit;unsigned unitGeneration;struct InputEvent ie;};
static int valid,consume;static unsigned checks,calls,deliveries,releases;
static struct ConUnit *expected_unit;static struct InputEvent *expected_event;
static int checkconunit(struct ConUnit *u,unsigned generation,void *base){assert(u==expected_unit&&generation==17&&!base);checks++;return valid;}
'''
dispatch=r'''
static uintptr_t DoMethodA(struct ConUnit *unit,Msg message){
 struct P_Console_FilterInput *p=message;
 assert(checks==1&&unit==expected_unit&&p->MethodID==M_Console_FilterInput&&p->Event==expected_event);
 assert(deliveries==0);calls++;return consume;
}
'''
main=r'''
int main(void){
 struct ConUnit units[2]={{1},{2}};unsigned cases=0;
 for(unsigned unit=0;unit<2;unit++)for(unsigned cls=0;cls<256;cls++)for(unsigned up=0;up<2;up++)for(unsigned qual=0;qual<16;qual++)for(valid=0;valid<2;valid++)for(consume=0;consume<=2;consume++){
  struct input_message message={units+unit,17,{cls,0x23|(up?0x80:0),qual}};
  struct input_message *cdihmsg=&message;void *ConsoleDevice=NULL;
  expected_unit=cdihmsg->unit;expected_event=&cdihmsg->ie;
  checks=calls=deliveries=releases=0;
  GATE
  {deliveries++;}
  /* Task cleanup remains outside the guarded delivery block. */
  releases++;
  assert(checks==1&&releases==1);
  assert(calls==(unsigned)(valid&&cls==IECLASS_RAWKEY));
  assert(deliveries==(unsigned)(valid&&!(cls==IECLASS_RAWKEY&&consume)));
  cases++;
 }
 printf("Local input PASS: %u valid/stale, event-class, key-up/down, qualifier and per-unit delivery cases\n",cases);
}
'''.replace('GATE','if ('+condition+')')
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'t.c';b=Path(d)/'t'
 p.write_text(prefix+message+dispatch+macro+helper+main)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(b)],check=True)
 subprocess.run([str(b)],check=True)
 for name,broken in [('key-only guard',helper.replace('== IECLASS_RAWKEY &&','== IECLASS_RAWKEY ||')),('swallowed input leak',helper.replace('Console_FilterInput(unit, event);','(Console_FilterInput(unit, event), 0);'))]:
  p.write_text(prefix+message+dispatch+macro+broken+main)
  subprocess.run(['cc','-Wall','-Werror',str(p),'-o',str(b)],check=True)
  assert subprocess.run([str(b)],capture_output=True).returncode!=0
  print('Negative control PASS:',name)
