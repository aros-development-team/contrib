#!/usr/bin/env python3
"""Actual zoom font worker/request handshake and safe DOS process launch tags."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'consoleclass.c').read_text();body=s[s.index('struct zoom_font_request {'):s.index('/* Each unit retains')]
prefix=r'''
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
typedef long LONG;typedef uintptr_t IPTR;typedef int BOOL;
#define FALSE 0
#define BNULL 0
#define TAG_DONE 0
#define CONSOLENG_FONT_NAME_SIZE 128
#define NP_Entry 1
#define NP_Name 2
#define NP_StackSize 3
#define NP_UserData 4
#define NP_CopyVars 5
#define NP_Cli 6
#define NP_Input 7
#define NP_Output 8
#define NP_Error 9
#define NP_CloseInput 10
#define NP_CloseOutput 11
#define NP_CloseError 12
#define NP_CurrentDir 13
#define NP_HomeDir 14
#define NP_Path 15
struct Message;
struct MsgPort{struct Message*reply;};
struct Message{struct MsgPort*mn_ReplyPort;unsigned mn_Length;};
struct Library{int id;};struct DosLibrary{int id;};struct TextFont{int valid;};
struct TextAttr{char*ta_Name;unsigned ta_YSize,ta_Style,ta_Flags;};
struct Process{struct {void*tc_UserData;} pr_Task;};
static struct Process process;static struct Library dos={1},diskfont={2};static struct TextFont result={1};static struct MsgPort port;
static int failure,ports,opens,closes,replies,worker,finished;
static void*FindTask(void*x){assert(worker);return &process;}
static struct Library*OpenLibrary(const char*n,int v){if(!strcmp(n,"dos.library")){assert(!worker);if(failure==1)return NULL;opens++;return &dos;}assert(worker&&!strcmp(n,"diskfont.library"));if(failure==4)return NULL;opens++;return &diskfont;}
static void CloseLibrary(struct Library*l){assert(l==&dos||l==&diskfont);if(l==&diskfont)assert(worker&&!replies);else assert(!worker);closes++;}
static struct TextFont*OpenDiskFont(struct TextAttr*a){assert(worker&&!strcmp(a->ta_Name,"fixed.font")&&a->ta_YSize==18);return failure==5?NULL:&result;}
static struct MsgPort*CreateMsgPort(void){if(failure==2)return NULL;ports++;port.reply=NULL;return &port;}
static void DeleteMsgPort(struct MsgPort*p){assert(p==&port&&ports==1&&!p->reply);ports--;}
static void ReplyMsg(struct Message*m){assert(worker&&m->mn_ReplyPort==&port&&m->mn_Length>sizeof(*m));assert(closes==(failure==4?0:1));port.reply=m;replies++;}
static struct Message*GetMsg(struct MsgPort*p){assert(!worker&&finished);struct Message*m=p->reply;p->reply=NULL;return m;}
static void WaitPort(struct MsgPort*p){assert(0);}
static struct Process*CreateNewProcTags(IPTR tag,...){
 IPTR values[16]={0};unsigned seen=0;va_list ap;va_start(ap,tag);
 while(tag){assert(tag<16);values[tag]=va_arg(ap,IPTR);seen|=1u<<tag;tag=va_arg(ap,IPTR);}va_end(ap);
 assert(seen==0xfffe&&values[NP_StackSize]>=65536&&values[NP_Name]&&values[NP_UserData]);
 for(unsigned t=NP_CopyVars;t<=NP_Path;t++)assert(!values[t]);
 if(failure==3)return NULL;
 process.pr_Task.tc_UserData=(void*)values[NP_UserData];worker=1;
 assert(!((LONG(*)(void))values[NP_Entry])());worker=0;finished=1;return &process;
}
'''
main=r'''
int main(void){for(failure=0;failure<=5;failure++){
 ports=opens=closes=replies=worker=finished=0;
 struct TextAttr a={"fixed.font",18,0,0};struct TextFont*f=console_zoom_open_font(&a);
 assert(f==(failure?NULL:&result));assert(!ports&&!worker&&opens==closes);
 assert(replies==(!failure||failure>=4));
 }
 puts("Font loader PASS: copied request, explicit null inherited state, worker-only DOS font open, completed reply and all allocation/open failure cleanup");
}
'''
with tempfile.TemporaryDirectory() as d:
 for label,old,new in [('production',None,None),('inherited DOS input','NP_Input,BNULL','NP_Input,123'),('copied font name','job.attr.ta_Name = job.name;','job.attr.ta_Name = NULL;')]:
  code=body
  if old:assert old in code;code=code.replace(old,new)
  p=Path(d)/'test.c';b=Path(d)/'test';p.write_text(prefix+code+main)
  subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined',str(p),'-o',str(b)],check=True)
  result=subprocess.run([str(b)],capture_output=True,text=True)
  assert (result.returncode==0)==(old is None),(label,result.stderr)
  print(result.stdout.strip() if old is None else 'Negative control PASS: '+label)
