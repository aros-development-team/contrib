/* Native DOS Write/Read and optional OSC52 clipboard permissions. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/console.h>
#include <devices/conunit.h>
#include <dos/dosextens.h>
#include <stdio.h>
#include <string.h>
#include "consoleng.h"
struct Device *ConsoleDevice;
static BPTR stream;
static struct MsgPort *port;static struct IOStdReq *control;
static int text(const char*s){return Write(stream,s,strlen(s))==(LONG)strlen(s);}
static int match(const char*s){for(unsigned i=0;i<strlen(s);i++){char c=0;if(!WaitForChar(stream,2000000)||Read(stream,&c,1)!=1||c!=s[i])return 0;}return !WaitForChar(stream,10000);}
static int policy(struct IOStdReq*io,ULONG mask){struct ConsolengClipboardPrefs p={sizeof(p),2,mask};io->io_Command=CD_SETCLIPBOARDPREFS;io->io_Data=&p;io->io_Length=sizeof(p);return !DoIO((struct IORequest*)io)&&io->io_Actual==sizeof(p);}
#define CHECK(c) do{if(!(c)){printf("OSCREAD_FAIL line=%d check=%s\n",__LINE__,#c);fflush(stdout);Delay(250);goto out;}}while(0)
int main(void){int rc=20;char*old=NULL;int saved=0;struct InfoData info;
 stream=Open((CONST_STRPTR)"RGCON:",MODE_OLDFILE);CHECK(stream);
 struct FileHandle*fh=BADDR(stream);CHECK(DoPkt(fh->fh_Type,ACTION_DISK_INFO,(SIPTR)MKBADDR(&info),0,0,0,0));
 struct IOStdReq*io=(void*)(IPTR)info.id_InUse;CHECK(io&&io->io_Unit);
 port=CreateMsgPort();CHECK(port);control=(void*)CreateIORequest(port,sizeof(*control));CHECK(control);control->io_Device=io->io_Device;control->io_Unit=io->io_Unit;
 ConsoleDevice=io->io_Device;struct ConUnit*u=(void*)io->io_Unit;struct Window*w=u->cu_Window;CHECK(w);
 SetWindowTitles(w,(CONST_STRPTR)"ConsoleNG clipboard permissions",(CONST_STRPTR)-1);MoveWindow(w,30-w->LeftEdge,30-w->TopEdge);SizeWindow(w,730-w->Width,450-w->Height);
 old=GetConSnip();saved=1;CHECK(SetConSnip("abc"));CHECK(SetMode(stream,TRUE));
 CHECK(text("\x1b%G\x1b[?2004h\x1b]52;p;?\7"));CHECK(!WaitForChar(stream,250000));
 CHECK(policy(control,2));CHECK(text("\x1b]52;p;?\7"));CHECK(match("\x1b]52;p;YWJj\7"));
 CHECK(text("\x1b]52;s;?\x1b\\"));CHECK(match("\x1b]52;s;YWJj\x1b\\"));
 CHECK(text("\x1b]52;p;eHl6\7\x1b]52;p;?\7"));CHECK(match("\x1b]52;p;YWJj\7"));
 CHECK(policy(control,3));CHECK(text("\x1b]52;p;eHl6\7\x1b]52;p;?\7"));CHECK(match("\x1b]52;p;eHl6\7"));
 CHECK(policy(control,1));CHECK(text("\x1b]52;p;?\7"));CHECK(!WaitForChar(stream,250000));
 CHECK(policy(control,3));CHECK(text("\x1b]52;c;zqnwn5iA\7"));Delay(30);
 CHECK(text("\x1b]52;c;?\7"));CHECK(match("\x1b]52;c;zqnwn5iA\7"));
 CHECK(text("\x1b[?2004l\x1b[2J\x1b[HClipboard permissions: PASS\r\n\r\nDefault: applications cannot read the clipboard\r\nRead only: query works, copy stays blocked\r\nRead and copy: both work\r\nRevoked access: query produces no reply\r\n\r\nLocal selection and native ConClip provider\r\nExact Base64, UTF-8, BEL and ST terminators\r\nReplies stay unframed with bracketed paste on"));
 puts("OSCREAD_PASS");fflush(stdout);Delay(500);rc=0;
 out:if(saved){SetConSnip(old);if(old)FreeVec(old);}if(control)DeleteIORequest((struct IORequest*)control);if(port)DeleteMsgPort(port);if(stream)Close(stream);printf("OSCREAD_RUN rc=%d\n",rc);return rc;}
