/* Copyright (C) 2026, The AROS Development Team. All rights reserved. */
#ifndef CON_BINDING_PREFS_H
#define CON_BINDING_PREFS_H
#include <string.h>
/* Named special keys only: modifiers, mouse wheels and undefined raw codes
 * cannot accidentally become application shortcuts. */
static inline BOOL con_binding_special(ULONG key)
{
    if (!(key & CONSOLENG_BIND_RAW)) return FALSE;
    switch (key) {
    case CONSOLENG_BIND_RAW+0x50:
    case CONSOLENG_BIND_RAW+0x51:
    case CONSOLENG_BIND_RAW+0x52:
    case CONSOLENG_BIND_RAW+0x53:
    case CONSOLENG_BIND_RAW+0x54:
    case CONSOLENG_BIND_RAW+0x55:
    case CONSOLENG_BIND_RAW+0x56:
    case CONSOLENG_BIND_RAW+0x57:
    case CONSOLENG_BIND_RAW+0x58:
    case CONSOLENG_BIND_RAW+0x59:
    case CONSOLENG_BIND_RAW+0x4b:
    case CONSOLENG_BIND_RAW+0x6f:
    case CONSOLENG_BIND_RAW+0x4c:
    case CONSOLENG_BIND_RAW+0x4d:
    case CONSOLENG_BIND_RAW+0x4f:
    case CONSOLENG_BIND_RAW+0x4e:
    case CONSOLENG_BIND_RAW+0x70:
    case CONSOLENG_BIND_RAW+0x71:
    case CONSOLENG_BIND_RAW+0x48:
    case CONSOLENG_BIND_RAW+0x49:
    case CONSOLENG_BIND_RAW+0x47:
    case CONSOLENG_BIND_RAW+0x46:
    case CONSOLENG_BIND_RAW+0x45:
    case CONSOLENG_BIND_RAW+0x42:
    case CONSOLENG_BIND_RAW+0x41:
    case CONSOLENG_BIND_RAW+0x44:
    case CONSOLENG_BIND_RAW+0x43:
    case CONSOLENG_BIND_RAW+0x5f:
        return TRUE;
    default: return FALSE;
    }
}
/* Physical character/keypad positions are independent of the active keymap.
 * Modifier/lock events and NewMouse codes are not shortcut keys. */
