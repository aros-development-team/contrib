#!/usr/bin/env python3
"""Negotiation and event bytes from the production keyboard module."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'keyboard_protocol.c').read_text();s=s[s.index('#define CON_KEYBOARD_SUPPORTED'):]
keys=r'''
#define RAWKEY_ESCAPE  	    0x45
#define RAWKEY_ESC          RAWKEY_ESCAPE /* AmigaOS 4 name */
#define RAWKEY_F1   	    0x50
#define RAWKEY_F2   	    0x51
#define RAWKEY_F3   	    0x52
#define RAWKEY_F4   	    0x53
#define RAWKEY_F5   	    0x54
#define RAWKEY_F6   	    0x55
#define RAWKEY_F7   	    0x56
#define RAWKEY_F8   	    0x57
#define RAWKEY_F9   	    0x58
#define RAWKEY_F10  	    0x59
#define RAWKEY_F11  	    0x4B
#define RAWKEY_F12  	    0x6F
#define RAWKEY_PAUSE	    0x6E
#define RAWKEY_TILDE	    0x00
#define RAWKEY_1    	    0x01
#define RAWKEY_2    	    0x02
#define RAWKEY_3    	    0x03
#define RAWKEY_4    	    0x04
#define RAWKEY_5    	    0x05
#define RAWKEY_6    	    0x06
#define RAWKEY_7    	    0x07
#define RAWKEY_8    	    0x08
#define RAWKEY_9    	    0x09
#define RAWKEY_0    	    0x0A
#define RAWKEY_MINUS	    0x0B
#define RAWKEY_EQUAL	    0x0C
#define RAWKEY_BACKSLASH    0x0D
#define RAWKEY_BACKSPACE    0x41
#define RAWKEY_TAB  	    0x42
#define RAWKEY_Q    	    0x10
#define RAWKEY_W    	    0x11
#define RAWKEY_E    	    0x12
#define RAWKEY_R    	    0x13
#define RAWKEY_T    	    0x14
#define RAWKEY_Y    	    0x15
#define RAWKEY_U    	    0x16
#define RAWKEY_I    	    0x17
#define RAWKEY_O    	    0x18
#define RAWKEY_P    	    0x19
#define RAWKEY_LBRACKET     0x1A
#define RAWKEY_RBRACKET     0x1B
#define RAWKEY_RETURN	    0x44
#define RAWKEY_CAPSLOCK     0x62
#define RAWKEY_A    	    0x20
#define RAWKEY_S    	    0x21
#define RAWKEY_D    	    0x22
#define RAWKEY_F    	    0x23
#define RAWKEY_G    	    0x24
#define RAWKEY_H    	    0x25
#define RAWKEY_J    	    0x26
#define RAWKEY_K    	    0x27
#define RAWKEY_L    	    0x28
#define RAWKEY_SEMICOLON    0x29
#define RAWKEY_QUOTE	    0x2A
#define RAWKEY_2B   	    0x2B
#define RAWKEY_LSHIFT	    0x60
#define RAWKEY_LESSGREATER  0x30
#define RAWKEY_Z    	    0x31
#define RAWKEY_X    	    0x32
#define RAWKEY_C    	    0x33
#define RAWKEY_V    	    0x34
#define RAWKEY_B    	    0x35
#define RAWKEY_N    	    0x36
#define RAWKEY_M    	    0x37
#define RAWKEY_COMMA	    0x38
#define RAWKEY_PERIOD	    0x39
#define RAWKEY_SLASH	    0x3A
#define RAWKEY_RSHIFT	    0x61
#define RAWKEY_CONTROL	    0x63
#define RAWKEY_LCONTROL     0x63
#define RAWKEY_LAMIGA	    0x66
#define RAWKEY_LALT 	    0x64
#define RAWKEY_SPACE	    0x40
#define RAWKEY_RALT 	    0x65
#define RAWKEY_RAMIGA	    0x67
#define RAWKEY_INSERT	    0x47
#define RAWKEY_DELETE	    0x46
#define RAWKEY_HOME 	    0x70
#define RAWKEY_END  	    0x71
#define RAWKEY_PAGEUP	    0x48
#define RAWKEY_PAGEDOWN     0x49
#define RAWKEY_HELP 	    0x5F
#define RAWKEY_UP   	    0x4C
#define RAWKEY_LEFT 	    0x4F
#define RAWKEY_DOWN 	    0x4D
#define RAWKEY_RIGHT	    0x4E
#define RAWKEY_KP_7 	    0x3D
#define RAWKEY_KP_8 	    0x3E
#define RAWKEY_KP_9 	    0x3F
#define RAWKEY_KP_4 	    0x2D
#define RAWKEY_KP_5 	    0x2E
#define RAWKEY_KP_6 	    0x2F
#define RAWKEY_KP_1 	    0x1D
#define RAWKEY_KP_2 	    0x1E
#define RAWKEY_KP_3 	    0x1F
#define RAWKEY_KP_0 	    0x0F
#define RAWKEY_KP_DECIMAL   0x3C
#define RAWKEY_KP_PLUS	    0x5E
#define RAWKEY_KP_MINUS     0x4A
#define RAWKEY_KP_ENTER     0x43
#define RAWKEY_KP_DIVIDE    0x5C
#define RAWKEY_KP_MULTIPLY  0x5D
#define RAWKEY_SCRLOCK      0x6B
#define RAWKEY_PRTSCREEN    0x6C
#define RAWKEY_NUMLOCK      0x6D
#define RAWKEY_MEDIA1       0x72
#define RAWKEY_MEDIA2       0x73
#define RAWKEY_MEDIA3       0x74
#define RAWKEY_MEDIA4       0x75
#define RAWKEY_MEDIA5       0x76
#define RAWKEY_MEDIA6       0x77
#define RAWKEY_CDTV_STOP    RAWKEY_MEDIA1
#define RAWKEY_CDTV_PLAY    RAWKEY_MEDIA2
#define RAWKEY_CDTV_PREV    RAWKEY_MEDIA3
#define RAWKEY_CDTV_NEXT    RAWKEY_MEDIA4
#define RAWKEY_CDTV_REW     RAWKEY_MEDIA5
#define RAWKEY_CDTV_FF      RAWKEY_MEDIA6
#define RAWKEY_NM_WHEEL_UP      0x7A
#define RAWKEY_NM_WHEEL_DOWN    0x7B
#define RAWKEY_NM_WHEEL_LEFT    0x7C
#define RAWKEY_NM_WHEEL_RIGHT   0x7D
#define RAWKEY_NM_BUTTON_FOURTH 0x7E
'''
prefix=r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
typedef uint8_t UBYTE;typedef uint16_t UWORD;typedef uint32_t ULONG;typedef int32_t LONG;typedef int BOOL;typedef void VOID;
#define TRUE 1
#define FALSE 0
#define MEMF_PUBLIC 1
#define MEMF_CLEAR 2
#define IECLASS_RAWKEY 1
#define IECODE_UP_PREFIX 128
#define IEQUALIFIER_LSHIFT 1
#define IEQUALIFIER_RSHIFT 2
#define IEQUALIFIER_LALT 4
#define IEQUALIFIER_RALT 8
#define IEQUALIFIER_CONTROL 16
#define IEQUALIFIER_REPEAT 512
#define IEQUALIFIER_LCOMMAND 32
#define IEQUALIFIER_RCOMMAND 64
struct ConsolengKeyboard;
struct ConUnit{struct ConsolengKeyboard*keyboard;BOOL keyboardAlternate;};
struct InputEvent{ULONG ie_Code,ie_Qualifier,ie_Class;};
struct ConsoleBase{int unused;};
#define ICU(u) (u)
static int allocations,fail,mapVariant;
static void*AllocMem(ULONG n,int f){if(fail)return NULL;allocations++;return calloc(1,n);}
static void FreeMem(void*p,ULONG n){assert(p);allocations--;free(p);}
static char response[16];
static void con_inject(struct ConsoleBase*b,struct ConUnit*u,UBYTE*s,LONG n){memcpy(response,s,n);response[n]=0;}
static LONG RawKeyConvert(struct InputEvent*e,UBYTE*out,int n,void*k){if(e->ie_Code==0x12){if(mapVariant==6&&(e->ie_Qualifier&12)){if(n<80)return -1;memset(out,255,80);return 80;}if(mapVariant==4){if(n<80)return -1;memset(out,233,80);return 80;}if(mapVariant==5)return -1;out[0]=(e->ie_Qualifier&3)?201:233;if(e->ie_Qualifier&3){if(mapVariant==1)return 0;if(mapVariant==2){out[1]='x';return 2;}if(mapVariant==3)out[0]=27;}return 1;}if(e->ie_Code==0x20){out[0]=(e->ie_Qualifier&3)?'A':'a';if(e->ie_Qualifier&16)out[0]=1;if(e->ie_Qualifier&12)out[0]|=128;return 1;}if(e->ie_Code==1){out[0]=(e->ie_Qualifier&3)?'!':'1';if(e->ie_Qualifier&12)out[0]|=128;return 1;}return 0;}
'''
test=r'''
static struct ConsoleBase base;
static void cmd(struct ConUnit*u,const char*s){assert(con_keyboard_csi(u,(const UBYTE*)s,strlen(s),&base)==strlen(s));}
int main(void){struct ConUnit u={0};UBYTE out[360];struct InputEvent e={RAWKEY_ESCAPE,0,IECLASS_RAWKEY};
 assert(con_keyboard_event(&u,&e,out,&base)==-1);cmd(&u,"?u");assert(!strcmp(response,"\x1b[?0u")&&!allocations);
 fail=1;cmd(&u,">1u");assert(!u.keyboard);fail=0;
 cmd(&u,">33u");cmd(&u,"?u");assert(!strcmp(response,"\x1b[?1u"));
 for(int n=0;n<4096;n++)cmd(&u,n%2?">1u":">0u");assert(u.keyboard->depth[0]==4096);
 cmd(&u,"<4095u");assert(u.keyboard->depth[0]==1&&u.keyboard->flags==0);cmd(&u,"<65535u");assert(!u.keyboard->flags&&!u.keyboard->depth[0]);
 cmd(&u,">1u");con_keyboard_switch(&u,1);assert(!u.keyboard->flags);cmd(&u,">0u");con_keyboard_switch(&u,0);assert(u.keyboard->flags);con_keyboard_switch(&u,1);assert(!u.keyboard->flags);con_keyboard_switch(&u,0);
 cmd(&u,"=1;3u");assert(!u.keyboard->flags);cmd(&u,"=1;2u");assert(u.keyboard->flags);cmd(&u,"=32u");assert(!u.keyboard->flags);cmd(&u,"=1u");
 const char*bad[]={"=65536u","=1;2;3u","=1:2u",">1;2u","?1u"};for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);i++){cmd(&u,bad[i]);assert(u.keyboard->flags==1);}
 int codes[]={RAWKEY_ESCAPE,RAWKEY_TAB,RAWKEY_RETURN,RAWKEY_BACKSPACE,RAWKEY_A,RAWKEY_1,RAWKEY_KP_0,RAWKEY_KP_ENTER,RAWKEY_F3,RAWKEY_UP};
 int values[]={27,9,13,127,97,49,57399,57414,13,0};int count=0;
 for(int key=0;key<10;key++)for(int mod=0;mod<8;mod++){
  e.ie_Code=codes[key];e.ie_Qualifier=((mod&1)?1:0)|((mod&2)?4:0)|((mod&4)?16:0);e.ie_Class=IECLASS_RAWKEY;
  int n=con_keyboard_event(&u,&e,out,&base);
  if((key>=1&&key<=3&&!mod)||((key==4||key==5)&&mod<2)){assert(n==-1);continue;}
  char expected[40],payload[20];if(values[key])sprintf(payload,"%d",values[key]);else strcpy(payload,mod?"1":"");
  if(mod)sprintf(expected,"\x1b[%s;%d%c",payload,mod+1,key==8?'~':key==9?'A':'u');else sprintf(expected,"\x1b[%s%c",payload,key==8?'~':key==9?'A':'u');
  assert(n==(int)strlen(expected)&&!memcmp(out,expected,n));e.ie_Code|=128;assert(con_keyboard_event(&u,&e,out,&base)==0);e.ie_Code&=127;e.ie_Qualifier|=32;assert(con_keyboard_event(&u,&e,out,&base)==-1);count++;
 }

 int eventCount=0;
 for(int flags=0;flags<32;flags++){
  char command[20],query[20];sprintf(command,"=%du",flags);cmd(&u,command);cmd(&u,"?u");sprintf(query,"\x1b[?%du",flags&31);assert(!strcmp(response,query));
  for(int key=0;key<10;key++)for(int mod=0;mod<8;mod++)for(int type=0;type<3;type++){
   int enabled=flags&11,all=flags&8,disambig=flags&1,events=flags&2;
   e.ie_Code=codes[key]|(type==2?128:0);e.ie_Qualifier=((mod&1)?1:0)|((mod&2)?4:0)|((mod&4)?16:0)|(type==1?512:0);e.ie_Class=IECLASS_RAWKEY;
   int n=con_keyboard_event(&u,&e,out,&base),fallback=0,suppressed=0;
   if(!enabled)fallback=1;
   else if(type==2&&!events)suppressed=1;
   else if(type==2&&key>=1&&key<=3&&!all)suppressed=1;
   else if(type==2&&key==7&&!all)suppressed=1;
   else if(type!=2&&!all){
    if(key==0&&!disambig)fallback=1;
    if(key>=1&&key<=3&&(!disambig||!mod))fallback=1;
    if((key==4||key==5)&&(!disambig||mod<2))fallback=1;
    if((key==6||key==7)&&!disambig)fallback=1;
   }
   if(fallback){assert(n==-1);eventCount++;continue;}if(suppressed){assert(n==0);eventCount++;continue;}
   char want[80],payload[20],event[8]="",mods[8]="",text[16]="";int hasEvent=events&&type;
   if(values[key])sprintf(payload,"%d",values[key]);else strcpy(payload,(mod||hasEvent)?"1":"");
   if((flags&4)&&(mod&1)&&(key==4||key==5))sprintf(payload+strlen(payload),":%d",key==4?65:33);
   if((flags&16)&&type!=2&&(key==4||key==5)){
    const int letters[]={97,65,225,193,1,1,129,129},digits[]={49,33,177,161,49,33,177,161};
    int cp=key==4?letters[mod]:digits[mod];
    if(cp>=32&&!(cp>=127&&cp<160))sprintf(text,";%d",cp);
   }
   if(mod||hasEvent||text[0])sprintf(mods,";%d",mod+1);
   if(hasEvent)sprintf(event,":%d",type+1);
   sprintf(want,"\x1b[%s%s%s%s%c",payload,mods,event,text,key==8?'~':key==9?'A':'u');
   assert(n==(int)strlen(want)&&!memcmp(out,want,n));eventCount++;
  }
 }
 cmd(&u,"=11u");int modifierCodes[]={RAWKEY_LSHIFT,RAWKEY_RSHIFT,RAWKEY_CONTROL,RAWKEY_LALT,RAWKEY_RALT};int modifierValues[]={57441,57447,57442,57443,57449};int bits[]={1,1,4,2,2};
 for(int key=0;key<5;key++)for(int release=0;release<2;release++){
  e.ie_Code=modifierCodes[key]|(release?128:0);e.ie_Qualifier=0;char want[40];
  if(release)sprintf(want,"\x1b[%d;1:3u",modifierValues[key]);else sprintf(want,"\x1b[%d;%du",modifierValues[key],bits[key]+1);
  int n=con_keyboard_event(&u,&e,out,&base);assert(n==(int)strlen(want)&&!memcmp(out,want,n));
 }
 cmd(&u,"=15u");e.ie_Code=RAWKEY_E;e.ie_Qualifier=1|4;
 for(mapVariant=0;mapVariant<4;mapVariant++){
  const char *want=mapVariant?"\x1b[233;4u":"\x1b[233:201;4u";
  int n=con_keyboard_event(&u,&e,out,&base);assert(n==(int)strlen(want)&&!memcmp(out,want,n));
 }
 mapVariant=0;e.ie_Qualifier=0;
 {const char *want="\x1b[233u";int n=con_keyboard_event(&u,&e,out,&base);assert(n==(int)strlen(want)&&!memcmp(out,want,n));}
 cmd(&u,"=24u");e.ie_Code=RAWKEY_E;e.ie_Qualifier=0;mapVariant=4;
 {char want[352]="\x1b[0;1";for(int i=0;i<80;i++)strcat(want,i?":233":";233");strcat(want,"u");memset(out,0x5a,sizeof(out));
  int n=con_keyboard_event(&u,&e,out,&base);assert(n==(int)strlen(want)&&n<352&&!memcmp(out,want,n));for(int i=352;i<360;i++)assert(out[i]==0x5a);
  mapVariant=5;assert(con_keyboard_event(&u,&e,out,&base)==-1);mapVariant=4;e.ie_Code|=128;assert(con_keyboard_event(&u,&e,out,&base)==0);
 }
 mapVariant=6;cmd(&u,"=31u");e.ie_Code=RAWKEY_E;e.ie_Qualifier=1|4|512;
 {char want[352]="\x1b[233:201;4:2";for(int i=0;i<80;i++)strcat(want,i?":255":";255");strcat(want,"u");memset(out,0x5a,sizeof(out));
  int n=con_keyboard_event(&u,&e,out,&base);assert(n==(int)strlen(want)&&n<352&&!memcmp(out,want,n));for(int i=352;i<360;i++)assert(out[i]==0x5a);
 }
 mapVariant=0;
 puts("Associated text PASS: 80-byte string mapping, exact bounded frame, overflow fallback and release suppression");
 puts("Alternate keys PASS: Latin-1, unshifted, missing/multiple/control shifted mappings");
 printf("Keyboard events PASS: %d mode/key/modifier/type cases and 10 modifier state cases\n",eventCount);
 con_keyboard_dispose(&u);assert(!allocations&&!u.keyboard);printf("Keyboard PASS: %d key/modifier/release/shortcut groups, stack bounds, two screens, modes, invalid requests and allocation failure\n",count);
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'t.c').write_text(prefix+keys+'\n'+s+test)
 subprocess.run(['cc','-Wall','-Werror','-Wno-misleading-indentation','-fsanitize=address,undefined',str(p/'t.c'),'-o',str(p/'t')],check=True)
 subprocess.run([str(p/'t')],check=True)
 for name,old,new in [
  ('unsupported flags', '#define CON_KEYBOARD_SUPPORTED 31', '#define CON_KEYBOARD_SUPPORTED 63'),
  ('missing alternate', "if(alternate){out[n++]=':';", "if(0){out[n++]=':';"),
  ('missing associated', 'i<textLength;i++', 'i<0;i++'),
  ('key release leak', 'if (release && !(k->flags&2)) return 0;', 'if (release && !(k->flags&2)) return -1;'),
  ('wrong Escape code', 'value=27;break;', 'value=28;break;')]:
  assert old in s
  (p/'t.c').write_text(prefix+keys+'\n'+s.replace(old,new)+test)
  subprocess.run(['cc','-Wall','-Werror','-Wno-misleading-indentation',str(p/'t.c'),'-o',str(p/'t')],check=True)
  result=subprocess.run([str(p/'t')],capture_output=True)
  assert result.returncode,name+' escaped detection'
  print('Negative control rejected:',name)
