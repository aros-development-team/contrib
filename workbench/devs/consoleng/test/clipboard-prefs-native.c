/* Clipboard API validation, defaults inheritance and legacy-profile isolation. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/console.h>
#include <devices/conunit.h>
#include <stdio.h>
#include <string.h>
#include "consoleng.h"
static struct IOStdReq *control,*first,*second;
static struct Window *w1,*w2;
struct Device *ConsoleDevice;
static char *prior;
static int request(struct IOStdReq *io,UWORD cmd,void *p,ULONG size){io->io_Command=cmd;io->io_Data=p;io->io_Length=size;return !DoIO((struct IORequest*)io)&&io->io_Actual==size;}
static int writecopy(struct IOStdReq *io,const char *s){return request(io,CMD_WRITE,(void*)s,strlen(s));}
static int snip(const char *expected){char *got=GetConSnip();int ok=got&&!strcmp(got,expected);if(got)FreeVec(got);return ok;}
#define CHECK(c) do{if(!(c)){printf("CLIPPREF_FAIL line=%d check=%s\n",__LINE__,#c);fflush(stdout);goto out;}}while(0)
int main(int argc,char **argv){int rc=20,copen=0,fopen=0,sopen=0,saved=0;struct ConsolengTerminalProfile original,current,bad;
struct MsgPort *port=CreateMsgPort();control=port?(void*)CreateIORequest(port,sizeof(*control)):NULL;first=port?(void*)CreateIORequest(port,sizeof(*first)):NULL;second=port?(void*)CreateIORequest(port,sizeof(*second)):NULL;CHECK(control&&first&&second);
CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",CONU_LIBRARY,(struct IORequest*)control,0));copen=1;
CHECK(request(control,CD_GETTERMINALPROFILE,&original,sizeof(original)));saved=1;
if(argc>1&&!strcmp(argv[1],"ON")){struct ConsolengClipboardPrefs on={sizeof(on),1,1};CHECK(request(control,CD_SETDEFAULTCLIPBOARDPREFS,&on,sizeof(on)));saved=0;puts("CLIPPREF_DEFAULT_ON_PASS");rc=0;goto out;}
if(argc>1){CHECK(original.rp_Clipboard.cbp_Enabled==0);puts("CLIPPREF_GUI_DEFAULT_OFF_PASS");rc=0;goto out;}
CHECK(original.rp_Clipboard.cbp_Enabled==1);
w1=OpenWindowTags(NULL,WA_Left,30,WA_Top,30,WA_Width,600,WA_Height,300,WA_Title,(IPTR)"ConsoleNG clipboard policy",WA_DragBar,TRUE,WA_DepthGadget,TRUE,TAG_DONE);CHECK(w1);
first->io_Data=w1;first->io_Length=sizeof(*w1);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",CONU_CHARMAP,(struct IORequest*)first,0));fopen=1;ConsoleDevice=first->io_Device;prior=GetConSnip();
struct ConsolengClipboardPrefs p={sizeof(p),1,0};CHECK(request(control,CD_SETDEFAULTCLIPBOARDPREFS,&p,sizeof(p)));
CHECK(request(first,CD_GETCLIPBOARDPREFS,&p,sizeof(p))&&p.cbp_Enabled==1);
struct ConsolengSessionProfile legacy;CHECK(request(control,CD_GETSESSIONPROFILE,&legacy,sizeof(legacy)));CHECK(request(control,CD_SETSESSIONPROFILE,&legacy,sizeof(legacy)));CHECK(request(control,CD_GETDEFAULTCLIPBOARDPREFS,&p,sizeof(p))&&p.cbp_Enabled==0);
w2=OpenWindowTags(NULL,WA_Left,60,WA_Top,60,WA_Width,600,WA_Height,300,WA_Title,(IPTR)"ConsoleNG clipboard-disabled session",WA_DragBar,TRUE,WA_DepthGadget,TRUE,TAG_DONE);CHECK(w2);
second->io_Data=w2;second->io_Length=sizeof(*w2);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",CONU_CHARMAP,(struct IORequest*)second,0));sopen=1;
CHECK(request(second,CD_GETCLIPBOARDPREFS,&p,sizeof(p))&&p.cbp_Enabled==0);CHECK(SetConSnip("original")==1);CHECK(writecopy(second,"\x1b]52;p;bmV3\7"));CHECK(snip("original"));
p.cbp_Enabled=1;CHECK(request(second,CD_SETCLIPBOARDPREFS,&p,sizeof(p)));CHECK(writecopy(second,"\x1b]52;p;bmV3\7"));CHECK(snip("new"));
CHECK(writecopy(second,"\x1b]52;p;b3"));p.cbp_Enabled=0;CHECK(request(second,CD_SETCLIPBOARDPREFS,&p,sizeof(p)));p.cbp_Enabled=1;CHECK(request(second,CD_SETCLIPBOARDPREFS,&p,sizeof(p)));CHECK(writecopy(second,"ZlcnJpZGU=\7"));CHECK(snip("new"));
CHECK(writecopy(second,"\x1b]52;s;ZnJlc2g=\7"));CHECK(snip("fresh"));
for(int field=0;field<3;field++){struct ConsolengClipboardPrefs invalid=p;((ULONG*)&invalid)[field]=0xffffffffUL;CHECK(!request(second,CD_SETCLIPBOARDPREFS,&invalid,sizeof(invalid))&&second->io_Actual==0);CHECK(request(second,CD_GETCLIPBOARDPREFS,&invalid,sizeof(invalid))&&invalid.cbp_Enabled==1);}
CHECK(!request(control,CD_SETCLIPBOARDPREFS,&p,sizeof(p))&&control->io_Actual==0);
CHECK(!request(first,CD_SETDEFAULTCLIPBOARDPREFS,&p,sizeof(p))&&first->io_Actual==0);
CHECK(!request(first,CD_SETCLIPBOARDPREFS,&p,sizeof(p)-1)&&first->io_Actual==0);
CHECK(request(control,CD_GETTERMINALPROFILE,&current,sizeof(current)));
for(int field=0;field<3;field++){bad=current;((ULONG*)&bad.rp_Clipboard)[field]=0xffffffffUL;bad.rp_Theme.tp_Cursor.cp_Style=6;CHECK(!request(control,CD_SETTERMINALPROFILE,&bad,sizeof(bad))&&control->io_Actual==0);CHECK(request(control,CD_GETTERMINALPROFILE,&bad,sizeof(bad))&&!memcmp(&bad,&current,sizeof(bad)));}
CHECK(request(control,CD_SETTERMINALPROFILE,&original,sizeof(original)));CHECK(request(control,CD_GETTERMINALPROFILE,&current,sizeof(current))&&!memcmp(&current,&original,sizeof(current)));
puts("CLIPPREF_PASS inheritance, blocked copy, discarded partial transfer, validation, legacy ABI and atomic profile");rc=0;
out:if(fopen){SetConSnip(prior);if(prior)FreeVec(prior);}if(saved)request(control,CD_SETTERMINALPROFILE,&original,sizeof(original));if(sopen)CloseDevice((struct IORequest*)second);if(fopen)CloseDevice((struct IORequest*)first);if(w2)CloseWindow(w2);if(w1)CloseWindow(w1);if(copen)CloseDevice((struct IORequest*)control);if(second)DeleteIORequest((struct IORequest*)second);if(first)DeleteIORequest((struct IORequest*)first);if(control)DeleteIORequest((struct IORequest*)control);if(port)DeleteMsgPort(port);printf("CLIPPREF_RUN rc=%d\n",rc);return rc;}
