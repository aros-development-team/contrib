#!/usr/bin/env python3
"""Actual title stack and modern parser: bounds, OOM, hidden state, cleanup."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'consoleclass.c').read_text();f=s[s.index('struct ConsoleTitleEntry\n'):s.index('/*********************\n**  Console::New()')]
s=(r/'support.c').read_text();a=s.index('static BOOL getwindowcommand(');end=s.index('{',a);depth=1;end+=1
while depth:depth+=(s[end]=='{')-(s[end]=='}');end+=1
parser=s[a:end]
code=r'''
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#define C_NIL 0
typedef void Class;typedef unsigned char UBYTE;typedef char BYTE;typedef long LONG;typedef unsigned long ULONG;typedef int BOOL;typedef char*STRPTR;
#define TRUE 1
#define FALSE 0
#define ESC 27
#define C_PUSH_TITLE 1
#define C_POP_TITLE 2
#define C_CLEAR_TITLE_STACK 3
#define C_REPORT_CELL_GEOMETRY 4
#define C_REPORT_PIXEL_GEOMETRY 5
#define CON_TITLE_SIZE 64
#define MEMF_PUBLIC 0
struct ConsoleTitleEntry;
typedef struct {UBYTE unitTitle[64],displayTitle[64];struct ConsoleTitleEntry*titleStack;unsigned short titleDepth;int hidden;void*cu_Window;} Object;
#define ICU(o) (o)
#define CU(o) (o)
#define CON_IS_OBSCURED(o) ((o)->hidden)
static int allocations,fail,draws;
static char title[64];
static void *AllocVec(size_t n,int flags){if(fail)return NULL;allocations++;return malloc(n);}
static void FreeVec(void*p){assert(p);allocations--;free(p);}
#define SetWindowTitles(w,t,s) (strcpy(title,t),draws++)
'''+f+parser+r'''
int main(void){
Object o={0};strcpy((char*)o.unitTitle,"original");console_title_stack(NULL,&o,C_POP_TITLE);assert(!draws&&!allocations);
for(int i=0;i<4100;i++){snprintf((char*)o.unitTitle,64,"title%d",i);console_title_stack(NULL,&o,C_PUSH_TITLE);}assert(allocations==4096&&o.titleDepth==4096);
fail=1;struct ConsoleTitleEntry *before=o.titleStack;console_title_stack(NULL,&o,C_PUSH_TITLE);assert(o.titleStack==before&&allocations==4096);fail=0;
for(int i=4099;i>=4;i--){char expected[64];snprintf(expected,64,"title%d",i);o.hidden=i%2;int was=draws;console_title_stack(NULL,&o,C_POP_TITLE);assert(!strcmp((char*)o.unitTitle,expected)&&draws==was+!o.hidden);if(!o.hidden)assert(!strcmp(title,expected));}assert(!allocations&&!o.titleDepth);
for(int i=0;i<200;i++){console_title_stack(NULL,&o,C_PUSH_TITLE);}
console_title_stack(NULL,&o,C_CLEAR_TITLE_STACK);assert(!allocations&&!o.titleDepth&&!o.titleStack);
const char*good[]={"\033[22t","\033[23t","\033[22;0t","\033[23;2t"};
for(int k=0;k<4;k++)for(unsigned n=0;n<=strlen(good[k]);n++){UBYTE*p=(void*)good[k],count=99;BYTE c=99;int yes=n==strlen(good[k]);assert(getwindowcommand(&c,&p,&count,n)==yes);assert(c==(yes?(k%2?2:1):99)&&count==(yes?0:99)&&p==(UBYTE*)good[k]+(yes?n:0));}
const char *geometry[]={"\033[14t","\033[18t","\033[14;0t","\033[18;0t"};
for(int k=0;k<4;k++)for(unsigned n=0;n<=strlen(geometry[k]);n++){UBYTE*p=(void*)geometry[k],count=99;BYTE c=99;int yes=n==strlen(geometry[k]);assert(getwindowcommand(&c,&p,&count,n)==yes);assert(c==(yes?(k%2?4:5):99)&&count==(yes?0:99)&&p==(UBYTE*)geometry[k]+(yes?n:0));}
const char*bad[]={"\23322t","\033[>22t","\033[22 t","\033[22;2x"};
for(int k=0;k<4;k++){UBYTE*p=(void*)bad[k],count=99;BYTE c=99;assert(!getwindowcommand(&c,&p,&count,strlen(bad[k]))&&p==(void*)bad[k]&&count==99&&c==99);}
const char *ignored[]={"\033[t","\033[0t","\033[1t","\033[8;80;132t","\033[21t","\033[24t","\033[999999999999999999999999999999999t"};
for(int k=0;k<7;k++)for(unsigned n=0;n<=strlen(ignored[k]);n++){UBYTE*p=(void*)ignored[k],count=99;BYTE c=99;int yes=n==strlen(ignored[k]);assert(getwindowcommand(&c,&p,&count,n)==yes);assert(c==(yes?0:99)&&count==(yes?0:99)&&p==(UBYTE*)ignored[k]+(yes?n:0));}
return 0;}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'t.c';b=Path(d)/'t';p.write_text(code);subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(b)],check=True);subprocess.run([str(b)],check=True)
 p.write_text(code.replace('selector == 23 ? C_POP_TITLE : C_NIL', 'selector == 23 ? C_POP_TITLE : C_PUSH_TITLE'))
 subprocess.run(['cc','-Wall','-Werror',str(p),'-o',str(b)],check=True)
 assert subprocess.run([str(b)],capture_output=True).returncode!=0
print('Title/window isolation PASS: 4100 pushes, 4096 LIFO pops, oldest eviction, OOM atomicity, hidden isolation, cleanup, fragmented/native parser isolation')
