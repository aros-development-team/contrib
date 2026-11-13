#!/usr/bin/env python3
"""Actual input profile: wheel, bindings and appearance publish atomically."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'console.c').read_text();h=(r/'include/consoleng.h').read_text()
h=h[h.index('#define CD_GETCURSORPREFS'):h.index('#endif /* DEVICES_CONSOLENG_H */')]
a=s.index('static BOOL ConsolengValidCursorPrefs(');validator=s[a:s.index('static int GM_UNIQUENAME(Init)',a)]
body=s[s.index('    case CD_GETINTERACTIONPROFILE:'):s.index('    case CD_GETAPPEARANCEPROFILE:')]
a=s.index('static BOOL ConsolengValidColorPrefs(');validator=s[a:s.index('static BOOL ConsolengFontAvailable(',a)]+validator
prefix=r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
typedef uint32_t ULONG;typedef unsigned char UBYTE;typedef int BOOL;typedef void VOID;
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
static struct {struct ConsolengCursorPrefs cursorDefaults;struct ConsolengHistoryPrefs historyDefaults;struct ConsolengFontPrefs fontDefaults;struct ConsolengColorPrefs colorDefaults;struct ConsolengEmojiPrefs emojiDefaults;struct ConsolengBellPrefs bellDefaults;struct ConsolengClipboardPrefs clipboardDefaults;struct ConsolengCursorColorPrefs cursorColorDefaults;struct ConsolengBindingPrefs bindingDefaults;ULONG wheelLinesDefault;struct ConsolengSemanticPrefs semanticDefaults;} state,*ConsoleDevice=&state;
static int ConsolengFontAvailable(const struct ConsolengFontPrefs *p){return strcmp(p->fp_Name,"missing.font")!=0;}
static int con_bindings_valid(const struct ConsolengBindingPrefs *p){return con_binding_prefs_valid(p);}
static int request(struct IOStdReq *ioreq){int error=0;switch(ioreq->io_Command){
'''+body+r'''
}return error;}
int main(void){
 struct ConsolengAppearanceProfile candidate={324,1,{264,1,{20,1,5,350,8000},{12,1,4096},{140,1,16,"fixed.font"},{84,1,3,{0xe5e5e5,0x203040}}},{12,1,1},{12,1,1},{12,1,1},{16,1,1,0x12a4e8}};
 struct ConsolengInputProfile full={sizeof(full),1,{sizeof(full.ip_Interaction),1,candidate,{0}},{12,1,7}};
 con_binding_prefs_defaults(&full.ip_Interaction.ip_Bindings);
 struct IOStdReq io={&full,(struct Unit*)CONU_LIBRARY,sizeof(full),999,CD_SETINPUTPROFILE};
 assert(!request(&io)&&io.io_Actual==sizeof(full)&&acquires==1&&!locked);
 assert(state.wheelLinesDefault==7&&state.cursorDefaults.cp_Style==5);
 for(unsigned field=0;field<sizeof(full)/sizeof(ULONG);field++){
  /* Font name and unused shortcut entries are not numeric policy fields. */
  if((field>=19&&field<51)||(field>=100&&field<136))continue;
  struct ConsolengInputProfile p=full;((ULONG*)&p)[field]=0xffffffffU;
  io.io_Data=&p;acquires=0;__typeof__(state) before=state;
  assert(request(&io)==CONSOLENG_ERR_BADPREFS&&io.io_Actual==0&&!acquires);
  assert(!memcmp(&before,&state,sizeof(state)));
 }
 struct {struct ConsolengInputProfile p;ULONG guard;} out={{0},0x12345678};
 io.io_Data=&out;io.io_Length=sizeof(out);io.io_Command=CD_GETINPUTPROFILE;acquires=0;
 assert(!request(&io)&&io.io_Actual==sizeof(full)&&acquires==1&&!locked&&out.guard==0x12345678);
 assert(!memcmp(&out.p,&full,sizeof(full)));
 io.io_Length=sizeof(full)-1;assert(request(&io)==IOERR_BADLENGTH&&io.io_Actual==0);
 io.io_Length=sizeof(full);io.io_Data=0;assert(request(&io)==IOERR_BADADDRESS);
 io.io_Data=&out;io.io_Unit=0;assert(request(&io)==IOERR_BADADDRESS);
 /* Legacy combined calls retain their exact size and do not alter wheel defaults. */
 struct ConsolengInteractionProfile legacy=full.ip_Interaction;
 io=(struct IOStdReq){&legacy,(struct Unit*)CONU_LIBRARY,sizeof(legacy),999,CD_SETINTERACTIONPROFILE};
 assert(!request(&io)&&io.io_Actual==sizeof(legacy)&&state.wheelLinesDefault==7);
 memset(&legacy,0,sizeof(legacy));io.io_Command=CD_GETINTERACTIONPROFILE;
 assert(!request(&io)&&!memcmp(&legacy,&full.ip_Interaction,sizeof(legacy)));

 struct ConsolengSelectionProfile selected={sizeof(selected),1,full,{0}};
 selected.sp_Semantic=con_semantic_defaults();selected.sp_Semantic.sp_Chars[0]=';';
 io=(struct IOStdReq){&selected,(struct Unit*)CONU_LIBRARY,sizeof(selected),999,CD_SETSELECTIONPROFILE};
 acquires=0;assert(!request(&io)&&acquires==1&&io.io_Actual==sizeof(selected));
 for(unsigned field=0;field<sizeof(selected)/sizeof(ULONG);field++){
  struct ConsolengSelectionProfile bad=selected;
  ((ULONG*)&bad)[field]=0xffffffffU;
  /* Font names and unused binding entries already have independent validators. */
  if(field>=2+19&&field<2+51)continue;
  if(field>=2+100&&field<2+136)continue;
  io.io_Data=&bad;acquires=0;__typeof__(state) before=state;
  assert(request(&io)==CONSOLENG_ERR_BADPREFS&&io.io_Actual==0&&!acquires);
  assert(!memcmp(&before,&state,sizeof(state)));
 }
 struct {struct ConsolengSelectionProfile p;ULONG guard;} result={{0},0x12345678};
 io.io_Data=&result;io.io_Command=CD_GETSELECTIONPROFILE;io.io_Length=sizeof(result);acquires=0;
 assert(!request(&io)&&acquires==1&&result.guard==0x12345678&&!memcmp(&result.p,&selected,sizeof(selected)));
 io.io_Length=sizeof(selected)-1;assert(request(&io)==IOERR_BADLENGTH);
 io=(struct IOStdReq){&full,(struct Unit*)CONU_LIBRARY,sizeof(full),999,CD_SETINPUTPROFILE};
 assert(!request(&io)&&!memcmp(&state.semanticDefaults,&selected.sp_Semantic,sizeof(selected.sp_Semantic)));
 return 0;
}

'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';exe=Path(d)/'test';p.write_text(prefix+h+(r/'clipboard_prefs.h').read_text()+(r/'binding_prefs.h').read_text()+(r/'semantic_prefs.h').read_text()+validator+extra)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
 broken=body.replace('if(extended)ConsoleDevice->wheelLinesDefault=input.ip_Scroll.sp_Lines;', '')
 p.write_text(prefix+h+(r/'clipboard_prefs.h').read_text()+(r/'binding_prefs.h').read_text()+(r/'semantic_prefs.h').read_text()+validator+extra.replace(body,broken))
 subprocess.run(['cc','-Wall','-Werror',str(p),'-o',str(exe)],check=True)
 assert subprocess.run([str(exe)],capture_output=True).returncode!=0
 print('Negative control PASS: combined defaults must publish wheel distance')
print('Input profile PASS: one publication lock, coherent snapshot, rejection of each malformed field without mutation, bounded GET and invalid buffer/unit')
