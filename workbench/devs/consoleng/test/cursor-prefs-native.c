/* Native preferences snapshot, validation, preview and Cancel-style restore. */
#include <proto/exec.h>
#include <proto/dos.h>
#include <devices/conunit.h>
#include <exec/errors.h>
#include <stdio.h>
#include <string.h>
#include "consoleng.h"
static int prefs(struct IOStdReq *io,int command,struct ConsolengCursorPrefs *p,ULONG size){io->io_Command=command;io->io_Data=p;io->io_Length=size;io->io_Actual=999;int e=DoIO((struct IORequest*)io);printf("PREFS_IO command=%d error=%d actual=%lu\n",command,e,(unsigned long)io->io_Actual);fflush(stdout);return e;}
static int put(struct IOStdReq *io,const char *text){io->io_Command=CMD_WRITE;io->io_Data=(APTR)text;io->io_Length=strlen(text);return DoIO((struct IORequest*)io)==0;}
int main(void){
 int rc=20;struct MsgPort *port=CreateMsgPort();struct IOStdReq *io=port?(struct IOStdReq*)CreateIORequest(port,sizeof(*io)):NULL;
 if(!io){if(port)DeleteMsgPort(port);return 20;}
 if(OpenDevice((CONST_STRPTR)"consoleng.device",CONU_CHARMAP,(struct IORequest*)io,0))goto cleanup;
 struct ConsolengCursorPrefs saved,preview,readback,bad;
 if(prefs(io,CD_GETCURSORPREFS,&saved,sizeof(saved))||saved.cp_Size!=sizeof(saved)||saved.cp_Version!=1||saved.cp_Style!=0||saved.cp_BlinkIntervalMS!=500||saved.cp_IdleTimeoutMS)goto done;
 preview=saved;preview.cp_Style=5;preview.cp_BlinkIntervalMS=350;preview.cp_IdleTimeoutMS=8000;
 if(!put(io,"\x1b%G\x1b]50;DejaVuSansMono,24\x07\x1b[2J\x1b[HConsoleNG preferences interface\r\nPreview: bar, 350 ms, 8 s idle\r\nSnapshot / validate / restore\x1b[6;1HCursor ->    \x1b[6;11H")||prefs(io,CD_SETCURSORPREFS,&preview,sizeof(preview)))goto done;
 if(prefs(io,CD_GETCURSORPREFS,&readback,sizeof(readback))||memcmp(&preview,&readback,sizeof(preview)))goto done;
 for(int i=0;i<4;i++){
  bad=preview;if(i==0)bad.cp_Style=7;if(i==1)bad.cp_Version=2;if(i==2)bad.cp_Size=0;if(i==3)bad.cp_BlinkIntervalMS=49;
  if(prefs(io,CD_SETCURSORPREFS,&bad,sizeof(bad))!=CONSOLENG_ERR_BADPREFS||io->io_Actual)goto done;
  if(prefs(io,CD_GETCURSORPREFS,&readback,sizeof(readback))||memcmp(&preview,&readback,sizeof(preview)))goto done;
 }
 if(prefs(io,CD_GETCURSORPREFS,&readback,sizeof(readback)-1)!=IOERR_BADLENGTH||io->io_Actual)goto done;
 puts("PREFS_PREVIEW_READY");fflush(stdout);Delay(500);
 puts("PREFS_IDLE_READY");fflush(stdout);Delay(400);
 if(prefs(io,CD_SETCURSORPREFS,&saved,sizeof(saved))||prefs(io,CD_GETCURSORPREFS,&readback,sizeof(readback))||memcmp(&saved,&readback,sizeof(saved)))goto done;
 puts("PREFS_RESTORE_READY");fflush(stdout);Delay(400);rc=0;
done:CloseDevice((struct IORequest*)io);
cleanup:DeleteIORequest((struct IORequest*)io);DeleteMsgPort(port);printf("PREFS_DONE rc=%d\n",rc);fflush(stdout);return rc;
}
