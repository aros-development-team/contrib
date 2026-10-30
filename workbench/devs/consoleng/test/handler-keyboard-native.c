/* Actual raw keys and cooked line editing through the DOS console handler. */
#include <proto/dos.h>
#include <proto/exec.h>
#include <exec/execbase.h>
#include <stdio.h>
#include <string.h>
static int put(BPTR fh,const char *s){return Write(fh,(APTR)s,strlen(s))==(LONG)strlen(s);}
static int line(BPTR fh,char *buf,int cap){int n=0;while(n<cap-1){if(!WaitForChar(fh,30000000)||Read(fh,buf+n,1)!=1)return -1;if(buf[n++]=='\n'||buf[n-1]=='\r')break;}buf[n]=0;return n;}
static void hex(const char *label,const char *buf,int n){printf("%s",label);for(int i=0;i<n;i++)printf(" %02x",(unsigned char)buf[i]);puts("");fflush(stdout);}
int main(int argc,char **argv){
 if(argc>1&&!strcmp(argv[1],"PRECHECK")){
  Forbid();int loaded=FindName(&SysBase->DeviceList,(CONST_STRPTR)"consoleng.device")!=NULL;Permit();
  printf("CANONICAL_PRELOADED=%d\n",loaded);return loaded?20:0;
 }
 BPTR fh=Open((CONST_STRPTR)"PARITY:",MODE_NEWFILE);int rc=20,n;char buf[64];
 if(!fh){printf("KEY_OPEN_FAIL %ld\n",(long)IoErr());return 20;}
 if(!SetMode(fh,1)||!put(fh,"\x1b%G\x1b]50;DejaVuSansMono,14\x07\x1b[?1049h\x1b[2J\x1b[HRaw input test: a, Left, b, Return\r\n"))goto done;
 puts("KEY_RAW_READY");fflush(stdout);n=line(fh,buf,sizeof(buf));hex("KEY_RAW_BYTES",buf,n>0?n:0);
 if(n<0||(strcmp(buf,"a\x9b" "Db\r")&&strcmp(buf,"a\x1b[Db\r")&&strcmp(buf,"a\x9b" "Db\n")&&strcmp(buf,"a\x1b[Db\n")))goto done;
 if(!put(fh,"\x1b[?1h\r\nApplication cursor mode: Left, Return\r\n"))goto done;
 puts("KEY_APP_READY");fflush(stdout);n=line(fh,buf,sizeof(buf));hex("KEY_APP_BYTES",buf,n>0?n:0);
 if(n<0||(strcmp(buf,"\x1bOD\r")&&strcmp(buf,"\x1bOD\n")))goto done;
 if(!put(fh,"\x1b[?1l"))goto done;
 if(!put(fh,"\r\nKeypad application state (default bindings): \x1b="))goto done;
 puts("KEY_KP_APP_READY");fflush(stdout);n=line(fh,buf,sizeof(buf));hex("KEY_KP_APP_BYTES",buf,n>0?n:0);
 if(n<0||(strcmp(buf,"1\r")&&strcmp(buf,"1\n")))goto done;
 if(!put(fh,"\r\nKeypad numeric state: \x1b>"))goto done;
 puts("KEY_KP_NUM_READY");fflush(stdout);n=line(fh,buf,sizeof(buf));hex("KEY_KP_NUM_BYTES",buf,n>0?n:0);
 if(n<0||(strcmp(buf,"2\r")&&strcmp(buf,"2\n")))goto done;
 if(!put(fh,"\x1b[?1049l\x1b[2J\x1b[HUTF-8 Cyrillic EL: \xd0\x9b\r\nCooked edit test: abc, Left, Backspace, X, Return\r\n")||!SetMode(fh,0))goto done;
 puts("KEY_COOKED_READY");fflush(stdout);n=line(fh,buf,sizeof(buf));hex("KEY_COOKED_BYTES",buf,n>0?n:0);
 if(n<0||(strcmp(buf,"aXc\n")&&strcmp(buf,"aXc\r")))goto done;
 put(fh,"\r\nPASS: raw cursor key and cooked line editing\r\n");puts("KEY_PASS");fflush(stdout);Delay(500);rc=0;
done:
 SetMode(fh,0);Close(fh);printf("KEY_DONE rc=%d\n",rc);fflush(stdout);return rc;
}
