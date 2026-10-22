/* Isolated C:OpenURL stand-in: verify native ReadArgs and exact URI bytes. */
#include <proto/dos.h>
#include <stdio.h>
#include <string.h>
static void trace(const char *s){BPTR log=Open((CONST_STRPTR)"SER:",MODE_OLDFILE);if(log){Write(log,s,strlen(s));Close(log);}}
int main(void){trace("URL_PROBE entered\n");SIPTR values[1]={0};struct RDArgs*args=ReadArgs((CONST_STRPTR)"URL/A",values,NULL);if(!args){trace("URL_PROBE ReadArgs failed\n");return 20;}
 CONST_STRPTR uri=(CONST_STRPTR)values[0];BPTR out=Open((CONST_STRPTR)"RAM:ConsoleNG-URL-Test",MODE_NEWFILE);int rc=20;
 if(out){LONG n=strlen((const char*)uri);if(Write(out,uri,n)==n)rc=0;Close(out);}FreeArgs(args);trace(rc?"URL_PROBE capture failed\n":"URL_PROBE capture written\n");return rc;}
