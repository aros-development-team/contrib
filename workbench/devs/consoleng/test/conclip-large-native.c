/* Isolated provider integration, or acceptance of the installed boot provider.
 * Usage: LargeCopy [child-command] [installed]. The default mode temporarily
 * replaces and restores the provider; installed mode leaves its lifetime alone.
 * Both modes overwrite clipboard contents and belong in a disposable guest. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <dos/conclip.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static struct MsgPort*reply;
static int exchange(struct ConClipTextMessage*m){m->msg.mn_Node.ln_Type=NT_MESSAGE;m->msg.mn_ReplyPort=reply;if(!m->msg.mn_Length)m->msg.mn_Length=sizeof(*m);Forbid();struct MsgPort*p=FindPort((CONST_STRPTR)"ConClip.rendezvous");if(p)PutMsg(p,&m->msg);Permit();if(!p)return 0;WaitPort(reply);return GetMsg(reply)==&m->msg;}
static int readmatch(const UBYTE*bytes,ULONG n){struct ConClipTextMessage m={0};m.code=CONCLIP_READ_TEXT;m.version=1;if(!exchange(&m)||m.status!=CONCLIP_TEXT_OK)return 0;int ok=m.length==n&&m.capacity>=n&&(!n||!memcmp(m.data,bytes,n));if(m.data)FreeMem(m.data,m.capacity);return ok;}
#define CHECK(c) do{if(!(c)){printf("LARGECOPY_FAIL line=%d check=%s\n",__LINE__,#c);fflush(stdout);goto out;}}while(0)
int main(int argc,char **argv){int rc=20,hidden=0,started=0,cases=0;int installed=argc==3&&!strcmp(argv[2],"installed");struct MsgPort*original=NULL;UBYTE*bytes=NULL;reply=CreateMsgPort();CHECK(reply);bytes=malloc(CONCLIP_WRITE_MAX);CHECK(bytes);for(ULONG i=0;i<CONCLIP_WRITE_MAX;i++)bytes[i]=(UBYTE)(i*37+11);
struct ConClipTextMessage m={0};if(!installed){m.code=CONCLIP_WRITE_TEXT;m.version=1;m.data=bytes;m.length=m.capacity=1;CHECK(exchange(&m)&&m.status==CONCLIP_TEXT_UNSUPPORTED);puts("LARGECOPY_OLD_UNSUPPORTED_PASS");
Forbid();original=FindPort((CONST_STRPTR)"ConClip.rendezvous");if(original)RemPort(original);Permit();CHECK(original);hidden=1;
CHECK(SystemTags((CONST_STRPTR)"Run >NIL: COPYLRG:ConClip",TAG_DONE)==0);started=1;
int ready=0;for(int i=0;i<300&&!ready;i++){Forbid();ready=FindPort((CONST_STRPTR)"ConClip.rendezvous")!=NULL;Permit();if(!ready)Delay(1);}CHECK(ready);
 }else{Forbid();original=FindPort((CONST_STRPTR)"ConClip.rendezvous");Permit();CHECK(original);puts("LARGECOPY_INSTALLED_PROVIDER_BEGIN");}
ULONG sizes[]={0,1,32767,32768,65536,CONCLIP_WRITE_MAX};
for(unsigned i=0;i<sizeof(sizes)/sizeof(sizes[0]);i++){memset(&m,0,sizeof(m));m.code=CONCLIP_WRITE_TEXT;m.version=1;m.data=bytes;m.length=m.capacity=sizes[i];CHECK(exchange(&m)&&m.status==CONCLIP_TEXT_OK);CHECK(readmatch(bytes,sizes[i]));printf("LARGECOPY_SIZE_PASS bytes=%lu\n",(unsigned long)sizes[i]);fflush(stdout);cases++;}
for(int kind=0;kind<6;kind++){memset(&m,0,sizeof(m));m.code=CONCLIP_WRITE_TEXT;m.version=1;m.data=bytes;m.length=m.capacity=1;if(kind==0)m.data=NULL;if(kind==1)m.capacity=0;if(kind==2)m.length=m.capacity=CONCLIP_WRITE_MAX+1;if(kind==3)m.sgw=(void*)1;if(kind==4)m.version=2;if(kind==5)m.msg.mn_Length=sizeof(m)-1;CHECK(exchange(&m));CHECK(m.status==(kind>=4?CONCLIP_TEXT_UNSUPPORTED:CONCLIP_TEXT_ERROR));CHECK(readmatch(bytes,CONCLIP_WRITE_MAX));cases++;}
printf("LARGECOPY_PROTOCOL_PASS cases=%d\n",cases);fflush(stdout);
CHECK(SystemTags((CONST_STRPTR)(argc>1?argv[1]:"COPYLRG:CopyAsync"),SYS_Output,Output(),SYS_Input,Input(),TAG_DONE)==0);puts("LARGECOPY_CHILD_EXIT_PASS");if(installed){Forbid();int same=FindPort((CONST_STRPTR)"ConClip.rendezvous")==original;Permit();CHECK(same);puts("LARGECOPY_INSTALLED_PROVIDER_PASS");}rc=0;
out:if(started){Forbid();struct MsgPort*p=FindPort((CONST_STRPTR)"ConClip.rendezvous");if(p)Signal(p->mp_SigTask,SIGBREAKF_CTRL_C);Permit();int gone=0;for(int i=0;i<300&&!gone;i++){Forbid();gone=FindPort((CONST_STRPTR)"ConClip.rendezvous")==NULL;Permit();if(!gone)Delay(1);}if(!gone){rc=20;hidden=0;puts("LARGECOPY_PROVIDER_STOP_FAILED");}}
if(hidden){Forbid();AddPort(original);Permit();}free(bytes);if(reply)DeleteMsgPort(reply);printf("LARGECOPY_RUN rc=%d\n",rc);return rc;}
