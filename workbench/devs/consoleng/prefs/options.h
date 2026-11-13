#ifndef CONSOLENG_PREFS_OPTIONS_H
#define CONSOLENG_PREFS_OPTIONS_H
#include <string.h>
enum PrefsMode { MODE_GUI, MODE_USE, MODE_SAVE, MODE_RESTORE };
struct PrefsOptions { enum PrefsMode mode; const char *from; };
static int optionEqual(const char *a,const char *b)
{
    while(*a && *b){
        unsigned char x=*a++,y=*b++;
        if(x>='a'&&x<='z')x-=32;
        if(y>='a'&&y<='z')y-=32;
        if(x!=y)return 0;
    }
    return !*a && !*b;
}
static int parseOptions(int argc,char **argv,struct PrefsOptions *out)
{
    struct PrefsOptions p={MODE_GUI,NULL};
    /* argc == 0 is Workbench; argv is not an argument vector in that case. */
    for(int i=1;i<argc;i++){
        const char *arg=argv[i];
        enum PrefsMode mode=optionEqual(arg,"USE")?MODE_USE:
            optionEqual(arg,"SAVE")?MODE_SAVE:optionEqual(arg,"RESTORE")?MODE_RESTORE:MODE_GUI;
        if(mode!=MODE_GUI){if(p.mode!=MODE_GUI)return 0;p.mode=mode;}
        else if(optionEqual(arg,"FROM")){
            if(p.from||++i>=argc||!*argv[i])return 0;
            p.from=argv[i];
        }else if(strlen(arg)>=5 && (arg[0]=='f'||arg[0]=='F') &&
            (arg[1]=='r'||arg[1]=='R') && (arg[2]=='o'||arg[2]=='O') &&
            (arg[3]=='m'||arg[3]=='M') && arg[4]=='='){
            if(p.from||!arg[5])return 0;
            p.from=arg+5;
        }else if(argc==3 && i==2 && p.mode!=MODE_GUI && !p.from && *arg){
            /* Preserve the original USE <path> spelling. */
            p.from=arg;
        }else return 0;
    }
    *out=p;return 1;
}
#endif
