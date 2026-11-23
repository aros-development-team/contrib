/* Copyright (C) 2026, The AROS Development Team. All rights reserved. */
#ifndef CONSOLENG_SEMANTIC_PREFS_H
#define CONSOLENG_SEMANTIC_PREFS_H
static inline BOOL con_semantic_prefs_valid(const struct ConsolengSemanticPrefs *p)
{
    if(!p || p->sp_Size!=sizeof(*p) || p->sp_Version!=1 ||
       p->sp_Count>CONSOLENG_SEMANTIC_MAX)return FALSE;
    for(ULONG i=0;i<CONSOLENG_SEMANTIC_MAX;i++) {
        ULONG cp=p->sp_Chars[i];
        if(i>=p->sp_Count){if(cp)return FALSE;continue;}
        if(!cp || cp>0x10ffff || (cp>=0xd800 && cp<=0xdfff))return FALSE;
        for(ULONG j=0;j<i;j++)if(p->sp_Chars[j]==cp)return FALSE;
    }
    return TRUE;
}
static inline struct ConsolengSemanticPrefs con_semantic_defaults(void)
{
    struct ConsolengSemanticPrefs p={sizeof(p),1,17,
        {',',0x2502,'`','|',':','"',39,' ','(',')','[',']','{','}','<','>','\t'}};
    return p;
}
#endif
