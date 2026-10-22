/* Native scheme reporting: queries, preference events, and application isolation. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <devices/conunit.h>
#include <stdio.h>
#include <string.h>
#include "consoleng.h"
static struct IOStdReq *io,*reader;
static int req(UWORD c,void*p,ULONG n){io->io_Command=c;io->io_Data=p;io->io_Length=n;return !DoIO((struct IORequest*)io)&&io->io_Actual==n;}
static int put(const char*s){return req(CMD_WRITE,(void*)s,strlen(s));}
static int receive(int scheme){char got[32]={0};const char *expected=scheme==1?"\x1b[?997;1n":"\x1b[?997;2n";ULONG n=scheme?strlen(expected):1;
 reader->io_Command=CMD_READ;reader->io_Data=got;reader->io_Length=n;SendIO((struct IORequest*)reader);
 for(int i=0;i<(scheme?100:3);i++){if(CheckIO((struct IORequest*)reader)){WaitIO((struct IORequest*)reader);return scheme&&!reader->io_Error&&reader->io_Actual==n&&!memcmp(got,expected,n);}Delay(1);}
 AbortIO((struct IORequest*)reader);WaitIO((struct IORequest*)reader);return !scheme;
}
static int query(int c1,int scheme){const char*q=c1?"\x9b?996n":"\x1b[?996n";for(ULONG i=0;i<strlen(q);i++)if(!req(CMD_WRITE,(void*)(q+i),1))return 0;return receive(scheme);}
#define CHECK(x) do{if(!(x)){printf("SCHEME_FAIL line=%d kind=%d count=%d\n",__LINE__,kind,count);fflush(stdout);goto out;}count++;}while(0)
int main(void){int rc=20,opened=0,kind=0,count=0;struct Window*w=NULL;struct MsgPort*port=CreateMsgPort();
 CHECK(port);io=(void*)CreateIORequest(port,sizeof(*io));reader=(void*)CreateIORequest(port,sizeof(*reader));CHECK(io&&reader);
 ULONG units[]={CONU_STANDARD,CONU_CHARMAP,CONU_SNIPMAP};
 for(kind=0;kind<3;kind++){
 w=OpenWindowTags(NULL,WA_Left,30,WA_Top,35,WA_Width,720,WA_Height,430,WA_Title,(IPTR)"ConsoleNG live color scheme reports",WA_Activate,TRUE,WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_SizeGadget,TRUE,WA_SizeBRight,TRUE,TAG_DONE);CHECK(w);
 io->io_Data=w;io->io_Length=sizeof(*w);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",units[kind],(struct IORequest*)io,0));opened=1;reader->io_Device=io->io_Device;reader->io_Unit=io->io_Unit;
 for(int hidden=0;hidden<2;hidden++)for(int utf=0;utf<2;utf++)for(int c1=0;c1<2;c1++){
 struct ConsolengColorPrefs p={sizeof(p),1,3,{0}};p.cp_RGB[0]=0xffffff;p.cp_RGB[1]=0x102030;
 CHECK(put("\x1b[?2031l"));CHECK(put(utf?"\x1b%G":"\x1b%@"));CHECK(put(hidden?"\x1b[>9h":"\x1b[>9l"));CHECK(req(CD_SETCOLORPREFS,&p,sizeof(p)));CHECK(receive(0));CHECK(query(c1,1));
 CHECK(put(c1?"\x9b?2031h":"\x1b[?2031h"));CHECK(receive(0));
 p.cp_RGB[0]=0;p.cp_RGB[1]=0xe0e0e0;CHECK(req(CD_SETCOLORPREFS,&p,sizeof(p)));CHECK(receive(2));CHECK(query(c1,2));
 CHECK(req(CD_SETCOLORPREFS,&p,sizeof(p)));CHECK(receive(0));
 p.cp_Mask|=4;p.cp_RGB[2]=0x123456;CHECK(req(CD_SETCOLORPREFS,&p,sizeof(p)));CHECK(receive(2));
 CHECK(put("\x1b]11;#000000\x07"));CHECK(receive(0));CHECK(query(c1,2));
 struct ConsolengColorPrefs bad=p;bad.cp_Size--;CHECK(!req(CD_SETCOLORPREFS,&bad,sizeof(bad)));CHECK(receive(0));CHECK(query(c1,2));
 p.cp_RGB[0]=0xffffff;p.cp_RGB[1]=0x102030;CHECK(req(CD_SETCOLORPREFS,&p,sizeof(p)));CHECK(receive(1));CHECK(query(c1,1));
 CHECK(put("\x1b[?2031l"));p.cp_RGB[0]=0;p.cp_RGB[1]=0xe0e0e0;CHECK(req(CD_SETCOLORPREFS,&p,sizeof(p)));CHECK(receive(0));CHECK(query(c1,2));
 CHECK(put("\x1b[?2031h\x1b" "c"));CHECK(receive(0));p.cp_RGB[1]=0x102030;CHECK(req(CD_SETCOLORPREFS,&p,sizeof(p)));CHECK(receive(0));CHECK(query(c1,1));
 }
 CHECK(put("\x1b[>9l\x1b[?2031l\x1b]10;#eeeeee\x07\x1b]11;#203e4c\x07\x1b[2J\x1b[HPASS: live dark/light reports follow Preferences.\r\nFragmented queries; standard, charmap and snipmap.\r\nHidden tabs, UTF8 and legacy CSI/C1 checked.\r\nOSC overrides, disabled mode and invalid prefs stay silent.\r\n"));printf("SCHEME_UNIT_PASS kind=%d checks=%d\n",kind,count);fflush(stdout);Delay(100);
 CloseDevice((struct IORequest*)io);opened=0;CloseWindow(w);w=NULL;
 }
 printf("SCHEME_PASS checks=%d\n",count);rc=0;
out:
 if(opened)CloseDevice((struct IORequest*)io);
 if(w)CloseWindow(w);
 if(reader)DeleteIORequest((struct IORequest*)reader);
 if(io)DeleteIORequest((struct IORequest*)io);
 if(port)DeleteMsgPort(port);
 printf("SCHEME_SCRIPT rc=%d\n",rc);return rc;
}
