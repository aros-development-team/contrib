/* Bounded, reversible text form for Unicode semantic delimiters. */
#ifndef CONSOLENG_SEMANTIC_TEXT_H
#define CONSOLENG_SEMANTIC_TEXT_H
#include "../semantic_prefs.h"
#define SEMANTIC_TEXT_CAP (CONSOLENG_SEMANTIC_MAX * 10 + 1)
static int semanticTextParse(const char *text,struct ConsolengSemanticPrefs *out)
{
    struct ConsolengSemanticPrefs p={sizeof(p),1,0,{0}};
    if(!text)return 0;
    const unsigned char *s=(const unsigned char *)text;
    while(*s) {
        ULONG cp=*s++;
        if(cp=='\\') {
            cp=*s++;if(!cp)return 0;
            if(cp=='t')cp='\t';else if(cp=='n')cp='\n';else if(cp=='r')cp='\r';
            else if(cp=='u'||cp=='U') {
                unsigned digits=cp=='u'?4:8;cp=0;
                while(digits--) {
                    unsigned c=*s++;if(!c)return 0;
                    if(c>='0'&&c<='9')c-='0';else if(c>='a'&&c<='f')c=c-'a'+10;
                    else if(c>='A'&&c<='F')c=c-'A'+10;else return 0;
                    cp=(cp<<4)|c;
                }
            } else if(cp!='\\')return 0;
        } else if(cp>=128) {
            unsigned n;ULONG minimum;
            if(cp>=0xc2&&cp<=0xdf){n=1;cp&=31;minimum=0x80;}
            else if(cp>=0xe0&&cp<=0xef){n=2;cp&=15;minimum=0x800;}
            else if(cp>=0xf0&&cp<=0xf4){n=3;cp&=7;minimum=0x10000;}
            else return 0;
            while(n--){unsigned c=*s++;if(c<0x80||c>0xbf)return 0;cp=(cp<<6)|(c&63);}
            if(cp<minimum)return 0;
        }
        if(p.sp_Count==CONSOLENG_SEMANTIC_MAX)return 0;
        p.sp_Chars[p.sp_Count++]=cp;
    }
    if(!con_semantic_prefs_valid(&p))return 0;
    *out=p;return 1;
}
static int semanticTextFormat(char *text,size_t cap,const struct ConsolengSemanticPrefs *p)
{
    if(!cap||!con_semantic_prefs_valid(p))return 0;
    size_t used=0;text[0]=0;
    for(ULONG i=0;i<p->sp_Count;i++) {
        ULONG cp=p->sp_Chars[i];char part[11];
        if(cp=='\\')strcpy(part,"\\\\");else if(cp=='\t')strcpy(part,"\\t");
        else if(cp=='\n')strcpy(part,"\\n");else if(cp=='\r')strcpy(part,"\\r");
        else if(cp>=32&&cp<127){part[0]=(char)cp;part[1]=0;}
        else snprintf(part,sizeof(part),cp<=0xffff?"\\u%04lx":"\\U%08lx",(unsigned long)cp);
        size_t n=strlen(part);if(n>=cap-used)return 0;
        memcpy(text+used,part,n+1);used+=n;
    }
    return 1;
}
#endif
