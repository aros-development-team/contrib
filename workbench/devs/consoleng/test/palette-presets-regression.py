#!/usr/bin/env python3
"""Actual presets and profile codec: bounded choices, editable concrete colors."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
h=(r/'include/consoleng.h').read_text();h=h[h.index('#define CD_GETCURSORPREFS'):h.index('#endif /* DEVICES_CONSOLENG_H */')]
code='''#include <assert.h>
#include <stdint.h>
typedef uint32_t ULONG;typedef int BOOL;typedef void VOID;
#define TRUE 1
#define FALSE 0
#define CMD_NONSTD 9
#define FindTask(x) ((void*)0x1234)
'''+h+(r/'prefs/settings.h').read_text().replace('#include "../binding_prefs.h"',(r/'binding_prefs.h').read_text()).replace('#include "semantic_text.h"',(r/'prefs/semantic_text.h').read_text().replace('#include "../semantic_prefs.h"',(r/'semantic_prefs.h').read_text()))+(r/'prefs/presets.h').read_text()+r'''
int main(void){
 struct ConsoleSettings initial={{20,1,5,350,8000},{12,1,4096},{140,1,16,"DejaVuSansMono.font"},{84,1,3,{0xe5e5e5,0x203040}},1};
 con_binding_prefs_defaults(&initial.bindings);
 const ULONG fg[]={0xd8dee9,0x20242c,0xd2e7e9,0xffcf75},bg[]={0x20242c,0xf7f5ef,0x143542,0x241c12};
 assert(!colorPresetNames[6]);
 for(ULONG index=0;index<100;index++){
  struct ConsoleSettings p=initial,decoded=initial;char text[1024];
  assert(colorPresetApply(&p.colors,index)==(index>=1&&index<=5));
  if(index<1||index>5){assert(!memcmp(&p,&initial,sizeof(p)));continue;}
  assert(p.colors.cp_Mask==(index==1?0:CONSOLENG_COLOR_MASK));
  if(index>1)assert(p.colors.cp_RGB[0]==fg[index-2]&&p.colors.cp_RGB[1]==bg[index-2]);
  for(unsigned i=0;i<18;i++)assert(p.colors.cp_RGB[i]<=0xffffff);
  assert(settingsValid(&p)&&settingsFormat(text,sizeof(text),&p)&&settingsParse(text,&decoded)&&!memcmp(&p,&decoded,sizeof(p)));
  assert(settingsSave("preset.profile",&p)&&settingsLoad("preset.profile",&decoded)&&!memcmp(&p,&decoded,sizeof(p)));
  p.colors.cp_RGB[0]=0x123456;p.colors.cp_Mask|=1;
  assert(settingsFormat(text,sizeof(text),&p)&&settingsParse(text,&decoded)&&decoded.colors.cp_RGB[0]==0x123456);
  p.colors=initial.colors;assert(!memcmp(&p,&initial,sizeof(p)));
 }
 assert(!colorPresetApply(NULL,2));puts("Palette presets PASS: five choices, bounds, exact colors, unchanged non-color settings, editable profile and file round trips");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'t.c';b=Path(d)/'t';p.write_text(code)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(b)],check=True)
 subprocess.run([str(b)],cwd=d,check=True)
 broken=code.replace('p.cp_Mask=CONSOLENG_COLOR_MASK','p.cp_Mask=3')
 p.write_text(broken);subprocess.run(['cc','-Wall','-Werror',str(p),'-o',str(b)],check=True)
 assert subprocess.run([str(b)],cwd=d,capture_output=True).returncode!=0
 print('Negative control PASS: preset must include all sixteen ANSI slots')
