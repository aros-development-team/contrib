#!/usr/bin/env python3
"""Actual search UI helpers: query isolation, lifetime, navigation and overlay."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'charmapconclass.c').read_text()
h=(r/'search.h').read_text().replace('#include <exec/types.h>','')
h+='\n'+(r/'navigation.h').read_text()
c='\n'.join(x for x in (r/'search.c').read_text().splitlines() if not x.startswith('#include'))
h += '\n'+'\n'.join(x for x in (r/'search_regex.h').read_text().splitlines() if not x.startswith('#include'))
h += '\n'+(r/'search_origin.h').read_text()
h += '\n'+(r/'search_seed.h').read_text()
h += '\n'+(r/'search_anchor.h').read_text()
u=(r/'support.c').read_text();h += '\n'+u[u.index('ULONG con_utf8_encode('):u.index('ULONG con_utf8_count(')]
h += '\n'+(r/'copy_encode.h').read_text()
c += '\n'+'\n'.join(x for x in (r/'search_unicode.h').read_text().splitlines() if not x.startswith('#include'))
c += '\n'+'\n'.join(x for x in (r/'search_case.h').read_text().splitlines() if not x.startswith('#include'))
c += '\n'+'\n'.join(x for x in (r/'search_regex.c').read_text().splitlines() if not x.startswith('#include'))
line=(r/'charmap.h').read_text();line=line[line.index('struct charmap_line\n'):line.index('VOID charmap_dispose_lines')]
state=s[s.index('struct charmap_search_ui\n'):s.index('// FIXME: Abstract out')]
f=s[s.index('static struct charmap_line *charmapcon_find_line('):s.index('/* Store a byte span')]+'\n'+s[s.index('static VOID charmap_search_remember('):s.index('/* Encoded byte length')]
f=s[s.index('static BOOL charmap_restore_live('):s.index('/* Keyboard clearing preserves')]+'\n'+f
a=s.index('static VOID charmap_block_span(');b=s.index('static VOID charmapcon_refresh_lines(',a);overlay=s[a:b]
prefix=r'''
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
typedef uint32_t ULONG;typedef uint16_t UWORD;typedef uint8_t UBYTE;typedef int8_t BYTE;typedef uint64_t UQUAD;typedef int BOOL;typedef void VOID;typedef int32_t LONG;typedef const char *CONST_STRPTR;
#define CHARMAP_ERASED 0x21
#define RAWKEY_LEFT 0x4f
#define RAWKEY_RIGHT 0x4e
#define CHAR_XMAX(o) 79
#define CHAR_YMAX(o) 29
#define Console_Copy(o) ((o)->copies++)
#define TRUE 1
#define FALSE 0
#define MIN(a,b) ((a)<(b)?(a):(b))
#define CON_TXTFLAGS_REVERSED 1
#define MEMF_ANY 0
#define MEMF_PUBLIC 1
#define MEMF_CLEAR 2
#define IECODE_UP_PREFIX 128
#define IEQUALIFIER_CONTROL 1
#define IEQUALIFIER_LSHIFT 2
#define IEQUALIFIER_RSHIFT 4
#define IEQUALIFIER_LALT 8
#define IEQUALIFIER_RALT 16
#define IEQUALIFIER_LCOMMAND 32
#define IEQUALIFIER_RCOMMAND 64
#define RAWKEY_UP 0x4c
#define RAWKEY_DOWN 0x4d
#define RAWKEY_ESCAPE 0x45
#define RAWKEY_RETURN 0x44
#define RAWKEY_KP_ENTER 0x43
#define RAWKEY_BACKSPACE 0x41
struct InputEvent {UWORD ie_Code,ie_Qualifier,ie_Class;};
struct ConsoleBase;
#define IECLASS_RAWKEY 0
#define IEQUALIFIER_NUMERICPAD 512
#define IEQUALIFIER_REPEAT 1024
'''
message=(r/'consoleif.h').read_text(encoding='latin1');message=message[message.index('struct P_Console_QueryPaste\n'):message.index('struct P_Console_GetDefaultParams')]
binding_header=(r/'include/consoleng.h').read_text().split('#define CONSOLENG_BIND_RAW')[1].split('#define CD_GETINTERACTIONPROFILE')[0].split('#endif')[0]
prefix += '#define CONSOLENG_BIND_RAW'+binding_header
semantic_header=(r/'include/consoleng.h').read_text()
prefix += semantic_header[semantic_header.index('#define CONSOLENG_SEMANTIC_MAX'):semantic_header.index('#define CD_GETSELECTIONPROFILE')]
prefix += (r/'semantic_prefs.h').read_text()
bindings=(r/'bindings.c').read_text();bindings=(r/'binding_prefs.h').read_text()+bindings[bindings.index('BOOL con_bindings_valid'):]
stubs=r'''
typedef int Class;
typedef uintptr_t IPTR;
struct Window {UBYTE *Title;void *WScreen;};
struct charmapcondata {struct charmap_search_ui *search;UBYTE *prepared_copy;ULONG prepared_copy_size;struct charmap_line *top_of_scrollback,*top_of_window,*saved_top_of_window;ULONG scrollback_pos,saved_scrollback_pos,scrollback_size,scrollback_max;LONG select_y_min,select_y_max,select_x_min,select_x_max;struct charmap_line *select_line_min,*select_line_max;BOOL unrendered,active_selection,ignore_drag,selection_exact_end,selection_block;};
typedef struct {struct charmapcondata data;struct Window window;BOOL hidden,keyboardAlternate;ULONG conFlags;ULONG cursor_x,cursor_y;int cursor_balance,cursor_operations;unsigned copies;struct ConsolengBindingPrefs bindings;struct ConsolengSemanticPrefs semanticPrefs;} Object;
#define ICU(o) (o)
#define CF_APPLICATION_CURSOR 256
#define CF_APPLICATION_KEYPAD 512
#define XCP ((o)->cursor_x)
#define YCP ((o)->cursor_y)
static struct ConsoleBase *ConsoleDevice;
static UQUAD requested_revision;static unsigned requests;static BOOL request_available=TRUE;
static BOOL con_query_clipboard_request(void*base,Object*unit,UQUAD revision){(void)base;(void)unit;requested_revision=revision;requests++;return request_available;}
#define INST_DATA(cl,o) (&(o)->data)
#define WINDOW(o) (&(o)->window)
#define CON_IS_UTF8(o) TRUE
#define FreeMem(p,n) free(p)
#define CON_IS_OBSCURED(o) ((o)->hidden)
static unsigned url_lookups,url_opens,url_beeps;static BOOL url_present,url_launch_ok=TRUE;
static struct charmap_line *url_row;static ULONG url_column;
static BOOL con_url_at(struct charmap_line *line,ULONG column,UBYTE *url,ULONG capacity){assert(capacity==1024);url_lookups++;url_row=line;url_column=column;strcpy((char*)url,"https://example.org/");return url_present;}
static BOOL con_open_url(const UBYTE *url){assert(!strcmp((const char*)url,"https://example.org/"));url_opens++;return url_launch_ok;}
static void DisplayBeep(void *screen){(void)screen;url_beeps++;}
static BOOL charmap_history_clear_action(Class *cl,Object *o);
static unsigned history_clears, font_zooms;
#define C_ZOOM_FONT 999
static void Console_DoCommand(Object *o,int command,int count,IPTR *args);
static unsigned redraws,allocations;static int fail_alloc;
static void *AllocMem(size_t n,int flags){(void)flags;if(fail_alloc)return NULL;allocations++;return calloc(1,n);}
static void SetWindowTitles(struct Window*w,CONST_STRPTR title,CONST_STRPTR screen){(void)screen;w->Title=(UBYTE*)title;}
static void charmapcon_refresh(Class*cl,Object*o,LONG off){(void)cl;(void)o;(void)off;redraws++;}
static void charmapcon_clear_selection(Class*cl,Object*o){(void)cl;(void)o;}
static void charmapcon_adj_prop(Class*cl,Object*o){(void)cl;(void)o;}
#define D(x) ((void)0)
static unsigned entry_allocations;static BOOL entry_failure;
static struct charmap_line *charmap_newline(ULONG size,struct charmap_line *previous){
 assert(!size);if(entry_failure)return NULL;entry_allocations++;
 struct charmap_line *line=calloc(1,sizeof(*line));assert(line);
 line->prev=previous;if(previous)previous->next=line;return line;
}
static void charmap_trim_selection(struct charmapcondata *d){(void)d;assert(0);}
static struct charmap_line *charmap_dispose_line(struct charmap_line *line){(void)line;assert(0);return NULL;}
static ULONG test_history_limit=10000;
static ULONG charmap_history_limit(struct charmapcondata*d){(void)d;return test_history_limit;}
#define Console_UnRenderCursor(o) ((o)->cursor_operations++,(o)->cursor_balance--)
#define Console_RenderCursor(o) ((o)->cursor_operations++,(o)->cursor_balance++)
static LONG MapRawKey(struct InputEvent *e,UBYTE *out,ULONG cap,void *unused){(void)unused;assert(cap);out[0]=e->ie_Code==6?'6':e->ie_Code==8?'8':e->ie_Code==3?'3':e->ie_Code==0x3a?'/':e->ie_Code==0x36?'n':e->ie_Code==0x31?'z':e->ie_Code==0x32?'x':e->ie_Code==0x14?'t':e->ie_Code==0x29?';':e->ie_Code==0x38?',':e->ie_Code==0x22?'d':e->ie_Code==0x37?'m':e->ie_Code==5?'5':e->ie_Code==0x1a?'[':e->ie_Code==0x1b?']':e->ie_Code==0x12?'e':e->ie_Code==0x35?'b':e->ie_Code==0x17?'i':e->ie_Code==0x33?'c':e->ie_Code==0x25?'h':e->ie_Code==0x26?'j':e->ie_Code==0x27?'k':e->ie_Code==0x28?'l':e->ie_Code==0x24?'g':e->ie_Code==0x15?'y':e->ie_Code==0x0a?'0':e->ie_Code==4?'4':e->ie_Code==0x23?'f':e->ie_Code==0x20?'a':e->ie_Code==0x21?'b':e->ie_Code==0x34?'v':e->ie_Code==0x11?'w':e->ie_Code==0x16?'u':e->ie_Code==0x13?'r':0;return out[0]?1:0;}
'''
main=r'''
static void Console_DoCommand(Object *o,int command,int count,IPTR *args){assert(command==C_ZOOM_FONT&&count==1&&*args>=14&&*args<=16);charmap_search_close(NULL,o,FALSE);font_zooms++;}
static BOOL charmap_history_clear_action(Class *cl,Object *o){charmap_search_close(cl,o,FALSE);history_clears++;return TRUE;}
static BOOL press(Object *o,UWORD code,UWORD qualifiers){if(!o->semanticPrefs.sp_Size)o->semanticPrefs=con_semantic_defaults();if(!o->bindings.bp_Size)con_bindings_defaults(&o->bindings);struct InputEvent e={code,qualifiers};return charmap_search_key(NULL,o,&e);}
static void key(Object*o,UWORD code,UWORD q){assert(press(o,code,q));assert(press(o,code|128,q));}
static void open_search(Object*o){key(o,0x23,3);assert(o->data.search->active&&!o->data.search->length);}
static void complete(Object*o){for(unsigned i=0;i<100;i++){charmap_search_tick(NULL,o);if(!o->data.search->scanning&&!o->data.search->locating&&!o->data.search->nav_dirty&&!o->data.search->nav_seed&&!o->data.search->nav_copy)return;}assert(0);}
int main(void){
 /* Explicit clear preserves navigation/query state and cancels deferred copy. */
 for(unsigned navigating=0;navigating<2;navigating++)for(unsigned querying=0;querying<2;querying++){
  Object clear={0};struct charmap_search_ui state={0};clear.data.search=&state;
  con_bindings_defaults(&clear.bindings);clear.bindings.bp_Version=10;
  clear.bindings.bp_Entries[0]=(struct ConsolengKeyBinding){CONSOLENG_BIND_RAW+0x50,0,CONSOLENG_BIND_CLEARSELECTION};
  state.active=TRUE;state.nav_mode=navigating;state.nav_query=querying;
  state.nav.selecting=TRUE;state.nav.cursor.row=9;state.nav.cursor.column=5;
  state.nav_copy=state.nav_copy_clear=TRUE;state.copy_size=32;state.copy_buffer=malloc(32);assert(state.copy_buffer);
  state.length=3;state.query[0]='a';state.query[1]='b';state.query[2]='c';clear.window.Title=(UBYTE*)state.title;
  clear.data.scrollback_pos=8;clear.cursor_x=3;clear.cursor_y=2;
  clear.data.active_selection=clear.data.ignore_drag=TRUE;
  key(&clear,0x50,0);
  assert(state.active&&state.nav_mode==navigating&&state.nav_query==querying);
  assert(!state.copy_buffer&&!state.nav_copy&&!state.nav_copy_clear);
  assert(state.nav.cursor.row==9&&state.nav.cursor.column==5&&state.length==3&&state.query[0]=='a'&&state.query[1]=='b'&&state.query[2]=='c');
  assert(clear.data.scrollback_pos==8&&clear.cursor_x==3&&clear.cursor_y==2);
  assert(!clear.data.active_selection&&!clear.data.ignore_drag&&!clear.copies);
  if(navigating&&!querying)assert(!state.nav.selecting&&state.nav_dirty);
 }

 for(unsigned action=14;action<=16;action++)for(int nav=0;nav<2;nav++){
  ULONG cells[]={'a','b'};struct charmap_line row={0};row.text=cells;row.size=2;
  Object o={0};o.data.top_of_scrollback=o.data.top_of_window=&row;o.window.Title=(UBYTE*)"owner";con_bindings_defaults(&o.bindings);o.bindings.bp_Version=8;o.bindings.bp_Count=5;
  o.bindings.bp_Entries[4]=(struct ConsolengKeyBinding){CONSOLENG_BIND_RAW+0x52,0,action};
  open_search(&o);o.data.search->nav_mode=nav;
  unsigned before=font_zooms;assert(press(&o,0x52,0));assert(font_zooms==before+1&&!o.data.search->active);
  assert(press(&o,0x52,IEQUALIFIER_REPEAT)&&font_zooms==before+1);assert(press(&o,0x52|128,0));
  FreeMem(o.data.search,sizeof(*o.data.search));
 }

 for(int navigation=0;navigation<2;navigation++){
  ULONG cells[]={'a','b'};struct charmap_line row={0};row.text=cells;row.size=2;
  Object o={0};o.data.top_of_scrollback=o.data.top_of_window=&row;o.window.Title=(UBYTE*)"clear owner";
  con_bindings_defaults(&o.bindings);o.bindings.bp_Version=7;o.bindings.bp_Count=5;
  o.bindings.bp_Entries[4]=(struct ConsolengKeyBinding){CONSOLENG_BIND_RAW+0x52,0,CONSOLENG_BIND_CLEARHISTORY};
  if(navigation)key(&o,0x40,3);else open_search(&o);
  unsigned before=history_clears;assert(press(&o,0x52,0));assert(history_clears==before+1);
  assert(!o.data.search->active&&!o.data.search->scanning&&!o.data.search->nav_mode);
  assert(o.window.Title==(UBYTE*)"clear owner");
  assert(press(&o,0x52,IEQUALIFIER_REPEAT)&&history_clears==before+1);
  assert(press(&o,0x52|128,0));free(o.data.search);
 }
 /* Backward regex uses its own match stream, including overlaps. */
 {
  const char *texts[]={"ababa","aaa","aab","bbb"},*patterns[]={"aba","a*","a+|ab","a*"};
  unsigned starts[]={2,0,1},ends[]={5,3,3};
  for(unsigned i=0;i<4;i++){
   ULONG cells[8],query[8];unsigned n=strlen(texts[i]),qn=strlen(patterns[i]);
   for(unsigned j=0;j<n;j++)cells[j]=texts[i][j];
   for(unsigned j=0;j<qn;j++)query[j]=patterns[i][j];
   struct charmap_line row={.text=cells,.size=n};
   Object o={0};o.data.top_of_scrollback=o.data.top_of_window=&row;con_bindings_defaults(&o.bindings);
   key(&o,0x23,3);struct charmap_search_ui *s=o.data.search;
   memcpy(s->query,query,qn*sizeof(ULONG));s->length=qn;s->regex_mode=TRUE;
   charmap_search_start(NULL,&o,TRUE,FALSE);complete(&o);
   if(i==3)assert(!s->found);
   else assert(s->found&&s->match.first.column==starts[i]&&s->match.end.column==ends[i]);
   charmap_search_close(NULL,&o,FALSE);free(s);
  }
 }
 /* Seeded navigation skips the source word, wraps, and repeats direction. */
 for(unsigned reverse=0;reverse<2;reverse++)for(unsigned column=0;column<9;column++){
  Object o={0};struct charmap_search_ui state={0};ULONG text[]={'a','b',' ','a','b',' ','a','b',' '};
  struct charmap_line row={.text=text,.size=9};o.data.search=&state;o.data.top_of_scrollback=o.data.top_of_window=&row;
  o.cursor_x=column;key(&o,0x40,3);complete(&o);key(&o,reverse?3:8,IEQUALIFIER_LSHIFT);
  assert(state.nav_seed&&strstr((char*)o.window.Title,"collecting"));complete(&o);
  unsigned source=(column+1)/3*3%9;unsigned expected=(source+(reverse?6:3))%9;
  assert(state.nav.cursor.column==expected&&state.length==2&&state.query[0]=='a'&&state.query[1]=='b');
  assert(!state.nav_query&&!state.nav.selecting&&state.reverse_search==reverse&&!state.seed.line);
  key(&o,0x36,0);complete(&o);assert(state.nav.cursor.column==(expected+(reverse?6:3))%9);
  key(&o,0x36,IEQUALIFIER_LSHIFT);complete(&o);assert(state.nav.cursor.column==expected);
  charmap_search_close(NULL,&o,FALSE);
 }
 /* A selected character overrides its containing word. Limit failures leave
  * the previous query and selection intact; pending seeds cancel on input. */
 {
  Object o={0};struct charmap_search_ui state={0};ULONG text[300];for(unsigned i=0;i<300;i++)text[i]='a';
  struct charmap_line row={.text=text,.size=300};o.data.search=&state;o.data.top_of_scrollback=o.data.top_of_window=&row;
  key(&o,0x40,3);complete(&o);key(&o,0x34,0);complete(&o);key(&o,8,IEQUALIFIER_LSHIFT);complete(&o);
  assert(state.length==1&&state.query[0]=='a'&&state.nav.cursor.column==1&&!state.nav.selecting);
  state.nav.width=300;key(&o,8,IEQUALIFIER_LSHIFT);complete(&o);assert(state.length==1&&strstr((char*)o.window.Title,"exceeds query limit"));
  key(&o,8,IEQUALIFIER_LSHIFT);assert(state.nav_seed);key(&o,RAWKEY_ESCAPE,0);complete(&o);
  assert(!state.nav_seed&&!state.seed.head&&state.length==1);
  key(&o,8,IEQUALIFIER_LSHIFT);charmap_search_close(NULL,&o,FALSE);assert(!state.nav_seed&&!state.seed.head);
 }
 /* Escape selected regex metacharacters, preserving the pinned reference's
  * dot wildcard policy, without publishing partial text or stale paste. */
 for(unsigned dot=0;dot<2;dot++){
  Object o={0};struct charmap_search_ui state={0};ULONG text[]={'a',dot?'.':'+','b',' ','a',dot?'X':'+','b'};
  struct charmap_line row={.text=text,.size=7};o.data.search=&state;o.data.top_of_scrollback=o.data.top_of_window=&row;
  key(&o,0x40,3);complete(&o);key(&o,8,IEQUALIFIER_LSHIFT);complete(&o);
  assert(state.nav.cursor.column==4&&state.length==(dot?3:4));
  if(!dot)assert(state.query[1]=='\\'&&state.query[2]=='+');
  charmap_search_close(NULL,&o,FALSE);
 }
 {
  Object o={0};struct charmap_search_ui state={0};struct charmap_line *rows=calloc(10000,sizeof(*rows));assert(rows);
  for(unsigned i=0;i<10000;i++){rows[i].next=i<9999?rows+i+1:NULL;rows[i].prev=i?rows+i-1:NULL;}
  o.data.search=&state;o.data.top_of_scrollback=o.data.top_of_window=rows;
  key(&o,0x40,3);complete(&o);state.query[0]='x';state.length=1;
  key(&o,8,IEQUALIFIER_LSHIFT);charmap_search_tick(NULL,&o);assert(state.nav_seed&&state.length==1);
  UQUAD revision=state.revision;key(&o,0x33,IEQUALIFIER_CONTROL);
  assert(!state.active&&!state.seed.head);
  struct P_Console_QueryPaste paste={.Revision=revision,.Valid=TRUE,.UTF8=TRUE,.Data=(UBYTE*)"b",.Length=1};
  charmap_search_paste(NULL,&o,&paste);assert(state.length==1&&state.query[0]=='x');free(rows);
 }
 /* A trimmed rectangle's cursor can lie after the source text. Anchor on
  * the next matching occurrence before choosing the directional target. */
 for(unsigned reverse=0;reverse<2;reverse++){
  Object o={0};struct charmap_search_ui state={0};ULONG text[]={'a','b',' ','1',' ','a','b',' ','2',' ','a','b',' ','3'};
  struct charmap_line row={.text=text,.size=14};o.data.search=&state;o.data.top_of_scrollback=o.data.top_of_window=&row;
  key(&o,0x40,3);complete(&o);key(&o,0x34,IEQUALIFIER_CONTROL);key(&o,RAWKEY_RIGHT,0);key(&o,RAWKEY_RIGHT,0);complete(&o);
  key(&o,reverse?3:8,IEQUALIFIER_LSHIFT);charmap_search_tick(NULL,&o);
  assert(state.nav_anchor&&state.length==2);complete(&o);
  assert(state.nav.cursor.column==(reverse?0:10)&&!state.nav_anchor&&!state.seed_anchor.line);
  charmap_search_close(NULL,&o,FALSE);
 }
 /* Cancellation during the separate anchor pass releases borrowed rows. */
 for(unsigned external=0;external<2;external++){
  Object o={0};struct charmap_search_ui state={0};ULONG text[]={'a','b',' '};struct charmap_line *rows=calloc(10000,sizeof(*rows));assert(rows);
  for(unsigned i=0;i<10000;i++){rows[i].next=i<9999?rows+i+1:NULL;rows[i].prev=i?rows+i-1:NULL;}
  rows[0].text=rows[9999].text=text;rows[0].size=rows[9999].size=3;
  o.data.search=&state;o.data.top_of_scrollback=o.data.top_of_window=rows;
  key(&o,0x40,3);complete(&o);key(&o,0x34,IEQUALIFIER_CONTROL);key(&o,RAWKEY_RIGHT,0);key(&o,RAWKEY_RIGHT,0);complete(&o);
  key(&o,8,IEQUALIFIER_LSHIFT);charmap_search_tick(NULL,&o);charmap_search_tick(NULL,&o);
  assert(state.nav_anchor&&state.scanning);
  if(external){o.window.Title=(UBYTE*)"new owner";charmap_search_tick(NULL,&o);}
  else key(&o,0x33,IEQUALIFIER_CONTROL);
  assert(!state.active&&!state.nav_anchor&&!state.seed_anchor.line);free(rows);
 }
 /* A discontiguous rectangle need not occur as a contiguous regex match. */
 {
  Object o={0};struct charmap_search_ui state={0};ULONG a[]={'a','x'},b[]={'b','y'};
  struct charmap_line rows[2]={{.text=a,.size=2},{.text=b,.size=2}};rows[0].next=rows+1;rows[1].prev=rows;
  o.data.search=&state;o.data.top_of_scrollback=o.data.top_of_window=rows;
  key(&o,0x40,3);complete(&o);key(&o,0x34,IEQUALIFIER_CONTROL);key(&o,RAWKEY_DOWN,0);complete(&o);
  key(&o,8,IEQUALIFIER_LSHIFT);complete(&o);
  assert(state.nav.cursor.row==1&&!state.nav.selecting&&!state.nav_anchor&&!state.seed_anchor.line);
  assert(state.length==3&&state.query[1]=='\n'&&strstr((char*)o.window.Title,"not found"));
  charmap_search_close(NULL,&o,FALSE);
 }
 /* A long semantic copy is queued, bounded and tied to its own snapshot. */
 for(unsigned action=0;action<4;action++){
  Object o={0};struct charmap_search_ui state={0};ULONG *text=calloc(6000,sizeof(*text));assert(text);
  for(unsigned i=0;i<6000;i++)text[i]='a';
  struct charmap_line row={.text=text,.size=6000};
  o.data.search=&state;o.data.top_of_scrollback=o.data.top_of_window=&row;
  key(&o,0x40,3);complete(&o);state.nav.width=6000;key(&o,0x34,IEQUALIFIER_LALT);key(&o,0x15,0);
  assert(state.nav_copy&&o.copies==0);charmap_search_tick(NULL,&o);assert(state.nav_copy&&o.copies==0);
  if(action==1){key(&o,RAWKEY_RIGHT,0);assert(state.nav.selecting&&!state.nav_copy_clear);}
  if(action==2){key(&o,0x33,IEQUALIFIER_CONTROL);assert(!state.active&&!state.nav_copy&&!state.copy_nav.cursor.line);}
  else if(action==3){o.window.Title=(UBYTE*)"new title";charmap_search_tick(NULL,&o);assert(!state.active&&!state.nav_copy&&!state.copy_nav.cursor.line);}
  else{
   complete(&o);assert(!state.nav_copy&&!state.copy_nav.cursor.line&&o.copies==1);
   assert(o.data.select_x_min==0&&o.data.select_x_max==6000);
   if(action==1)assert(state.nav.selecting&&state.nav.cursor.column==1);else assert(!state.nav.selecting);
   charmap_search_close(NULL,&o,FALSE);
  }
  free(text);
 }
 /* Cancel an allocated partial encoding, preserve selection on allocation
  * failure, and replace a pending buffer with a newer small copy. */
 for(unsigned action=0;action<3;action++){
  Object o={0};struct charmap_search_ui state={0};ULONG *text=calloc(6000,sizeof(*text));assert(text);
  for(unsigned i=0;i<6000;i++)text[i]=0x3a9;
  struct charmap_line row={.text=text,.size=6000};o.data.search=&state;o.data.top_of_scrollback=o.data.top_of_window=&row;
  key(&o,0x40,3);complete(&o);state.nav.width=6000;state.nav.selecting=TRUE;state.nav.anchor=state.nav.cursor;state.nav.cursor.column=5999;
  if(action==1)fail_alloc=1;
  key(&o,0x15,0);assert(state.nav_copy&&!state.copy_buffer&&!o.copies);
  for(unsigned ticks=0;state.nav_copy&&!state.copy_buffer;ticks++){assert(ticks<100);charmap_search_tick(NULL,&o);}
  if(action==1){
   fail_alloc=0;assert(!state.nav_copy&&state.nav.selecting&&!o.copies&&strstr((char*)o.window.Title,"allocation failed"));
  }else{
   assert(state.copy_buffer&&state.copy_phase==2&&state.copy_encoder.offset<state.copy_size&&!o.copies);
   if(action==2){
    state.nav.pending=FALSE;state.nav.selecting=TRUE;state.nav.semanticwise=state.nav.linewise=state.nav.blockwise=FALSE;
    state.nav.cursor.column=3;state.nav.anchor=state.nav.cursor;state.nav.match_ready=FALSE;
    key(&o,0x15,0);assert(o.copies==1&&!state.nav_copy&&!state.copy_buffer);
   }
  }
  charmap_search_close(NULL,&o,FALSE);assert(!state.copy_buffer&&!state.copy_encoder.line&&!state.nav_copy);free(text);
 }
 /* Query mode begins at the cursor, confirms into navigation, repeats in the
  * original/opposite direction, and restores origin on cancellation. */
 for(unsigned reverse=0;reverse<2;reverse++)for(unsigned column=0;column<8;column++){
  Object o={0};struct charmap_search_ui state={0};ULONG text[]={'a','b','a','b','a'};
  struct charmap_line row={.text=text,.size=5};o.data.search=&state;o.data.top_of_scrollback=o.data.top_of_window=&row;
  o.cursor_x=column;o.window.Title=(UBYTE*)"query owner";key(&o,0x40,3);complete(&o);
  key(&o,0x3a,reverse?IEQUALIFIER_LSHIFT:0);assert(state.nav_query&&state.nav_mode);
  key(&o,0x20,0);complete(&o);
  ULONG expected=reverse?(column>=4?4:column>=2?2:0):(column>4?0:(column+1)/2*2);
  assert(state.nav.cursor.column==expected&&state.nav_query&&state.found);
  assert(strstr((char*)o.window.Title,"Enter confirm / Esc cancel")&&!strstr((char*)o.window.Title,"Enter next"));
  key(&o,RAWKEY_RETURN,0);complete(&o);assert(!state.nav_query&&state.nav_mode&&state.nav.cursor.column==expected);
  key(&o,0x36,0);complete(&o);ULONG next=reverse?(expected+4)%6:(expected+2)%6;
  assert(state.nav.cursor.column==next);
  key(&o,0x36,IEQUALIFIER_LSHIFT);complete(&o);assert(state.nav.cursor.column==expected);
  key(&o,0x3a,0);key(&o,0x21,0);complete(&o);key(&o,RAWKEY_ESCAPE,0);complete(&o);
  assert(!state.nav_query&&state.active&&state.nav.cursor.column==expected&&!state.length);
  key(&o,0x3a,0);key(&o,0x31,0);complete(&o);assert(state.nav_query_miss&&state.nav.cursor.column==expected);
  assert(strstr((char*)o.window.Title,"not found"));key(&o,RAWKEY_RETURN,0);complete(&o);
  key(&o,0x3a,0);key(&o,0x1a,0);complete(&o);assert(strstr((char*)o.window.Title,"invalid or unsupported regex"));
  key(&o,0x33,IEQUALIFIER_CONTROL);complete(&o);assert(!state.nav_query&&state.nav_mode&&state.nav.cursor.column==expected);
  charmap_search_close(NULL,&o,FALSE);assert(!state.nav_origin_view&&!state.origin_search.line&&o.window.Title==(UBYTE*)"query owner");
 }
 {
  Object o={0};struct charmap_search_ui state={0};struct charmap_line *rows=calloc(5000,sizeof(*rows));assert(rows);
  ULONG text[]={'a','b'};for(unsigned i=0;i<5000;i++){rows[i].next=i<4999?rows+i+1:NULL;rows[i].prev=i?rows+i-1:NULL;}
  test_history_limit=4970;rows[4999].text=text;rows[4999].size=2;o.data.search=&state;o.data.top_of_scrollback=o.data.top_of_window=rows;
  key(&o,0x40,3);complete(&o);key(&o,0x3a,0);key(&o,0x20,0);charmap_search_tick(NULL,&o);
  assert(state.scanning&&state.nav_search);key(&o,RAWKEY_RETURN,0);assert(state.nav_confirm&&state.nav_query);
  complete(&o);assert(!state.nav_query&&state.nav_mode&&state.nav.cursor.row==4999);
  key(&o,0x3a,0);key(&o,0x21,0);complete(&o);key(&o,RAWKEY_ESCAPE,0);complete(&o);
  assert(state.nav.cursor.row==4999&&o.data.scrollback_pos==4970);
  key(&o,0x3a,0);key(&o,0x20,0);UQUAD revision=state.revision;key(&o,RAWKEY_ESCAPE,0);complete(&o);
  struct P_Console_QueryPaste paste={.Revision=revision,.Valid=TRUE,.UTF8=TRUE,.Data=(UBYTE*)"b",.Length=1};
  charmap_search_paste(NULL,&o,&paste);assert(!state.length&&!state.nav_query);
  charmap_search_close(NULL,&o,FALSE);free(rows);test_history_limit=10000;
 }

 /* Query movement preserves a visible view and extends an active selection. */
 {
  Object o={0};struct charmap_search_ui state={0};ULONG text[]={'a','b','c'};
  struct charmap_line rows[30]={0};for(unsigned i=0;i<30;i++){rows[i].next=i<29?rows+i+1:NULL;rows[i].prev=i?rows+i-1:NULL;}
  rows[20].text=text;rows[20].size=3;o.data.search=&state;o.data.top_of_scrollback=o.data.top_of_window=rows;
  key(&o,0x40,3);complete(&o);key(&o,0x34,0);complete(&o);
  key(&o,0x3a,0);key(&o,0x20,0);complete(&o);assert(o.data.scrollback_pos==0&&state.nav.cursor.row==20);
  key(&o,RAWKEY_RETURN,0);complete(&o);assert(state.nav.selecting&&state.nav.anchor.row==0&&state.nav.cursor.row==20);
  charmap_search_close(NULL,&o,FALSE);
 }
 /* Entry uses the visible application cursor, including virtual blank rows.
  * A cursor outside the view uses its top-left. No history-sized traversal. */
 for(unsigned delta=0;delta<35;delta++)for(unsigned y=0;y<30;y++){
  Object o={0};struct charmap_search_ui state={0};struct charmap_line rows[65]={0};
  for(unsigned i=0;i<65;i++){rows[i].prev=i?&rows[i-1]:NULL;rows[i].next=i<64?&rows[i+1]:NULL;}
  o.data.search=&state;o.data.top_of_scrollback=rows;o.data.top_of_window=&rows[5];
  o.data.scrollback_pos=5;o.data.unrendered=delta!=0;o.data.saved_scrollback_pos=5+delta;
  o.data.saved_top_of_window=&rows[5+delta];o.cursor_x=17;o.cursor_y=y;o.window.Title=(UBYTE*)"owner";
  unsigned expected=delta+y<30?delta+y:0, column=delta+y<30?17:0;
  key(&o,0x40,3);assert(state.nav_mode&&state.nav.cursor.line==&rows[5+expected]);
  assert(state.nav.cursor.row==5+expected&&state.nav.cursor.column==column);
  charmap_search_close(NULL,&o,FALSE);assert(o.window.Title==(UBYTE*)"owner");
 }
 {
  Object o={0};struct charmap_search_ui state={0};o.data.search=&state;
  entry_allocations=0;o.cursor_x=78;o.cursor_y=29;o.window.Title=(UBYTE*)"blank owner";
  key(&o,0x40,3);assert(state.nav.cursor.row==29&&state.nav.cursor.column==78&&entry_allocations==30);
  complete(&o);assert(state.found&&o.data.scrollback_pos==0);
  charmap_search_close(NULL,&o,FALSE);
  struct charmap_line *line=o.data.top_of_scrollback;while(line){struct charmap_line *next=line->next;free(line);line=next;}
  o.data.top_of_scrollback=o.data.top_of_window=NULL;entry_failure=TRUE;
  key(&o,0x40,3);assert(!state.active&&!state.nav_mode&&o.window.Title==(UBYTE*)"blank owner");entry_failure=FALSE;
 }
 {
  Object o={0};struct charmap_search_ui state={0};ULONG text[]={'A',0x4e2d,0xfffffffe,'B'};
  struct charmap_line row={.text=text,.size=4};o.data.search=&state;o.data.top_of_scrollback=o.data.top_of_window=&row;
  o.cursor_x=2;key(&o,0x40,3);assert(state.nav.cursor.column==1);charmap_search_close(NULL,&o,FALSE);
  o.cursor_x=999;key(&o,0x40,3);assert(state.nav.cursor.column==79);charmap_search_close(NULL,&o,FALSE);
 }

 /* Burst arrows must preserve every event even without a timer tick. */
 {
  Object o={0};struct charmap_search_ui state={0};struct charmap_line rows[30]={0};
  for(unsigned i=0;i<30;i++){rows[i].prev=i?&rows[i-1]:NULL;rows[i].next=i<29?&rows[i+1]:NULL;}
  o.data.search=&state;o.data.top_of_scrollback=o.data.top_of_window=rows;o.cursor_x=10;o.cursor_y=20;
  key(&o,0x40,3);complete(&o);
  for(unsigned i=0;i<10;i++){key(&o,RAWKEY_UP,0);assert(state.nav.cursor.row==19-i);}
  for(unsigned i=0;i<10;i++){key(&o,RAWKEY_RIGHT,0);assert(state.nav.cursor.column==11+i);}
  for(unsigned i=0;i<10;i++){key(&o,RAWKEY_DOWN,0);key(&o,RAWKEY_LEFT,0);}
  assert(state.nav.cursor.row==20&&state.nav.cursor.column==10);complete(&o);
  charmap_search_close(NULL,&o,FALSE);
 }
 /* A completed character selection copies without any display tick. */
 {
  Object o={0};struct charmap_search_ui state={0};ULONG text[]={'a','b','c'};
  struct charmap_line row={.text=text,.size=3};o.data.search=&state;o.data.top_of_scrollback=o.data.top_of_window=&row;
  key(&o,0x40,3);key(&o,RAWKEY_RIGHT,0);key(&o,0x34,0);key(&o,0x15,0);
  assert(o.copies==1&&!state.nav.selecting&&state.active);
  assert(o.data.select_line_min==&row&&o.data.select_x_min==1&&o.data.select_x_max==2);
  key(&o,0x34,0);state.nav.pending=TRUE;state.nav.motion=CON_NAV_TOP;
  key(&o,0x15,0);assert(o.copies==1&&state.nav.selecting&&state.nav.pending);
  charmap_search_close(NULL,&o,FALSE);
 }
 /* Insert/toggle restores live view; Ctrl+C retains history. No cursor drift. */
 for(unsigned mode=0;mode<3;mode++)for(unsigned delta=0;delta<90;delta++){
  Object o={0};struct charmap_search_ui state={0};struct charmap_line rows[120]={0};
  for(unsigned i=0;i<120;i++){rows[i].prev=i?&rows[i-1]:NULL;rows[i].next=i<119?&rows[i+1]:NULL;}
  o.data.search=&state;o.data.top_of_scrollback=rows;o.data.top_of_window=&rows[2];o.data.scrollback_pos=2;
  o.data.unrendered=delta!=0;o.data.saved_top_of_window=&rows[2+delta];o.data.saved_scrollback_pos=2+delta;
  o.cursor_balance=delta?-1:0;o.cursor_x=9;o.cursor_y=29;o.window.Title=(UBYTE*)"exit owner";
  key(&o,0x40,3);complete(&o);assert(o.data.scrollback_pos==2);
  if(!mode)key(&o,0x17,0);else if(mode==1)key(&o,0x40,3);else key(&o,0x33,1);
  assert(!state.active&&!state.nav.cursor.line&&o.window.Title==(UBYTE*)"exit owner");
  assert(o.data.scrollback_pos==(mode<2?2+delta:2));
  assert(o.data.top_of_window==&rows[mode<2?2+delta:2]);
  assert(o.data.unrendered==(mode==2&&delta!=0));assert(o.cursor_balance==(mode==2&&delta?-1:0));
  assert(o.cursor_x==9&&o.cursor_y==29);
 }
 /* Accept stable matches into native selection; cancellation never does so. */
 {
  Object accept={0};struct charmap_search_ui state={0};struct charmap_line first={0},last={0};
  accept.data.search=&state;accept.data.scrollback_pos=13;
  for(unsigned mode=0;mode<6;mode++){
   memset(&state,0,sizeof(state));state.active=TRUE;state.found=TRUE;
   state.first_row=10;state.last_row=17;state.match=(struct con_search_match){{&first,2},{&last,9}};
   accept.data.select_line_min=accept.data.select_line_max=NULL;
   accept.window.Title=(UBYTE*)state.title;state.saved_title=(UBYTE*)"owner";
   if(mode==1)state.scanning=TRUE;
   if(mode==2)state.locating=TRUE;
   if(mode==3)state.found=FALSE;
   if(mode==4)state.match.end=state.match.first;
   if(mode==5)charmap_search_close(NULL,&accept,TRUE);else charmap_search_accept(NULL,&accept);
   if(!mode){assert(accept.data.select_line_min==&first&&accept.data.select_line_max==&last);
    assert(accept.data.select_x_min==2&&accept.data.select_x_max==9&&accept.data.select_y_min==-3&&accept.data.select_y_max==4);
    assert(!accept.data.active_selection&&!accept.data.ignore_drag);
   }else assert(!accept.data.select_line_min&&!accept.data.select_line_max);
   assert(!state.active&&!strcmp((char*)accept.window.Title,"owner"));
  }
 }

 /* Native byte titles retain query identity without splitting escapes. */
 {
  Object display={0};struct charmap_search_ui query={0};display.data.search=&query;
  ULONG sample[]={0x3c9,'\\','u','{','0','3','C','9','}', '\n','\r','\t',0x7f,0x85,0xe9,0x1f431,0x10ffff};
  query.length=sizeof(sample)/sizeof(*sample);memcpy(query.query,sample,sizeof(sample));
  charmap_search_title(NULL,&display,"found");
  assert(!strcmp(query.title,"Find: \\u{03C9}\\\\u{03C9}\\n\\r\\t\\u{007F}\\u{0085}\xe9\\u{1F431}\\u{10FFFF} [found]"));
  assert(!memcmp(query.query,sample,sizeof(sample)));
  char status[1000];memset(status,'x',sizeof(status));status[999]=0;
  for(unsigned n=0;n<=CON_SEARCH_MAX;n++)for(unsigned k=0;k<4;k++){
   query.length=n;query.regex_mode=k&1;
   for(unsigned j=0;j<n;j++)query.query[j]=(ULONG[]){'x',0x3c9,0x1f431,0x10ffff}[k];
   charmap_search_title(NULL,&display,status);
   size_t bytes=strlen(query.title);assert(bytes<sizeof(query.title)&&query.title[bytes-1]==']');
   unsigned at=query.regex_mode?7:6,count=0;
   while(query.title[at] && query.title[at]!=' ' && query.title[at]!='.'){
    if(query.title[at]=='\\'){
     assert(query.title[at+1]=='u'&&query.title[at+2]=='{');at+=3;
     while(query.title[at]!='}')assert(query.title[at++] && at<270);
     at++;
    }else at++;
    count++;
   }
   assert(count<=n);
   if(count<n)assert(!strncmp(query.title+at,"... [",5));else assert(query.title[at]==' ');
   for(unsigned j=0;j<n;j++)assert(query.query[j]==(ULONG[]){'x',0x3c9,0x1f431,0x10ffff}[k]);
  }
 }

 /* Virtual blank cells remain navigable across short and empty retained rows. */
 {ULONG text[]={'a','b','c'};struct charmap_line rows[3]={0};Object vi={0};
  rows[0].text=text;rows[0].size=3;rows[2].text=text;rows[2].size=3;
  for(unsigned i=0;i<3;i++){rows[i].prev=i?&rows[i-1]:NULL;rows[i].next=i<2?&rows[i+1]:NULL;}
  vi.data.top_of_scrollback=vi.data.top_of_window=rows;con_bindings_defaults(&vi.bindings);
  key(&vi,0x40,IEQUALIFIER_CONTROL|IEQUALIFIER_LSHIFT);complete(&vi);
  for(unsigned i=0;i<6;i++){key(&vi,RAWKEY_RIGHT,0);complete(&vi);}
  assert(vi.data.search->match.first.column==6&&vi.data.search->match.end.column==7);
  key(&vi,0x34,IEQUALIFIER_LALT);complete(&vi);
  assert(vi.data.search->match.first.column==6&&vi.data.search->match.end.column==7);
  key(&vi,RAWKEY_ESCAPE,0);complete(&vi);
  key(&vi,RAWKEY_DOWN,0);complete(&vi);assert(vi.data.search->nav.cursor.row==1&&vi.data.search->nav.cursor.column==6);
  key(&vi,RAWKEY_DOWN,0);complete(&vi);key(&vi,RAWKEY_UP,0);complete(&vi);key(&vi,RAWKEY_UP,0);complete(&vi);
  key(&vi,0x34,0);complete(&vi);for(unsigned i=0;i<5;i++){key(&vi,RAWKEY_LEFT,0);complete(&vi);}
  assert(vi.data.search->match.first.column==1&&vi.data.search->match.end.column==7);
  charmap_search_publish(NULL,&vi);assert(vi.data.select_x_min==1&&vi.data.select_x_max==7&&vi.data.selection_exact_end);
  charmap_search_close(NULL,&vi,FALSE);free(vi.data.search);
 }
 /* The charmap input path forwards actual terminal state, including query mode. */
 for(unsigned rule=0;rule<81;rule++)for(unsigned state=0;state<32;state++){
  Object o={0};struct charmap_search_ui ui={0};o.data.search=&ui;ui.active=TRUE;ui.nav_mode=!!(state&1);ui.nav_query=!!(state&16);
  o.keyboardAlternate=!!(state&2);o.conFlags=(state&4?CF_APPLICATION_CURSOR:0)|(state&8?CF_APPLICATION_KEYPAD:0);
  con_bindings_defaults(&o.bindings);o.bindings.bp_Version=18;o.bindings.bp_Count=1;memset(o.bindings.bp_Entries,0,sizeof(o.bindings.bp_Entries));
  unsigned n=rule,conditions=0;int expected=1;
  for(unsigned d=0;d<4;d++){unsigned condition=n%3;n/=3;conditions|=condition<<(5+2*d);int active=!!(state&(1<<d));if(!d&&(state&16))active=0;if(condition&&((condition==1)!=active))expected=0;}
  o.bindings.bp_Entries[0]=(struct ConsolengKeyBinding){CONSOLENG_BIND_RAW+0x50,conditions,CONSOLENG_BIND_COPY};
  struct InputEvent e={0x50,0,IECLASS_RAWKEY};assert(charmap_binding_action(NULL,&o,&e)==(expected?CONSOLENG_BIND_COPY:0));
 }
 /* The charmap input path forwards actual terminal state, including query mode. */
 for(unsigned rule=0;rule<243;rule++)for(unsigned state=0;state<64;state++){
  Object o={0};struct charmap_search_ui ui={0};o.data.search=&ui;ui.active=!!(state&32);ui.nav_mode=!!(state&1);ui.nav_query=!!(state&16);
  o.keyboardAlternate=!!(state&2);o.conFlags=(state&4?CF_APPLICATION_CURSOR:0)|(state&8?CF_APPLICATION_KEYPAD:0);
  con_bindings_defaults(&o.bindings);o.bindings.bp_Version=19;o.bindings.bp_Count=1;memset(o.bindings.bp_Entries,0,sizeof(o.bindings.bp_Entries));
  unsigned n=rule,conditions=0;int expected=1;
  for(unsigned d=0;d<5;d++){unsigned condition=n%3;n/=3;conditions|=condition<<(5+2*d);int active=!!(state&(1<<d));if(d==4)active=!(state&1)||!!(state&16);if((d==0||d==4)&&!(state&32))active=0;if(condition&&((condition==1)!=active))expected=0;}
  o.bindings.bp_Entries[0]=(struct ConsolengKeyBinding){CONSOLENG_BIND_RAW+0x50,conditions,CONSOLENG_BIND_COPY};
  struct InputEvent e={0x50,0,IECLASS_RAWKEY};assert(charmap_binding_action(NULL,&o,&e)==(expected?CONSOLENG_BIND_COPY:0));
 }
 /* Configured query/word/repeat bindings preserve the editor and app cursor. */
 for(unsigned action=59;action<=64;action++){
  ULONG text[]={'a',' ','b',' ','a',' ','b',' ','a'};struct charmap_line row={.text=text,.size=9};Object vi={0};
  vi.data.top_of_scrollback=vi.data.top_of_window=&row;vi.cursor_x=action<61?3:4;
  con_bindings_defaults(&vi.bindings);vi.bindings.bp_Version=17;
  vi.bindings.bp_Entries[3]=(struct ConsolengKeyBinding){CONSOLENG_BIND_RAW+0x50,4,action};
  key(&vi,0x40,3);complete(&vi);
  if(action>=63){key(&vi,0x3a,0);key(&vi,0x20,0);complete(&vi);key(&vi,RAWKEY_RETURN,0);complete(&vi);}
  key(&vi,0x50,IEQUALIFIER_CONTROL);
  if(action<61){assert(vi.data.search->nav_query&&vi.data.search->reverse_search==(action==60));key(&vi,0x20,0);}
  complete(&vi);unsigned expected[]={4,0,8,0,8,0};assert(vi.data.search->nav.cursor.column==expected[action-59]);
  if(action<61){key(&vi,RAWKEY_RETURN,0);complete(&vi);}
  assert(vi.data.search->nav_mode&&!vi.data.search->nav_query);
  key(&vi,0x3a,0);key(&vi,0x20,0);complete(&vi);UQUAD revision=vi.data.search->revision;
  key(&vi,0x50,IEQUALIFIER_CONTROL);assert(vi.data.search->nav_query&&vi.data.search->revision==revision&&vi.data.search->length==1);
  key(&vi,RAWKEY_ESCAPE,0);complete(&vi);assert(!vi.data.search->nav_query);
  assert(vi.cursor_x==(action<61?3:4)&&!vi.cursor_y);
  charmap_search_close(NULL,&vi,FALSE);free(vi.data.search);
 }
 /* Configured find actions have independent destinations and leave queries alone. */
 for(unsigned action=53;action<=58;action++){
  ULONG text[]={'a','x','a','x','a','x','a'};struct charmap_line row={0};Object vi={0};row.text=text;row.size=7;
  vi.data.top_of_scrollback=vi.data.top_of_window=&row;con_bindings_defaults(&vi.bindings);
  vi.bindings.bp_Version=16;vi.bindings.bp_Entries[3]=(struct ConsolengKeyBinding){CONSOLENG_BIND_RAW+0x50,4,action};
  key(&vi,0x40,IEQUALIFIER_CONTROL|IEQUALIFIER_LSHIFT);complete(&vi);
  key(&vi,RAWKEY_RIGHT,0);key(&vi,RAWKEY_RIGHT,0);complete(&vi);
  if(action>=57){key(&vi,0x23,0);key(&vi,0x32,0);complete(&vi);assert(vi.data.search->nav.cursor.column==3);}
  key(&vi,0x50,IEQUALIFIER_CONTROL);
  if(action<57){assert(vi.data.search->nav_find_pending);key(&vi,0x32,0);}
  complete(&vi);unsigned expected[]={3,1,2,2,5,1};assert(vi.data.search->nav.cursor.column==expected[action-53]);
  key(&vi,0x3a,0);key(&vi,0x20,0);complete(&vi);
  assert(vi.data.search->nav_query);UQUAD revision=vi.data.search->revision;
  key(&vi,0x50,IEQUALIFIER_CONTROL);assert(vi.data.search->nav_query&&!vi.data.search->nav_find_pending&&vi.data.search->revision==revision&&vi.data.search->length==1);
  charmap_search_close(NULL,&vi,FALSE);free(vi.data.search);
 }
 {ULONG text[]={'a','x','a','x','a'};struct charmap_line row={0};Object vi={0};row.text=text;row.size=5;
  vi.data.top_of_scrollback=vi.data.top_of_window=&row;con_bindings_defaults(&vi.bindings);
  key(&vi,0x40,IEQUALIFIER_CONTROL|IEQUALIFIER_LSHIFT);complete(&vi);
  key(&vi,0x23,0);assert(vi.data.search->nav_find_pending);key(&vi,0x32,0);complete(&vi);assert(vi.data.search->nav.cursor.column==1);
  key(&vi,0x29,0);complete(&vi);assert(vi.data.search->nav.cursor.column==3);
  key(&vi,0x38,0);complete(&vi);assert(vi.data.search->nav.cursor.column==1);
  key(&vi,0x23,IEQUALIFIER_LSHIFT);key(&vi,0x20,0);complete(&vi);assert(vi.data.search->nav.cursor.column==0);
  key(&vi,0x14,0);key(&vi,0x32,0);complete(&vi);assert(vi.data.search->nav.cursor.column==0);
  key(&vi,0x23,0);key(&vi,0x17,0);complete(&vi);assert(vi.data.search->active&&vi.data.search->nav.cursor.column==0);
  key(&vi,0x23,0);key(&vi,RAWKEY_ESCAPE,0);assert(!vi.data.search->nav_find_pending&&vi.data.search->active);
  key(&vi,0x23,0);key(&vi,0x33,IEQUALIFIER_CONTROL);assert(!vi.data.search->active&&!vi.data.search->nav_find_pending);free(vi.data.search);
 }
 {ULONG text[]={'x'};struct charmap_line rows[120]={0};Object vi={0};
  for(unsigned i=0;i<120;i++){rows[i].text=text;rows[i].size=1;rows[i].prev=i?&rows[i-1]:NULL;rows[i].next=i<119?&rows[i+1]:NULL;}
  vi.data.top_of_scrollback=vi.data.top_of_window=rows;con_bindings_defaults(&vi.bindings);
  key(&vi,0x40,IEQUALIFIER_CONTROL|IEQUALIFIER_LSHIFT);complete(&vi);test_history_limit=90;
  for(unsigned i=0;i<500;i++){
   unsigned target=i*37%120;vi.data.search->nav.cursor=(struct con_nav_cursor){&rows[target],target,7};
   vi.data.search->nav_dirty=TRUE;complete(&vi);key(&vi,0x31,0);complete(&vi);
   unsigned expected=target<14?0:target-14;if(expected>90)expected=90;
   assert(vi.data.scrollback_pos==expected&&vi.data.search->nav.cursor.row==target&&vi.data.search->nav.cursor.column==7);
  }
  url_present=TRUE;key(&vi,RAWKEY_RETURN,0);assert(url_opens==1&&url_row==vi.data.search->nav.cursor.line&&url_column==7);
  key(&vi,RAWKEY_RETURN,IEQUALIFIER_REPEAT);key(&vi,RAWKEY_RETURN,IEQUALIFIER_CONTROL);assert(url_opens==1);
  url_present=FALSE;key(&vi,RAWKEY_RETURN,0);assert(url_opens==1);
  url_present=TRUE;url_launch_ok=FALSE;key(&vi,RAWKEY_KP_ENTER,0);assert(url_opens==2&&url_beeps==1);
  vi.data.search->nav_dirty=TRUE;key(&vi,RAWKEY_RETURN,0);assert(url_opens==2);complete(&vi);
  key(&vi,0x23,0);key(&vi,RAWKEY_RETURN,0);assert(url_opens==2&&vi.data.search->nav_find_pending);
  key(&vi,RAWKEY_ESCAPE,0);complete(&vi);
  vi.bindings.bp_Version=11;vi.bindings.bp_Count=2;memset(vi.bindings.bp_Entries,0,sizeof(vi.bindings.bp_Entries));
  vi.bindings.bp_Entries[0]=(struct ConsolengKeyBinding){CONSOLENG_BIND_RAW+0x50,4,CONSOLENG_BIND_NAVOPENLINK};
  vi.bindings.bp_Entries[1]=(struct ConsolengKeyBinding){CONSOLENG_BIND_RAW+0x51,2,CONSOLENG_BIND_NAVCENTER};
  assert(con_bindings_valid(&vi.bindings));url_launch_ok=TRUE;
  unsigned opens=url_opens;
  key(&vi,0x50,IEQUALIFIER_CONTROL);assert(url_opens==opens+1);
  key(&vi,0x50,IEQUALIFIER_CONTROL|IEQUALIFIER_REPEAT);assert(url_opens==opens+1);
  key(&vi,0x50|128,IEQUALIFIER_CONTROL);assert(url_opens==opens+1);
  vi.data.search->nav_dirty=TRUE;key(&vi,0x50,IEQUALIFIER_CONTROL);assert(url_opens==opens+1);complete(&vi);
  vi.data.search->nav_find_pending=TRUE;key(&vi,0x50,IEQUALIFIER_CONTROL);assert(url_opens==opens+1&&vi.data.search->nav_find_pending);vi.data.search->nav_find_pending=FALSE;
  for(unsigned i=0;i<120;i++) {
   vi.data.search->nav.cursor=(struct con_nav_cursor){&rows[i],i,7};vi.data.search->nav_dirty=TRUE;complete(&vi);
   key(&vi,0x51,IEQUALIFIER_LALT);complete(&vi);unsigned expected=i<14?0:i-14;if(expected>90)expected=90;
   assert(vi.data.scrollback_pos==expected&&vi.data.search->nav.cursor.row==i);
  }
  vi.data.search->nav_mode=FALSE;vi.data.search->length=0;
  key(&vi,0x50,IEQUALIFIER_CONTROL);key(&vi,0x51,IEQUALIFIER_LALT);
  assert(url_opens==opens+1&&vi.data.search->length==0);vi.data.search->nav_mode=TRUE;
  key(&vi,RAWKEY_ESCAPE,0);charmap_search_close(NULL,&vi,FALSE);free(vi.data.search);test_history_limit=10000;url_present=FALSE;url_launch_ok=TRUE;
 }
 /* The same F1 chord enters navigation outside and centers inside. */
 {ULONG text[]={'x'};struct charmap_line rows[80]={0};Object vi={0};
  for(unsigned i=0;i<80;i++){rows[i].text=text;rows[i].size=1;rows[i].prev=i?&rows[i-1]:NULL;rows[i].next=i<79?&rows[i+1]:NULL;}
  vi.data.top_of_scrollback=vi.data.top_of_window=rows;vi.bindings=(struct ConsolengBindingPrefs){sizeof(vi.bindings),12,3,{{CONSOLENG_BIND_RAW+0x50,64,CONSOLENG_BIND_NAVIGATE},{CONSOLENG_BIND_RAW+0x50,32,CONSOLENG_BIND_NAVCENTER},{CONSOLENG_BIND_RAW+0x51,32,CONSOLENG_BIND_NAVOPENLINK}}};
  assert(con_bindings_valid(&vi.bindings));
  struct InputEvent ev={0x51,0,IECLASS_RAWKEY};assert(!charmap_search_key(NULL,&vi,&ev));
  key(&vi,0x50,0);complete(&vi);assert(vi.data.search->active&&vi.data.search->nav_mode);
  vi.data.search->nav.cursor=(struct con_nav_cursor){&rows[40],40,0};vi.data.search->nav_dirty=TRUE;complete(&vi);
  key(&vi,0x50,0);complete(&vi);assert(vi.data.search->active&&vi.data.scrollback_pos==26);
  url_present=TRUE;unsigned before=url_opens;key(&vi,0x51,0);assert(url_opens==before+1);
  vi.data.search->nav_query=TRUE;key(&vi,0x51,0);assert(url_opens==before+1);vi.data.search->nav_query=FALSE;
  key(&vi,0x33,IEQUALIFIER_CONTROL);assert(!vi.data.search->active);
  assert(!charmap_search_key(NULL,&vi,&ev));free(vi.data.search);url_present=FALSE;
 }
 /* Configurable selection actions share built-in toggle/type-switch behavior. */
 {ULONG text[]={'w','o','r','d',' ','x'};struct charmap_line row={0};Object vi={0};
  row.text=text;row.size=6;vi.data.top_of_scrollback=vi.data.top_of_window=&row;
  vi.bindings=(struct ConsolengBindingPrefs){sizeof(vi.bindings),13,5,{{CONSOLENG_BIND_RAW+0x50,0,CONSOLENG_BIND_NAVIGATE}}};
  for(unsigned i=0;i<4;i++)vi.bindings.bp_Entries[i+1]=(struct ConsolengKeyBinding){CONSOLENG_BIND_RAW+0x51+i,32,22+i};
  assert(con_bindings_valid(&vi.bindings));key(&vi,0x50,0);complete(&vi);
  for(unsigned i=0;i<4;i++) {
   key(&vi,0x51+i,0);complete(&vi);assert(vi.data.search->nav.selecting);
   assert(vi.data.search->nav.linewise==(i==1)&&vi.data.search->nav.blockwise==(i==2)&&vi.data.search->nav.semanticwise==(i==3));
   key(&vi,0x51+i,IEQUALIFIER_REPEAT);complete(&vi);assert(vi.data.search->nav.selecting);
   key(&vi,(0x51+i)|128,0);assert(vi.data.search->nav.selecting);
   key(&vi,0x51+i,0);complete(&vi);assert(!vi.data.search->nav.selecting);
  }
  key(&vi,0x51,0);complete(&vi);key(&vi,0x52,0);complete(&vi);assert(vi.data.search->nav.selecting&&vi.data.search->nav.linewise);
  key(&vi,RAWKEY_ESCAPE,0);charmap_search_close(NULL,&vi,FALSE);free(vi.data.search);
 }
 /* Configured movements match built-in keys, including repeats and selection. */
 {const UWORD codes[]={0x4f,0x4e,0x4c,0x4d,0x0a,4,0x24,0x24,0x11,0x12,0x35,0x11,0x12,0x35,0x1a,0x1b,5,6};
  const UWORD shifted[]={0,0,0,0,0,1,0,1,1,1,1,0,0,0,1,1,1,1};
  ULONG text[]={' ','(','w','o','r','d',')',' ','x'};struct charmap_line rows[80]={0};
  for(unsigned j=0;j<80;j++){rows[j].text=text;rows[j].size=j%7?9:0;rows[j].prev=j?&rows[j-1]:NULL;rows[j].next=j<79?&rows[j+1]:NULL;}
  for(unsigned i=0;i<18;i++)for(unsigned selected=0;selected<2;selected++) {
   Object actual={0},expected={0};Object *objects[]={&actual,&expected};
   for(unsigned j=0;j<2;j++){
    Object *v=objects[j];v->data.top_of_scrollback=v->data.top_of_window=rows;
    v->bindings=(struct ConsolengBindingPrefs){sizeof(v->bindings),14,2,{{CONSOLENG_BIND_RAW+0x50,0,CONSOLENG_BIND_NAVIGATE},{CONSOLENG_BIND_RAW+0x51,4,26+i}}};
    key(v,0x50,0);complete(v);v->data.search->nav.cursor=(struct con_nav_cursor){&rows[40],40,1};v->data.search->nav_dirty=TRUE;complete(v);
    if(selected){key(v,0x34,0);complete(v);}
   }
   for(unsigned repeat=0;repeat<3;repeat++){
    key(&expected,codes[i],(shifted[i]?IEQUALIFIER_LSHIFT:0)|(repeat?IEQUALIFIER_REPEAT:0));complete(&expected);
    key(&actual,0x51,IEQUALIFIER_CONTROL|(repeat?IEQUALIFIER_REPEAT:0));complete(&actual);
    assert(actual.data.search->nav.cursor.row==expected.data.search->nav.cursor.row&&actual.data.search->nav.cursor.column==expected.data.search->nav.cursor.column);
    assert(actual.data.search->nav.selecting==expected.data.search->nav.selecting);
   }
   actual.data.search->nav_query=TRUE;ULONG row=actual.data.search->nav.cursor.row,col=actual.data.search->nav.cursor.column;
   key(&actual,0x51,IEQUALIFIER_CONTROL);assert(actual.data.search->nav.cursor.row==row&&actual.data.search->nav.cursor.column==col);actual.data.search->nav_query=FALSE;
   for(unsigned j=0;j<2;j++){charmap_search_close(NULL,objects[j],FALSE);free(objects[j]->data.search);}
  }
 }
 /* Configured view movements match built-in keys, including repeats and selection. */
 {const UWORD codes[]={0x25,0x37,0x28,0x35,0x23,0x16,0x22,0x15,0x12};
  const UWORD quals[]={IEQUALIFIER_LSHIFT,IEQUALIFIER_LSHIFT,IEQUALIFIER_LSHIFT,IEQUALIFIER_CONTROL,IEQUALIFIER_CONTROL,IEQUALIFIER_CONTROL,IEQUALIFIER_CONTROL,IEQUALIFIER_CONTROL,IEQUALIFIER_CONTROL};
  ULONG text[]={' ','(','w','o','r','d',')',' ','x'};struct charmap_line rows[80]={0};
  for(unsigned j=0;j<80;j++){rows[j].text=text;rows[j].size=j%7?9:0;rows[j].prev=j?&rows[j-1]:NULL;rows[j].next=j<79?&rows[j+1]:NULL;}
  for(unsigned i=0;i<9;i++)for(unsigned selected=0;selected<2;selected++) {
   Object actual={0},expected={0};Object *objects[]={&actual,&expected};
   for(unsigned j=0;j<2;j++){
    Object *v=objects[j];v->data.top_of_scrollback=v->data.top_of_window=rows;
    v->bindings=(struct ConsolengBindingPrefs){sizeof(v->bindings),15,2,{{CONSOLENG_BIND_RAW+0x50,0,CONSOLENG_BIND_NAVIGATE},{CONSOLENG_BIND_RAW+0x51,4,44+i}}};
    key(v,0x50,0);complete(v);v->data.search->nav.cursor=(struct con_nav_cursor){&rows[40],40,1};v->data.search->nav_dirty=TRUE;complete(v);
    if(selected){key(v,0x34,0);complete(v);}
   }
   for(unsigned repeat=0;repeat<3;repeat++){
    key(&expected,codes[i],quals[i]|(repeat?IEQUALIFIER_REPEAT:0));complete(&expected);
    key(&actual,0x51,IEQUALIFIER_CONTROL|(repeat?IEQUALIFIER_REPEAT:0));complete(&actual);
    assert(actual.data.search->nav.cursor.row==expected.data.search->nav.cursor.row&&actual.data.search->nav.cursor.column==expected.data.search->nav.cursor.column);
    assert(actual.data.search->nav.selecting==expected.data.search->nav.selecting);
    assert(actual.data.scrollback_pos==expected.data.scrollback_pos);
   }
   actual.data.search->nav_query=TRUE;ULONG row=actual.data.search->nav.cursor.row,col=actual.data.search->nav.cursor.column;
   key(&actual,0x51,IEQUALIFIER_CONTROL);assert(actual.data.search->nav.cursor.row==row&&actual.data.search->nav.cursor.column==col);actual.data.search->nav_query=FALSE;
   for(unsigned j=0;j<2;j++){charmap_search_close(NULL,objects[j],FALSE);free(objects[j]->data.search);}
  }
 }
 /* Repeated history/live jumps, including hidden tabs, balance the cursor. */
 for(int hidden=0;hidden<2;hidden++){
  struct charmap_line history={0},middle={0},live={0};Object view={0};
  view.hidden=hidden;view.data.top_of_window=&live;view.data.scrollback_pos=100;
  struct charmap_search_ui target={0};target.view=&live;target.view_row=100;
  charmap_search_view(NULL,&view,&target);assert(!view.data.unrendered&&view.cursor_balance==0&&view.cursor_operations==0);
  for(int i=0;i<100;i++){
   target.view=&history;target.view_row=0;charmap_search_view(NULL,&view,&target);
   assert(view.data.unrendered&&view.cursor_balance==-1&&view.data.saved_top_of_window==&live&&view.data.saved_scrollback_pos==100);
   target.view=&middle;target.view_row=50;charmap_search_view(NULL,&view,&target);
   assert(view.data.unrendered&&view.cursor_balance==-1&&view.data.select_y_min==50);
   target.view=&live;target.view_row=100;charmap_search_view(NULL,&view,&target);
   assert(!view.data.unrendered&&view.cursor_balance==0&&view.data.select_y_min==0);
  }
 }
 ULONG cells[]={'a','b','a','b','a'};struct charmap_line row={0};row.text=cells;row.size=5;
 {
  Object reverse={0};reverse.data.top_of_scrollback=reverse.data.top_of_window=&row;
  con_bindings_defaults(&reverse.bindings);reverse.bindings.bp_Version=4;
  reverse.bindings.bp_Entries[3]=(struct ConsolengKeyBinding){CONSOLENG_BIND_RAW+0x51,0,CONSOLENG_BIND_SEARCH_BACKWARD};
  key(&reverse,0x51,0);assert(reverse.data.search->reverse_search);
  key(&reverse,0x20,0);complete(&reverse);assert(reverse.data.search->match.first.column==4);
  key(&reverse,RAWKEY_RETURN,0);complete(&reverse);assert(reverse.data.search->match.first.column==2);
  key(&reverse,RAWKEY_RETURN,IEQUALIFIER_LSHIFT);complete(&reverse);assert(reverse.data.search->match.first.column==4);
  key(&reverse,RAWKEY_BACKSPACE,0);key(&reverse,0x20,0);complete(&reverse);assert(reverse.data.search->match.first.column==4);
  charmap_search_close(NULL,&reverse,FALSE);free(reverse.data.search);
 }
 {
  Object vi={0};vi.data.top_of_scrollback=vi.data.top_of_window=&row;
  con_bindings_defaults(&vi.bindings);vi.bindings.bp_Version=6;
  vi.bindings.bp_Entries[3].kb_Action=CONSOLENG_BIND_NAVIGATE;
  key(&vi,0x23,3);complete(&vi);assert(vi.data.search->nav_mode&&vi.data.search->match.first.column==0);
  key(&vi,0x28,0);complete(&vi);assert(vi.data.search->match.first.column==1);
  key(&vi,0x34,0);complete(&vi);assert(vi.data.search->nav.selecting);
  key(&vi,0x28,0);complete(&vi);assert(vi.data.search->match.first.column==1&&vi.data.search->match.end.column==3);
  key(&vi,0x15,0);complete(&vi);assert(vi.data.search->active&&!vi.data.search->nav.selecting);
  key(&vi,RAWKEY_ESCAPE,0);complete(&vi);assert(vi.data.search->active);key(&vi,0x17,0);assert(!vi.data.search->active);
  key(&vi,0x23,3);complete(&vi);key(&vi,4,IEQUALIFIER_LSHIFT);complete(&vi);assert(vi.data.search->nav.cursor.column==4);
  key(&vi,0x0a,0);complete(&vi);assert(vi.data.search->nav.cursor.column==0);
  charmap_search_close(NULL,&vi,TRUE);assert(!vi.data.search->nav.cursor.line);
  key(&vi,0x40,IEQUALIFIER_CONTROL|IEQUALIFIER_LSHIFT);complete(&vi);assert(vi.data.search->nav_mode);
  assert(press(&vi,0x40,IEQUALIFIER_CONTROL|IEQUALIFIER_LSHIFT|IEQUALIFIER_REPEAT));assert(vi.data.search->active);
  key(&vi,0x33,IEQUALIFIER_CONTROL);assert(!vi.data.search->active);
  ULONG words[]={'a','b',',','c','d',' ','e','f'};row.text=words;row.size=8;
  key(&vi,0x23,3);complete(&vi);
  key(&vi,0x11,0);complete(&vi);assert(vi.data.search->nav.cursor.column==2);
  key(&vi,0x11,0);complete(&vi);assert(vi.data.search->nav.cursor.column==3);
  key(&vi,0x12,0);complete(&vi);assert(vi.data.search->nav.cursor.column==4);
  key(&vi,0x35,0);complete(&vi);assert(vi.data.search->nav.cursor.column==3);
  key(&vi,0x11,IEQUALIFIER_LSHIFT);complete(&vi);assert(vi.data.search->nav.cursor.column==6);
  key(&vi,0x35,IEQUALIFIER_LSHIFT);complete(&vi);assert(vi.data.search->nav.cursor.column==0);
  charmap_search_close(NULL,&vi,TRUE);row.text=cells;row.size=5;
  ULONG braces[]={'(','a','(','b',')','c',')'};row.text=braces;row.size=7;
  key(&vi,0x23,3);complete(&vi);key(&vi,5,IEQUALIFIER_LSHIFT);complete(&vi);assert(vi.data.search->nav.cursor.column==6);
  key(&vi,5,IEQUALIFIER_LSHIFT);complete(&vi);assert(vi.data.search->nav.cursor.column==0);
  charmap_search_close(NULL,&vi,TRUE);row.text=cells;row.size=5;
  free(vi.data.search);
 }
 /* Navigation keeps visible rows stable and scrolls only at viewport edges. */
 {
  Object vi={0};struct charmap_line rows[40]={0};ULONG text[]={'x'};
  for(unsigned n=0;n<40;n++){rows[n].text=text;rows[n].size=1;
   rows[n].prev=n?&rows[n-1]:NULL;rows[n].next=n<39?&rows[n+1]:NULL;}
  vi.data.top_of_scrollback=vi.data.top_of_window=rows;
  key(&vi,0x40,IEQUALIFIER_CONTROL|IEQUALIFIER_LSHIFT);complete(&vi);
  for(unsigned n=1;n<40;n++){
   key(&vi,RAWKEY_DOWN,0);complete(&vi);
   assert(vi.data.search->nav.cursor.row==n);
   assert(vi.data.scrollback_pos==(n<30?0:n-29));
   assert(vi.data.top_of_window==&rows[vi.data.scrollback_pos]);
  }
  for(int n=38;n>=0;n--){key(&vi,RAWKEY_UP,0);complete(&vi);
   assert(vi.data.scrollback_pos==(n<10?(unsigned)n:10));
   assert(vi.data.top_of_window==&rows[vi.data.scrollback_pos]);}
  ULONG indented[]={' ','\t','x'};rows[14].text=indented;rows[14].size=3;
  key(&vi,0x37,IEQUALIFIER_LSHIFT);complete(&vi);
  assert(vi.data.search->nav.cursor.row==14&&vi.data.search->nav.cursor.column==2&&vi.data.scrollback_pos==0);
  key(&vi,0x28,IEQUALIFIER_LSHIFT);complete(&vi);assert(vi.data.search->nav.cursor.row==29&&vi.data.scrollback_pos==0);
  key(&vi,0x25,IEQUALIFIER_LSHIFT);complete(&vi);assert(vi.data.search->nav.cursor.row==0&&vi.data.scrollback_pos==0);
  charmap_search_close(NULL,&vi,TRUE);assert(vi.cursor_balance==0);free(vi.data.search);
 }
 /* Page motions move cursor and viewport; line scroll only clamps cursor. */
 {
  Object vi={0};struct charmap_line rows[100]={0};ULONG text[]={' ',' ','x'};
  for(unsigned n=0;n<100;n++){rows[n].text=text;rows[n].size=3;
   rows[n].prev=n?&rows[n-1]:NULL;rows[n].next=n<99?&rows[n+1]:NULL;}
  vi.data.top_of_scrollback=vi.data.top_of_window=rows;test_history_limit=70;
  key(&vi,0x40,IEQUALIFIER_CONTROL|IEQUALIFIER_LSHIFT);complete(&vi);
  const unsigned codes[]={0x23,0x22,0x35,0x16,0x12,0x15};
  const unsigned targets[]={30,45,15,0,1,1},views[]={30,45,15,0,1,0};
  for(unsigned i=0;i<6;i++){key(&vi,codes[i],IEQUALIFIER_CONTROL);complete(&vi);
   assert(vi.data.search->nav.cursor.row==targets[i]&&vi.data.scrollback_pos==views[i]);
   assert(vi.data.search->nav.cursor.column==2);}
  for(unsigned i=0;i<10;i++){key(&vi,0x23,IEQUALIFIER_CONTROL);complete(&vi);}
  assert(vi.data.search->nav.cursor.row==99&&vi.data.scrollback_pos==70);
  key(&vi,0x12,IEQUALIFIER_CONTROL);complete(&vi);assert(vi.data.search->nav.cursor.row==99&&vi.data.scrollback_pos==70);
  key(&vi,0x15,IEQUALIFIER_CONTROL);complete(&vi);assert(vi.data.search->nav.cursor.row==98&&vi.data.scrollback_pos==69&&vi.data.search->nav.cursor.column==2);
  key(&vi,0x35,IEQUALIFIER_CONTROL);complete(&vi);assert(vi.data.search->nav.cursor.row==68&&vi.data.scrollback_pos==39);
  unsigned seed=185;
  for(unsigned i=0;i<2000;i++){
   seed=seed*1664525u+1013904223u;unsigned top=seed%71;
   seed=seed*1664525u+1013904223u;unsigned cursor=top+seed%30;
   seed=seed*1664525u+1013904223u;unsigned amount=1+seed%40;
   BOOL up=!!(seed&128),page=!!(seed&256);
   int expected_view=(int)top+(up?-(int)amount:(int)amount);
   if(expected_view<0)expected_view=0;
   if(expected_view>70)expected_view=70;
   int expected_cursor=(int)cursor+(page?(up?-(int)amount:(int)amount):0);
   if(expected_cursor<expected_view)expected_cursor=expected_view;
   if(expected_cursor>expected_view+29)expected_cursor=expected_view+29;
   vi.data.scrollback_pos=top;vi.data.top_of_window=&rows[top];
   vi.data.search->nav.cursor=(struct con_nav_cursor){&rows[cursor],cursor,0};
   charmap_nav_scroll(NULL,&vi,amount,up,page);complete(&vi);
   assert(vi.data.scrollback_pos==(unsigned)expected_view);
   assert(vi.data.search->nav.cursor.row==(unsigned)expected_cursor);
   assert(vi.data.search->nav.cursor.column==(page?2:0));
  }
  charmap_search_close(NULL,&vi,TRUE);free(vi.data.search);test_history_limit=10000;
 }
 /* Line selection expands soft wraps and switches type without losing anchor. */
 {
  Object vi={0};struct charmap_line rows[3]={0};ULONG text[]={'a','b'};
  for(unsigned n=0;n<3;n++){rows[n].text=text;rows[n].size=2;rows[n].prev=n?&rows[n-1]:NULL;rows[n].next=n<2?&rows[n+1]:NULL;}
  rows[0].wrapColumns=2;vi.data.top_of_scrollback=vi.data.top_of_window=rows;
  key(&vi,0x40,IEQUALIFIER_CONTROL|IEQUALIFIER_LSHIFT);complete(&vi);
  key(&vi,RAWKEY_DOWN,0);complete(&vi);key(&vi,0x34,IEQUALIFIER_LSHIFT);complete(&vi);
  assert(vi.data.search->first_row==0&&vi.data.search->last_row==1&&vi.data.search->match.first.column==0&&vi.data.search->match.end.column==80);
  charmap_search_publish(NULL,&vi);assert(!vi.data.selection_exact_end);
  key(&vi,0x34,0);complete(&vi);assert(vi.data.search->nav.selecting&&!vi.data.search->nav.linewise);
  assert(vi.data.search->first_row==1&&vi.data.search->last_row==1);charmap_search_publish(NULL,&vi);assert(vi.data.selection_exact_end);
  key(&vi,0x34,IEQUALIFIER_LSHIFT);complete(&vi);key(&vi,RAWKEY_DOWN,0);complete(&vi);
  assert(vi.data.search->first_row==0&&vi.data.search->last_row==2);
  key(&vi,0x34,IEQUALIFIER_LSHIFT);complete(&vi);assert(!vi.data.search->nav.selecting);
  charmap_search_close(NULL,&vi,TRUE);free(vi.data.search);
 }
 /* Rectangular bounds remain column-based on every row, including wide edges. */
 {
  Object vi={0};struct charmap_line rows[3]={0};ULONG text[]={'a','b','c','d'};
  for(unsigned n=0;n<3;n++){rows[n].text=text;rows[n].size=4;rows[n].prev=n?&rows[n-1]:NULL;rows[n].next=n<2?&rows[n+1]:NULL;}
  vi.data.top_of_scrollback=vi.data.top_of_window=rows;
  key(&vi,0x40,IEQUALIFIER_CONTROL|IEQUALIFIER_LSHIFT);complete(&vi);key(&vi,RAWKEY_RIGHT,0);complete(&vi);
  key(&vi,0x34,IEQUALIFIER_CONTROL);complete(&vi);key(&vi,RAWKEY_DOWN,0);complete(&vi);key(&vi,RAWKEY_DOWN,0);complete(&vi);key(&vi,RAWKEY_RIGHT,0);complete(&vi);
  assert(vi.data.search->nav.blockwise&&vi.data.search->match.first.column==1&&vi.data.search->match.end.column==3);
  for(unsigned row=0;row<3;row++)for(unsigned col=0;col<5;col++){
   ULONG stop=5;assert(charmap_search_flags(vi.data.search,&rows[row],row,col,5,&stop,0)==(col>=1&&col<3?1:0));}
  charmap_search_publish(NULL,&vi);assert(vi.data.selection_block&&vi.data.selection_exact_end);
  ULONG wide[]={'a',0x4e2d,0xfffffffe,'b'};rows[1].text=wide;
  vi.data.search->match.first.column=2;vi.data.search->match.end.column=3;
  for(unsigned col=0;col<4;col++){ULONG stop=4;assert(charmap_search_flags(vi.data.search,&rows[1],1,col,4,&stop,0)==(col==1||col==2?1:0));}
  key(&vi,0x34,IEQUALIFIER_LSHIFT);complete(&vi);assert(vi.data.search->nav.linewise&&!vi.data.search->nav.blockwise);
  key(&vi,0x34,0);complete(&vi);assert(vi.data.search->nav.selecting&&!vi.data.search->nav.linewise&&!vi.data.search->nav.blockwise);
  charmap_search_close(NULL,&vi,TRUE);free(vi.data.search);
 }
 {
  Object vi={0};ULONG text[]={'a','b',',','c','d'};struct charmap_line row={0};row.text=text;row.size=5;
  vi.data.top_of_scrollback=vi.data.top_of_window=&row;
  key(&vi,0x40,IEQUALIFIER_CONTROL|IEQUALIFIER_LSHIFT);complete(&vi);key(&vi,RAWKEY_RIGHT,0);complete(&vi);
  key(&vi,0x34,IEQUALIFIER_LALT);complete(&vi);assert(vi.data.search->nav.semanticwise&&vi.data.search->match.first.column==0&&vi.data.search->match.end.column==2);
  key(&vi,RAWKEY_RIGHT,0);complete(&vi);key(&vi,RAWKEY_RIGHT,0);complete(&vi);assert(vi.data.search->match.first.column==0&&vi.data.search->match.end.column==5);
  key(&vi,0x34,IEQUALIFIER_CONTROL);complete(&vi);assert(vi.data.search->nav.blockwise&&!vi.data.search->nav.semanticwise&&vi.data.search->nav.anchor.column==1);
  key(&vi,0x34,IEQUALIFIER_LALT);complete(&vi);assert(vi.data.search->nav.semanticwise&&!vi.data.search->nav.blockwise);
  key(&vi,0x34,IEQUALIFIER_LALT);complete(&vi);assert(!vi.data.search->nav.selecting);
  charmap_search_close(NULL,&vi,TRUE);free(vi.data.search);
 }
 Object one={0},two={0};one.data.top_of_scrollback=one.data.top_of_window=&row;two.data.top_of_scrollback=two.data.top_of_window=&row;
 one.window.Title=(UBYTE*)"one";two.window.Title=(UBYTE*)"two";
 unsigned before_idle=allocations;assert(!press(&one,0x20,0)&&allocations==before_idle);fail_alloc=1;assert(!press(&one,0x23,3)&&!one.data.search);fail_alloc=0;
 open_search(&one);key(&one,0x20,0);key(&one,0x21,0);key(&one,0x20,0);complete(&one);
 assert(!one.data.unrendered&&one.cursor_balance==0);
 assert(one.data.search->found&&one.data.search->match.first.column==0&&one.data.search->match.end.column==3);
 assert(!two.data.search&&!strcmp((char*)two.window.Title,"two"));
 key(&one,RAWKEY_RETURN,0);complete(&one);assert(one.data.search->match.first.column==2);
 key(&one,RAWKEY_RETURN,2);complete(&one);assert(one.data.search->match.first.column==0);
 key(&one,RAWKEY_RETURN,2);complete(&one);assert(one.data.search->match.first.column==2);
 for(unsigned flags=0;flags<16;flags++)for(unsigned x=0;x<6;x++){
  ULONG stop=999;UBYTE got=charmap_search_flags(one.data.search,NULL,0,x,5,&stop,flags);
  assert(got==(flags^((x>=2&&x<5)?1:0)));assert(stop==(x<2?2:x<5?5:999));
 }
 struct charmap_search_ui overlay_state=*one.data.search;
 overlay_state.first_row=1;overlay_state.last_row=3;overlay_state.match.first.column=2;overlay_state.match.end.column=4;
 for(unsigned row=0;row<5;row++)for(unsigned col=0;col<8;col++)for(unsigned flags=0;flags<16;flags++){
  ULONG stop=99;int inside=(row==1&&col>=2)||row==2||(row==3&&col<4);
  assert(charmap_search_flags(&overlay_state,NULL,row,col,8,&stop,flags)==(flags^(inside?1:0)));
  assert(stop==(row==1?(col<2?2:8):row==2?8:row==3&&col<4?4:99));
 }
 open_search(&two);key(&two,0x21,0);complete(&two);assert(two.data.search->match.first.column==1&&one.data.search->match.first.column==2);
 key(&one,RAWKEY_ESCAPE,0);assert(!one.data.search->active&&!strcmp((char*)one.window.Title,"one"));
 assert(!one.data.search->selection.scan.stream.cursor.line&&!one.data.search->seek&&!one.data.search->view&&!one.data.search->match.first.line);
 assert(!press(&one,0x20,0));
 /* Closing on mutation must still swallow the matching release/repeat. */
 open_search(&one);assert(press(&one,0x20,0));charmap_search_close(NULL,&one,TRUE);
 assert(press(&one,0x20,IEQUALIFIER_REPEAT));assert(press(&one,0x20|128,0));assert(!press(&one,0x20,0));
 open_search(&one);one.window.Title=(UBYTE*)"external";charmap_search_tick(NULL,&one);assert(!one.data.search->active&&!strcmp((char*)one.window.Title,"external"));
 open_search(&one);one.window.Title=(UBYTE*)"new owner";assert(!press(&one,0x20,0));assert(!one.data.search->active&&!strcmp((char*)one.window.Title,"new owner"));
 open_search(&one);for(unsigned i=0;i<CON_SEARCH_MAX+9;i++)key(&one,0x20,0);assert(one.data.search->length==CON_SEARCH_MAX);complete(&one);assert(!one.data.search->found&&strstr((char*)one.window.Title,"not found"));


 /* Clipboard status survives independently finishing found/not-found scans.
  * Success, editing, navigation and reopening clear it without stopping scans. */
 for(int absent=0;absent<2;absent++)for(int phase=0;phase<3;phase++){
  charmap_search_close(NULL,&one,FALSE);open_search(&one);
  key(&one,0x20,0);if(absent){key(&one,0x20,0);}
  assert(one.data.search->scanning);
  request_available=phase!=2;key(&one,0x34,IEQUALIFIER_RCOMMAND);
  struct P_Console_QueryPaste rejected={0,requested_revision,(const UBYTE*)"",0,TRUE,FALSE};
  if(phase==1)charmap_search_paste(NULL,&one,&rejected);
  const char *notice=phase==0?"pasting":phase==1?"paste rejected":"clipboard busy";
  complete(&one);assert(strstr((char*)one.window.Title,notice));
  assert(one.data.search->found==!absent&&one.data.search->length==(ULONG)(absent?2:1));
  for(int i=0;i<10;i++)charmap_search_tick(NULL,&one);
  assert(strstr((char*)one.window.Title,notice));
  key(&one,RAWKEY_RETURN,0);complete(&one);assert(!one.data.search->paste_status&&!strstr((char*)one.window.Title,notice));
  charmap_search_paste(NULL,&one,&rejected);assert(!one.data.search->paste_status); /* stale */
  request_available=TRUE;
  key(&one,0x34,IEQUALIFIER_RCOMMAND);rejected.Revision=requested_revision;rejected.Valid=TRUE;
  charmap_search_paste(NULL,&one,&rejected);complete(&one);assert(!one.data.search->paste_status);
  key(&one,RAWKEY_ESCAPE,0);assert(!one.data.search->paste_status);
 }
 open_search(&one);
 /* Word deletion is scalar-based and recognizes Unicode whitespace. */
 const ULONG whitespace[]={9,10,11,12,13,32,0x85,0xa0,0x1680,0x2000,0x2001,0x2002,0x2003,0x2004,0x2005,0x2006,0x2007,0x2008,0x2009,0x200a,0x2028,0x2029,0x202f,0x205f,0x3000};
 for(unsigned ws=0;ws<sizeof(whitespace)/sizeof(*whitespace);ws++)for(unsigned count=0;count<=CON_SEARCH_MAX;count++)for(int action=0;action<3;action++){
  struct charmap_search_ui *q=one.data.search;
  q->length=count;for(unsigned i=0;i<count;i++)q->query[i]=i%4<2?0x1f431:whitespace[ws];
  unsigned expected=count;
  if(action==2)expected=0;
  else{while(expected&&(expected-1)%4>=2)expected--;while(expected&&(expected-1)%4<2)expected--;}
  UQUAD before=q->revision;
  key(&one,action==0?0x11:action==1?RAWKEY_BACKSPACE:0x16,IEQUALIFIER_CONTROL);
  assert(q->length==expected&&q->revision>before);
  for(unsigned i=0;i<expected;i++)assert(q->query[i]==(i%4<2?0x1f431:whitespace[ws]));
 }
 /* Editing cancels a pending clipboard response, even when already empty. */
 key(&one,0x34,IEQUALIFIER_RCOMMAND);
 struct P_Console_QueryPaste cancelled={0,requested_revision,(const UBYTE*)"old",3,TRUE,TRUE};
 key(&one,0x16,IEQUALIFIER_CONTROL);charmap_search_paste(NULL,&one,&cancelled);assert(one.data.search->length==0);
 key(&one,0x34,IEQUALIFIER_RCOMMAND);cancelled.Revision=requested_revision;
 key(&one,0x11,IEQUALIFIER_CONTROL);charmap_search_paste(NULL,&one,&cancelled);assert(one.data.search->length==0);
 requests=0;

 /* History evicts only the oldest query and deduplicates adjacent saves. */
 Object recall={0};recall.window.Title=(UBYTE*)"history";
 for(unsigned item=0;item<40;item++){
  open_search(&recall);recall.data.search->query[0]=0x100+item;recall.data.search->length=1;
  key(&recall,RAWKEY_ESCAPE,0);
 }
 assert(recall.data.search->history_count==16);
 open_search(&recall);key(&recall,0x20,0); /* draft a */
 for(unsigned i=0;i<16;i++){key(&recall,RAWKEY_UP,0);assert(recall.data.search->length==1&&recall.data.search->query[0]==0x100+39-i);}
 key(&recall,RAWKEY_UP,0);assert(recall.data.search->query[0]==0x100+24);
 for(unsigned i=0;i<16;i++)key(&recall,RAWKEY_DOWN,0);
 assert(recall.data.search->length==1&&recall.data.search->query[0]=='a');
 key(&recall,RAWKEY_UP,0);key(&recall,RAWKEY_ESCAPE,0);assert(recall.data.search->history_count==16&&recall.data.search->history[1][0]==0x100+38);
 open_search(&recall);key(&recall,RAWKEY_UP,0);key(&recall,0x21,0); /* edited recall is a fresh draft */
 key(&recall,RAWKEY_UP,0);assert(recall.data.search->length==1);
 key(&recall,RAWKEY_DOWN,0);assert(recall.data.search->length==2&&recall.data.search->query[1]=='b');
 key(&recall,0x34,IEQUALIFIER_RCOMMAND);
 struct P_Console_QueryPaste old_history={0,requested_revision,(const UBYTE*)"stale",5,TRUE,TRUE};
 key(&recall,RAWKEY_UP,0);charmap_search_paste(NULL,&recall,&old_history);assert(recall.data.search->length==1);
 charmap_search_close(NULL,&recall,FALSE);free(recall.data.search);requests=0;

 /* Maximum-size history copies retain every scalar, independent of units. */
 struct charmap_search_ui max_history={0};
 max_history.length=CON_SEARCH_MAX;
 for(unsigned item=0;item<32;item++){
  for(unsigned col=0;col<CON_SEARCH_MAX;col++)max_history.query[col]=0x10000+item*256+col;
  charmap_search_remember(&max_history);
 }
 max_history.length=CON_SEARCH_MAX;for(unsigned col=0;col<CON_SEARCH_MAX;col++)max_history.query[col]=0x1f431;
 for(unsigned item=0;item<16;item++){
  assert(charmap_search_recall(&max_history,TRUE)&&max_history.length==CON_SEARCH_MAX);
  for(unsigned col=0;col<CON_SEARCH_MAX;col++)assert(max_history.query[col]==0x10000+(31-item)*256+col);
 }
 assert(!charmap_search_recall(&max_history,TRUE));
 for(unsigned item=0;item<16;item++)assert(charmap_search_recall(&max_history,FALSE));
 for(unsigned col=0;col<CON_SEARCH_MAX;col++)assert(max_history.query[col]==0x1f431);
 assert(!charmap_search_recall(&max_history,FALSE));
 /* Clipboard results belong to one active query revision. */
 charmap_search_close(NULL,&one,FALSE);open_search(&one);key(&one,0x34,IEQUALIFIER_RCOMMAND);
 assert(requests==1&&requested_revision==one.data.search->revision);
 struct P_Console_QueryPaste paste={0,requested_revision,(const UBYTE*)"a\xce\xa9",3,TRUE,TRUE};
 charmap_search_paste(NULL,&one,&paste);assert(one.data.search->length==2&&one.data.search->query[1]==0x3a9);
 charmap_search_paste(NULL,&one,&paste);assert(one.data.search->length==2); /* duplicate stale */
 key(&one,0x34,IEQUALIFIER_CONTROL|IEQUALIFIER_LSHIFT);paste.Revision=requested_revision;
 key(&one,0x21,0);charmap_search_paste(NULL,&one,&paste);assert(one.data.search->length==3);
 key(&one,0x34,IEQUALIFIER_RCOMMAND);paste.Revision=requested_revision;
 charmap_search_close(NULL,&one,FALSE);open_search(&one);charmap_search_paste(NULL,&one,&paste);assert(one.data.search->length==0);
 key(&one,0x34,IEQUALIFIER_RCOMMAND);paste.Revision=requested_revision;paste.Data=(const UBYTE*)"\xc0\xaf";paste.Length=2;
 charmap_search_paste(NULL,&one,&paste);assert(one.data.search->length==0&&strstr((char*)one.window.Title,"rejected"));
 one.hidden=TRUE;paste.Data=(const UBYTE*)"abc";paste.Length=3;charmap_search_paste(NULL,&one,&paste);assert(!one.data.search->active);one.hidden=FALSE;
 open_search(&one);for(unsigned i=0;i<CON_SEARCH_MAX;i++)key(&one,0x20,0);
 key(&one,RAWKEY_BACKSPACE,0);assert(one.data.search->length==CON_SEARCH_MAX-1);
 /* A release can disappear while a search is cancelled or focus changes.
  * Consume held repeats, but a fresh down must recover immediately. */
 charmap_search_close(NULL,&one,FALSE);
 assert(press(&one,0x23,3)&&one.data.search->active);
 charmap_search_close(NULL,&one,FALSE);
 assert(press(&one,0x23,3|IEQUALIFIER_REPEAT)&&!one.data.search->active);
 assert(press(&one,0x23,3)&&one.data.search->active);
 assert(press(&one,0x23|128,3));
 charmap_search_close(NULL,&one,FALSE);
 /* Explicit assignments must override the legacy Ctrl+Shift+V alias. */
 one.bindings.bp_Entries[one.bindings.bp_Count++]=(struct ConsolengKeyBinding){'v',CONSOLENG_BIND_CTRL|CONSOLENG_BIND_SHIFT,CONSOLENG_BIND_COPY};
 open_search(&one);unsigned explicitRequests=requests;
 key(&one,0x34,IEQUALIFIER_CONTROL|IEQUALIFIER_LSHIFT);assert(requests==explicitRequests);
 key(&one,0x34,IEQUALIFIER_RCOMMAND);assert(requests==explicitRequests+1);
 charmap_search_close(NULL,&one,FALSE);one.bindings.bp_Count--;
 /* Configured paste must work inside the query as well as in applications. */
 one.bindings.bp_Version=2;one.bindings.bp_Entries[1]=(struct ConsolengKeyBinding){CONSOLENG_BIND_RAW+0x50,0,CONSOLENG_BIND_PASTE};
 open_search(&one);unsigned beforeRequests=requests;
 key(&one,0x50,0);assert(requests==beforeRequests+1);
 key(&one,0x34,IEQUALIFIER_RCOMMAND);assert(requests==beforeRequests+1);
 key(&one,0x34,IEQUALIFIER_CONTROL|IEQUALIFIER_LSHIFT);assert(requests==beforeRequests+1);

 charmap_search_close(NULL,&one,FALSE);open_search(&one);
 one.data.search->query[0]='a';one.data.search->query[1]='.';one.data.search->length=2;
 key(&one,0x13,IEQUALIFIER_CONTROL);complete(&one);
 assert(one.data.search->regex_mode&&one.data.search->found&&one.data.search->match.first.column==0&&one.data.search->match.end.column==2);
 assert(strstr((char*)one.window.Title,"Regex: a."));
 key(&one,RAWKEY_RETURN,0);complete(&one);assert(one.data.search->match.first.column==2);
 key(&one,RAWKEY_RETURN,IEQUALIFIER_LSHIFT);complete(&one);assert(one.data.search->match.first.column==0);
 key(&one,0x13,IEQUALIFIER_CONTROL|IEQUALIFIER_REPEAT);assert(one.data.search->regex_mode);
 one.data.search->query[0]='[';one.data.search->length=1;charmap_search_start(NULL,&one,FALSE,FALSE);complete(&one);
 assert(!one.data.search->found&&!one.data.search->scanning&&strstr((char*)one.window.Title,"invalid or unsupported regex"));
 key(&one,0x13,IEQUALIFIER_CONTROL);complete(&one);assert(!one.data.search->regex_mode&&strstr((char*)one.window.Title,"not found"));
 charmap_search_close(NULL,&one,FALSE);charmap_search_close(NULL,&two,FALSE);free(one.data.search);free(two.data.search);
 assert(!memcmp(cells,(ULONG[]){'a','b','a','b','a'},sizeof(cells)));
 puts("Search UI PASS: isolated query editing, overlap/next/previous/wrap, overlay boundaries, cancellation/release, title ownership, query limit and OOM");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'t.c';b=Path(d)/'t';p.write_text(prefix+line+h+c+state+message+stubs+bindings+overlay+f+main)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(b)],check=True);subprocess.run([str(b)],check=True)
 broken=f.replace('s->swallowed[code >> 3] |= mask;', 's->swallowed[code >> 3] |= 0;')
 p.write_text(prefix+line+h+c+state+message+stubs+bindings+overlay+broken+main)
 subprocess.run(['cc','-Wall','-Werror',str(p),'-o',str(b)],check=True)
 assert subprocess.run([str(b)],capture_output=True).returncode!=0
 print('Negative control PASS: release leakage rejected')

with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'t.c';b=Path(d)/'t'
 broken=overlay.replace('flags ^= CON_TXTFLAGS_REVERSED;', 'flags |= CON_TXTFLAGS_REVERSED;')
 p.write_text(prefix+line+h+c+state+message+stubs+bindings+broken+f+main)
 subprocess.run(['cc','-Wall','-Werror',str(p),'-o',str(b)],check=True)
 assert subprocess.run([str(b)],capture_output=True).returncode!=0
 print('Negative control PASS: lost reverse-rendition toggle rejected')

with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'t.c';b=Path(d)/'t'
 for label,broken in [('same-view suppression',f.replace('!data->unrendered && s->view != data->top_of_window','!data->unrendered')),('missing live restore',f.replace('if (live) Console_RenderCursor(o);','/* lost balancing render */'))]:
  p.write_text(prefix+line+h+c+state+message+stubs+bindings+overlay+broken+main)
  subprocess.run(['cc','-Wall','-Werror',str(p),'-o',str(b)],check=True)
  assert subprocess.run([str(b)],capture_output=True).returncode!=0
  print('Negative control PASS:',label)

with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'t.c';b=Path(d)/'t'
 broken=f.replace('else charmap_search_delete_word(s);','else s->length = 0;')
 # Keep the helper referenced so -Werror does not mask the behavioral failure.
 broken=broken.replace('else s->length = 0;','else { charmap_search_delete_word(s); s->length = 0; }')
 p.write_text(prefix+line+h+c+state+message+stubs+bindings+overlay+broken+main)
 subprocess.run(['cc','-Wall','-Werror',str(p),'-o',str(b)],check=True)
 assert subprocess.run([str(b)],capture_output=True).returncode!=0
 print('Negative control PASS: word deletion must preserve preceding words; 19275 Unicode editing cases')

with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'t.c';b=Path(d)/'t'
 broken=f.replace('memcpy(s->draft, s->query, s->length * sizeof(ULONG));','memset(s->draft, 0, s->length * sizeof(ULONG));')
 p.write_text(prefix+line+h+c+state+message+stubs+bindings+overlay+broken+main)
 subprocess.run(['cc','-Wall','-Werror',str(p),'-o',str(b)],check=True)
 assert subprocess.run([str(b)],capture_output=True).returncode!=0
 print('Negative control PASS: recalled history must preserve the unfinished draft')

with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'t.c';b=Path(d)/'t'
 broken=f.replace('if (s->paste_status) status = s->paste_status;', '/* old background search overwrites clipboard status */')
 p.write_text(prefix+line+h+c+state+message+stubs+bindings+overlay+broken+main)
 subprocess.run(['cc','-Wall','-Werror',str(p),'-o',str(b)],check=True)
 assert subprocess.run([str(b)],capture_output=True).returncode!=0
 print('Negative control PASS: background search must preserve clipboard status')

with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'t.c';b=Path(d)/'t'
 broken=f.replace('if (s->row <= desired &&', 'if (s->row <= (desired = s->nav_mode ? s->nav.cursor.row : desired) &&')
 assert broken != f
 p.write_text(prefix+line+h+c+state+message+stubs+bindings+overlay+broken+main)
 subprocess.run(['cc','-Wall','-Werror',str(p),'-o',str(b)],check=True)
 assert subprocess.run([str(b)],capture_output=True).returncode!=0
 print('Negative control PASS: visible navigation must not jump the viewport')

with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'t.c';b=Path(d)/'t'
 broken=f.replace('s->nav_view ? s->nav_view_row : data->scrollback_pos', 'data->scrollback_pos')
 assert broken != f
 p.write_text(prefix+line+h+c+state+message+stubs+bindings+overlay+broken+main)
 subprocess.run(['cc','-Wall','-Werror',str(p),'-o',str(b)],check=True)
 assert subprocess.run([str(b)],capture_output=True).returncode!=0
 print('Negative control PASS: page movement must move the viewport as well as the cursor')

with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'t.c';b=Path(d)/'t'
 broken=h.replace('ULONG moved=con_nav_grid_advance(&probe,left),cp=0;', 'ULONG moved=con_nav_advance(&probe,left),cp=0;')
 assert broken != h
 p.write_text(prefix+line+broken+c+state+message+stubs+bindings+overlay+f+main)
 subprocess.run(['cc','-Wall','-Werror',str(p),'-o',str(b)],check=True)
 assert subprocess.run([str(b)],capture_output=True).returncode!=0
 print('Negative control PASS: semantic selection on a blank cell must not jump into stored text')

for label,old,new in [
 ('clear cancels pending copy','if(s){\n        charmap_nav_copy_cancel(s);','if(s){\n        /* leave pending copy */'),
 ('clear retains navigation','s->nav.selecting=FALSE;s->nav_dirty=TRUE;s->found=FALSE;\n            s->nav_notice=NULL;','s->active=FALSE;s->nav.selecting=FALSE;s->nav_dirty=TRUE;s->found=FALSE;\n            s->nav_notice=NULL;'),
 ('pending copy completes','if(s->nav_copy){','if(FALSE && s->nav_copy){'),
 ('pending copy preserves newer navigation','if(result==CON_COPY_DONE && s->nav_copy_clear){','if(result==CON_COPY_DONE){'),
 ('seed anchor uses original cursor','struct con_nav_cursor origin=s->nav.cursor;','struct con_nav_cursor origin=s->seed.start;'),
 ('seed skips source occurrence','s->origin_search.inclusive=!s->reverse_search;','s->origin_search.origin_column=0;s->origin_search.inclusive=TRUE;'),
 ('seed uses active selection','con_seed_begin(&s->seed,&s->nav,data->top_of_scrollback);','s->nav.selecting=FALSE;con_seed_begin(&s->seed,&s->nav,data->top_of_scrollback);'),
 ('query escape restores origin','if(cancel){','if(FALSE && cancel){'),
 ('copy before display tick','charmap_nav_copy_step(cl,o,64):CON_COPY_MORE;','CON_COPY_MORE:CON_COPY_MORE;'),
 ('failed entry key release','s->swallowed[code>>3]|=mask;return TRUE;','return TRUE;'),
 ('exit live view',"BOOL restored=(toggle || text[0]=='i') && charmap_restore_live(data);",'BOOL restored=FALSE; if(0)charmap_restore_live(data);'),
 ('burst single cell motions' ,'if(motion<=CON_NAV_DOWN)con_nav_step(&s->nav,1);','/* defer and lose burst motion */'),
 ('entry application cursor','column=(ULONG)XCP;','column=0;'),
 ('entry visible range','live-data->scrollback_pos+(ULONG)YCP','(ULONG)YCP'),
 ('navigation query mode boundary','(!s->nav_query || ICU(o)->bindings.bp_Version>=19)','TRUE'),
 ('configured navigation action dispatch','if(!(event->ie_Qualifier&IEQUALIFIER_REPEAT))charmap_nav_action(cl,o,action);','if(FALSE)charmap_nav_action(cl,o,action);'),
 ('configured selection type','action==CONSOLENG_BIND_NAVSELECTBLOCK,action==CONSOLENG_BIND_NAVSELECTSEMANTIC','FALSE,action==CONSOLENG_BIND_NAVSELECTSEMANTIC'),
 ('configured motion routing','motion=motions[action-CONSOLENG_BIND_NAVLEFT];','motion=CON_NAV_LEFT; (void)motions;'),
 ('configured scroll distance','ULONG amount=line?1:action>=CONSOLENG_BIND_NAVHALFUP?height/2:height;','ULONG amount=height;'),
 ('center upper middle row','middle=height>1?height/2-1:0','middle=height/2'),
 ('URL activation repeat suppression','if(!(event->ie_Qualifier&IEQUALIFIER_REPEAT))charmap_nav_action(cl,o,CONSOLENG_BIND_NAVOPENLINK);','charmap_nav_action(cl,o,CONSOLENG_BIND_NAVOPENLINK);'),
]:
 with tempfile.TemporaryDirectory() as d:
  p=Path(d)/'t.c';b=Path(d)/'t';broken=f.replace(old,new);assert broken!=f
  p.write_text(prefix+line+h+c+state+message+stubs+bindings+overlay+broken+main)
  subprocess.run(['cc','-Wall','-Werror',str(p),'-o',str(b)],check=True)
  assert subprocess.run([str(b)],capture_output=True).returncode!=0
  print('Negative control PASS:',label)
