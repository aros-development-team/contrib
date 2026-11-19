/* Native retained-history search through the real console input task. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/iffparse.h>
#include <datatypes/textclass.h>
#include <dos/dosextens.h>
#include <devices/rawkeycodes.h>
#include <stdio.h>
#include <string.h>
#include "../console_gcc.h"
struct Device *ConsoleDevice;
struct Library *IFFParseBase;
static struct ConsoleBase *base;
static Object *unit;
static BPTR stream;
static struct MsgPort *prefsPort;
static struct IOStdReq *prefsIO;
static int prefs(UWORD cmd,struct ConsolengBindingPrefs*p){prefsIO->io_Command=cmd;prefsIO->io_Data=p;prefsIO->io_Length=sizeof(*p);return !DoIO((struct IORequest*)prefsIO)&&prefsIO->io_Actual==sizeof(*p);}
static int fontprefs(UWORD cmd,struct ConsolengFontPrefs*p){prefsIO->io_Command=cmd;prefsIO->io_Data=p;prefsIO->io_Length=sizeof(*p);return !DoIO((struct IORequest*)prefsIO)&&prefsIO->io_Actual==sizeof(*p);}
static struct Window *window;
static int write_text(const char *s){return Write(stream,s,strlen(s))==(LONG)strlen(s);}
static int enqueue(UWORD code,UWORD qual)
{
    for(int up=0;up<2;up++){
        struct cdihMessage *m=AllocMem(sizeof(*m),MEMF_PUBLIC|MEMF_CLEAR);
        if(!m)return 0;
        m->msg.mn_Length=sizeof(*m);m->unit=unit;m->unitGeneration=ICU(unit)->inputGeneration;
        m->ie.ie_Class=IECLASS_RAWKEY;m->ie.ie_Code=code|(up?IECODE_UP_PREFIX:0);m->ie.ie_Qualifier=qual;
        ObtainSemaphore(&base->consoleTaskLock);PutMsg(base->consIHData.inputPort,(struct Message*)m);ReleaseSemaphore(&base->consoleTaskLock);
    }
    return 1;
}
static int zoom_event(UWORD code,UWORD qual){
 struct cdihMessage*m=AllocMem(sizeof(*m),MEMF_PUBLIC|MEMF_CLEAR);if(!m)return 0;
 m->msg.mn_Length=sizeof(*m);m->unit=unit;m->unitGeneration=ICU(unit)->inputGeneration;
 m->ie.ie_Class=IECLASS_RAWKEY;m->ie.ie_Code=code;m->ie.ie_Qualifier=qual;
 ObtainSemaphore(&base->consoleTaskLock);PutMsg(base->consIHData.inputPort,(struct Message*)m);ReleaseSemaphore(&base->consoleTaskLock);Delay(3);return 1;
}
static int found(void)
{
    for(int i=0;i<100;i++){
        if(window->Title&&strstr((const char*)window->Title,"[found;"))return 1;
        Delay(2);
    }
    return 0;
}
static int key(UWORD code,UWORD qual){int ok=enqueue(code,qual);Delay(3);return ok;}
static int status(const char *text){for(int i=0;i<100;i++){if(window->Title&&strstr((char*)window->Title,text))return 1;Delay(2);}return 0;}
static int clipboard(const char*bytes,ULONG size){
 int ok=0;struct IFFHandle *iff=AllocIFF();if(!iff)return 0;
 iff->iff_Stream=(IPTR)OpenClipboard(0);if(!iff->iff_Stream){FreeIFF(iff);return 0;}
 InitIFFasClip(iff);
 if(!OpenIFF(iff,IFFF_WRITE)){
  if(!PushChunk(iff,ID_FTXT,ID_FORM,IFFSIZE_UNKNOWN)){
   if(!PushChunk(iff,0,ID_CHRS,IFFSIZE_UNKNOWN)){ok=WriteChunkBytes(iff,(APTR)bytes,size)==(LONG)size;PopChunk(iff);}
   PopChunk(iff);
  }
  CloseIFF(iff);
 }
 CloseClipboard((struct ClipboardHandle*)iff->iff_Stream);FreeIFF(iff);return ok;
}
static int url_captured(const char *expected){char got[1024]={0};BPTR file=Open((CONST_STRPTR)"RAM:ConsoleNG-URL-Test",MODE_OLDFILE);if(!file)return 0;LONG count=Read(file,got,sizeof(got)-1);Close(file);return count==(LONG)strlen(expected)&&!memcmp(got,expected,count);}
static int clipboard_matches(const char *expected)
{
 int match=0;struct IFFHandle *h=AllocIFF();if(!h)return 0;
 h->iff_Stream=(IPTR)OpenClipboard(0);
 if(h->iff_Stream){
  InitIFFasClip(h);
  if(!OpenIFF(h,IFFF_READ)){
   StopChunk(h,MAKE_ID('F','T','X','T'),MAKE_ID('C','H','R','S'));
   if(!ParseIFF(h,IFFPARSE_SCAN)){
    struct ContextNode *c=CurrentChunk(h);ULONG length=strlen(expected),offset=0;char text[256];
    if(c&&c->cn_Size==(LONG)length){
     match=1;
     while(offset<length){
      ULONG count=length-offset;if(count>sizeof(text))count=sizeof(text);
      if(ReadChunkBytes(h,text,count)!=(LONG)count||memcmp(text,expected+offset,count)){match=0;break;}
      offset+=count;
     }
    }
   }
   CloseIFF(h);
  }
  CloseClipboard((void*)h->iff_Stream);
 }
 FreeIFF(h);return match;
}

#define CHECK(c) do{if(!(c)){printf("SEARCH_PASTE_FAIL line=%d title=%s\n",__LINE__,window&&window->Title?(char*)window->Title:"none");fflush(stdout);Delay(500);goto out;}}while(0)
#define OPEN() key(0x23,IEQUALIFIER_CONTROL|IEQUALIFIER_LSHIFT)
#define PASTE() key(0x34,IEQUALIFIER_RCOMMAND)
#define CLOSE() key(RAWKEY_ESCAPE,0)
int main(int argc,char **argv){
 int rc=20;struct InfoData info;char got;
 IFFParseBase=OpenLibrary((CONST_STRPTR)"iffparse.library",0);CHECK(IFFParseBase);
 CHECK(FindPort((CONST_STRPTR)"ConClip.rendezvous"));
 stream=Open((CONST_STRPTR)"RGCON:",MODE_OLDFILE);CHECK(stream);struct FileHandle *fh=BADDR(stream);
 CHECK(DoPkt(fh->fh_Type,ACTION_DISK_INFO,(SIPTR)MKBADDR(&info),0,0,0,0));
 struct IOStdReq *io=(void*)(IPTR)info.id_InUse;CHECK(io&&io->io_Unit);
 unit=(Object*)io->io_Unit;base=(void*)io->io_Device;ConsoleDevice=io->io_Device;window=CU(unit)->cu_Window;CHECK(window);
 SetWindowTitles(window,(CONST_STRPTR)"ConsoleNG clipboard search",(CONST_STRPTR)-1);
 MoveWindow(window,30-window->LeftEdge,30-window->TopEdge);SizeWindow(window,730-window->Width,450-window->Height);Delay(50);ActivateWindow(window);Delay(10);
 CHECK(SetMode(stream,TRUE));
 CHECK(write_text("\x1b%G\x1b[?25l\x1b]50;DejaVuSansMono,16\7\x1b[2J\x1b[Hneedle target\r\nGreek \xce\xa9 target\r\nCaf\xc3\xa9 target\r\nfirst\r\nsecond\r\n"));

 CHECK(write_text("Digits \xd9\xa1\xd9\xa2 target\r\nSpace \xe2\x80\x83 target\r\n"));
 CHECK(clipboard("needle",6));CHECK(OPEN());CHECK(PASTE());CHECK(found());CHECK(status("Find: needle ["));CHECK(!WaitForChar(stream,20000));CHECK(CLOSE());

 /* C0 padding is removed by search normalization, so old provider truncation
  * would silently turn the oversized clipboard into the valid word needle. */
 {
  static char oversized[32768];memset(oversized,1,sizeof(oversized));memcpy(oversized,"needle",6);
  CHECK(clipboard(oversized,1024));CHECK(OPEN());CHECK(PASTE());CHECK(found());CHECK(status("Find: needle ["));CHECK(CLOSE());
  const ULONG sizes[]={1025,2048,32768};
  for(unsigned i=0;i<sizeof(sizes)/sizeof(sizes[0]);i++){
   CHECK(clipboard(oversized,sizes[i]));CHECK(OPEN());CHECK(key(0x36,0));CHECK(PASTE());
   CHECK(status("paste rejected"));Delay(30);CHECK(status("paste rejected"));CHECK(status("Find: n ["));CHECK(!WaitForChar(stream,20000));
   if(!i){puts("SEARCH_PASTE_SHOT rejected");fflush(stdout);Delay(100);CHECK(status("paste rejected"));}
   CHECK(key(RAWKEY_RETURN,0));CHECK(found());CHECK(CLOSE());
  }
  puts("SEARCH_PASTE_BOUNDARY_PASS exact1024 oversized1025/2048/32768 rejected without truncation");fflush(stdout);
 }
 CHECK(clipboard("eedle",5));CHECK(OPEN());CHECK(key(0x36,0));CHECK(key(0x34,IEQUALIFIER_CONTROL|IEQUALIFIER_LSHIFT));CHECK(found());CHECK(status("Find: needle ["));CHECK(CLOSE());
 CHECK(clipboard("first\r\nsecond",13));CHECK(OPEN());CHECK(PASTE());CHECK(found());CHECK(status("Find: first\\nsecond ["));CHECK(CLOSE());
 CHECK(clipboard("\xce\xa9",2));CHECK(OPEN());CHECK(PASTE());CHECK(found());
 puts("SEARCH_PASTE_SHOT unicode");fflush(stdout);Delay(100);CHECK(CLOSE());
 CHECK(write_text("\x1b%@"));CHECK(clipboard("\xe9",1));CHECK(OPEN());CHECK(PASTE());CHECK(found());CHECK(CLOSE());CHECK(write_text("\x1b%G"));
 CHECK(clipboard("\xc0\xaf",2));CHECK(OPEN());CHECK(key(0x36,0));CHECK(PASTE());CHECK(status("paste rejected"));CHECK(status("Find: n ["));CHECK(!WaitForChar(stream,20000));CHECK(CLOSE());
 CHECK(clipboard("n\033\003eedle",8));CHECK(OPEN());CHECK(PASTE());CHECK(found());CHECK(status("Find: needle ["));CHECK(CLOSE());
 CHECK(clipboard("needle",6));CHECK(OPEN());
 ObtainSemaphore(&base->consoleTaskLock);
 int queued=enqueue(0x34,IEQUALIFIER_RCOMMAND)&&enqueue(RAWKEY_ESCAPE,0)&&enqueue(0x23,IEQUALIFIER_CONTROL|IEQUALIFIER_LSHIFT)&&enqueue(0x32,0);
 ReleaseSemaphore(&base->consoleTaskLock);CHECK(queued);Delay(20);
 CHECK(status("Find: x [not found]"));CHECK(!WaitForChar(stream,20000));CHECK(CLOSE());
 CHECK(key(0x32,0));CHECK(WaitForChar(stream,5000000)&&Read(stream,&got,1)==1&&got=='x');CHECK(!WaitForChar(stream,20000));
 CHECK(clipboard("needle",6));CHECK(OPEN());CHECK(PASTE());CHECK(found());CHECK(!WaitForChar(stream,20000));

 CHECK(CLOSE());
 CHECK(clipboard("needle \xce\xa9\xe2\x80\x83",12));CHECK(OPEN());CHECK(PASTE());CHECK(status("not found"));
 CHECK(key(0x11,IEQUALIFIER_CONTROL));CHECK(status("Find: needle  ["));CHECK(found());CHECK(!WaitForChar(stream,20000));
 CHECK(key(RAWKEY_BACKSPACE,IEQUALIFIER_CONTROL));CHECK(status("Find:  ["));CHECK(!WaitForChar(stream,20000));
 CHECK(PASTE());CHECK(status("not found"));CHECK(key(0x16,IEQUALIFIER_CONTROL));CHECK(status("Find:  ["));CHECK(!WaitForChar(stream,20000));
 /* Queue a clear while ConClip still owns the reply: no stale resurrection. */
 ObtainSemaphore(&base->consoleTaskLock);
 queued=enqueue(0x34,IEQUALIFIER_RCOMMAND)&&enqueue(0x16,IEQUALIFIER_CONTROL);
 ReleaseSemaphore(&base->consoleTaskLock);CHECK(queued);Delay(20);CHECK(status("Find:  ["));CHECK(!WaitForChar(stream,20000));
 CHECK(CLOSE());CHECK(clipboard("needle",6));CHECK(OPEN());CHECK(PASTE());CHECK(found());

 CHECK(CLOSE());CHECK(clipboard("recallone",9));CHECK(OPEN());CHECK(PASTE());CHECK(status("not found"));CHECK(CLOSE());
 CHECK(clipboard("needle",6));CHECK(OPEN());CHECK(PASTE());CHECK(found());CHECK(CLOSE());CHECK(OPEN());CHECK(key(0x32,0));
 CHECK(key(RAWKEY_UP,0));CHECK(status("Find: needle ["));CHECK(found());
 CHECK(key(RAWKEY_UP,0));CHECK(status("Find: recallone ["));
 CHECK(key(RAWKEY_DOWN,0));CHECK(status("Find: needle ["));CHECK(found());
 CHECK(key(RAWKEY_DOWN,0));CHECK(status("Find: x ["));CHECK(!WaitForChar(stream,20000));
 CHECK(key(0x16,IEQUALIFIER_CONTROL));CHECK(key(RAWKEY_UP,0));CHECK(found());CHECK(status("Find: needle ["));
 CHECK(CLOSE());
 prefsPort=CreateMsgPort();CHECK(prefsPort);prefsIO=(void*)CreateIORequest(prefsPort,sizeof(*prefsIO));CHECK(prefsIO);
 prefsIO->io_Device=io->io_Device;prefsIO->io_Unit=io->io_Unit;
 struct ConsolengBindingPrefs original,bindings;CHECK(prefs(CD_GETBINDINGPREFS,&original));bindings=original;
 bindings.bp_Entries[bindings.bp_Count++]=(struct ConsolengKeyBinding){'v',CONSOLENG_BIND_CTRL|CONSOLENG_BIND_SHIFT,CONSOLENG_BIND_COPY};
 CHECK(prefs(CD_SETBINDINGPREFS,&bindings));CHECK(clipboard("needle",6));CHECK(OPEN());
 CHECK(key(0x34,IEQUALIFIER_CONTROL|IEQUALIFIER_LSHIFT));Delay(20);CHECK(status("Find:  ["));CHECK(!WaitForChar(stream,20000));
 CHECK(PASTE());CHECK(found());CHECK(CLOSE());bindings=original;
 bindings.bp_Version=2;bindings.bp_Entries[1]=(struct ConsolengKeyBinding){CONSOLENG_BIND_RAW+RAWKEY_F1,0,CONSOLENG_BIND_PASTE};
 CHECK(prefs(CD_SETBINDINGPREFS,&bindings));CHECK(clipboard("needle",6));CHECK(OPEN());
 CHECK(PASTE());CHECK(key(0x34,IEQUALIFIER_CONTROL|IEQUALIFIER_LSHIFT));Delay(20);CHECK(status("Find:  ["));CHECK(!WaitForChar(stream,20000));
 CHECK(key(RAWKEY_F1,0));CHECK(found());CHECK(status("Find: needle ["));CHECK(!WaitForChar(stream,20000));CHECK(CLOSE());
 bindings.bp_Entries[1]=(struct ConsolengKeyBinding){'p',5,CONSOLENG_BIND_PASTE};CHECK(prefs(CD_SETBINDINGPREFS,&bindings));
 CHECK(OPEN());CHECK(key(RAWKEY_P,IEQUALIFIER_CONTROL|IEQUALIFIER_LSHIFT));CHECK(found());CHECK(status("Find: needle ["));CHECK(!WaitForChar(stream,20000));CHECK(CLOSE());
 bindings.bp_Entries[1].kb_Action=CONSOLENG_BIND_COPY;CHECK(prefs(CD_SETBINDINGPREFS,&bindings));
 CHECK(OPEN());CHECK(PASTE());CHECK(key(0x34,IEQUALIFIER_CONTROL|IEQUALIFIER_LSHIFT));CHECK(key(RAWKEY_P,IEQUALIFIER_CONTROL|IEQUALIFIER_LSHIFT));Delay(20);CHECK(status("Find:  ["));CHECK(!WaitForChar(stream,20000));CHECK(CLOSE());
 CHECK(prefs(CD_SETBINDINGPREFS,&original));CHECK(OPEN());CHECK(PASTE());CHECK(found());
 puts("SEARCH_BINDINGS_PASS explicit alias override, special/logical paste remapping, removed shortcuts and default alias restoration");fflush(stdout);

 CHECK(CLOSE());CHECK(clipboard("n(e|a)+dle",10));CHECK(OPEN());CHECK(PASTE());CHECK(status("not found"));
 CHECK(key(0x13,IEQUALIFIER_CONTROL));CHECK(found());CHECK(status("Regex: n(e|a)+dle ["));CHECK(!WaitForChar(stream,20000));
 CHECK(key(RAWKEY_RETURN,0));CHECK(found());CHECK(key(RAWKEY_RETURN,IEQUALIFIER_LSHIFT));CHECK(found());
 CHECK(key(0x16,IEQUALIFIER_CONTROL));CHECK(clipboard("[",1));CHECK(PASTE());CHECK(status("invalid or unsupported regex"));
 CHECK(key(0x16,IEQUALIFIER_CONTROL));CHECK(clipboard("Greek .* target",15));CHECK(PASTE());CHECK(found());
 CHECK(key(0x13,IEQUALIFIER_CONTROL));CHECK(status("Find: Greek .* target ["));CHECK(status("not found"));
 CHECK(key(0x13,IEQUALIFIER_CONTROL));CHECK(found());CHECK(!WaitForChar(stream,20000));

 {
  static const struct {const char *pattern,*expected;} cases[]={
   {"n(e|a){2}dle","[found;"}, {"^Greek .{1,2} target$","[found;"},
   {"n(e|a){2,}dle","[found;"}, {"(x){0}needle","[found;"},
   {"(a{128}){128}","invalid or unsupported regex"},
   {"[abc]{256}","invalid or unsupported regex"},
   {"((a{256}){0}){256}","invalid or unsupported regex"}, {"needle{2}","not found"},
   {"^Greek .{1} target$","[found;"}
  };
  for(unsigned i=0;i<sizeof(cases)/sizeof(*cases);i++){
   CHECK(key(0x16,IEQUALIFIER_CONTROL));CHECK(clipboard(cases[i].pattern,strlen(cases[i].pattern)));
   CHECK(PASTE());CHECK(status(cases[i].expected));CHECK(!WaitForChar(stream,20000));
  }
 }
 puts("SEARCH_REGEX_COUNTS_PASS exact, bounded, unbounded, zero repetition and expansion rejection");fflush(stdout);

 {
  static const struct {const char *pattern,*expected;} cases[]={
   {"^Digits \\d{2} target$","[found;"}, {"^Digits [\\d]{2} target$","[found;"},
   {"^Digits \\D{2} target$","not found"}, {"^Space\\s+target$","[found;"},
   {"^Greek \\D target$","[found;"}, {"^Greek \\W target$","not found"},
   {"^Greek [\\d\\w] target$","[found;"}, {"^Greek [^\\w] target$","not found"},
   {"^Greek \\w+ target$","[found;"}
  };
  for(unsigned i=0;i<sizeof(cases)/sizeof(*cases);i++){
   CHECK(key(0x16,IEQUALIFIER_CONTROL));CHECK(clipboard(cases[i].pattern,strlen(cases[i].pattern)));
   CHECK(PASTE());CHECK(status(cases[i].expected));CHECK(!WaitForChar(stream,20000));
  }
 }
 puts("SEARCH_REGEX_UNICODE_PASS Greek word, Arabic-Indic digits, Unicode space, complements and class unions");fflush(stdout);

 {
  static const struct {const char *pattern,*expected;} cases[]={
   {"^greek \xcf\x89 target$","[found;"},
   {"^Greek \xcf\x89 target$","not found"},
   {"^greek [\xcf\x89] target$","[found;"},
   {"^greek [^\xcf\x89] target$","not found"},
   {"^NEEDLE target$","not found"}, {"^needle target$","[found;"},
   {"^greek \xce\xa9 target$","not found"},
   {"^Greek \xce\xa9 target$","[found;"},
   {"^greek \xcf\x89 target$","[found;"}
  };
  for(unsigned i=0;i<sizeof(cases)/sizeof(*cases);i++){
   CHECK(key(0x16,IEQUALIFIER_CONTROL));CHECK(clipboard(cases[i].pattern,strlen(cases[i].pattern)));
   CHECK(PASTE());CHECK(status(cases[i].expected));CHECK(!WaitForChar(stream,20000));
  }
 }
 CHECK(status("Regex: ^greek \\u{03C9} target$ [found;"));
 puts("SEARCH_TITLE_PASS Unicode scalar identity and explicit newline presentation");fflush(stdout);

 {
  static const struct {const char *pattern,*expected;} cases[]={
   {"\\bneedle\\b","[found;"}, {"\\bneed\\b","not found"},
   {"n\\Beedle","[found;"}, {"n\\beedle","not found"},
   {"\\Bneedle","not found"}, {"target\\b","[found;"},
   {"^Greek \\b\xce\xa9\\b target$","[found;"},
   {"^greek \\b\xcf\x89\\b target$","[found;"}
  };
  for(unsigned i=0;i<sizeof(cases)/sizeof(*cases);i++){
   CHECK(key(0x16,IEQUALIFIER_CONTROL));CHECK(clipboard(cases[i].pattern,strlen(cases[i].pattern)));
   CHECK(PASTE());CHECK(status(cases[i].expected));CHECK(!WaitForChar(stream,20000));
  }
 }
 puts("SEARCH_BOUNDARY_PASS whole-word, non-boundary, endpoints and Greek smartcase");fflush(stdout);

 CHECK(CLOSE());CHECK(clipboard("sentinel",8));CHECK(key(0x33,IEQUALIFIER_RCOMMAND));
 {int match=0;for(int i=0;i<200&&!match;i++){match=clipboard_matches("Greek \xce\xa9 target");if(!match)Delay(2);}CHECK(match);}
 CHECK(!WaitForChar(stream,20000));
 puts("SEARCH_COPY_PASS Escape retains selection; Right-Amiga-C copies exact UTF8 match without extra newline");fflush(stdout);

 {
  static const struct {const char *text,*query,*expected;} cases[]={
   {"ab","a|ab","a"},{"ab","ab|a","ab"},{"abb","a|ab*","a"},
   {"ab","a?|ab","ab"},{"abab","(ab|a){1,2}","abab"},
   {"\xce\xa9\xe4\xb8\xad","\xce\xa9|\xce\xa9\xe4\xb8\xad","\xce\xa9"},
   {"aaa","a+?","a"},{"aaa","a*?","aaa"},{"aaab","a+?b","aaab"},
   {"aaabaaaab","a.*?b","aaab"},{"aaaa","a{1,3}?","a"},
   {"aaaa","a{0,3}?","aaa"},{"ababab","(ab){1,3}?","ab"},
   {"ababab","(ab){0,3}?","ababab"},
   {"\xce\xa9\xce\xa9\xe4\xb8\xad","\xce\xa9+?","\xce\xa9"},
   {"ab","(?i)AB","ab"},{"AB ab","(?-i)ab","ab"},
   {"aB AB ab","(?i:a)(?-i:b)","ab"},{"aB AB ab","(?-i:a)(?i:b)","aB"},
   {"abab","(?:ab){1,2}","abab"},
   {"\xcf\x89\xce\xa9","(?i:\xce\xa9)+","\xcf\x89\xce\xa9"},
   {"\xce\xa9\xcf\x89","(?-i:\xcf\x89)","\xcf\x89"},
   {"aaa","(?U)a+","a"},{"aaa","(?U)a+?","aaa"},
   {"aaaa","(?U)a{1,3}","a"},{"aaaa","(?U)a{1,3}?","aaa"},
   {"ababab","(?U:(ab){1,3})","ab"},
   {"abbb","(?U:a+)(?-U:b+)","abbb"},
   {"AAA","(?iU)a+","A"},{"aaa","(?U)a*","aaa"},
   {"cab\r\nab","\\Aab\\z","ab"},{"ab\r\nac","\\Aab","ab"},
   {"cab","ab\\z","ab"},{"abab","\\A(?:ab)+\\z","abab"},
   {"AB","(?i:\\Aab\\z)","AB"},{"ab","\\A.*\\z","ab"}
  };
  for(unsigned i=0;i<sizeof(cases)/sizeof(*cases);i++){
   CHECK(write_text("\x1b[3J\x1b[2J\x1b[H"));CHECK(write_text(cases[i].text));
   CHECK(write_text(i<6?
    "\r\nRegex priority: ordered alternatives, greedy repetition, nullable longest.":i<15?
    "\r\nLazy regex: shortest preferred span; nullable expressions keep reference policy.":i<22?
    "\r\nInline case flags: explicit overrides and nested scopes, including Unicode.":
    i<30?"\r\nUngreedy mode: scoped U defaults to shortest; ? restores greedy repetition.":
    "\r\nAbsolute anchors: whole logical lines, including groups and case scopes."));
   CHECK(OPEN());
   if(!strstr((char*)window->Title,"Regex:"))CHECK(key(0x13,IEQUALIFIER_CONTROL));
   CHECK(clipboard(cases[i].query,strlen(cases[i].query)));CHECK(PASTE());CHECK(found());
   if(!i){puts("SEARCH_PRIORITY_SHOT");fflush(stdout);Delay(100);}
   if(i==6){puts("SEARCH_LAZY_SHOT");fflush(stdout);Delay(100);}
   if(i==15){puts("SEARCH_FLAGS_SHOT");fflush(stdout);Delay(100);}
   if(i==22){puts("SEARCH_UNGREEDY_SHOT");fflush(stdout);Delay(100);}
   if(i==30){puts("SEARCH_ABSOLUTE_SHOT");fflush(stdout);Delay(100);}
   CHECK(CLOSE());CHECK(clipboard("sentinel",8));CHECK(key(0x33,IEQUALIFIER_RCOMMAND));
   int match=0;for(int j=0;j<200&&!match;j++){match=clipboard_matches(cases[i].expected);if(!match)Delay(2);}CHECK(match);
   CHECK(!WaitForChar(stream,20000));
  }
 }
 puts("SEARCH_PRIORITY_PASS ordered alternatives, greedy and nullable expressions, Unicode endpoints; exact native clipboard");fflush(stdout);
 puts("SEARCH_LAZY_PASS simple and counted lazy repetitions, required suffix, nullable policy and Greek text; exact native clipboard");fflush(stdout);
 puts("SEARCH_FLAGS_PASS explicit and scoped case overrides, mixed scopes, noncapturing groups and Greek text; exact native clipboard");fflush(stdout);
 puts("SEARCH_UNGREEDY_PASS global scoped and disabled U, suffix inversion, case combinations and nullable policy; exact native clipboard");fflush(stdout);
 puts("SEARCH_ABSOLUTE_PASS logical input start/end, groups, nullable and case-scoped anchors; exact native clipboard");fflush(stdout);

 /* Regex hard-break isolation does not change literal multiline searching. */
 {
  CHECK(write_text("\x1b[3J\x1b[2J\x1b[Ha\r\nb"));
  CHECK(OPEN());CHECK(clipboard("a[\\s\\S]*b",9));CHECK(PASTE());CHECK(status("not found"));CHECK(CLOSE());
  CHECK(OPEN());CHECK(key(0x13,IEQUALIFIER_CONTROL));CHECK(clipboard("a\nb",3));CHECK(PASTE());CHECK(found());CHECK(CLOSE());
  CHECK(clipboard("sentinel",8));CHECK(key(0x33,IEQUALIFIER_RCOMMAND));
  int match=0;for(int j=0;j<200&&!match;j++){match=clipboard_matches("a\rb");if(!match)Delay(2);}CHECK(match);
  char wrapped[512];ULONG columns=CHAR_XMAX(unit)+1;CHECK(columns>4&&columns<500);
  memset(wrapped,'x',columns-1);strcpy(wrapped+columns-1,"ab");
  CHECK(write_text("\x1b[3J\x1b[2J\x1b[H"));CHECK(write_text(wrapped));
  CHECK(OPEN());CHECK(key(0x13,IEQUALIFIER_CONTROL));CHECK(clipboard("a[\\s\\S]*b",9));CHECK(PASTE());CHECK(found());
  puts("SEARCH_LINES_SHOT");fflush(stdout);Delay(100);CHECK(CLOSE());
  CHECK(clipboard("sentinel",8));CHECK(key(0x33,IEQUALIFIER_RCOMMAND));
  match=0;for(int j=0;j<200&&!match;j++){match=clipboard_matches("ab");if(!match)Delay(2);}CHECK(match);
  CHECK(OPEN());CHECK(clipboard("\\Ax+ab\\z",8));CHECK(PASTE());CHECK(found());CHECK(CLOSE());
  CHECK(clipboard("sentinel",8));CHECK(key(0x33,IEQUALIFIER_RCOMMAND));
  match=0;for(int j=0;j<200&&!match;j++){match=clipboard_matches(wrapped);if(!match)Delay(2);}CHECK(match);
  CHECK(write_text("\x1b[3J\x1b[2J\x1b[Hax\r\nab"));
  CHECK(OPEN());CHECK(clipboard("^ab",3));CHECK(PASTE());CHECK(found());CHECK(CLOSE());
  CHECK(clipboard("sentinel",8));CHECK(key(0x33,IEQUALIFIER_RCOMMAND));
  match=0;for(int j=0;j<200&&!match;j++){match=clipboard_matches("ab");if(!match)Delay(2);}CHECK(match);
  CHECK(!WaitForChar(stream,20000));
 }
 puts("SEARCH_LINES_PASS hard-break isolation, soft-wrap continuity, later-line anchor and literal multiline preservation; exact native clipboard");fflush(stdout);

 CHECK(write_text("\x1b[2J\x1b[Hmatch first\r\nmatch second"));
 bindings=original;bindings.bp_Version=4;bindings.bp_Entries[3]=(struct ConsolengKeyBinding){CONSOLENG_BIND_RAW+RAWKEY_F2,0,CONSOLENG_BIND_SEARCH_BACKWARD};
 CHECK(prefs(CD_SETBINDINGPREFS,&bindings));
 for(int phase=0;phase<6;phase++){
  if(phase==3){bindings.bp_Version=5;bindings.bp_Entries[3].kb_Key=CONSOLENG_BIND_RAW|0x20;CHECK(prefs(CD_SETBINDINGPREFS,&bindings));}
  CHECK(clipboard("match.*",7));CHECK(key(phase<3?RAWKEY_F2:0x20,0));CHECK(PASTE());CHECK(found());
  if(phase%3){CHECK(key(RAWKEY_RETURN,0));CHECK(found());}
  if(phase%3==2){CHECK(key(RAWKEY_RETURN,IEQUALIFIER_LSHIFT));CHECK(found());}
  CHECK(CLOSE());CHECK(clipboard("sentinel",8));CHECK(key(0x33,IEQUALIFIER_RCOMMAND));
  int match=0;for(int i=0;i<200&&!match;i++){match=clipboard_matches(phase%3==1?"match first":"match second");if(!match)Delay(2);}CHECK(match);
  CHECK(!WaitForChar(stream,20000));
 }
 CHECK(prefs(CD_SETBINDINGPREFS,&original));
 bindings=original;bindings.bp_Version=6;bindings.bp_Entries[3]=(struct ConsolengKeyBinding){CONSOLENG_BIND_RAW+RAWKEY_F2,0,CONSOLENG_BIND_NAVIGATE};
 CHECK(prefs(CD_SETBINDINGPREFS,&bindings));
 /* Deliberately leave the application cursor away from home for entry. */
 CHECK(write_text("\x1b[3J\x1b[2J\x1b[Hfirst\r\nabcde\x1b[2;3H"));
 CHECK(key(RAWKEY_F2,0));CHECK(status("Navigate:"));Delay(10);CHECK(key(0x34,0));
 CHECK(clipboard("sentinel",8));CHECK(key(0x15,0));
 {int match=0;for(int i=0;i<200&&!match;i++){match=clipboard_matches("c");if(!match)Delay(2);}CHECK(match);}
 puts("NAVIGATION_ENTRY_SHOT");fflush(stdout);Delay(100);CHECK(key(0x17,0));
 /* Blank row allocation must keep the real grid position. Move up into a
  * retained row to expose both the starting row and the preserved column. */
 CHECK(write_text("\x1b[3J\x1b[2J\x1b[Habcde\x1b[3;3H"));
 CHECK(key(RAWKEY_F2,0));CHECK(status("Navigate:"));Delay(10);
 CHECK(key(RAWKEY_UP,0));CHECK(key(RAWKEY_UP,0));Delay(10);CHECK(key(0x34,0));
 CHECK(clipboard("sentinel",8));CHECK(key(0x15,0));
 {int match=0;for(int i=0;i<200&&!match;i++){match=clipboard_matches("c");if(!match)Delay(2);}CHECK(match);}
 CHECK(key(0x17,0));
 {ULONG height=CHAR_YMAX(unit)+1;char line[16];
  CHECK(write_text("\x1b[3J\x1b[2J\x1b[H"));
  for(ULONG row=0;row<height*3;row++){snprintf(line,sizeof(line),"%c%s",(int)('A'+row%26),row+1<height*3?"\r\n":"");CHECK(write_text(line));}
  CHECK(key(RAWKEY_F2,0));CHECK(status("Navigate:"));Delay(10);
  CHECK(key(0x24,0));Delay(20);CHECK(key(0x33,IEQUALIFIER_CONTROL));
  CHECK(key(RAWKEY_F2,0));CHECK(status("Navigate:"));Delay(10);CHECK(key(0x34,0));
  CHECK(clipboard("sentinel",8));CHECK(key(0x15,0));
  int match=0;for(int i=0;i<200&&!match;i++){match=clipboard_matches("A");if(!match)Delay(2);}CHECK(match);
  CHECK(key(0x17,0));
 }
 puts("NAVIGATION_ENTRY_PASS visible cursor, blank row, offscreen history and exact native clipboard");fflush(stdout);
 for(unsigned mode=0;mode<3;mode++){
  ULONG height=CHAR_YMAX(unit)+1;char line[16];
  CHECK(write_text("\x1b[3J\x1b[2J\x1b[H"));
  for(ULONG row=0;row<height*3;row++){snprintf(line,sizeof(line),"%c%s",(int)('A'+row%26),row+1<height*3?"\r\n":"");CHECK(write_text(line));}
  CHECK(write_text("\x1b[D"));CHECK(key(RAWKEY_F2,0));CHECK(status("Navigate:"));Delay(10);
  CHECK(key(0x24,0));Delay(20);
  if(!mode)CHECK(key(0x17,0));else if(mode==1)CHECK(key(RAWKEY_F2,0));else CHECK(key(0x33,IEQUALIFIER_CONTROL));
  CHECK(!WaitForChar(stream,20000));
  CHECK(key(RAWKEY_F2,0));CHECK(status("Navigate:"));Delay(10);CHECK(key(0x34,0));
  CHECK(clipboard("sentinel",8));CHECK(key(0x15,0));
  char expected[2]={(char)(mode==2?'A':'A'+(height*3-1)%26),0};int match=0;
  for(int i=0;i<200&&!match;i++){match=clipboard_matches(expected);if(!match)Delay(2);}CHECK(match);
  if(!mode){puts("NAVIGATION_EXIT_SHOT");fflush(stdout);Delay(100);}
  CHECK(key(0x17,0));
 }
 puts("NAVIGATION_EXIT_PASS insert and toggle restore live view; control C retains history; exact clipboard");fflush(stdout);
 {
  static char long_word[6001];memset(long_word,'a',sizeof(long_word)-1);
  CHECK(write_text("\x1b[3J\x1b[2J\x1b[H"));CHECK(write_text(long_word));
  CHECK(key(RAWKEY_F2,0));CHECK(status("Navigate:"));CHECK(key(0x24,0));Delay(20);
  CHECK(clipboard("sentinel",8));CHECK(enqueue(0x34,IEQUALIFIER_LALT));CHECK(enqueue(0x15,0));
  CHECK(status("copying selection"));
  int match=0;for(int i=0;i<500&&!match;i++){match=clipboard_matches(long_word);if(!match)Delay(2);}CHECK(match);
  puts("NAVIGATION_COPY_SHOT");fflush(stdout);Delay(100);
  CHECK(key(0x17,0));CHECK(!WaitForChar(stream,20000));
 }
 puts("NAVIGATION_COPY_PASS queued semantic selection copies all 6000 wrapped bytes through native clipboard");fflush(stdout);

 for(unsigned reverse=0;reverse<2;reverse++){
  CHECK(write_text("\x1b[3J\x1b[2J\x1b[Hab1 ab2 ab3\x1b[1;5H"));
  CHECK(key(RAWKEY_F2,0));CHECK(status("Navigate:"));Delay(10);
  CHECK(key(0x3a,reverse?IEQUALIFIER_LSHIFT:0));CHECK(status(reverse?"Navigate ?: ":"Navigate /: "));
  CHECK(key(0x20,0));CHECK(key(0x35,0));Delay(20);
  if(!reverse){puts("NAVIGATION_QUERY_SHOT");fflush(stdout);Delay(100);}
  CHECK(key(RAWKEY_RETURN,0));Delay(20);
  CHECK(!strstr((char*)window->Title,"Navigate /:")&&!strstr((char*)window->Title,"Navigate ?:"));
  for(unsigned phase=0;phase<3;phase++){
   if(phase){CHECK(key(0x36,phase==2?IEQUALIFIER_LSHIFT:0));Delay(20);}
   CHECK(key(RAWKEY_RIGHT,0));CHECK(key(RAWKEY_RIGHT,0));CHECK(key(0x34,0));
   CHECK(clipboard("sentinel",8));CHECK(key(0x15,0));char expected[2]={(char)(phase==1?(reverse?'1':'3'):'2'),0};
   int match=0;for(int i=0;i<200&&!match;i++){match=clipboard_matches(expected);if(!match)Delay(2);}CHECK(match);
   CHECK(key(RAWKEY_LEFT,0));CHECK(key(RAWKEY_LEFT,0));
  }
  CHECK(key(0x3a,0));CHECK(key(0x35,0));Delay(20);CHECK(key(RAWKEY_ESCAPE,0));Delay(20);
  CHECK(key(RAWKEY_RIGHT,0));CHECK(key(RAWKEY_RIGHT,0));CHECK(key(0x34,0));CHECK(clipboard("sentinel",8));CHECK(key(0x15,0));
  int match=0;for(int i=0;i<200&&!match;i++){match=clipboard_matches("2");if(!match)Delay(2);}CHECK(match);
  CHECK(key(0x17,0));CHECK(!WaitForChar(stream,20000));
 }
 puts("NAVIGATION_QUERY_PASS forward backward query, confirm, n N direction, cancel origin and exact clipboard");fflush(stdout);
 for(unsigned reverse=0;reverse<2;reverse++)for(unsigned selected=0;selected<2;selected++){
  CHECK(write_text("\x1b[3J\x1b[2J\x1b[Hab 1 ab 2 ab 3\x1b[1;6H"));
  CHECK(key(RAWKEY_F2,0));CHECK(status("Navigate:"));Delay(10);
  if(selected){CHECK(key(0x34,0));Delay(10);}
  CHECK(key(reverse?3:8,IEQUALIFIER_LSHIFT));Delay(30);
  for(unsigned phase=0;phase<3;phase++){
   if(phase){CHECK(key(0x36,phase==2?IEQUALIFIER_LSHIFT:0));Delay(20);}
   CHECK(key(RAWKEY_RIGHT,0));CHECK(key(RAWKEY_RIGHT,0));CHECK(key(RAWKEY_RIGHT,0));CHECK(key(0x34,0));
   CHECK(clipboard("sentinel",8));CHECK(key(0x15,0));
   char expected[2]={(char)(phase==1?(reverse?'3':'1'):(reverse?'1':'3')),0};
   int match=0;for(int i=0;i<200&&!match;i++){match=clipboard_matches(expected);if(!match)Delay(2);}CHECK(match);
   CHECK(key(RAWKEY_LEFT,0));CHECK(key(RAWKEY_LEFT,0));CHECK(key(RAWKEY_LEFT,0));
  }
  if(!reverse&&!selected){puts("NAVIGATION_SEED_SHOT");fflush(stdout);Delay(100);}
  CHECK(key(0x17,0));CHECK(!WaitForChar(stream,20000));
 }
 puts("NAVIGATION_SEED_PASS word and selected text, forward backward skip, n N wrap and exact clipboard");fflush(stdout);
 for(unsigned reverse=0;reverse<2;reverse++){
  CHECK(write_text("\x1b[3J\x1b[2J\x1b[Hab 1 ab 2 ab 3\x1b[H"));
  CHECK(key(RAWKEY_F2,0));CHECK(status("Navigate:"));Delay(10);
  CHECK(key(0x34,IEQUALIFIER_CONTROL));CHECK(key(RAWKEY_RIGHT,0));CHECK(key(RAWKEY_RIGHT,0));Delay(10);
  CHECK(key(reverse?3:8,IEQUALIFIER_LSHIFT));Delay(30);
  if(!reverse){puts("NAVIGATION_ANCHOR_SHOT");fflush(stdout);Delay(100);}
  CHECK(key(RAWKEY_RIGHT,0));CHECK(key(RAWKEY_RIGHT,0));CHECK(key(RAWKEY_RIGHT,0));CHECK(key(0x34,0));
  CHECK(clipboard("sentinel",8));CHECK(key(0x15,0));
  int match=0;for(int i=0;i<200&&!match;i++){match=clipboard_matches(reverse?"1":"3");if(!match)Delay(2);}CHECK(match);
  CHECK(key(0x17,0));CHECK(!WaitForChar(stream,20000));
 }
 puts("NAVIGATION_ANCHOR_PASS trimmed rectangle uses original cursor match end, both directions and exact clipboard");fflush(stdout);

 /* Discriminate actual reverse matching by copying a cell relative to the
  * resulting cursor. Merely copying "aba" would hide the overlap defect. */
 {
  const char *texts[]={"ababaZ","aaaZ","aabZ","aaaZ","aaaaZ","AaZ","AaZ","aaaZ","aaaZ"};
  const char *queries[]={"aba","a*","a+|ab","a+?","a{1,3}?","(?-i:a)","(?i)A","(?U)a+","(?U)a+?"};
  const unsigned origin[]={6,4,4,4,5,3,3,4,4},moves[]={3,0,2,1,1,1,1,1,3};
  const char *expected[]={"Z","a","Z","Z","Z","Z","Z","Z","Z"};
  for(unsigned i=0;i<9;i++){
   char position[32];sprintf(position,"\x1b[1;%uH",origin[i]);
   CHECK(write_text("\x1b[3J\x1b[2J\x1b[H"));CHECK(write_text(texts[i]));
   CHECK(write_text("\r\nBackward regex search: overlap, greedy spans and empty-match filtering."));
   CHECK(write_text(position));CHECK(key(RAWKEY_F2,0));CHECK(status("Navigate:"));Delay(10);
   CHECK(key(0x3a,IEQUALIFIER_LSHIFT));CHECK(clipboard(queries[i],strlen(queries[i])));
   CHECK(PASTE());Delay(30);
   if(!i){puts("NAVIGATION_REVERSE_SHOT");fflush(stdout);Delay(100);}
   CHECK(key(RAWKEY_RETURN,0));Delay(20);
   for(unsigned j=0;j<moves[i];j++)CHECK(key(RAWKEY_RIGHT,0));
   CHECK(key(0x34,0));CHECK(clipboard("sentinel",8));CHECK(key(0x15,0));
   int match=0;for(int j=0;j<200&&!match;j++){match=clipboard_matches(expected[i]);if(!match)Delay(2);}CHECK(match);
   CHECK(key(0x17,0));CHECK(!WaitForChar(stream,20000));
  }
  char wrapped[512];ULONG columns=CHAR_XMAX(unit)+1;CHECK(columns>4&&columns<500);
  memset(wrapped,'x',columns-2);strcpy(wrapped+columns-2,"\xe4\xb8\xad\xce\xa9" "b");
  CHECK(write_text("\x1b[3J\x1b[2J\x1b[H"));CHECK(write_text(wrapped));CHECK(write_text("\x1b[2;2H"));
  CHECK(key(RAWKEY_F2,0));CHECK(status("Navigate:"));Delay(10);
  CHECK(key(0x3a,IEQUALIFIER_LSHIFT));CHECK(clipboard("\xce\xa9",2));CHECK(PASTE());Delay(30);
  puts("NAVIGATION_REVERSE_WIDE_SHOT");fflush(stdout);Delay(100);
  CHECK(key(RAWKEY_RETURN,0));Delay(20);CHECK(key(0x34,0));CHECK(clipboard("sentinel",8));CHECK(key(0x15,0));
  int match=0;for(int j=0;j<200&&!match;j++){match=clipboard_matches("\xce\xa9");if(!match)Delay(2);}CHECK(match);
  CHECK(key(0x17,0));CHECK(!WaitForChar(stream,20000));
 }
 puts("NAVIGATION_REVERSE_PASS backward overlap, greedy nonempty span, directional alternative endpoint, lazy repetition and wide soft-wrap row; exact cursor-relative clipboard");fflush(stdout);


 for(int phase=0;phase<3;phase++){
  CHECK(write_text(phase==2?"\x1b[2J\x1b[HA\xe4\xb8\xad" "B":"\x1b[2J\x1b[Habcde\r\nuvwxyz"));
  CHECK(write_text("\x1b[H"));CHECK(key(RAWKEY_F2,0));CHECK(status("Navigate:"));Delay(10);
  if(phase==1){CHECK(key(RAWKEY_DOWN,0));Delay(10);}
  CHECK(key(0x34,0));Delay(10);CHECK(key(RAWKEY_RIGHT,0));Delay(10);CHECK(key(RAWKEY_RIGHT,0));Delay(10);
  CHECK(clipboard("sentinel",8));CHECK(key(0x15,0));
  int match=0;for(int i=0;i<200&&!match;i++){match=clipboard_matches(phase==0?"abc":phase==1?"uvw":"A\xe4\xb8\xad" "B");if(!match)Delay(2);}CHECK(match);
  CHECK(status("Navigate:"));CHECK(key(RAWKEY_ESCAPE,0));CHECK(status("Navigate:"));CHECK(key(0x17,0));
  CHECK(!WaitForChar(stream,20000));
 }
 for(int reverse=0;reverse<2;reverse++){
  CHECK(write_text("\x1b[3J\x1b[2J\x1b[Habcde\r\nuvwxyz"));CHECK(write_text("\x1b[H"));CHECK(key(RAWKEY_F2,0));CHECK(status("Navigate:"));Delay(10);
  if(reverse){CHECK(key(RAWKEY_DOWN,0));Delay(10);}
  CHECK(key(0x34,IEQUALIFIER_LSHIFT));Delay(10);CHECK(key(reverse?RAWKEY_UP:RAWKEY_DOWN,0));Delay(20);
  if(!reverse){puts("NAVIGATION_LINES_SHOT");fflush(stdout);Delay(100);}
  CHECK(clipboard("sentinel",8));CHECK(key(0x15,0));
  int match=0;for(int j=0;j<200&&!match;j++){match=clipboard_matches("abcde\ruvwxyz\r");if(!match)Delay(2);}CHECK(match);
  CHECK(key(0x17,0));CHECK(!WaitForChar(stream,20000));
 }
 {char wrapped[512],expected[512];ULONG columns=CHAR_XMAX(unit)+1;CHECK(columns>4&&columns<500);
  memset(wrapped,'a',columns-1);strcpy(wrapped+columns-1,"\xe4\xb8\xad" "b");strcpy(expected,wrapped);strcat(expected,"\r");
  CHECK(write_text("\x1b[3J\x1b[2J\x1b[H"));CHECK(write_text(wrapped));CHECK(write_text("\x1b[H"));CHECK(key(RAWKEY_F2,0));CHECK(status("Navigate:"));Delay(10);
  CHECK(key(RAWKEY_DOWN,0));Delay(10);CHECK(key(0x34,IEQUALIFIER_LSHIFT));Delay(20);
  CHECK(clipboard("sentinel",8));CHECK(key(0x15,0));
  int match=0;for(int j=0;j<200&&!match;j++){match=clipboard_matches(expected);if(!match)Delay(2);}CHECK(match);
  CHECK(key(0x17,0));CHECK(!WaitForChar(stream,20000));
 }
 CHECK(write_text("\x1b[3J\x1b[2J\x1b[H"));CHECK(write_text("\x1b[H"));CHECK(key(RAWKEY_F2,0));CHECK(status("Navigate:"));Delay(10);
 CHECK(key(0x34,IEQUALIFIER_LSHIFT));Delay(20);CHECK(clipboard("sentinel",8));CHECK(key(0x15,0));
 {int match=0;for(int j=0;j<200&&!match;j++){match=clipboard_matches("\r");if(!match)Delay(2);}CHECK(match);}
 CHECK(key(0x17,0));CHECK(!WaitForChar(stream,20000));
 puts("NAVIGATION_LINES_PASS whole logical lines both directions with exact final native line separator");fflush(stdout);
 for(int reverse=0;reverse<2;reverse++){
  CHECK(write_text("\x1b[3J\x1b[2J\x1b[Habcde\r\nuvwxyz\r\n12345"));CHECK(write_text("\x1b[H"));CHECK(key(RAWKEY_F2,0));CHECK(status("Navigate:"));Delay(10);
  CHECK(key(RAWKEY_RIGHT,0));Delay(10);
  if(reverse){CHECK(key(RAWKEY_RIGHT,0));Delay(10);CHECK(key(RAWKEY_DOWN,0));Delay(10);CHECK(key(RAWKEY_DOWN,0));Delay(10);}
  CHECK(key(0x34,IEQUALIFIER_CONTROL));Delay(10);
  CHECK(key(reverse?RAWKEY_UP:RAWKEY_DOWN,0));Delay(10);CHECK(key(reverse?RAWKEY_UP:RAWKEY_DOWN,0));Delay(10);
  CHECK(key(reverse?RAWKEY_LEFT:RAWKEY_RIGHT,0));Delay(20);
  if(!reverse){puts("NAVIGATION_BLOCK_SHOT");fflush(stdout);Delay(100);}
  CHECK(clipboard("sentinel",8));CHECK(key(0x15,0));
  int match=0;for(int j=0;j<200&&!match;j++){match=clipboard_matches("bc\rvw\r23");if(!match)Delay(2);}CHECK(match);
  CHECK(key(0x17,0));CHECK(!WaitForChar(stream,20000));
 }
 CHECK(write_text("\x1b[3J\x1b[2J\x1b[Habcde\r\nA\xe4\xb8\xad" "B\r\n12345"));CHECK(write_text("\x1b[H"));CHECK(key(RAWKEY_F2,0));CHECK(status("Navigate:"));Delay(10);
 for(int i=0;i<2;i++){CHECK(key(RAWKEY_RIGHT,0));Delay(10);}
 CHECK(key(0x34,IEQUALIFIER_CONTROL));Delay(10);for(int i=0;i<2;i++){CHECK(key(RAWKEY_DOWN,0));Delay(10);}
 CHECK(key(0x0a,0));Delay(10);for(int i=0;i<3;i++){CHECK(key(RAWKEY_RIGHT,0));Delay(10);}
 CHECK(clipboard("sentinel",8));CHECK(key(0x15,0));
 {int match=0;for(int j=0;j<200&&!match;j++){match=clipboard_matches("cd\r\xe4\xb8\xad" "B\r34");if(!match)Delay(2);}CHECK(match);}
 CHECK(key(0x17,0));CHECK(!WaitForChar(stream,20000));
 puts("NAVIGATION_BLOCK_PASS rectangular copy both directions and wide-cell edge expansion without final separator");fflush(stdout);
 {const char *texts[]={"alpha,beta gamma","alpha,beta gamma","(a(b)c)","(a(b)c)","(word","A\xe4\xb8\xad" "B tail"};
  const char *expected[]={"alpha","alpha,beta","(a(b)c)","(a(b)c)","(word","A\xe4\xb8\xad" "B"};
  const int moves[]={2,5,0,6,0,1};
  for(unsigned i=0;i<6;i++){
   CHECK(write_text("\x1b[3J\x1b[2J\x1b[H"));CHECK(write_text(texts[i]));CHECK(write_text("\x1b[H"));CHECK(key(RAWKEY_F2,0));CHECK(status("Navigate:"));Delay(10);
   for(int j=0;j<moves[i];j++){CHECK(key(RAWKEY_RIGHT,0));Delay(5);}
   CHECK(key(0x34,IEQUALIFIER_LALT));Delay(30);
   if(!i){puts("NAVIGATION_SEMANTIC_SHOT");fflush(stdout);Delay(100);}
   CHECK(clipboard("sentinel",8));CHECK(key(0x15,0));
   int match=0;for(int j=0;j<200&&!match;j++){match=clipboard_matches(expected[i]);if(!match)Delay(2);}CHECK(match);
   CHECK(key(0x17,0));CHECK(!WaitForChar(stream,20000));
  }
 }
 puts("NAVIGATION_SEMANTIC_PASS words delimiters bracket preference unmatched bracket and CJK exact clipboard");fflush(stdout);
 CHECK(write_text("\x1b[3J\x1b[2J\x1b[Habc\r\n\r\nuvwxyz"));CHECK(write_text("\x1b[H"));CHECK(key(RAWKEY_F2,0));CHECK(status("Navigate:"));Delay(10);
 for(int i=0;i<6;i++){CHECK(key(RAWKEY_RIGHT,0));Delay(5);}
 CHECK(key(RAWKEY_DOWN,0));Delay(10);CHECK(key(RAWKEY_DOWN,0));Delay(10);
 CHECK(key(RAWKEY_UP,0));Delay(10);CHECK(key(RAWKEY_UP,0));Delay(10);
 CHECK(key(0x34,0));Delay(10);for(int i=0;i<5;i++){CHECK(key(RAWKEY_LEFT,0));Delay(5);}
 puts("NAVIGATION_GRID_SHOT");fflush(stdout);Delay(100);
 CHECK(clipboard("sentinel",8));CHECK(key(0x15,0));
 {int match=0;for(int j=0;j<200&&!match;j++){match=clipboard_matches("bc");if(!match)Delay(2);}CHECK(match);}
 CHECK(key(0x17,0));CHECK(!WaitForChar(stream,20000));
 puts("NAVIGATION_GRID_PASS blank-column traversal across short and empty rows with exact native clipboard");fflush(stdout);
 CHECK(write_text("\x1b[3J\x1b[2J\x1b[H   alpha   "));CHECK(write_text("\x1b[H"));CHECK(key(RAWKEY_F2,0));CHECK(status("Navigate:"));Delay(10);
 CHECK(key(6,IEQUALIFIER_LSHIFT));Delay(10);CHECK(key(0x34,0));Delay(10);CHECK(key(4,IEQUALIFIER_LSHIFT));Delay(20);
 puts("NAVIGATION_ENDPOINTS_SHOT");fflush(stdout);Delay(100);
 CHECK(clipboard("sentinel",8));CHECK(key(0x15,0));
 {int match=0;for(int j=0;j<200&&!match;j++){match=clipboard_matches("alpha");if(!match)Delay(2);}CHECK(match);}
 CHECK(key(0x17,0));CHECK(!WaitForChar(stream,20000));
 {char text[512];ULONG columns=CHAR_XMAX(unit)+1;CHECK(columns>8&&columns<450);
  strcpy(text,"abc\r\n");memset(text+5,' ',columns-1);text[columns+4]='Z';text[columns+5]=0;
  CHECK(write_text("\x1b[3J\x1b[2J\x1b[H"));CHECK(write_text(text));CHECK(write_text("\x1b[H"));CHECK(key(RAWKEY_F2,0));CHECK(status("Navigate:"));Delay(10);
  CHECK(key(4,IEQUALIFIER_LSHIFT));Delay(10);CHECK(key(4,IEQUALIFIER_LSHIFT));Delay(10);CHECK(key(RAWKEY_DOWN,0));Delay(10);
  CHECK(key(0x34,0));Delay(10);CHECK(clipboard("sentinel",8));CHECK(key(0x15,0));
  int match=0;for(int j=0;j<200&&!match;j++){match=clipboard_matches("Z");if(!match)Delay(2);}CHECK(match);
  CHECK(key(0x17,0));CHECK(!WaitForChar(stream,20000));
  memset(text,'a',columns);text[0]=text[1]=' ';strcpy(text+columns,"  beta");
  CHECK(write_text("\x1b[3J\x1b[2J\x1b[H"));CHECK(write_text(text));CHECK(write_text("\x1b[H"));CHECK(key(RAWKEY_F2,0));CHECK(status("Navigate:"));Delay(10);
  CHECK(key(6,IEQUALIFIER_LSHIFT));Delay(10);CHECK(key(0x34,0));Delay(10);
  CHECK(key(4,IEQUALIFIER_LSHIFT));Delay(10);CHECK(key(4,IEQUALIFIER_LSHIFT));Delay(10);
  CHECK(clipboard("sentinel",8));CHECK(key(0x15,0));match=0;
  for(int j=0;j<200&&!match;j++){match=clipboard_matches(text+2);if(!match)Delay(2);}CHECK(match);
  CHECK(key(6,IEQUALIFIER_LSHIFT));Delay(10);CHECK(key(6,IEQUALIFIER_LSHIFT));Delay(10);CHECK(key(0x34,0));Delay(10);
  CHECK(clipboard("sentinel",8));CHECK(key(0x15,0));match=0;
  for(int j=0;j<200&&!match;j++){match=clipboard_matches("a");if(!match)Delay(2);}CHECK(match);
  CHECK(key(0x17,0));CHECK(!WaitForChar(stream,20000));
 }
 puts("NAVIGATION_ENDPOINTS_PASS occupied endpoints right margin wrapped line traversal and exact clipboard");fflush(stdout);
 CHECK(write_text("\x1b[3J\x1b[2J\x1b[Haxaxa"));CHECK(write_text("\x1b[H"));CHECK(key(RAWKEY_F2,0));CHECK(status("Navigate:"));Delay(10);
 {const int command[]={0x23,0x29,0x38,0x23,0x14,0x23,0x23,0x14};const int target[]={0x32,-1,-1,0x20,0x32,0x17,0x32,0x20};
  const int qual[]={0,0,0,IEQUALIFIER_LSHIFT,0,0,0,IEQUALIFIER_LSHIFT};const char *expected[]={"x","x","x","a","a","a","x","x"};
  for(unsigned i=0;i<8;i++){
   CHECK(key(command[i],qual[i]));Delay(10);if(target[i]>=0){CHECK(status("find character"));CHECK(key(target[i],0));Delay(10);}
   CHECK(key(0x34,0));Delay(20);if(!i){puts("NAVIGATION_FIND_SHOT");fflush(stdout);Delay(100);}
   CHECK(clipboard("sentinel",8));CHECK(key(0x15,0));int match=0;
   for(int j=0;j<200&&!match;j++){match=clipboard_matches(expected[i]);if(!match)Delay(2);}CHECK(match);
  }
 }
 CHECK(key(0x23,0));Delay(10);CHECK(key(RAWKEY_ESCAPE,0));Delay(10);CHECK(key(0x17,0));CHECK(!WaitForChar(stream,20000));
 puts("NAVIGATION_FIND_PASS forward reverse short repeat opposite missing target and cancel with exact clipboard");fflush(stdout);
 /* Opt-in requires isolated C:OpenURL probe; never launch a real browser by default. */
 if(argc>1 && !strcmp(argv[1],"--url-probe")){char wrapped[180]="https://example.org/";memset(wrapped+strlen(wrapped),'a',110);wrapped[129]=0;
  const char *uris[]={"https://example.org/path?x=1&y=two*three",wrapped,"mailto:demo@example.org?subject=AmiWest","https://example.org/explicit"};
  for(unsigned i=0;i<4;i++){
   DeleteFile((CONST_STRPTR)"RAM:ConsoleNG-URL-Test");CHECK(write_text("\x1b[3J\x1b[2J\x1b[H"));
   if(i==3)CHECK(write_text("\x1b]8;;https://example.org/explicit\x1b\\Link label\x1b]8;;\x1b\\"));else CHECK(write_text(uris[i]));
   CHECK(write_text("\x1b[H"));CHECK(key(RAWKEY_F2,0));CHECK(status("Navigate:"));Delay(20);
   if(i==1){CHECK(key(RAWKEY_DOWN,0));Delay(10);}
   if(!i){puts("NAVIGATION_ACTIONS_SHOT");fflush(stdout);Delay(100);}
   CHECK(key(i==3?RAWKEY_KP_ENTER:RAWKEY_RETURN,0));int captured=0;
   for(unsigned j=0;j<400&&!captured;j++){captured=url_captured(uris[i]);if(!captured)Delay(2);}CHECK(captured);
   CHECK(key(0x17,0));CHECK(!WaitForChar(stream,20000));
  }
  DeleteFile((CONST_STRPTR)"RAM:ConsoleNG-URL-Test");CHECK(write_text("\x1b[3J\x1b[2J\x1b[Hjavascript:alert(1)"));
  CHECK(write_text("\x1b[H"));CHECK(key(RAWKEY_F2,0));CHECK(status("Navigate:"));Delay(10);CHECK(key(RAWKEY_RETURN,0));Delay(30);
  BPTR file=Open((CONST_STRPTR)"RAM:ConsoleNG-URL-Test",MODE_OLDFILE);CHECK(!file);
  CHECK(key(0x17,0));CHECK(!WaitForChar(stream,20000));
 puts("NAVIGATION_OPEN_PASS exact web wrapped mailto OSC8 URIs and rejected unsupported scheme via isolated OpenURL command");fflush(stdout);
 }




 for(int words=0;words<2;words++){
  CHECK(write_text("\x1b[2J\x1b[Hab,cd ef"));CHECK(write_text("\x1b[H"));CHECK(key(RAWKEY_F2,0));CHECK(status("Navigate:"));Delay(10);
  CHECK(key(0x11,words?IEQUALIFIER_LSHIFT:0));Delay(10);
  if(!words){CHECK(key(0x11,0));Delay(10);}
  CHECK(key(0x34,0));Delay(10);CHECK(key(0x12,words?IEQUALIFIER_LSHIFT:0));Delay(10);
  CHECK(clipboard("sentinel",8));CHECK(key(0x15,0));
  int match=0;for(int i=0;i<200&&!match;i++){match=clipboard_matches(words?"ef":"cd");if(!match)Delay(2);}CHECK(match);
  CHECK(status("Navigate:"));CHECK(key(0x17,0));CHECK(!WaitForChar(stream,20000));
 }
 {char wrapped[512];ULONG columns=CHAR_XMAX(unit)+1;CHECK(columns>4&&columns<500);
  memset(wrapped,'a',columns-1);strcpy(wrapped+columns-1,"\xe4\xb8\xad" "b d");
  CHECK(write_text("\x1b[2J\x1b[H"));CHECK(write_text(wrapped));CHECK(write_text("\x1b[H"));CHECK(key(RAWKEY_F2,0));CHECK(status("Navigate:"));Delay(10);
  CHECK(key(0x11,0));Delay(20);CHECK(key(0x34,0));Delay(10);
  CHECK(clipboard("sentinel",8));CHECK(key(0x15,0));
  int match=0;for(int i=0;i<200&&!match;i++){match=clipboard_matches("d");if(!match)Delay(2);}CHECK(match);
  CHECK(key(0x17,0));CHECK(!WaitForChar(stream,20000));
 }
 {char wrapped[512];ULONG columns=CHAR_XMAX(unit)+1;CHECK(columns>4&&columns<500);
  memset(wrapped,'a',columns-1);strcpy(wrapped+columns-1,"\xe4\xb8\xad" "b");
  CHECK(write_text("\x1b[3J\x1b[2J\x1b[H"));CHECK(write_text(wrapped));CHECK(write_text("\x1b[H"));CHECK(key(RAWKEY_F2,0));CHECK(status("Navigate:"));Delay(10);
  CHECK(key(4,IEQUALIFIER_LSHIFT));Delay(10);
  for(int reverse=0;reverse<2;reverse++){
   CHECK(key(0x34,0));Delay(10);CHECK(key(reverse?RAWKEY_LEFT:RAWKEY_RIGHT,0));Delay(10);
   CHECK(key(reverse?RAWKEY_LEFT:RAWKEY_RIGHT,0));Delay(10);
   if(!reverse){puts("NAVIGATION_WRAP_SHOT");fflush(stdout);Delay(100);}
   CHECK(clipboard("sentinel",8));CHECK(key(0x15,0));
   int match=0;for(int j=0;j<200&&!match;j++){match=clipboard_matches("a\xe4\xb8\xad" "b");if(!match)Delay(2);}CHECK(match);
  }
  CHECK(key(0x17,0));CHECK(!WaitForChar(stream,20000));
 }
 puts("NAVIGATION_WRAP_PASS arrows cross soft wraps both directions with wide cells and exact clipboard");fflush(stdout);
 for(int reverse=0;reverse<2;reverse++){
  CHECK(write_text("\x1b[2J\x1b[H(a(b)c)"));CHECK(write_text("\x1b[H"));CHECK(key(RAWKEY_F2,0));CHECK(status("Navigate:"));Delay(10);
  if(reverse)for(int i=0;i<6;i++){CHECK(key(RAWKEY_RIGHT,0));Delay(5);}
  CHECK(key(0x34,0));Delay(10);CHECK(key(5,IEQUALIFIER_LSHIFT));Delay(20);
  if(!reverse){puts("NAVIGATION_STRUCTURAL_SHOT");fflush(stdout);Delay(100);}
  CHECK(clipboard("sentinel",8));CHECK(key(0x15,0));
  int match=0;for(int i=0;i<200&&!match;i++){match=clipboard_matches("(a(b)c)");if(!match)Delay(2);}CHECK(match);
  CHECK(key(0x17,0));CHECK(!WaitForChar(stream,20000));
 }
 CHECK(write_text("\x1b[2J\x1b[Ha\r\n\r\nbb\r\n\r\nc"));CHECK(write_text("\x1b[H"));CHECK(key(RAWKEY_F2,0));CHECK(status("Navigate:"));Delay(10);
 CHECK(key(0x1b,IEQUALIFIER_LSHIFT));Delay(20);CHECK(key(RAWKEY_DOWN,0));Delay(10);
 CHECK(key(0x34,0));Delay(10);CHECK(key(RAWKEY_RIGHT,0));Delay(10);CHECK(clipboard("sentinel",8));CHECK(key(0x15,0));
 {int match=0;for(int i=0;i<200&&!match;i++){match=clipboard_matches("bb");if(!match)Delay(2);}CHECK(match);}
 CHECK(key(0x17,0));CHECK(!WaitForChar(stream,20000));
 {ULONG height=CHAR_YMAX(unit)+1;char screen[256];
  CHECK(height>=4);snprintf(screen,sizeof(screen),"\x1b[2J\x1b[HT\x1b[%lu;1H  M\x1b[%lu;1H  L",(unsigned long)(height/2),(unsigned long)height);
  CHECK(write_text(screen));CHECK(write_text("\x1b[H"));CHECK(key(RAWKEY_F2,0));CHECK(status("Navigate:"));Delay(10);
  const UWORD motions[]={0x37,0x28,0x25};const char *expected[]={"M","L","T"};
  for(unsigned i=0;i<3;i++){
   CHECK(key(motions[i],IEQUALIFIER_LSHIFT));Delay(20);CHECK(key(0x34,0));Delay(10);
   if(!i){puts("NAVIGATION_VIEWPORT_SHOT");fflush(stdout);Delay(100);}
   CHECK(clipboard("sentinel",8));CHECK(key(0x15,0));
   int match=0;for(int j=0;j<200&&!match;j++){match=clipboard_matches(expected[i]);if(!match)Delay(2);}CHECK(match);
  }
  CHECK(key(0x17,0));CHECK(!WaitForChar(stream,20000));
 }
 {ULONG height=CHAR_YMAX(unit)+1;char line[16];
  CHECK(write_text("\x1b[3J\x1b[2J\x1b[H"));
  for(ULONG row=0;row<height*3;row++){
   snprintf(line,sizeof(line),"  %c%s",(int)('A'+row%26),row+1<height*3?"\r\n":"");CHECK(write_text(line));
  }
  CHECK(write_text("\x1b[H"));CHECK(key(RAWKEY_F2,0));CHECK(status("Navigate:"));Delay(20);
  const UWORD motions[]={0x35,0x16,0x23,0x22,0x12,0x15};
  ULONG targets[]={height,height-height/2,height*2-height/2,height*2,height*2,height*2};
  for(unsigned i=0;i<6;i++){
   CHECK(key(motions[i],IEQUALIFIER_CONTROL));Delay(20);CHECK(key(0x34,0));Delay(10);
   if(!i){puts("NAVIGATION_PAGES_SHOT");fflush(stdout);Delay(100);}
   CHECK(clipboard("sentinel",8));CHECK(key(0x15,0));char expected[2]={(char)('A'+targets[i]%26),0};
   int match=0;for(int j=0;j<200&&!match;j++){match=clipboard_matches(expected);if(!match)Delay(2);}CHECK(match);
  }
  CHECK(key(0x17,0));CHECK(!WaitForChar(stream,20000));
 }
 {ULONG height=CHAR_YMAX(unit)+1;char line[16];
  CHECK(write_text("\x1b[3J\x1b[2J\x1b[H"));
  for(ULONG row=0;row<height*3;row++){snprintf(line,sizeof(line),"  %c%s",(int)('A'+row%26),row+1<height*3?"\r\n":"");CHECK(write_text(line));}
  CHECK(write_text("\x1b[H"));CHECK(key(RAWKEY_F2,0));CHECK(status("Navigate:"));Delay(10);CHECK(key(0x24,IEQUALIFIER_LSHIFT));Delay(10);
  CHECK(key(0x35,IEQUALIFIER_CONTROL));Delay(10);CHECK(key(0x31,0));Delay(20);
  puts("NAVIGATION_CENTER_SHOT");fflush(stdout);Delay(100);
  CHECK(key(0x25,IEQUALIFIER_LSHIFT));Delay(20);CHECK(key(0x34,0));Delay(10);CHECK(clipboard("sentinel",8));CHECK(key(0x15,0));
  char expected[2]={(char)('A'+(height*2-height/2)%26),0};int match=0;
  for(int j=0;j<200&&!match;j++){match=clipboard_matches(expected);if(!match)Delay(2);}CHECK(match);
  CHECK(key(0x17,0));CHECK(!WaitForChar(stream,20000));
 }
 puts("NAVIGATION_ACTIONS_PASS centered viewport with exact native clipboard");fflush(stdout);
 puts("NAVIGATION_PAGES_PASS full half and line scroll in retained history with exact native clipboard");fflush(stdout);
 puts("NAVIGATION_VIEWPORT_PASS H M L first occupied cells and exact native clipboard");fflush(stdout);
 puts("NAVIGATION_STRUCTURAL_PASS nested brackets both directions, paragraph jump and exact native clipboard");fflush(stdout);
 puts("NAVIGATION_WORD_PASS semantic and whitespace word selection with exact clipboard");fflush(stdout);
 CHECK(write_text("\x1b[H"));CHECK(key(RAWKEY_F2,0));CHECK(status("Navigate:"));CHECK(write_text("\r\nNavigation cancelled by output"));
 CHECK(key(0x20,0));CHECK(WaitForChar(stream,500000)&&Read(stream,&got,1)==1&&got=='a');
 CHECK(prefs(CD_SETBINDINGPREFS,&original));
 CHECK(key(0x40,IEQUALIFIER_CONTROL|IEQUALIFIER_LSHIFT));CHECK(status("Navigate:"));
 CHECK(key(0x33,IEQUALIFIER_CONTROL));CHECK(!WaitForChar(stream,20000));
 puts("NAVIGATION_PASS arrows, character selection, wide cells, y native copy, input isolation and output cancellation");fflush(stdout);
 puts("SEARCH_PHYSICAL_PASS raw character binding, no input leakage, reverse search and exact clipboard");fflush(stdout);
 puts("SEARCH_BACKWARD_PASS configurable action, last match, Enter previous, Shift-Enter next, exact native clipboard");fflush(stdout);
 puts("SEARCH_REGEX_CASE_PASS Unicode fold, classes, negation and raw-query smartcase");fflush(stdout);
 puts("SEARCH_REGEX_PASS toggle, grouping, alternation, repetition, Unicode dot, invalid syntax, next/previous and input isolation");fflush(stdout);
 CHECK(write_text("\x1b[3J\x1b[2J\x1b[Hhex A a \xce\xa9 \xcf\x89 ababa"));
 CHECK(OPEN());if(!strstr((char*)window->Title,"Regex:"))CHECK(key(0x13,IEQUALIFIER_CONTROL));
 const char *hexqueries[]={"\\x41","(?-i)\\u0061","\\u03A9","(?i)\\U000003A9","[\\x61-\\x62]{3}","(?-i)\\u{03c9}","\\x{3a9}"};
 const char *hexmatches[]={"A","a","\xce\xa9","\xce\xa9","aba","\xcf\x89","\xce\xa9"};
 for(unsigned h=0;h<7;h++){
  if(h){CHECK(OPEN());}
  CHECK(clipboard(hexqueries[h],strlen(hexqueries[h])));CHECK(PASTE());CHECK(found());CHECK(CLOSE());
  CHECK(clipboard("sentinel",8));CHECK(key(0x33,IEQUALIFIER_RCOMMAND));
  int matched=0;for(int j=0;j<200&&!matched;j++){matched=clipboard_matches(hexmatches[h]);if(!matched)Delay(2);}CHECK(matched);
 }
 puts("SEARCH_HEX_SHOT");fflush(stdout);Delay(100);
 puts("SEARCH_HEX_PASS fixed/braced scalar compiler, native ASCII and Greek hex queries, case scope and class range exact clipboard");fflush(stdout);
 CHECK(write_text("\x1b[3J\x1b[2J\x1b[Hextended aaa ab a b a#b"));
 const char *extendedqueries[]={"(?x) a b","(?x)a\\ b","(?x)a\\#b","(?x)[a - b]{2}","(?x)a# note\n b","(?x:a b) a","(?x)a + ?","(?x)a +?","(?x)a {1,3} ?","(?x)a\xe2\x80\x83" "b"};
 const char *extendedmatches[]={"ab","a b","a#b","aa","ab","ab a","aaa","a","a","ab"};
 for(unsigned h=0;h<10;h++){
  CHECK(OPEN());CHECK(clipboard(extendedqueries[h],strlen(extendedqueries[h])));CHECK(PASTE());CHECK(found());
  if(h==9){puts("SEARCH_EXTENDED_SHOT");fflush(stdout);Delay(100);}
  CHECK(CLOSE());CHECK(clipboard("sentinel",8));CHECK(key(0x33,IEQUALIFIER_RCOMMAND));
  int matched=0;for(int j=0;j<200&&!matched;j++){matched=clipboard_matches(extendedmatches[h]);if(!matched)Delay(2);}CHECK(matched);
 }
 puts("SEARCH_EXTENDED_PASS whitespace comments scopes class ranges quantifier spacing Unicode whitespace and exact clipboard");fflush(stdout);
 CHECK(write_text("\x1b[3J\x1b[2J\x1b[HMODE a\xce\xa9" "b a b aaab\r\nHARDEND\r\nHARDSTART"));
 const char *dotqueries[]={"(?s)a.b","(?-s:a.b)","(?s:a(?-s:.)b)","(?sU)a.*b","(?sx) a . b","(?si)A.B"};
 for(unsigned h=0;h<6;h++){
  CHECK(OPEN());CHECK(clipboard(dotqueries[h],strlen(dotqueries[h])));CHECK(PASTE());CHECK(found());
  if(h==5){puts("SEARCH_DOTALL_SHOT");fflush(stdout);Delay(100);}
  CHECK(CLOSE());CHECK(clipboard("sentinel",8));CHECK(key(0x33,IEQUALIFIER_RCOMMAND));
  int matched=0;for(int j=0;j<200&&!matched;j++){matched=clipboard_matches("a\xce\xa9" "b");if(!matched)Delay(2);}CHECK(matched);
 }
 CHECK(OPEN());CHECK(clipboard("(?s)HARDEND.*HARDSTART",strlen("(?s)HARDEND.*HARDSTART")));CHECK(PASTE());CHECK(status("not found"));CHECK(CLOSE());
 puts("SEARCH_DOTALL_PASS global scoped disabled and mixed flags, Unicode dot exact clipboard and hard-line isolation");fflush(stdout);
 bindings=original;bindings.bp_Version=7;bindings.bp_Count=5;
 bindings.bp_Entries[4]=(struct ConsolengKeyBinding){CONSOLENG_BIND_RAW+RAWKEY_F3,0,CONSOLENG_BIND_CLEARHISTORY};
 CHECK(prefs(CD_SETBINDINGPREFS,&bindings));
 for(int phase=0;phase<3;phase++){
  CHECK(write_text("\x1b[3J\x1b[2J\x1b[HHISTORY-CLEAR-ONLY\r\n"));
  for(ULONG row=0;row<((ULONG)CHAR_YMAX(unit)+1)*3;row++)CHECK(write_text("history padding\r\n"));
  CHECK(write_text("\x1b[H\x1b[2KLIVE-CLEAR-KEEP\x1b[3;5H"));
  CHECK(OPEN());if(strstr((char*)window->Title,"Regex:"))CHECK(key(0x13,IEQUALIFIER_CONTROL));
  CHECK(clipboard("HISTORY-CLEAR-ONLY",18));CHECK(PASTE());CHECK(found());
  if(phase!=1)CHECK(CLOSE());
  if(phase==2){CHECK(key(0x40,IEQUALIFIER_CONTROL|IEQUALIFIER_LSHIFT));CHECK(status("Navigate:"));}
  CHECK(key(RAWKEY_F3,0));CHECK(!strstr((char*)window->Title,"Find:")&&!strstr((char*)window->Title,"Navigate:"));
  CHECK(write_text("\x1b[6n"));
  char position[6];for(unsigned i=0;i<sizeof(position);i++)CHECK(WaitForChar(stream,500000)&&Read(stream,position+i,1)==1);
  CHECK(!memcmp(position,"\x1b[3;5R",6));CHECK(!WaitForChar(stream,20000));
  CHECK(OPEN());CHECK(clipboard("HISTORY-CLEAR-ONLY",18));CHECK(PASTE());CHECK(status("not found"));CHECK(CLOSE());
  CHECK(OPEN());CHECK(clipboard("LIVE-CLEAR-KEEP",15));CHECK(PASTE());CHECK(found());CHECK(CLOSE());
  CHECK(clipboard("sentinel",8));CHECK(key(0x33,IEQUALIFIER_RCOMMAND));
  int match=0;for(int j=0;j<200&&!match;j++){match=clipboard_matches("LIVE-CLEAR-KEEP");if(!match)Delay(2);}CHECK(match);
 }
 CHECK(prefs(CD_SETBINDINGPREFS,&original));
 puts("CLEAR_BINDING_SHOT");fflush(stdout);Delay(100);
 puts("CLEAR_BINDING_PASS normal/search/navigation clear, live text and cursor preserved, exact clipboard and no input leakage");fflush(stdout);
 struct ConsolengFontPrefs savedFont={sizeof(savedFont),1,0,{0}},zoomFont={sizeof(zoomFont),1,16,"DejaVuSansMono.font"},gotFont;
 CHECK(fontprefs(CD_GETFONTPREFS,&savedFont));CHECK(fontprefs(CD_SETFONTPREFS,&zoomFont));
 bindings=original;bindings.bp_Version=8;bindings.bp_Count=7;
 bindings.bp_Entries[4]=(struct ConsolengKeyBinding){CONSOLENG_BIND_RAW+RAWKEY_F1,0,CONSOLENG_BIND_FONTLARGER};
 bindings.bp_Entries[5]=(struct ConsolengKeyBinding){CONSOLENG_BIND_RAW+RAWKEY_F2,0,CONSOLENG_BIND_FONTSMALLER};
 bindings.bp_Entries[6]=(struct ConsolengKeyBinding){CONSOLENG_BIND_RAW+RAWKEY_F3,0,CONSOLENG_BIND_FONTRESET};
 CHECK(prefs(CD_SETBINDINGPREFS,&bindings));
 CHECK(write_text("\x1b[3J\x1b[2J\x1b[HZOOM-KEEP"));
 CHECK(key(RAWKEY_F1,0));CHECK(fontprefs(CD_GETFONTPREFS,&gotFont)&&gotFont.fp_Pixels==17);
 CHECK(key(RAWKEY_F1,0));CHECK(fontprefs(CD_GETFONTPREFS,&gotFont)&&gotFont.fp_Pixels==18);
 CHECK(key(RAWKEY_F2,0));CHECK(fontprefs(CD_GETFONTPREFS,&gotFont)&&gotFont.fp_Pixels==17);
 CHECK(key(RAWKEY_F3,0));CHECK(fontprefs(CD_GETFONTPREFS,&gotFont)&&gotFont.fp_Pixels==16);
 for(int mode=0;mode<2;mode++){
  CHECK(OPEN());if(strstr((char*)window->Title,"Regex:"))CHECK(key(0x13,IEQUALIFIER_CONTROL));
  CHECK(clipboard("ZOOM-KEEP",9));CHECK(PASTE());CHECK(found());
  if(mode){CHECK(CLOSE());CHECK(key(0x40,IEQUALIFIER_CONTROL|IEQUALIFIER_LSHIFT));CHECK(status("Navigate:"));}
  CHECK(key(RAWKEY_F1,0));CHECK(fontprefs(CD_GETFONTPREFS,&gotFont)&&gotFont.fp_Pixels==17);
  CHECK(!strstr((char*)window->Title,"Find:")&&!strstr((char*)window->Title,"Navigate:"));
  CHECK(key(RAWKEY_F3,0));CHECK(fontprefs(CD_GETFONTPREFS,&gotFont)&&gotFont.fp_Pixels==16);
 }
 CHECK(key(RAWKEY_F1,0));CHECK(write_text("\x1b]50;DejaVuSansMono,20\x07"));
 CHECK(key(RAWKEY_F3,0));CHECK(fontprefs(CD_GETFONTPREFS,&gotFont)&&gotFont.fp_Pixels==20);
 CHECK(zoom_event(RAWKEY_F1,0));CHECK(zoom_event(RAWKEY_F1,IEQUALIFIER_REPEAT));CHECK(zoom_event(RAWKEY_F1,IEQUALIFIER_REPEAT));CHECK(zoom_event(RAWKEY_F1|IECODE_UP_PREFIX,0));
 CHECK(fontprefs(CD_GETFONTPREFS,&gotFont)&&gotFont.fp_Pixels==23);CHECK(!WaitForChar(stream,20000));
 CHECK(key(RAWKEY_F3,0));CHECK(fontprefs(CD_GETFONTPREFS,&gotFont)&&gotFont.fp_Pixels==20);
 CHECK(OPEN());CHECK(clipboard("ZOOM-KEEP",9));CHECK(PASTE());CHECK(found());CHECK(CLOSE());
 CHECK(clipboard("sentinel",8));CHECK(key(0x33,IEQUALIFIER_RCOMMAND));
 int zoomCopy=0;for(int j=0;j<200&&!zoomCopy;j++){zoomCopy=clipboard_matches("ZOOM-KEEP");if(!zoomCopy)Delay(2);}CHECK(zoomCopy);
 CHECK(write_text("\x1b[2J\x1b[HFont zoom shortcuts: PASS\r\nLarger / smaller / reset\r\nSearch and navigation cancellation\r\nApplication font changes preserved\r\nHeld-key repeat: PASS"));
 puts("FONT_ZOOM_SHOT");fflush(stdout);Delay(100);
 CHECK(fontprefs(CD_SETFONTPREFS,&savedFont));CHECK(prefs(CD_SETBINDINGPREFS,&original));
 puts("FONT_ZOOM_PASS exact sizes, baseline reset, external font override, active search/navigation, held-repeat and retained exact clipboard");fflush(stdout);
 puts("SEARCH_PASTE_PASS ConClip UTF8 Latin1 multiline append controls malformed stale-reply word-delete clear-query history-draft no-input-leak");fflush(stdout);Delay(600);rc=0;
 out:if(prefsIO)DeleteIORequest((struct IORequest*)prefsIO);if(prefsPort)DeleteMsgPort(prefsPort);if(stream){SetMode(stream,FALSE);Close(stream);}if(IFFParseBase)CloseLibrary(IFFParseBase);
 printf("SEARCH_PASTE_SCRIPT rc=%d\n",rc);return rc;
}
