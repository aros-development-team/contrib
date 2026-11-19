#!/usr/bin/env python3
"""Actual session profile: bell and render defaults publish atomically."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'console.c').read_text();h=(r/'include/consoleng.h').read_text()
h=h[h.index('#define CD_GETCURSORPREFS'):h.index('#endif /* DEVICES_CONSOLENG_H */')]
a=s.index('static BOOL ConsolengValidCursorPrefs(');validator=s[a:s.index('static int GM_UNIQUENAME(Init)',a)]
body=s[s.index('    case CD_GETSESSIONPROFILE:'):s.index('    case CD_GETRENDERPROFILE:')]
a=s.index('static BOOL ConsolengValidColorPrefs(');validator=s[a:s.index('static BOOL ConsolengFontAvailable(',a)]+validator
prefix=r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
typedef uint32_t ULONG;typedef unsigned char UBYTE;typedef int BOOL;
#define TRUE 1
#define FALSE 0
#define CMD_NONSTD 9
#define CONU_LIBRARY -1
#define IOERR_BADADDRESS -5
#define IOERR_BADLENGTH -4
#define CopyMem(a,b,n) memcpy(b,a,n)
static int locked,acquires;
#define ObtainSemaphore(x) do{assert(!locked);locked=1;acquires++;}while(0)
#define ReleaseSemaphore(x) do{assert(locked);locked=0;}while(0)
struct Unit {int unused;};
struct IOStdReq {void *io_Data;struct Unit *io_Unit;ULONG io_Length,io_Actual,io_Command;};
'''
extra=r'''
static struct {struct ConsolengCursorPrefs cursorDefaults;struct ConsolengHistoryPrefs historyDefaults;struct ConsolengFontPrefs fontDefaults;struct ConsolengColorPrefs colorDefaults;struct ConsolengEmojiPrefs emojiDefaults;struct ConsolengBellPrefs bellDefaults;} state,*ConsoleDevice=&state;
static int ConsolengFontAvailable(const struct ConsolengFontPrefs *p){return strcmp(p->fp_Name,"missing.font")!=0;}
static int request(struct IOStdReq *ioreq){int error=0;switch(ioreq->io_Command){
'''+body+r'''
}return error;}
int main(void){
 struct ConsolengSessionProfile candidate={296,1,{264,1,{20,1,5,350,8000},{12,1,4096},{140,1,16,"fixed.font"},{84,1,3,{0xe5e5e5,0x203040}}},{12,1,1},{12,1,1}};
 struct IOStdReq io={&candidate,(struct Unit*)CONU_LIBRARY,296,999,CD_SETSESSIONPROFILE};
 assert(!request(&io)&&io.io_Actual==296&&acquires==1&&!locked);
 assert(state.bellDefaults.bp_Enabled==1&&state.emojiDefaults.ep_Enabled==1&&state.cursorDefaults.cp_Style==5);
 for(int field=0;field<74;field++){
  if(field>=15&&field<47)continue;
  struct ConsolengSessionProfile p=candidate;((ULONG*)&p)[field]=0xffffffffU;io.io_Data=&p;acquires=0;
  assert(request(&io)==CONSOLENG_ERR_BADPREFS&&io.io_Actual==0&&!acquires);
  assert(state.bellDefaults.bp_Enabled==1&&state.emojiDefaults.ep_Enabled==1&&state.cursorDefaults.cp_Style==5);
 }
 struct {struct ConsolengSessionProfile p;ULONG guard;} out={{0},0x12345678};
 io.io_Data=&out;io.io_Length=sizeof(out);io.io_Command=CD_GETSESSIONPROFILE;acquires=0;
 assert(!request(&io)&&io.io_Actual==296&&acquires==1&&!locked&&out.guard==0x12345678);
 assert(!memcmp(&out.p,&candidate,sizeof(candidate)));
 io.io_Length=295;assert(request(&io)==IOERR_BADLENGTH&&io.io_Actual==0);
 io.io_Length=296;io.io_Data=0;assert(request(&io)==IOERR_BADADDRESS);
 io.io_Data=&out;io.io_Unit=0;assert(request(&io)==IOERR_BADADDRESS);
 assert(sizeof(struct ConsolengThemeProfile)==264);
 assert(sizeof(struct ConsolengRenderProfile)==284);
 return 0;
}

'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';exe=Path(d)/'test';p.write_text(prefix+h+validator+extra)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
print('Session profile PASS: one publication lock, coherent snapshot, rejection of each malformed field without mutation, bounded GET and invalid buffer/unit')
