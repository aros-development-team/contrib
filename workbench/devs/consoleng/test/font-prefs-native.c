/* Native additive font/default profile contract, including legacy coexistence. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <devices/conunit.h>
#include <stdio.h>
#include <string.h>
#include "consoleng.h"
static int req(struct IOStdReq *io,UWORD cmd,void *p,ULONG size){
 io->io_Command=cmd;io->io_Data=p;io->io_Length=size;
 return !DoIO((struct IORequest*)io)&&io->io_Actual==size;
}
#define CHECK(x) do{if(!(x)){printf("FONT_PREFS_FAIL line=%d\n",__LINE__);goto out;}}while(0)
int main(void){
 int rc=20,opened=0,unitOpen=0,saved=0;
 struct MsgPort *port=CreateMsgPort();struct IOStdReq *io=NULL,*unit=NULL;
 struct ConsolengDefaultProfile original,p,got;
 CHECK(port);io=(void*)CreateIORequest(port,sizeof(*io));unit=(void*)CreateIORequest(port,sizeof(*unit));CHECK(io&&unit);
 CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",CONU_LIBRARY,(struct IORequest*)io,0));opened=1;
 CHECK(req(io,CD_GETDEFAULTPROFILE,&original,sizeof(original)));saved=1;p=original;
 p.dp_Font=(struct ConsolengFontPrefs){sizeof(p.dp_Font),1,16,"DejaVuSansMono.font"};
 CHECK(req(io,CD_VALIDATEFONTPREFS,&p.dp_Font,sizeof(p.dp_Font)));
 CHECK(req(io,CD_SETDEFAULTPROFILE,&p,sizeof(p)));
 CHECK(req(io,CD_GETDEFAULTPROFILE,&got,sizeof(got))&&!memcmp(&p,&got,sizeof(p)));
 struct ConsolengDefaultPrefs legacy={sizeof(legacy),1,p.dp_Cursor,p.dp_History};legacy.dp_History.hp_Lines=128;
 CHECK(req(io,CD_SETDEFAULTPREFS,&legacy,sizeof(legacy)));
 CHECK(req(io,CD_GETDEFAULTPROFILE,&got,sizeof(got))&&got.dp_History.hp_Lines==128&&!memcmp(&got.dp_Font,&p.dp_Font,sizeof(p.dp_Font)));p=got;
 struct ConsolengDefaultProfile bad=p;bad.dp_Font.fp_Version=99;
 CHECK(!req(io,CD_SETDEFAULTPROFILE,&bad,sizeof(bad))&&io->io_Actual==0);
 bad=p;memset(bad.dp_Font.fp_Name,'x',sizeof(bad.dp_Font.fp_Name));
 CHECK(!req(io,CD_SETDEFAULTPROFILE,&bad,sizeof(bad))&&io->io_Actual==0);
 bad=p;strcpy(bad.dp_Font.fp_Name,"DefinitelyMissingConsoleNGFont.font");
 CHECK(!req(io,CD_VALIDATEFONTPREFS,&bad.dp_Font,sizeof(bad.dp_Font)));
 CHECK(!req(io,CD_SETDEFAULTPROFILE,&bad,sizeof(bad)));
 CHECK(req(io,CD_GETDEFAULTPROFILE,&got,sizeof(got))&&!memcmp(&p,&got,sizeof(p)));
 unit->io_Data=NULL;unit->io_Length=0;
 CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",CONU_CHARMAP,(struct IORequest*)unit,0));unitOpen=1;
 CHECK(((struct ConUnit*)unit->io_Unit)->cu_YRSize==16);
 struct ConsolengFontPrefs font;
 CHECK(req(unit,CD_GETFONTPREFS,&font,sizeof(font))&&font.fp_Pixels==16);
 font.fp_Pixels=12;CHECK(req(unit,CD_SETFONTPREFS,&font,sizeof(font)));
 CHECK(((struct ConUnit*)unit->io_Unit)->cu_YRSize==12);
 CHECK(!req(unit,CD_SETFONTPREFS,&font,sizeof(font)-1)&&unit->io_Actual==0);
 font=(struct ConsolengFontPrefs){sizeof(font),1,0,{0}};
 CHECK(req(unit,CD_SETFONTPREFS,&font,sizeof(font)));
 CHECK(req(unit,CD_GETFONTPREFS,&font,sizeof(font))&&!font.fp_Name[0]&&!font.fp_Pixels);
 puts("FONT_PREFS_PASS");rc=0;
out:
 if(unitOpen)CloseDevice((struct IORequest*)unit);
 if(saved&&!req(io,CD_SETDEFAULTPROFILE,&original,sizeof(original)))rc=20;
 if(opened)CloseDevice((struct IORequest*)io);
 if(unit)DeleteIORequest((struct IORequest*)unit);
 if(io)DeleteIORequest((struct IORequest*)io);
 if(port)DeleteMsgPort(port);
 printf("FONT_PREFS_SCRIPT rc=%d\n",rc);return rc;
}
