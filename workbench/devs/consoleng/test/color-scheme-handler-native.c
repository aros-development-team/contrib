/* Real raw DOS streams: no paste framing or echo around scheme replies. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <devices/conunit.h>
#include <dos/dosextens.h>
#include <stdio.h>
#include <string.h>
#include "consoleng.h"
static BPTR stream;
static struct IOStdReq *control;
static int put(const char*s,int split){ULONG n=strlen(s);if(!split)return Write(stream,s,n)==(LONG)n;for(ULONG i=0;i<n;i++)if(Write(stream,s+i,1)!=1)return 0;return 1;}
static int prefs(struct ConsolengColorPrefs*p){control->io_Command=CD_SETCOLORPREFS;control->io_Data=p;control->io_Length=sizeof(*p);return !DoIO((struct IORequest*)control)&&control->io_Actual==sizeof(*p);}
static int receive(int scheme){const char*want=scheme==1?"\x1b[?997;1n":"\x1b[?997;2n";char got[32]={0};ULONG n=strlen(want),used=0;if(!scheme)return !WaitForChar(stream,60000);
 while(used<n){if(!WaitForChar(stream,2000000))return 0;LONG count=Read(stream,got+used,n-used);if(count<=0)return 0;used+=count;}
 if(memcmp(got,want,n)||WaitForChar(stream,60000)){printf("SCHEMEDOS_WIRE bytes=%lu\n",(unsigned long)used);return 0;}return 1;}
#define CHECK(x) do{if(!(x)){printf("SCHEMEDOS_FAIL line=%d count=%d\n",__LINE__,count);fflush(stdout);goto out;}count++;}while(0)
int main(void){int rc=20,count=0;struct MsgPort*port=CreateMsgPort();struct Window*w=NULL;CHECK(port);control=(void*)CreateIORequest(port,sizeof(*control));CHECK(control);
 stream=Open((CONST_STRPTR)"RGCON:",MODE_OLDFILE);CHECK(stream);struct InfoData info;struct FileHandle*fh=BADDR(stream);CHECK(DoPkt(fh->fh_Type,ACTION_DISK_INFO,(SIPTR)MKBADDR(&info),0,0,0,0));
 struct IOStdReq*handler=(void*)(IPTR)info.id_InUse;CHECK(handler&&handler->io_Unit&&handler->io_Device);CHECK(!strcmp((char*)handler->io_Device->dd_Library.lib_Node.ln_Name,"consoleng.device"));
 control->io_Device=handler->io_Device;control->io_Unit=handler->io_Unit;w=((struct ConUnit*)handler->io_Unit)->cu_Window;CHECK(w);ActivateWindow(w);SetWindowTitles(w,(CONST_STRPTR)"ConsoleNG scheme reports through raw DOS",(CONST_STRPTR)-1);CHECK(SetMode(stream,1));
 for(int utf=0;utf<2;utf++)for(int split=0;split<2;split++)for(int c1=0;c1<2;c1++)for(int bracket=0;bracket<2;bracket++){
 struct ConsolengColorPrefs p={sizeof(p),1,3,{0xffffff,0x102030}};
 CHECK(put("\x1b[?2031l",split));CHECK(put(utf?"\x1b%G":"\x1b%@",split));CHECK(put(bracket?"\x1b[?2004h":"\x1b[?2004l",split));CHECK(prefs(&p));CHECK(receive(0));
 CHECK(put(c1?"\x9b?996n":"\x1b[?996n",split));CHECK(receive(1));
 CHECK(put(c1?"\x9b?2031h":"\x1b[?2031h",split));CHECK(receive(0));
 p.cp_RGB[0]=0;p.cp_RGB[1]=0xf0f0f0;CHECK(prefs(&p));CHECK(receive(2));CHECK(prefs(&p));CHECK(receive(0));
 CHECK(put("\x1b]11;#000000\x07",split));CHECK(receive(0));CHECK(put(c1?"\x9b?996n":"\x1b[?996n",split));CHECK(receive(2));
 CHECK(put("\x1b[?2031l",split));p.cp_RGB[0]=0xffffff;p.cp_RGB[1]=0x102030;CHECK(prefs(&p));CHECK(receive(0));
 }
 CHECK(put("\x1b[?2031l\x1b[?2004l\x1b]10;#eeeeee\x07\x1b]11;#203e4c\x07\x1b[2J\x1b[HPASS: color scheme reports through raw DOS streams.\r\nQueries and live preference changes, exact unframed bytes.\r\nUTF8/legacy, whole/fragmented, CSI/C1, paste mode on/off.\r\nOSC overrides and disabled reporting stay silent.\r\n",0));printf("SCHEMEDOS_PASS checks=%d\n",count);fflush(stdout);Delay(150);rc=0;
out:
 if(stream){SetMode(stream,0);Close(stream);}
 if(control)DeleteIORequest((struct IORequest*)control);
 if(port)DeleteMsgPort(port);
 printf("SCHEMEDOS_SCRIPT rc=%d\n",rc);return rc;
}
