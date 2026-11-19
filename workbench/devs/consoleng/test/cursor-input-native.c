/* DEC12 shape preservation and no-echo raw key blink reset. */
#include <proto/dos.h>
#include <stdio.h>
#include <string.h>
static int put(BPTR f,const char *s){return Write(f,(APTR)s,strlen(s))==(LONG)strlen(s);}
int main(void){
 BPTR f=Open((CONST_STRPTR)"PARITY:",MODE_NEWFILE);int rc=20;char key=0;
 if(!f)return 20;
 if(!SetMode(f,1)||!put(f,"\x1b%G\x1b]50;DejaVuSansMono,24\x07\x1b[2J\x1b[HNative ConsoleNG cursor input\r\nDEC12 preserves underline shape\r\nRaw key restores blink, no echo\x1b[6;1HCursor ->    \x1b[6;11H\x1b[4 q\x1b[?12h"))goto done;
 puts("MODE_BLINK_READY");fflush(stdout);Delay(400);
 if(!put(f,"\x1b[?12l"))goto done;
 puts("MODE_STEADY_READY");fflush(stdout);Delay(400);
 if(!put(f,"\x1b[?12h"))goto done;
 puts("MODE_KEY_READY");fflush(stdout);
 if(!WaitForChar(f,30000000)||Read(f,&key,1)!=1)goto done;
 printf("MODE_KEY_RECEIVED=%02x\n",(unsigned char)key);fflush(stdout);
 /* Deliberately no console output: input alone must restart the cursor. */
 if(key!='x')goto done;
 Delay(400);rc=0;
done:SetMode(f,0);Close(f);printf("MODE_DONE rc=%d\n",rc);fflush(stdout);return rc;
}
