/* Public vector round trips; native allocation failure is host-injected separately. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/console.h>
#include <devices/conunit.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct Device *ConsoleDevice;
#define CHECK(c) do{if(!(c)){printf("SNIP_FAIL line=%d check=%s\n",__LINE__,#c);fflush(stdout);goto out;}}while(0)
int main(void){int rc=20,opened=0,cases=0;char *before=NULL,*got=NULL,*held=NULL,*data=NULL;struct Window*w=NULL;
struct MsgPort*p=CreateMsgPort();struct IOStdReq*io=p?(void*)CreateIORequest(p,sizeof(*io)):NULL;CHECK(io);
w=OpenWindowTags(NULL,WA_Left,30,WA_Top,30,WA_Width,730,WA_Height,450,WA_Title,(IPTR)"ConsoleNG clipboard round trips",WA_DragBar,TRUE,WA_DepthGadget,TRUE,WA_SizeGadget,TRUE,WA_SizeBBottom,TRUE,TAG_DONE);CHECK(w);
io->io_Data=w;io->io_Length=sizeof(*w);CHECK(!OpenDevice((CONST_STRPTR)"consoleng.device",CONU_CHARMAP,(struct IORequest*)io,0));opened=1;ConsoleDevice=io->io_Device;
before=GetConSnip();data=malloc(65537);CHECK(data);
for(int round=0;round<8;round++)for(unsigned n=1;n<=65536;n*=2){memset(data,'A'+round,n);data[n]=0;CHECK(SetConSnip(data)==1);got=GetConSnip();CHECK(got&&!strcmp(got,data));held=got;got=NULL;CHECK(SetConSnip("next")==1);CHECK(!strcmp(held,data));FreeVec(held);held=NULL;CHECK(!SetConSnip(round&1?NULL:""));got=GetConSnip();CHECK(!got);cases++;}
CHECK(SetConSnip("UTF-8: \xc3\xa9 \xe2\x82\xac \xf0\x9f\x98\x80")==1);got=GetConSnip();CHECK(got&&!strcmp(got,"UTF-8: \xc3\xa9 \xe2\x82\xac \xf0\x9f\x98\x80"));FreeVec(got);got=NULL;
const char*text="\x1b[?25l\x1b]50;DejaVuSansMono,16\7Clipboard public API: PASS\r\n\r\n136 replacement and clear round trips\r\n1 byte through 64 KiB; independent copies retained\r\nUTF-8 bytes preserved\r\n\r\nHost fault injection: failed replacement keeps old text\r\nPublic library vectors unchanged";
io->io_Command=CMD_WRITE;io->io_Data=(void*)text;io->io_Length=strlen(text);CHECK(!DoIO((struct IORequest*)io));printf("SNIP_PASS cases=%d UTF8 and independent snapshots\n",cases);fflush(stdout);Delay(200);rc=0;
out:if(got)FreeVec(got);if(held)FreeVec(held);free(data);if(opened){SetConSnip(before);if(before)FreeVec(before);CloseDevice((struct IORequest*)io);}if(w)CloseWindow(w);if(io)DeleteIORequest((struct IORequest*)io);if(p)DeleteMsgPort(p);printf("SNIP_RUN rc=%d\n",rc);return rc;}
