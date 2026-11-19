/* DOS -> aux-handler -> con-handler -> private ConsoleNG integration probe. */
#include <proto/dos.h>
#include <stdio.h>
#include <string.h>
static int put(BPTR fh,const char *s) {
    LONG size=strlen(s),n=Write(fh,(APTR)s,size);
    if(n!=size){printf("WRITE_FAIL actual=%ld expected=%ld error=%ld\n",(long)n,(long)size,(long)IoErr());return 0;}
    return 1;
}
int main(int argc,char **argv) {
    const char *path=argc>1?argv[1]:"PARITY:";
    BPTR fh=Open((CONST_STRPTR)path,MODE_NEWFILE);int rc=20;
    if(!fh){printf("OPEN_FAIL error=%ld\n",(long)IoErr());return 20;}
    if(!SetMode(fh,0)||!put(fh,"\x1b%G\x1b]50;DejaVuSansMono,14\x07\x1b[2J\x1b[H"))goto done;
    for(int i=0;i<60;i++){char line[80];snprintf(line,sizeof(line),"DOS primary row %02d - preserved through raw alternate screen\r\n",i);if(!put(fh,line))goto done;}
    puts("HANDLER_PRIMARY_READY");fflush(stdout);Delay(500);
    if(!SetMode(fh,1)){puts("RAW_MODE_FAIL");goto done;}
    if(!put(fh,"\x1b[?1049h\x1b[2J\x1b[HDOS raw alternate screen\r\nHandler input and output test\x1b[3;5H\x1b[6n"))goto done;
    char reply[32];int len=0;
    while(len<31){
        if(!WaitForChar(fh,5000000)){puts("REPLY_TIMEOUT");goto done;}
        if(Read(fh,reply+len,1)!=1){puts("REPLY_READ_FAIL");goto done;}
        if(reply[len++]=='R')break;
    }
    reply[len]=0;
    printf("HANDLER_REPLY");for(int i=0;i<len;i++)printf(" %02x",(unsigned char)reply[i]);puts("");
    if(strcmp(reply,"\x1b[3;5R")){puts("REPLY_MISMATCH");goto done;}
    puts("HANDLER_ALTERNATE_READY");fflush(stdout);Delay(500);
    if(!put(fh,"\x1b" "7\x1b[?1049lDOS PRIMARY RESTORED")||!SetMode(fh,0))goto done;
    puts("HANDLER_PRIMARY_RESTORED");fflush(stdout);Delay(1000);
    rc=0;
done:
    SetMode(fh,0);Close(fh);printf("HANDLER_DONE rc=%d\n",rc);fflush(stdout);return rc;
}
