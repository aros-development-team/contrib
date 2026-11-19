#!/usr/bin/env python3
"""Production font zoom: ownership, rollback, limits and baseline restoration."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'consoleclass.c').read_text();body=s[s.index('static VOID console_zoom_font('):s.index('/* DECRPM:')]
h=(r/'include/consoleng.h').read_text();header=h[h.index('#define CONSOLENG_FONT_NAME_SIZE'):h.index('struct ConsolengDefaultProfile')]
header+='\n'+'\n'.join(x for x in h.splitlines() if x.startswith('#define CONSOLENG_BIND_FONT'))
prefix=r'''
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
typedef uint32_t ULONG;typedef int BOOL;typedef void VOID;typedef char* STRPTR;typedef void Class;
#define TRUE 1
#define FALSE 0
#define FS_NORMAL 0
#define FPF_PROPORTIONAL 1
#define TAGGEDOPEN_GRAPHICS 1
#define strnicmp strncasecmp
struct Library{int unused;};
struct TextFont{struct {struct {char *ln_Name;} mn_Node;} tf_Message;ULONG tf_YSize,tf_XSize,tf_Flags;int owned;};
struct TextAttr{STRPTR ta_Name;ULONG ta_YSize,ta_Style,ta_Flags;};
struct ConsolengFontWindow{struct TextFont *original;};
'''
extra=r'''
typedef struct {struct TextFont *setFont;struct ConsolengFontWindow *fontWindow;struct ConsolengFontPrefs fontZoomBase;BOOL fontZoomActive,fontZoomBorrowed,fontSetOK,hidden;} Object;
#define ICU(o) (o)
#define CON_IS_OBSCURED(o) ((o)->hidden)
static unsigned opened,closed,installed,live;static int failure;static struct Library lib;
static struct Library *TaggedOpenLibrary(int tag){return failure==2?NULL:&lib;}
static struct TextFont *makefont(const char*n,ULONG size){struct TextFont*f=calloc(1,sizeof(*f));assert(f);f->tf_Message.mn_Node.ln_Name=strdup(n);f->tf_YSize=size;f->tf_XSize=8;f->owned=1;live++;return f;}
static struct TextFont *OpenDiskFont(struct TextAttr*t){opened++;if(failure==3)return NULL;struct TextFont*f=makefont(t->ta_Name,t->ta_YSize);if(failure==4)f->tf_Flags=FPF_PROPORTIONAL;if(failure==5)f->tf_YSize--;if(failure==6)f->tf_XSize=0;return f;}
static struct TextFont *console_zoom_open_font(const struct TextAttr*t){return failure==1?NULL:OpenDiskFont((struct TextAttr*)t);}
static void CloseFont(struct TextFont*f){assert(f&&f->owned&&live);closed++;live--;free(f->tf_Message.mn_Node.ln_Name);free(f);}
static void console_install_font(Class*c,Object*o,struct TextFont*f){if(o->setFont)CloseFont(o->setFont);o->setFont=f;o->fontSetOK=TRUE;installed++;}
'''
main=r'''
int main(void){
 struct TextFont original={{{"base.font"}},8,8,0,0};struct ConsolengFontWindow window={&original};
 for(int owned=0;owned<2;owned++){
  Object o={0};o.fontWindow=&window;if(owned)o.setFont=makefont("outline.font",12);unsigned start=owned?12:8;
  console_zoom_font(NULL,&o,14);assert(o.fontSetOK&&o.fontZoomActive&&o.fontZoomBorrowed==!owned&&o.setFont->tf_YSize==start+1);
  assert(o.fontZoomBase.fp_Pixels==start);
  console_zoom_font(NULL,&o,14);assert(o.setFont->tf_YSize==start+2&&o.fontZoomBase.fp_Pixels==start);
  console_zoom_font(NULL,&o,15);assert(o.setFont->tf_YSize==start+1&&o.fontZoomBase.fp_Pixels==start);
  struct TextFont *before=o.setFont;unsigned calls=installed;
  o.hidden=TRUE;console_zoom_font(NULL,&o,14);assert(o.setFont==before&&installed==calls&&o.fontZoomActive);o.hidden=FALSE;
  for(failure=1;failure<=6;failure++){console_zoom_font(NULL,&o,14);assert(o.setFont==before&&!o.fontSetOK&&installed==calls&&o.fontZoomActive&&live==1);}
  failure=0;console_zoom_font(NULL,&o,16);assert(o.fontSetOK&&!o.fontZoomActive);
  if(owned){assert(o.setFont&&o.setFont->tf_YSize==start&&!strcmp(o.setFont->tf_Message.mn_Node.ln_Name,"outline.font"));CloseFont(o.setFont);}else assert(!o.setFont);
  assert(!live);
 }
 for(failure=1;failure<=6;failure++){
  Object o={0};o.fontWindow=&window;console_zoom_font(NULL,&o,14);assert(!o.setFont&&!o.fontZoomActive&&!o.fontSetOK&&!live);
 }
 failure=0;Object o={0};o.fontWindow=&window;o.setFont=makefont("outline.font",18);
 console_zoom_font(NULL,&o,14);struct TextFont *before=o.setFont;failure=3;console_zoom_font(NULL,&o,16);assert(o.setFont==before&&o.fontZoomActive&&!o.fontSetOK);
 failure=0;console_zoom_font(NULL,&o,16);assert(o.setFont->tf_YSize==18&&!o.fontZoomActive);CloseFont(o.setFont);o.setFont=NULL;
 unsigned calls=opened;original.tf_YSize=65535;console_zoom_font(NULL,&o,14);assert(opened==calls&&!o.setFont&&!o.fontZoomActive);
 original.tf_YSize=1;console_zoom_font(NULL,&o,15);assert(opened==calls&&!o.setFont&&!o.fontZoomActive);
 console_zoom_font(NULL,&o,16);assert(opened==calls);o.fontWindow=NULL;console_zoom_font(NULL,&o,14);assert(opened==calls&&!live);
 puts("Font zoom PASS: borrowed and owned baselines, repeated steps, failed open/library/size/width/proportional checks, failed reset, hidden units and pixel limits");
}
'''
with tempfile.TemporaryDirectory() as d:
 for label,old,new in [('production',None,None),('baseline replacement','!reset && !ICU(o)->fontZoomActive','!reset'),('exact size','font->tf_YSize != wanted.fp_Pixels','FALSE')]:
  code=body
  if old:assert old in code;code=code.replace(old,new)
  p=Path(d)/'test.c';b=Path(d)/'test';p.write_text(prefix+header+extra+code+main)
  subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(b)],check=True)
  result=subprocess.run([str(b)],capture_output=True,text=True)
  assert (result.returncode==0)==(old is None),(label,result.stderr)
  print(result.stdout.strip() if old is None else 'Negative control PASS: '+label)
