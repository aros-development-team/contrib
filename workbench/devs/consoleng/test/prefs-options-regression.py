#!/usr/bin/env python3
"""Actual portable CLI parser: traditional keywords, legacy forms, and atomic failure."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
s=r'''
#include <assert.h>
#include "options.h"
static void test(int argc,char **argv,int ok,int mode,const char *path){
 struct PrefsOptions p={MODE_SAVE,"unchanged"};
 assert(parseOptions(argc,argv,&p)==ok);
 if(ok){assert(p.mode==mode);assert(path? p.from&&!strcmp(p.from,path):!p.from);}
 else {assert(p.mode==MODE_SAVE&&!strcmp(p.from,"unchanged"));}
}
int main(void){
 char *args[]={"ConsoleNG", "FROM", "RAM:My profile.prefs", "USE"};
 test(0,(char**)1,1,MODE_GUI,0);test(1,args,1,MODE_GUI,0);
 test(4,args,1,MODE_USE,args[2]);test(3,args,1,MODE_GUI,args[2]);test(2,args,0,0,0);
 char *legacy[]={"ConsoleNG","use","RAM:My profile.prefs"};test(3,legacy,1,MODE_USE,legacy[2]);
 char *assign[]={"ConsoleNG","SAVE","fRoM=SYS:foo.prefs"};test(3,assign,1,MODE_SAVE,"SYS:foo.prefs");
 char *restore[]={"ConsoleNG","restore"};test(2,restore,1,MODE_RESTORE,0);
 const char *bad[][4]={{"USE","SAVE",0},{"FROM=",0},{"FROM","",0},{"FROM=x","FROM=y",0},{"RESTORE","RESTORE",0},{"unknown",0},{"USE","FROM",0},{"USE","FROM=x","extra",0}};
 for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);i++){char *v[5]={"ConsoleNG"};int n=1;while(bad[i][n-1]){v[n]=(char*)bad[i][n-1];n++;}test(n,v,0,0,0);}
 return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';exe=Path(d)/'test';p.write_text(s)
 subprocess.run(['cc','-Wall','-Werror','-fsanitize=address,undefined','-I'+str(r/'prefs'),str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
print('Prefs options PASS: Workbench, FROM, USE/SAVE/RESTORE, case, spaced paths, malformed/duplicate options leave output unchanged')