static inline BOOL con_binding_physical(ULONG key)
{
    if (con_binding_special(key)) return TRUE;
    if (!(key & CONSOLENG_BIND_RAW) || key > (CONSOLENG_BIND_RAW | 0x7f)) return FALSE;
    ULONG code = key & 0x7f;
    return code <= 0x40 || code == 0x4a || (code >= 0x5a && code <= 0x5e);
}
static inline ULONG con_binding_hex_key(const char *text)
{
    if (!text || !text[0] || !text[1] || text[2]) return (ULONG)~(ULONG)0;
    ULONG code=0;
    for (unsigned i=0;i<2;i++) {
        unsigned c=(unsigned char)text[i];
        if (c>='0' && c<='9') c-='0';
        else if (c>='a' && c<='f') c=c-'a'+10;
        else if (c>='A' && c<='F') c=c-'A'+10;
        else return (ULONG)~(ULONG)0;
        code=code*16+c;
    }
    ULONG key=CONSOLENG_BIND_RAW|code;
    return con_binding_physical(key)?key:(ULONG)~(ULONG)0;
}
static inline BOOL con_binding_context_matches(ULONG binding,ULONG context)
{
    for(ULONG yes=CONSOLENG_BIND_NAVONLY;yes<=CONSOLENG_BIND_SEARCHMODE;yes<<=2) {
        if((binding & yes) && !(context & yes))return FALSE;
        if((binding & (yes<<1)) && (context & yes))return FALSE;
    }
    return TRUE;
}
static inline BOOL con_binding_context_overlap(ULONG a,ULONG b)
{
    for(ULONG yes=CONSOLENG_BIND_NAVONLY;yes<=CONSOLENG_BIND_SEARCHMODE;yes<<=2)
        if(((a & yes) && (b & (yes<<1))) || ((b & yes) && (a & (yes<<1))))return FALSE;
    return TRUE;
}
static inline BOOL con_binding_context_valid(ULONG binding)
{
    for(ULONG yes=CONSOLENG_BIND_NAVONLY;yes<=CONSOLENG_BIND_SEARCHMODE;yes<<=2)
        if((binding & (yes|(yes<<1)))==(yes|(yes<<1)))return FALSE;
    return TRUE;
}
static inline BOOL con_binding_prefs_valid(const struct ConsolengBindingPrefs *p)
{
    if (p->bp_Size != sizeof(*p) || (p->bp_Version < 1 || p->bp_Version > 19) ||
        p->bp_Count > CONSOLENG_BINDING_MAX) return FALSE;
    for (ULONG i=0; i<CONSOLENG_BINDING_MAX; i++) {
        const struct ConsolengKeyBinding *b = &p->bp_Entries[i];
        if (i >= p->bp_Count) {
            if (b->kb_Key || b->kb_Modifiers || b->kb_Action) return FALSE;
            continue;
        }
        BOOL logical = b->kb_Key >= 32 && b->kb_Key <= 255 &&
            !(b->kb_Key >= 127 && b->kb_Key < 160) &&
            !(b->kb_Key >= 'A' && b->kb_Key <= 'Z');
        if ((!logical && !(p->bp_Version >= 2 && con_binding_special(b->kb_Key)) &&
             !(p->bp_Version >= 5 && con_binding_physical(b->kb_Key))) ||
            (b->kb_Modifiers & ~(p->bp_Version>=19?32767UL:p->bp_Version>=18?8191UL:p->bp_Version>=12?127UL:31UL)) ||
            !con_binding_context_valid(b->kb_Modifiers) ||
            b->kb_Action < CONSOLENG_BIND_COPY ||
            b->kb_Action > (p->bp_Version >= 17 ? CONSOLENG_BIND_NAVSEARCHPREVIOUS : p->bp_Version >= 16 ? CONSOLENG_BIND_NAVFINDREVERSE : p->bp_Version >= 15 ? CONSOLENG_BIND_NAVLINEDOWN : p->bp_Version >= 14 ? CONSOLENG_BIND_NAVOCCUPIED : p->bp_Version >= 13 ? CONSOLENG_BIND_NAVSELECTSEMANTIC : p->bp_Version >= 11 ? CONSOLENG_BIND_NAVOPENLINK : p->bp_Version >= 10 ? CONSOLENG_BIND_CLEARSELECTION : p->bp_Version >= 9 ? CONSOLENG_BIND_HALFDOWN : p->bp_Version >= 8 ? CONSOLENG_BIND_FONTRESET : p->bp_Version >= 7 ? CONSOLENG_BIND_CLEARHISTORY : p->bp_Version >= 6 ? CONSOLENG_BIND_NAVIGATE : p->bp_Version >= 4 ? CONSOLENG_BIND_SEARCH_BACKWARD :
                p->bp_Version >= 3 ? CONSOLENG_BIND_BOTTOM : CONSOLENG_BIND_SEARCH)) return FALSE;
        for (ULONG j=0; j<i; j++)
            if (b->kb_Key == p->bp_Entries[j].kb_Key &&
                (b->kb_Modifiers & 31UL) == (p->bp_Entries[j].kb_Modifiers & 31UL) &&
                con_binding_context_overlap(b->kb_Modifiers,p->bp_Entries[j].kb_Modifiers)) return FALSE;
    }
    return TRUE;
}
static inline VOID con_binding_prefs_defaults(struct ConsolengBindingPrefs *p)
{
    memset(p, 0, sizeof(*p));
    p->bp_Size=sizeof(*p); p->bp_Version=1; p->bp_Count=4;
    p->bp_Entries[0]=(struct ConsolengKeyBinding){'c',16,CONSOLENG_BIND_COPY};
    p->bp_Entries[1]=(struct ConsolengKeyBinding){'v',16,CONSOLENG_BIND_PASTE};
    p->bp_Entries[2]=(struct ConsolengKeyBinding){'a',16,CONSOLENG_BIND_SELECTALL};
    p->bp_Entries[3]=(struct ConsolengKeyBinding){'f',5,CONSOLENG_BIND_SEARCH};
}
#endif
