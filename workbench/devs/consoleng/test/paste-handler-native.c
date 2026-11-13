/* Isolated ConClip -> DOS handler -> raw application paste acceptance. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/iffparse.h>
#include <proto/intuition.h>
#include <datatypes/textclass.h>
#include <devices/conunit.h>
#include <dos/dosextens.h>
#include <exec/errors.h>
#include <stdio.h>
#include <string.h>
#include "../include/consoleng.h"
struct Library *IFFParseBase;
static BPTR stream;
static UBYTE data[16372],expected[16384],got[16384];
static int put(const char *s){return Write(stream,s,strlen(s))==(LONG)strlen(s);}
static int clipboard(ULONG n){
 int ok=0;struct IFFHandle *iff=AllocIFF();if(!iff)return 0;
 iff->iff_Stream=(IPTR)OpenClipboard(0);if(!iff->iff_Stream){FreeIFF(iff);return 0;}
 InitIFFasClip(iff);
 if(!OpenIFF(iff,IFFF_WRITE)){
  if(!PushChunk(iff,ID_FTXT,ID_FORM,IFFSIZE_UNKNOWN)){
   if(!PushChunk(iff,0,ID_CHRS,IFFSIZE_UNKNOWN)){ok=WriteChunkBytes(iff,data,n)==(LONG)n;PopChunk(iff);}
   PopChunk(iff);
  }
  CloseIFF(iff);
 }
 CloseClipboard((struct ClipboardHandle*)iff->iff_Stream);FreeIFF(iff);return ok;
}
#define CHECK(c) do{if(!(c)){printf("PASTE_FAIL line=%d\n",__LINE__);goto out;}}while(0)
int main(void){
 int rc=20;struct InfoData info;
 IFFParseBase=OpenLibrary((CONST_STRPTR)"iffparse.library",0);CHECK(IFFParseBase);
 stream=Open((CONST_STRPTR)"RGCON:",MODE_OLDFILE);CHECK(stream);
 struct FileHandle *fh=BADDR(stream);CHECK(DoPkt(fh->fh_Type,ACTION_DISK_INFO,(SIPTR)MKBADDR(&info),0,0,0,0));
 struct IOStdReq *io=(void*)(IPTR)info.id_InUse;CHECK(io&&io->io_Unit);
 struct Window *w=((struct ConUnit*)io->io_Unit)->cu_Window;
 SetWindowTitles(w,(CONST_STRPTR)"ConsoleNG native bracketed paste",(CONST_STRPTR)-1);
 CHECK(SetMode(stream,TRUE));CHECK(put("\x1b%G\x1b]50;topaz,8\x07\x1b[2J\x1b[HNative clipboard paste acceptance\r\n"));
 ULONG sizes[]={1,4096,16360};
 for(int mode=0;mode<2;mode++)for(int test=0;test<3;test++){
  ULONG n=sizes[test],length=0,pos=0,enabled=9;struct IOStdReq query=*io;
  CHECK(put(mode?"\x1b[?2004h":"\x1b[?2004l"));
  query.io_Command=CD_GETPASTEMODE;query.io_Data=&enabled;query.io_Length=sizeof(enabled);
  CHECK(!DoIO((struct IORequest*)&query)&&query.io_Actual==sizeof(enabled)&&enabled==(ULONG)mode);
  for(ULONG i=0;i<n;i++)data[i]=(i%79==78)?'\n':'a'+i%26;
  if(n>200){memcpy(data+100,"\x1b[201~\x03",7);memcpy(data+150,"\x9b" "0 v",4);}
  if(mode){memcpy(expected,"\x1b[200~",6);length=6;}
  for(ULONG i=0;i<n;i++)if(!mode||(data[i]!=27&&data[i]!=3))expected[length++]=data[i];
  if(mode){memcpy(expected+length,"\x1b[201~",6);length+=6;}
  CHECK(clipboard(n));printf("PASTE_READY mode=%d test=%d bytes=%lu\n",mode,test,(unsigned long)n);fflush(stdout);
  while(pos<length){
   CHECK(WaitForChar(stream,10000000));ULONG want=pos%3==0?1:pos%3==1?7:257;if(want>length-pos)want=length-pos;
   LONG count=Read(stream,got+pos,want);CHECK(count>0);pos+=count;
  }
  CHECK(!memcmp(got,expected,length));CHECK(!WaitForChar(stream,100000));
  printf("PASTE_CASE_PASS mode=%d test=%d received=%lu\n",mode,test,(unsigned long)length);fflush(stdout);
 }
 CHECK(put("\x1b[?2004l\r\nPASS: plain and bracketed paste, 1/4096/16360 bytes.\r\nESC/ETX filtered only in bracketed mode.\r\n"));
 puts("PASTE_PASS");fflush(stdout);Delay(600);rc=0;
out:
 if(stream){put("\x1b[?2004l");SetMode(stream,FALSE);Close(stream);}
 if(IFFParseBase)CloseLibrary(IFFParseBase);
 printf("PASTE_SCRIPT rc=%d\n",rc);return rc;
}
