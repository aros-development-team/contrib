/* Native title stack, hidden/reveal, encoding and reset lifecycle. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <devices/conunit.h>
#include <stdio.h>
#include <string.h>
static struct IOStdReq *io;
static int put(const char*s){io->io_Command=CMD_WRITE;io->io_Data=(APTR)s;io->io_Length=strlen(s);return !DoIO((struct IORequest*)io)&&io->io_Actual==strlen(s);}
static int split(const char*s){char b[2]={0};while(*s){b[0]=*s++;if(!put(b))return 0;}return 1;}
#define CHECK(c) do{if(!(c)){printf("TITLE_FAIL line=%d kind=%d hidden=%d\n",__LINE__,kind,hidden);fflush(stdout);Delay(500);goto out;}}while(0)
int main(void){int rc=20,opened=0,kind=0,hidden=0,count=0;struct Window*w=NULL;struct MsgPort*p=CreateMsgPort();io=p?(void*)CreateIORequest(p,sizeof(*io)):NULL;CHECK(io);
for(kind=0;kind<2;kind++){
 w=OpenWindowTags(NULL,WA_Left,30,WA_Top,30,WA_Width,730,WA_Height,470,WA_Title,(IPTR)"Original console title",WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_SizeGadget,TRUE,WA_SizeBRight,TRUE,WA_MinWidth,200,WA_MinHeight,160,WA_MaxWidth,-1,WA_MaxHeight,-1,TAG_DONE);CHECK(w);
 io->io_Data=w;io->io_Length=sizeof(*w);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",kind,(struct IORequest*)io,0));opened=1;
 CHECK(split("\x1b[22t\x1b]2;Changed\x07\x1b[23t"));CHECK(!strcmp((char*)w->Title,"Original console title"));count++;
 for(hidden=0;hidden<2;hidden++){
  CHECK(put("\x1b]2;Visible anchor\x07"));CHECK(put(hidden?"\x1b[>9h":"\x1b[>9l"));
  CHECK(split("\x1b]2;Caf\xc3\xa9\x07\x1b[22;0t\x1b]2;Nested\x07\x1b[22;2t\x1b]2;Temporary\x07\x1b[23;2t"));
  CHECK(!strcmp((char*)w->Title,hidden?"Visible anchor":"Nested"));CHECK(put("\x1b[>9l"));CHECK(!strcmp((char*)w->Title,"Nested"));
  CHECK(split("\x1b[23;0t"));CHECK(!strcmp((char*)w->Title,"Caf\xe9"));CHECK(put("\x1b[23t"));CHECK(!strcmp((char*)w->Title,"Caf\xe9"));count++;
 }
 for(int n=0;n<64;n++){char b[64];snprintf(b,sizeof(b),"\x1b]2;Title %d\x07\x1b[22t",n);CHECK(put(b));}
 for(int n=63;n>=0;n--){char b[64];snprintf(b,sizeof(b),"Title %d",n);CHECK(put("\x1b[23t"));CHECK(!strcmp((char*)w->Title,b));}count++;
 CHECK(put("\x1b[22t\x1b" "c\x1b[23t"));CHECK(!strcmp((char*)w->Title,""));count++;
 printf("TITLE_UNIT kind=%d PASS\n",kind);fflush(stdout);
 CHECK(put("\x1b[?25l\x1b]2;ConsoleNG - restored application titles\x07\x1b[38;2;230;230;230m\x1b[48;2;25;60;80m\x1b[2J\x1b[HPASS: title stack restores nested application titles.\r\nHidden titles wait until reveal; Latin-1 bytes preserved.\r\nOriginal window title and reset cleanup verified.\r\n64 nested saves and restores pass on both unit types."));
 if(kind){printf("TITLE_PASS cases=%d\n",count);fflush(stdout);Delay(600);}
 /* Leave allocated entries for the close cleanup path. */
 CHECK(put("\x1b[22t\x1b[22t"));CloseDevice((struct IORequest*)io);opened=0;CloseWindow(w);w=NULL;
}rc=0;
out:if(opened)CloseDevice((struct IORequest*)io);if(w)CloseWindow(w);if(io)DeleteIORequest((struct IORequest*)io);if(p)DeleteMsgPort(p);printf("TITLE_SCRIPT rc=%d\n",rc);return rc;}
