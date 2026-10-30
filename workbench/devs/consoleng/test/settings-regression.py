#!/usr/bin/env python3
"""Compile actual expanded preferences codec; protect old files and partial edits."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];h=(r/'include/consoleng.h').read_text()
h=h[h.index('#define CD_GETCURSORPREFS'):h.index('#endif /* DEVICES_CONSOLENG_H */')]
prefix='''#include <assert.h>
#include <stdint.h>
#include <sys/stat.h>
typedef uint32_t ULONG;typedef int BOOL;typedef void VOID;
#define TRUE 1
#define FALSE 0
#define CMD_NONSTD 9
#define FindTask(x) ((void*)0x1234)
'''
main=r'''
int main(void){
 struct ConsoleSettings p={{20,1,5,350,8000},{12,1,4096},{140,1,16,"DejaVuSansMono.font"},{84,1,3,{0xe5e5e5,0x203040}}},out=p;
 con_binding_prefs_defaults(&p.bindings);out=p;
 {
  struct ConsolengSemanticPrefs sem=con_semantic_defaults(),decoded=sem;
  char text[SEMANTIC_TEXT_CAP];
  assert(semanticTextFormat(text,sizeof(text),&sem)&&semanticTextParse(text,&decoded)&&!memcmp(&sem,&decoded,sizeof(sem)));
  const char *bad[]={"aa","\\", "\\q", "\\u12", "\\u0000", "\\ud800", "\\U00110000", "\\Uffffffff", "\xc0\xaf", "\xed\xa0\x80", "\xf4\x90\x80\x80", "\xe2\x82", "a\\u0061"};
  for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);i++){decoded=sem;assert(!semanticTextParse(bad[i],&decoded));assert(!memcmp(&sem,&decoded,sizeof(sem)));}
  assert(semanticTextParse("\\t\\n\\r\\\\\\u2502\\U0001f600",&decoded)&&decoded.sp_Count==6&&decoded.sp_Chars[5]==0x1f600);
  assert(semanticTextParse("\xe2\x94\x82\xf0\x9f\x98\x80",&decoded)&&decoded.sp_Count==2);
  sem=(struct ConsolengSemanticPrefs){sizeof(sem),1,64,{0}};
  for(unsigned i=0;i<64;i++)sem.sp_Chars[i]=0x10000+i;
  assert(semanticTextFormat(text,sizeof(text),&sem)&&strlen(text)==640);
  assert(semanticTextParse(text,&decoded)&&!memcmp(&sem,&decoded,sizeof(sem)));
  char tooMany[642];memcpy(tooMany,text,640);strcpy(tooMany+640,"a");assert(!semanticTextParse(tooMany,&decoded));
  assert(!semanticTextFormat(text,640,&sem));assert(!semanticTextFormat(text,0,&sem));
  struct ConsoleSettings newer=p,roundtrip=p;char encoded[4096];newer.semantic=sem;
  assert(settingsFormat(encoded,sizeof(encoded),&newer)&&!strncmp(encoded,"ConsoleNG 29\nsemantic=",21));
  assert(settingsParse(encoded,&roundtrip)&&!memcmp(&newer,&roundtrip,sizeof(newer)));
  assert(settingsFormat(encoded,sizeof(encoded),&p));roundtrip=newer;
  assert(settingsParse(encoded,&roundtrip)&&!memcmp(&roundtrip.semantic,&sem,sizeof(sem)));
  assert(semanticTextParse("",&newer.semantic)&&newer.semantic.sp_Count==0);
  assert(settingsFormat(encoded,sizeof(encoded),&newer)&&settingsParse(encoded,&roundtrip)&&!memcmp(&newer,&roundtrip,sizeof(newer)));
  roundtrip=newer;assert(!settingsParse("ConsoleNG 29\nsemantic=aa\nConsoleNG 19\n",&roundtrip)&&!memcmp(&roundtrip,&newer,sizeof(newer)));
 }

 for(unsigned action=CONSOLENG_BIND_LINEUP;action<=CONSOLENG_BIND_BOTTOM;action++) {
  struct ConsoleSettings newer=p,roundtrip=p;char encoded[2048];newer.bindings.bp_Version=3;
  newer.bindings.bp_Entries[0].kb_Action=action;
  assert(settingsFormatLegacy(encoded,sizeof(encoded),&newer));assert(!strncmp(encoded,"ConsoleNG 10\n",13));
  assert(settingsParse(encoded,&roundtrip)&&!memcmp(&newer,&roundtrip,sizeof(newer)));
 }
 {struct ConsoleSettings reverse=p,roundtrip=p;char encoded[2048];
 reverse.bindings.bp_Version=4;reverse.bindings.bp_Entries[3].kb_Action=CONSOLENG_BIND_SEARCH_BACKWARD;
 assert(settingsFormatLegacy(encoded,sizeof(encoded),&reverse)&&!strncmp(encoded,"ConsoleNG 11\n",13));
 assert(settingsParse(encoded,&roundtrip)&&!memcmp(&reverse,&roundtrip,sizeof(reverse)));
 reverse.bindings.bp_Version=3;assert(!con_binding_prefs_valid(&reverse.bindings));}
 {struct ConsoleSettings physical=p,roundtrip=p;char encoded[2048];
 physical.bindings.bp_Version=5;physical.bindings.bp_Entries[3].kb_Key=CONSOLENG_BIND_RAW|0x23;
 assert(settingsFormatLegacy(encoded,sizeof(encoded),&physical)&&!strncmp(encoded,"ConsoleNG 12\n",13));
 assert(settingsParse(encoded,&roundtrip)&&!memcmp(&physical,&roundtrip,sizeof(physical)));
 physical.bindings.bp_Version=4;assert(!con_binding_prefs_valid(&physical.bindings));}
 {struct ConsoleSettings nav=p,roundtrip=p;char encoded[2048];nav.bindings.bp_Version=6;nav.bindings.bp_Entries[3].kb_Action=CONSOLENG_BIND_NAVIGATE;
 assert(settingsFormatLegacy(encoded,sizeof(encoded),&nav)&&!strncmp(encoded,"ConsoleNG 13\n",13));
 assert(settingsParse(encoded,&roundtrip)&&!memcmp(&nav,&roundtrip,sizeof(nav)));nav.bindings.bp_Version=5;assert(!con_binding_prefs_valid(&nav.bindings));}
 {struct ConsoleSettings clear=p,roundtrip=p;char encoded[2048];clear.bindings.bp_Version=7;clear.bindings.bp_Entries[3].kb_Action=CONSOLENG_BIND_CLEARHISTORY;
 assert(settingsFormatLegacy(encoded,sizeof(encoded),&clear)&&!strncmp(encoded,"ConsoleNG 14\n",13));
 assert(settingsParse(encoded,&roundtrip)&&!memcmp(&clear,&roundtrip,sizeof(clear)));clear.bindings.bp_Version=6;assert(!con_binding_prefs_valid(&clear.bindings));}
 for(unsigned action=14;action<=16;action++) {struct ConsoleSettings zoom=p,roundtrip=p;char encoded[2048];zoom.bindings.bp_Version=8;zoom.bindings.bp_Entries[3].kb_Action=action;
 assert(settingsFormatLegacy(encoded,sizeof(encoded),&zoom)&&!strncmp(encoded,"ConsoleNG 15\n",13));
 assert(settingsParse(encoded,&roundtrip)&&!memcmp(&zoom,&roundtrip,sizeof(zoom)));zoom.bindings.bp_Version=7;assert(!con_binding_prefs_valid(&zoom.bindings));}
 for(unsigned action=17;action<=18;action++){struct ConsoleSettings half=p,roundtrip=p;char encoded[2048];half.bindings.bp_Version=9;half.bindings.bp_Entries[3].kb_Action=action;
 assert(settingsFormatLegacy(encoded,sizeof(encoded),&half)&&!strncmp(encoded,"ConsoleNG 16\n",13));
 assert(settingsParse(encoded,&roundtrip)&&!memcmp(&half,&roundtrip,sizeof(half)));half.bindings.bp_Version=8;assert(!con_binding_prefs_valid(&half.bindings));}

 for(unsigned mask=0;mask<4;mask++){
  struct ConsoleSettings clear=p,restored=p;char encoded[2048];clear.oscCopy=mask;
  clear.bindings.bp_Version=10;clear.bindings.bp_Entries[3].kb_Action=CONSOLENG_BIND_CLEARSELECTION;
  assert(settingsFormatLegacy(encoded,sizeof(encoded),&clear));
  assert(strstr(encoded,"ConsoleNG 18\n"));
  assert(settingsParse(encoded,&restored)&&!memcmp(&clear,&restored,sizeof(clear)));
  clear.bindings.bp_Version=9;assert(!settingsValid(&clear));
 }
 for(unsigned action=20;action<=21;action++)for(unsigned mask=0;mask<4;mask++) {
  struct ConsoleSettings nav=p,restored=p;char encoded[2048];nav.oscCopy=mask;
  nav.bindings.bp_Version=11;nav.bindings.bp_Entries[3].kb_Action=action;
  assert(settingsFormat(encoded,sizeof(encoded),&nav)&&strstr(encoded,"ConsoleNG 20\n"));
  assert(settingsParse(encoded,&restored)&&!memcmp(&nav,&restored,sizeof(nav)));
  nav.bindings.bp_Version=10;assert(!settingsValid(&nav));
 }
 for(unsigned condition=0;condition<81;condition++)for(unsigned mask=0;mask<4;mask++){
  struct ConsoleSettings nav=p,restored=p;char encoded[2048];nav.oscCopy=mask;
  nav.bindings.bp_Version=18;unsigned n=condition;nav.bindings.bp_Entries[3].kb_Modifiers=4;
  for(unsigned d=0;d<4;d++){nav.bindings.bp_Entries[3].kb_Modifiers|=(n%3)<<(5+2*d);n/=3;}
  assert(settingsFormat(encoded,sizeof(encoded),&nav)&&strstr(encoded,"ConsoleNG 27\n"));
  assert(settingsParse(encoded,&restored)&&!memcmp(&nav,&restored,sizeof(nav)));
  if(condition>=3){nav.bindings.bp_Version=17;assert(!settingsValid(&nav));}
 }
 for(unsigned condition=0;condition<243;condition++)for(unsigned mask=0;mask<4;mask++){
  struct ConsoleSettings nav=p,restored=p;char encoded[2048];nav.oscCopy=mask;
  nav.bindings.bp_Version=19;unsigned n=condition;nav.bindings.bp_Entries[3].kb_Modifiers=4;
  for(unsigned d=0;d<5;d++){nav.bindings.bp_Entries[3].kb_Modifiers|=(n%3)<<(5+2*d);n/=3;}
  assert(settingsFormat(encoded,sizeof(encoded),&nav)&&strstr(encoded,"ConsoleNG 28\n"));
  assert(settingsParse(encoded,&restored)&&!memcmp(&nav,&restored,sizeof(nav)));
  if(condition>=81){nav.bindings.bp_Version=18;assert(!settingsValid(&nav));}
 }
 for(unsigned action=59;action<=64;action++)for(unsigned mask=0;mask<4;mask++) {
  struct ConsoleSettings nav=p,restored=p;char encoded[2048];nav.oscCopy=mask;
  nav.bindings.bp_Version=17;nav.bindings.bp_Entries[3].kb_Action=action;
  assert(settingsFormat(encoded,sizeof(encoded),&nav)&&strstr(encoded,"ConsoleNG 26\n"));
  assert(settingsParse(encoded,&restored)&&!memcmp(&nav,&restored,sizeof(nav)));
  nav.bindings.bp_Version=16;assert(!settingsValid(&nav));
 }
 for(unsigned action=53;action<=58;action++)for(unsigned mask=0;mask<4;mask++) {
  struct ConsoleSettings nav=p,restored=p;char encoded[2048];nav.oscCopy=mask;
  nav.bindings.bp_Version=16;nav.bindings.bp_Entries[3].kb_Action=action;
  assert(settingsFormat(encoded,sizeof(encoded),&nav)&&strstr(encoded,"ConsoleNG 25\n"));
  assert(settingsParse(encoded,&restored)&&!memcmp(&nav,&restored,sizeof(nav)));
  nav.bindings.bp_Version=15;assert(!settingsValid(&nav));
 }
 for(unsigned action=44;action<=52;action++)for(unsigned mask=0;mask<4;mask++) {
  struct ConsoleSettings nav=p,restored=p;char encoded[2048];nav.oscCopy=mask;
  nav.bindings.bp_Version=15;nav.bindings.bp_Entries[3].kb_Action=action;
  assert(settingsFormat(encoded,sizeof(encoded),&nav)&&strstr(encoded,"ConsoleNG 24\n"));
  assert(settingsParse(encoded,&restored)&&!memcmp(&nav,&restored,sizeof(nav)));
  nav.bindings.bp_Version=14;assert(!settingsValid(&nav));
 }
 for(unsigned action=26;action<=43;action++)for(unsigned mask=0;mask<4;mask++) {
  struct ConsoleSettings nav=p,restored=p;char encoded[2048];nav.oscCopy=mask;
  nav.bindings.bp_Version=14;nav.bindings.bp_Entries[3].kb_Action=action;
  assert(settingsFormat(encoded,sizeof(encoded),&nav)&&strstr(encoded,"ConsoleNG 23\n"));
  assert(settingsParse(encoded,&restored)&&!memcmp(&nav,&restored,sizeof(nav)));
  nav.bindings.bp_Version=13;assert(!settingsValid(&nav));
 }
 for(unsigned action=22;action<=25;action++)for(unsigned mask=0;mask<4;mask++) {
  struct ConsoleSettings nav=p,restored=p;char encoded[2048];nav.oscCopy=mask;
  nav.bindings.bp_Version=13;nav.bindings.bp_Entries[3].kb_Action=action;
  assert(settingsFormat(encoded,sizeof(encoded),&nav)&&strstr(encoded,"ConsoleNG 22\n"));
  assert(settingsParse(encoded,&restored)&&!memcmp(&nav,&restored,sizeof(nav)));
  nav.bindings.bp_Version=12;assert(!settingsValid(&nav));
 }
 for(unsigned mode=0;mode<3;mode++)for(unsigned mask=0;mask<4;mask++){
  struct ConsoleSettings nav=p,restored=p;char encoded[2048];nav.oscCopy=mask;nav.bindings.bp_Version=12;
  nav.bindings.bp_Entries[0].kb_Modifiers|=mode==1?32:mode==2?64:0;
  assert(settingsFormat(encoded,sizeof(encoded),&nav)&&strstr(encoded,"ConsoleNG 21\n"));
  assert(settingsParse(encoded,&restored)&&!memcmp(&nav,&restored,sizeof(nav)));
 }
 for(unsigned mask=0;mask<4;mask++)for(unsigned v=1;v<=15;v++){
  struct ConsoleSettings newer=p,restored=p;char encoded[2048];newer.oscCopy=mask;newer.bindings.bp_Version=v;
  assert(settingsFormatLegacy(encoded,sizeof(encoded),&newer));restored.oscCopy=3;
  assert(settingsParse(encoded,&restored)&&!memcmp(&newer,&restored,sizeof(newer)));
  if(mask&2){assert(!strncmp(encoded,"ConsoleNG 17\nosc52read=1\n",23));char tiny[24];assert(!settingsFormatLegacy(tiny,sizeof(tiny),&newer));}
 }
 {struct ConsoleSettings saved=p;char bad[]="ConsoleNG 17\nosc52read=1\nConsoleNG 17\nosc52read=1\n";
 assert(!settingsParse(bad,&saved)&&!memcmp(&saved,&p,sizeof(p)));}
 for(unsigned lines=0;lines<=100;lines++)for(unsigned mask=0;mask<4;mask++){
  struct ConsoleSettings wheel=p,restored=p;char encoded[2048];wheel.wheelLines=lines;wheel.oscCopy=mask;
  assert(settingsFormat(encoded,sizeof(encoded),&wheel));assert(!strncmp(encoded,"ConsoleNG 19\nwheel=",19));
  restored.wheelLines=101;assert(settingsParse(encoded,&restored));assert(!memcmp(&wheel,&restored,sizeof(wheel)));
  char tiny[20];assert(!settingsFormat(tiny,sizeof(tiny),&wheel));
 }
 {const char *bad[]={"ConsoleNG 19\nwheel=101\n","ConsoleNG 19\nwheel=-1\n","ConsoleNG 19\nwheel=1x\n","ConsoleNG 19\nwheel=999999999999999999999999\n","ConsoleNG 19\nwheel=1\nConsoleNG 19\nwheel=2\n"};
 for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);i++){struct ConsoleSettings before=p;assert(!settingsParse(bad[i],&p)&&!memcmp(&before,&p,sizeof(p)));}}
 char text[1024];assert(settingsFormatLegacy(text,sizeof(text),&p));
 assert(settingsParse(text,&out)&&!memcmp(&out,&p,sizeof(p)));
 p.emoji=1;assert(settingsFormatLegacy(text,sizeof(text),&p));out.emoji=0;
 assert(settingsParse(text,&out)&&out.emoji==1);
 char wrongEmoji[1024];strcpy(wrongEmoji,text);strstr(wrongEmoji,"emoji=")[6]='2';
 assert(!settingsParse(wrongEmoji,&out)&&out.emoji==1);
 assert(settingsParse("ConsoleNG 1\ncursor=5 350 8000\nhistory=128\n",&out)&&out.emoji==1);
 p.bell=1;assert(settingsFormatLegacy(text,sizeof(text),&p));out.bell=0;
 assert(settingsParse(text,&out)&&out.bell==1);
 char wrongBell[1024];strcpy(wrongBell,text);strstr(wrongBell,"bell=")[5]='2';
 assert(!settingsParse(wrongBell,&out)&&out.bell==1);
 char legacy4[1024];strcpy(legacy4,text);legacy4[10]='4';strstr(legacy4,"bell=")[0]=0;
 assert(settingsParse(legacy4,&out)&&out.bell==1);
 p.bell=0;assert(settingsFormatLegacy(text,sizeof(text),&p)&&settingsParse(text,&out)&&out.bell==0);
 p.oscCopy=1;assert(settingsFormatLegacy(text,sizeof(text),&p));out.oscCopy=0;
 assert(settingsParse(text,&out)&&out.oscCopy==1);
 char wrongCopy[1024];strcpy(wrongCopy,text);strstr(wrongCopy,"osc52=")[6]='2';
 assert(!settingsParse(wrongCopy,&out)&&out.oscCopy==1);
 char legacy5[1024];strcpy(legacy5,text);legacy5[10]='5';strstr(legacy5,"osc52=")[0]=0;
 assert(settingsParse(legacy5,&out)&&out.oscCopy==1);
 p.oscCopy=0;assert(settingsFormatLegacy(text,sizeof(text),&p)&&settingsParse(text,&out)&&out.oscCopy==0);
 p.cursorCustom=1;p.cursorRGB=0x12a4e8;assert(settingsFormatLegacy(text,sizeof(text),&p));out.cursorRGB=0;
 assert(settingsParse(text,&out)&&out.cursorCustom==1&&out.cursorRGB==0x12a4e8);
 char legacy6[1024];strcpy(legacy6,text);legacy6[10]='6';strstr(legacy6,"cursorcolor=")[0]=0;
 assert(settingsParse(legacy6,&out)&&out.cursorCustom==1&&out.cursorRGB==0x12a4e8);
 const char *badCursor[]={"2 123456\n","1 1000000\n","1 -1\n","1 +123456\n","1 xyz\n","1 123456 trailing\n","1 123456\nextra\n"};
 for(unsigned k=0;k<sizeof(badCursor)/sizeof(*badCursor);k++){char invalid[1024];strcpy(invalid,text);strcpy(strstr(invalid,"cursorcolor=")+12,badCursor[k]);struct ConsoleSettings before=out;assert(!settingsParse(invalid,&out)&&!memcmp(&out,&before,sizeof(out)));}
 p.cursorCustom=0;assert(settingsFormatLegacy(text,sizeof(text),&p)&&settingsParse(text,&out)&&!out.cursorCustom&&out.cursorRGB==0x12a4e8);
 struct ConsoleSettings original=p;
 for(unsigned i=0;i<18;i++)p.colors.cp_RGB[i]=0x010203+i;
 p.colors.cp_Mask=CONSOLENG_COLOR_MASK;
 assert(settingsFormatLegacy(text,sizeof(text),&p)&&settingsParse(text,&out)&&!memcmp(&out,&p,sizeof(p)));
 char broken[1024];strcpy(broken,text);strstr(broken,"colors=")[7]='z';out=p;assert(!settingsParse(broken,&out)&&!memcmp(&out,&p,sizeof(p)));
 strcpy(broken,text);strcat(broken,"junk");assert(!settingsParse(broken,&out));
 strcpy(broken,text);broken[strlen(broken)-8]=0;assert(!settingsParse(broken,&out));
 p=original;assert(settingsFormatLegacy(text,sizeof(text),&p));out=p;
 assert(settingsParse("ConsoleNG-Cursor 1\n6 500 0\n",&out));assert(out.cursor.cp_Style==6&&out.history.hp_Lines==4096&&out.font.fp_Pixels==16);
 assert(settingsParse("ConsoleNG 1\ncursor=5 350 8000\nhistory=128\n",&out)&&out.font.fp_Pixels==16);
 const char *bad[]={"ConsoleNG 2\ncursor=5 350 8000\nhistory=8\nfont=0 DejaVuSansMono.font\n", "ConsoleNG 2\ncursor=5 350 8000\nhistory=8\nfont=65536 fixed.font\n", "ConsoleNG 2\ncursor=5 350 8000\nhistory=8\nfont=16 bad\tfont\n","", "ConsoleNG 2\ncursor=5 350 8000\nhistory=4096\n", "ConsoleNG 1\ncursor=5 350 8000\n", "ConsoleNG 1\ncursor=5 350 8000\nhistory=-1\n", "ConsoleNG 1\ncursor=5 49 8000\nhistory=0\n", "ConsoleNG 1\ncursor=7 350 8000\nhistory=4096\n", "ConsoleNG 1\ncursor=5 350 8000\nhistory=4294967296\n", "ConsoleNG 1\ncursor=5 350 8000\nhistory=4096 extra\n", "ConsoleNG-Cursor 1\n5 350 8000 extra\n"};
 for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);i++){out=p;assert(!settingsParse(bad[i],&out));assert(!memcmp(&out,&p,sizeof(p)));}
 assert(settingsParse("ConsoleNG 2\ncursor=5 350 8000\nhistory=128\nfont=0 -\n",&out)&&!out.font.fp_Name[0]);
 struct ConsoleSettings malformed=p;memset(malformed.font.fp_Name,'x',128);assert(!settingsValid(&malformed));
 malformed=p;malformed.font.fp_Version=2;assert(!settingsValid(&malformed));
 malformed=p;memset(malformed.font.fp_Name,'x',127);malformed.font.fp_Name[127]=0;assert(settingsFormatLegacy(text,sizeof(text),&malformed));assert(settingsParse(text,&out)&&!memcmp(&out,&malformed,sizeof(out)));
 assert(settingsFormatLegacy(text,sizeof(text),&p));
 assert(settingsSave("prefs",&p));assert(settingsLoad("prefs",&out)&&!memcmp(&out,&p,sizeof(p)));
 p.history.hp_Lines=0;assert(settingsSave("prefs",&p));assert(settingsLoad("prefs",&out)&&out.history.hp_Lines==0);
 p.history.hp_Lines=CONSOLENG_HISTORY_MAX_LINES;assert(settingsSave("prefs",&p));assert(settingsLoad("prefs",&out)&&out.history.hp_Lines==p.history.hp_Lines);
 assert(!settingsSave("missing/prefs",&p));assert(!mkdir("dir",0700));assert(!settingsSave("dir",&p));
 assert(!fopen("dir.0x1234.tmp","rb"));
 FILE *f=fopen("nul","wb");assert(f);assert(fwrite(text,1,strlen(text)+1,f)==strlen(text)+1);fclose(f);assert(!settingsLoad("nul",&out));
 p.bindings.bp_Version=2;p.bindings.bp_Entries[0].kb_Key=CONSOLENG_BIND_RAW+0x50;
 assert(settingsFormatLegacy(text,sizeof(text),&p)&&!strncmp(text,"ConsoleNG 9",11));
 assert(settingsParse(text,&out)&&!memcmp(&p,&out,sizeof(p)));
 char wrongVersion[1024];strcpy(wrongVersion,text);wrongVersion[10]='8';struct ConsoleSettings beforeVersion=out;
 assert(!settingsParse(wrongVersion,&out)&&!memcmp(&out,&beforeVersion,sizeof(out)));
 assert(!settingsFormatLegacy(text,4,&p));
 con_binding_prefs_defaults(&p.bindings);
 p.bindings.bp_Entries[0]=(struct ConsolengKeyBinding){'x',5,1};
 assert(settingsFormatLegacy(text,sizeof(text),&p));out=p;assert(settingsParse(text,&out)&&!memcmp(&p,&out,sizeof(p)));
 char legacy7[1024];strcpy(legacy7,text);legacy7[10]='7';*strstr(legacy7,"bindings=")=0;
 assert(settingsParse(legacy7,&out)&&out.bindings.bp_Entries[0].kb_Key=='x');
 const char *badBindings[]={"17\n","-1\n","1\n120 32 1\n","1\n120 5 5\n","2\n120 5 1\n120 5 2\n","1\n65 5 1\n","1\n120 5 1\nextra\n","1\n120 5","0\n1 2 3\n"};
 for(unsigned i=0;i<sizeof(badBindings)/sizeof(*badBindings);i++) {
  char invalid[1024];strcpy(invalid,text);strcpy(strstr(invalid,"bindings=")+9,badBindings[i]);
  struct ConsoleSettings before=out;assert(!settingsParse(invalid,&out)&&!memcmp(&before,&out,sizeof(out)));
 }
 memset(&p.bindings,0,sizeof(p.bindings));p.bindings.bp_Size=sizeof(p.bindings);p.bindings.bp_Version=1;
 assert(settingsFormatLegacy(text,sizeof(text),&p)&&settingsParse(text,&out)&&!out.bindings.bp_Count);
 for(unsigned i=0;i<16;i++)p.bindings.bp_Entries[i]=(struct ConsolengKeyBinding){'a'+i,5,1+i%4};
 p.bindings.bp_Count=16;assert(settingsFormatLegacy(text,sizeof(text),&p)&&settingsParse(text,&out)&&!memcmp(&p,&out,sizeof(p)));
 assert(settingsSave("bindings",&p)&&settingsLoad("bindings",&out)&&!memcmp(&p,&out,sizeof(p)));

 return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';exe=Path(d)/'test';p.write_text(prefix+h+(r/'prefs/settings.h').read_text().replace('#include "../binding_prefs.h"',(r/'binding_prefs.h').read_text()).replace('#include "semantic_text.h"',(r/'prefs/semantic_text.h').read_text().replace('#include "../semantic_prefs.h"',(r/'semantic_prefs.h').read_text()))+main)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],cwd=d,check=True)
print('Settings PASS: versioned cursor/history/font/color roundtrip, legacy import, malformed input unchanged, history boundaries, atomic file replacement/failure, embedded NUL rejection')
