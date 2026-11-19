/* Persistent editor model, independent of Zune and of user identity resolution.
 * ENV:/ENVARC: are resolved by DOS; no private usernames or home paths here.
 */
#ifndef CONSOLENG_SETTINGS_H
#define CONSOLENG_SETTINGS_H
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "../binding_prefs.h"
#include "semantic_text.h"
struct ConsoleSettings {
    struct ConsolengCursorPrefs cursor;
    struct ConsolengHistoryPrefs history;
    struct ConsolengFontPrefs font;
    struct ConsolengColorPrefs colors;
    ULONG emoji, bell, oscCopy;
    ULONG cursorCustom, cursorRGB;
    struct ConsolengBindingPrefs bindings;
    ULONG wheelLines;
    struct ConsolengSemanticPrefs semantic;
};
static int settingsValid(const struct ConsoleSettings *p)
{
    if(p->semantic.sp_Size) {if(!con_semantic_prefs_valid(&p->semantic))return 0;}
    else {struct ConsolengSemanticPrefs zero={0};if(memcmp(&p->semantic,&zero,sizeof(zero)))return 0;}
    if(p->wheelLines>CONSOLENG_SCROLL_MAX_LINES)return 0;
    if(!con_binding_prefs_valid(&p->bindings))return 0;
    if(p->cursorCustom>1||p->cursorRGB>0xffffffUL)return 0;
    if(p->emoji>1||p->bell>1||p->oscCopy>3)return 0;
    if(p->colors.cp_Size!=sizeof(p->colors)||p->colors.cp_Version!=1||(p->colors.cp_Mask&~CONSOLENG_COLOR_MASK))return 0;
    for(unsigned i=0;i<CONSOLENG_COLOR_COUNT;i++)if(p->colors.cp_RGB[i]>0xffffffUL)return 0;
    if(p->font.fp_Size!=sizeof(p->font)||p->font.fp_Version!=1||p->font.fp_Pixels>65535||
       !memchr(p->font.fp_Name,0,sizeof(p->font.fp_Name)))return 0;
    if((!p->font.fp_Name[0])!=(p->font.fp_Pixels==0))return 0;
    for(const unsigned char *n=(const void*)p->font.fp_Name;*n;n++)if(*n<32||*n==127)return 0;
    return p->cursor.cp_Size==sizeof(p->cursor)&&p->cursor.cp_Version==1&&
        p->cursor.cp_Style<=6&&p->cursor.cp_BlinkIntervalMS>=50&&p->cursor.cp_BlinkIntervalMS<=5000&&
        (!p->cursor.cp_IdleTimeoutMS||(p->cursor.cp_IdleTimeoutMS>=1000&&p->cursor.cp_IdleTimeoutMS<=3600000))&&
        p->history.hp_Size==sizeof(p->history)&&p->history.hp_Version==1&&p->history.hp_Lines<=CONSOLENG_HISTORY_MAX_LINES;
}
static int settingsParseLegacy(const char *text,struct ConsoleSettings *out)
{
    unsigned long style,interval,timeout,lines,pixels;char extra,name[128];int n;
    struct ConsoleSettings p=*out;
    p.oscCopy &= 1;
    const char readHeader[]="ConsoleNG 17\nosc52read=1\n";
    if(!strncmp(text,"ConsoleNG 17\n",13)) {
        if(strncmp(text,readHeader,sizeof(readHeader)-1))return 0;
        const char *nested=text+sizeof(readHeader)-1;
        if(!strncmp(nested,"ConsoleNG 17\n",13)||!settingsParseLegacy(nested,&p))return 0;
        p.oscCopy|=2;*out=p;return 1;
    }

    if(!strncmp(text,"ConsoleNG 8\n",12)||!strncmp(text,"ConsoleNG 9\n",12)||!strncmp(text,"ConsoleNG 10\n",13)||!strncmp(text,"ConsoleNG 11\n",13)||!strncmp(text,"ConsoleNG 12\n",13)||!strncmp(text,"ConsoleNG 13\n",13)||!strncmp(text,"ConsoleNG 14\n",13)||!strncmp(text,"ConsoleNG 15\n",13)||!strncmp(text,"ConsoleNG 16\n",13)||!strncmp(text,"ConsoleNG 18\n",13)||!strncmp(text,"ConsoleNG 20\n",13)||!strncmp(text,"ConsoleNG 21\n",13)||!strncmp(text,"ConsoleNG 22\n",13)||!strncmp(text,"ConsoleNG 23\n",13)||!strncmp(text,"ConsoleNG 24\n",13)||!strncmp(text,"ConsoleNG 25\n",13)||!strncmp(text,"ConsoleNG 26\n",13)||!strncmp(text,"ConsoleNG 27\n",13)||!strncmp(text,"ConsoleNG 28\n",13)) {
        unsigned header = text[10]=='1'||text[10]=='2'?13:12;
        const char *start=strstr(text+12,"\nbindings=");char legacy[512],*end;
        if(!start||(size_t)(start-text)>=sizeof(legacy)-2)return 0;
        const char *cur=start+10;
        if(*cur<'0'||*cur>'9')return 0;
        errno=0;unsigned long count=strtoul(cur,&end,10);
        if(errno||count>CONSOLENG_BINDING_MAX||*end!='\n')return 0;
        struct ConsolengBindingPrefs bindings={sizeof(bindings),header==13?(text[10]=='2'?(text[11]-'0'+11):text[11]=='8'?10:text[11]-'0'+3):(text[10]=='9'?2:1),count,{{0}}};
        cur=end+1;
        for(unsigned i=0;i<count;i++) {
            unsigned long values[3];
            for(unsigned j=0;j<3;j++) {
                if(*cur<'0'||*cur>'9')return 0;
                errno=0;values[j]=strtoul(cur,&end,10);
                if(errno||values[j]>(j==0?CONSOLENG_BIND_RAW+127:j==1?32767:255)||*end!=(j==2?'\n':' '))return 0;
                cur=end+1;
            }
            bindings.bp_Entries[i]=(struct ConsolengKeyBinding){values[0],values[1],values[2]};
        }
        if(*cur||!con_binding_prefs_valid(&bindings))return 0;
        memcpy(legacy,"ConsoleNG 7\n",12);
        memcpy(legacy+12,text+header,start-text-header);
        size_t legacylen=12+(start-text-header);legacy[legacylen]='\n';legacy[legacylen+1]=0;
        if(!settingsParseLegacy(legacy,&p))return 0;
        p.bindings=bindings;*out=p;return 1;
    }
    if(!strncmp(text,"ConsoleNG 7\n",12)) {
        const char *color=strstr(text+12,"\ncursorcolor=");char legacy[512],*end;
        if(!color||(size_t)(color-text)>=sizeof(legacy)-2)return 0;
        const char *value=color+13;
        if((value[0]!='0'&&value[0]!='1')||value[1]!=' ')return 0;
        const char *rgb=value+2;
        if(!((*rgb>='0'&&*rgb<='9')||(*rgb>='a'&&*rgb<='f')||(*rgb>='A'&&*rgb<='F')))return 0;
        errno=0;unsigned long parsed=strtoul(rgb,&end,16);
        if(errno||parsed>0xffffffUL||strcmp(end,"\n"))return 0;
        memcpy(legacy,text,color-text);legacy[10]='6';legacy[color-text]='\n';legacy[color-text+1]=0;
        if(!settingsParseLegacy(legacy,&p))return 0;
        p.cursorCustom=value[0]-'0';p.cursorRGB=parsed;*out=p;return 1;
    }
    if(!strncmp(text,"ConsoleNG 6\n",12)) {
        const char *copy=strstr(text+12,"\nosc52=");char legacy[512];
        if(!copy||(size_t)(copy-text)>=sizeof(legacy)-2)return 0;
        if(strcmp(copy+7,"0\n")&&strcmp(copy+7,"1\n"))return 0;
        memcpy(legacy,text,copy-text);legacy[10]='5';legacy[copy-text]='\n';legacy[copy-text+1]=0;
        if(!settingsParseLegacy(legacy,&p))return 0;
        p.oscCopy=copy[7]-'0';*out=p;return 1;
    }
    if(!strncmp(text,"ConsoleNG 5\n",12)) {
        const char *bell=strstr(text+12,"\nbell=");char legacy[512];
        if(!bell||(size_t)(bell-text)>=sizeof(legacy)-2)return 0;
        if(strcmp(bell+6,"0\n")&&strcmp(bell+6,"1\n"))return 0;
        memcpy(legacy,text,bell-text);legacy[10]='4';legacy[bell-text]='\n';legacy[bell-text+1]=0;
        if(!settingsParseLegacy(legacy,&p))return 0;
        p.bell=bell[6]-'0';*out=p;return 1;
    }
    if(!strncmp(text,"ConsoleNG 4\n",12)) {
        const char *emoji=strstr(text+12,"\nemoji=");
        char legacy[512];
        if(!emoji||(size_t)(emoji-text)>=sizeof(legacy)-2)return 0;
        if(strcmp(emoji+7,"0\n")&&strcmp(emoji+7,"1\n"))return 0;
        memcpy(legacy,text,emoji-text);legacy[10]='3';legacy[emoji-text]='\n';legacy[emoji-text+1]=0;
        if(!settingsParseLegacy(legacy,&p))return 0;
        p.emoji=emoji[7]-'0';*out=p;return 1;
    }
    if(!strncmp(text,"ConsoleNG 3\n",12)) {
        const char *color=strstr(text+12,"\ncolors=");
        char legacy[256];
        if(!color||(size_t)(color-text)>=sizeof(legacy)-2)return 0;
        memcpy(legacy,text,color-text);legacy[10]='2';legacy[color-text]='\n';legacy[color-text+1]=0;
        if(!settingsParseLegacy(legacy,&p))return 0;
        const char *cur=color+8;char *end;
        unsigned long values[19];
        for(unsigned i=0;i<19;i++) {
            if(!((*cur>='0'&&*cur<='9')||(*cur>='a'&&*cur<='f')||(*cur>='A'&&*cur<='F')))return 0;
            errno=0;values[i]=strtoul(cur,&end,16);
            if(errno||end==cur||values[i]>(i?0xffffffUL:CONSOLENG_COLOR_MASK))return 0;
            if(i<18){if(*end!=' ')return 0;cur=end+1;}else {if(strcmp(end,"\n")&&*end)return 0;}
        }
        p.colors=(struct ConsolengColorPrefs){sizeof(p.colors),1,values[0],{0}};
        for(unsigned i=0;i<18;i++)p.colors.cp_RGB[i]=values[i+1];
        if(!settingsValid(&p))return 0;
        *out=p;return 1;
    }
    if(!strncmp(text,"ConsoleNG-Cursor 1\n",19)) {
        n=sscanf(text+19,"%lu %lu %lu %c",&style,&interval,&timeout,&extra);
        if(n!=3)return 0;
        lines=p.history.hp_Lines;
    } else if(!strncmp(text,"ConsoleNG 2\n",12)) {
        n=sscanf(text+12,"cursor=%lu %lu %lu\nhistory=%lu\nfont=%lu %127[^\n] %c",&style,&interval,&timeout,&lines,&pixels,name,&extra);
        if(n!=6||pixels>65535)return 0;
        p.font=(struct ConsolengFontPrefs){sizeof(p.font),1,pixels,{0}};
        if(pixels)strcpy(p.font.fp_Name,name);
        else if(strcmp(name,"-"))return 0;
    } else {
        if(strncmp(text,"ConsoleNG 1\n",12))return 0;
        n=sscanf(text+12,"cursor=%lu %lu %lu\nhistory=%lu %c",&style,&interval,&timeout,&lines,&extra);
        if(n!=4)return 0;
    }
    if(style>6||interval>5000||timeout>3600000||lines>CONSOLENG_HISTORY_MAX_LINES)return 0;
    p.cursor=(struct ConsolengCursorPrefs){sizeof(p.cursor),1,style,interval,timeout};
    p.history=(struct ConsolengHistoryPrefs){sizeof(p.history),1,lines};
    if(!settingsValid(&p))return 0;
    *out=p;return 1;
}
static int settingsParseBase(const char *text,struct ConsoleSettings *out)
{
    struct ConsoleSettings p=*out;
    const char header[]="ConsoleNG 19\nwheel=";
    if(!strncmp(text,header,sizeof(header)-1)) {
        const char *value=text+sizeof(header)-1;char *end;
        if(*value<'0'||*value>'9')return 0;
        errno=0;unsigned long lines=strtoul(value,&end,10);
        if(errno||lines>CONSOLENG_SCROLL_MAX_LINES||*end!='\n')return 0;
        p.wheelLines=lines;
        if(!settingsParseLegacy(end+1,&p))return 0;
        *out=p;return 1;
    }
    return settingsParseLegacy(text,out);
}
static int settingsParse(const char *text,struct ConsoleSettings *out)
{
    const char header[]="ConsoleNG 29\nsemantic=";
    if(strncmp(text,header,sizeof(header)-1))return settingsParseBase(text,out);
    const char *start=text+sizeof(header)-1,*end=strchr(start,'\n');
    if(!end||(size_t)(end-start)>=SEMANTIC_TEXT_CAP)return 0;
    char value[SEMANTIC_TEXT_CAP];memcpy(value,start,end-start);value[end-start]=0;
    struct ConsoleSettings p=*out;
    if(!semanticTextParse(value,&p.semantic)||!settingsParseBase(end+1,&p)||!settingsValid(&p))return 0;
    *out=p;return 1;
}
static int settingsFormatLegacy(char *text,size_t cap,const struct ConsoleSettings *p)
{
    if(!settingsValid(p))return 0;
    if(p->oscCopy&2) {
        const char header[]="ConsoleNG 17\nosc52read=1\n";
        if(cap<=sizeof(header)-1)return 0;
        struct ConsoleSettings copy=*p;copy.oscCopy&=1;
        memcpy(text,header,sizeof(header)-1);
        int length=settingsFormatLegacy(text+sizeof(header)-1,cap-(sizeof(header)-1),&copy);
        return length?(int)(sizeof(header)-1)+length:0;
    }
    int n=snprintf(text,cap,"ConsoleNG %u\ncursor=%lu %lu %lu\nhistory=%lu\nfont=%lu %s\n",
        p->bindings.bp_Version==19?28:p->bindings.bp_Version==18?27:p->bindings.bp_Version==17?26:p->bindings.bp_Version==16?25:p->bindings.bp_Version==15?24:p->bindings.bp_Version==14?23:p->bindings.bp_Version==13?22:p->bindings.bp_Version==12?21:p->bindings.bp_Version==11?20:p->bindings.bp_Version==10?18:p->bindings.bp_Version+7,(unsigned long)p->cursor.cp_Style,(unsigned long)p->cursor.cp_BlinkIntervalMS,
        (unsigned long)p->cursor.cp_IdleTimeoutMS,(unsigned long)p->history.hp_Lines,
        (unsigned long)p->font.fp_Pixels,p->font.fp_Name[0]?p->font.fp_Name:"-");
    if(n<=0||(size_t)n>=cap)return 0;
    int add=snprintf(text+n,cap-n,"colors=%lx",(unsigned long)p->colors.cp_Mask);
    if(add<0||(size_t)add>=cap-n)return 0;
        n+=add;
    for(unsigned i=0;i<18;i++) {
        add=snprintf(text+n,cap-n," %06lx",(unsigned long)p->colors.cp_RGB[i]);
        if(add<0||(size_t)add>=cap-n)return 0;
        n+=add;
    }
    if((size_t)n+1>=cap)return 0;
    add=snprintf(text+n,cap-n,"\nemoji=%lu\nbell=%lu\nosc52=%lu\ncursorcolor=%lu %06lx\n",(unsigned long)p->emoji,(unsigned long)p->bell,(unsigned long)p->oscCopy,(unsigned long)p->cursorCustom,(unsigned long)p->cursorRGB);
    if(add<0||(size_t)add>=cap-n)return 0;
    n+=add;
    add=snprintf(text+n,cap-n,"bindings=%lu\n",(unsigned long)p->bindings.bp_Count);
    if(add<0||(size_t)add>=cap-n)return 0;
    n+=add;
    for(unsigned i=0;i<p->bindings.bp_Count;i++) {
        const struct ConsolengKeyBinding *b=&p->bindings.bp_Entries[i];
        add=snprintf(text+n,cap-n,"%lu %lu %lu\n",(unsigned long)b->kb_Key,(unsigned long)b->kb_Modifiers,(unsigned long)b->kb_Action);
        if(add<0||(size_t)add>=cap-n)return 0;
        n+=add;
    }
    return n;
}
static int settingsFormat(char *text,size_t cap,const struct ConsoleSettings *p)
{
    if(!settingsValid(p))return 0;
    size_t offset=0;
    if(p->semantic.sp_Size) {
        char value[SEMANTIC_TEXT_CAP];if(!semanticTextFormat(value,sizeof(value),&p->semantic))return 0;
        int added=snprintf(text,cap,"ConsoleNG 29\nsemantic=%s\n",value);
        if(added<=0||(size_t)added>=cap)return 0;
        offset=added;text+=offset;cap-=offset;
    }
    int n=snprintf(text,cap,"ConsoleNG 19\nwheel=%lu\n",(unsigned long)p->wheelLines);
    if(n<=0||(size_t)n>=cap)return 0;
    int len=settingsFormatLegacy(text+n,cap-n,p);
    return len?(int)offset+n+len:0;
}
static int settingsLoad(const char *path,struct ConsoleSettings *p)
{
    char text[4096];FILE *f=fopen(path,"rb");if(!f)return 0;
    size_t n=fread(text,1,sizeof(text)-1,f);int ok=!ferror(f)&&feof(f);
    fclose(f);text[n]=0;
    return ok&&strlen(text)==n&&settingsParse(text,p);
}
static int settingsSave(const char *path,const struct ConsoleSettings *p)
{
    char text[4096],tmp[1100];int n=settingsFormat(text,sizeof(text),p);
    int len=snprintf(tmp,sizeof(tmp),"%s.%p.tmp",path,(void*)FindTask(NULL));
    if(!n||len<0||len>=(int)sizeof(tmp))return 0;
    FILE *f=fopen(tmp,"wb");if(!f)return 0;
    int ok=fwrite(text,1,n,f)==(size_t)n;
    if(fclose(f))ok=0;
    if(ok&&!rename(tmp,path))return 1;
    remove(tmp);return 0;
}
#endif
