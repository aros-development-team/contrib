/* Copyright (C) 2026, The AROS Development Team. All rights reserved. */
#include <proto/exec.h>
#include <proto/console.h>
#include <devices/rawkeycodes.h>
#include <string.h>
#include "console_gcc.h"

/* All five negotiated keyboard enhancements have native encoders. */
#define CON_KEYBOARD_SUPPORTED 31
#define CON_KEYBOARD_DEPTH 4096
struct ConsolengKeyboard {
    UBYTE stack[2][CON_KEYBOARD_DEPTH];
    UWORD depth[2];
    UBYTE flags;
};

static ULONG con_keyboard_decimal(UBYTE *out, ULONG value);

VOID con_keyboard_dispose(struct ConUnit *unit)
{
    if (ICU(unit)->keyboard) FreeMem(ICU(unit)->keyboard, sizeof(struct ConsolengKeyboard));
    ICU(unit)->keyboard = NULL;
}
VOID con_keyboard_switch(struct ConUnit *unit, BOOL alternate)
{
    struct ConsolengKeyboard *k = ICU(unit)->keyboard;
    ICU(unit)->keyboardAlternate = !!alternate;
    if (k) k->flags = k->depth[!!alternate] ? k->stack[!!alternate][k->depth[!!alternate]-1] : 0;
}
/* Return zero for non-keyboard syntax; consume malformed recognized requests
 * without changing state. The common parser retains incomplete CSI writes. */
ULONG con_keyboard_csi(struct ConUnit *unit, const UBYTE *s, ULONG n,
    struct ConsoleBase *base)
{
    if (!n || !strchr("?<=>", s[0]) || !s[0]) return 0;
    ULONG end = 1;
    while (end < n && (s[end] < 0x40 || s[end] > 0x7e)) end++;
    if (end == n || s[end] != 'u') return 0;
    ULONG value[2] = {0,1}, field = 0;
    BOOL valid = TRUE;
    for (ULONG i=1; i<end; i++) {
        if (s[i] == ';' && !field) { field++; value[field]=0; continue; }
        if (s[i] < '0' || s[i] > '9' || value[field] > 6553) { valid=FALSE; break; }
        value[field] = value[field]*10 + s[i]-'0';
        if (value[field] > 65535) { valid=FALSE; break; }
    }
    if (!valid || (field && s[0] != '=') || (s[0]=='?' && end!=1)) return end+1;
    struct ConsolengKeyboard *k = ICU(unit)->keyboard;
    if (s[0] == '?') {
        UBYTE reply[16] = "\x1b[?";
        ULONG length = 3 + con_keyboard_decimal(reply+3, k ? k->flags : 0);
        reply[length++] = 'u';
        con_inject(base, unit, reply, length);
        return end+1;
    }
    if (!k) {
        k = AllocMem(sizeof(*k), MEMF_PUBLIC|MEMF_CLEAR);
        if (!k) return end+1;
        ICU(unit)->keyboard = k;
    }
    UBYTE screen = !!ICU(unit)->keyboardAlternate;
    UBYTE flags = value[0] & CON_KEYBOARD_SUPPORTED;
    if (s[0] == '>') {
        if (k->depth[screen] == CON_KEYBOARD_DEPTH) {
            memmove(k->stack[screen], k->stack[screen]+1, CON_KEYBOARD_DEPTH-1);
            k->depth[screen]--;
        }
        k->stack[screen][k->depth[screen]++] = flags;
        k->flags = flags;
    } else if (s[0] == '<') {
        ULONG pop = value[0] ? value[0] : 1;
        k->depth[screen] = pop > k->depth[screen] ? 0 : k->depth[screen]-pop;
        k->flags = k->depth[screen] ? k->stack[screen][k->depth[screen]-1] : 0;
    } else {
        if (value[1] == 2) k->flags |= flags;
        else if (value[1] == 3) k->flags &= ~flags;
        else k->flags = flags;
    }
    return end+1;
}
static ULONG con_keyboard_decimal(UBYTE *out, ULONG value)
{
    UBYTE reverse[10]; ULONG n=0, length;
    do { reverse[n++]='0'+value%10; value/=10; } while(value);
    length=n;
    while(n) *out++=reverse[--n];
    return length;
}
/* Match the native input path's 80-byte keymap limit. Decimal encoding
 * needs at most four bytes per Latin-1 byte, plus the sequence header. */
static LONG con_keyboard_text(const struct InputEvent *event, UBYTE *text,
    struct ConsoleBase *ConsoleDevice)
{
    struct InputEvent printable=*event;
    printable.ie_Code &= ~IECODE_UP_PREFIX;
    printable.ie_Qualifier &= ~IEQUALIFIER_REPEAT;
    LONG length=RawKeyConvert(&printable,text,80,NULL);
    if(length<1 || length>80 || (length==1 &&
        (text[0]<32 || (text[0]>=127 && text[0]<160))))return 0;
    return length;
}
/* Returns -1 to use the existing Amiga/keymap path, 0 for a suppressed key,
 * or a complete ASCII escape sequence. Caller supplies at least 352 bytes. */
LONG con_keyboard_event(struct ConUnit *unit, const struct InputEvent *event,
    UBYTE *out, struct ConsoleBase *ConsoleDevice)
{
    struct ConsolengKeyboard *k=ICU(unit)->keyboard;
    ULONG q=event->ie_Qualifier, code=event->ie_Code, value=0, alternate=0, modifier=1;
    UBYTE final='u', text[80];
    LONG textLength=0;
    BOOL textual=FALSE;
    if (!k || !(k->flags&11) || event->ie_Class!=IECLASS_RAWKEY ||
        (q&(IEQUALIFIER_LCOMMAND|IEQUALIFIER_RCOMMAND))) return -1;
    BOOL release = !!(code&IECODE_UP_PREFIX);
    BOOL all = !!(k->flags&8), disambiguate = !!(k->flags&1);
    BOOL eventType = (k->flags&2) && (release || (q&IEQUALIFIER_REPEAT));
    if (release && !(k->flags&2)) return 0;
    code &= ~IECODE_UP_PREFIX;
    if (q&(IEQUALIFIER_LSHIFT|IEQUALIFIER_RSHIFT)) modifier+=1;
    if (q&(IEQUALIFIER_LALT|IEQUALIFIER_RALT)) modifier+=2;
    if (q&IEQUALIFIER_CONTROL) modifier+=4;
    switch(code) {
    case RAWKEY_ESCAPE: if(!release&&!all&&!disambiguate)return -1;value=27;break;
    case RAWKEY_TAB:
        if(release&&!all)return 0;
        if(!all&&(!disambiguate||modifier==1))return -1;
        value=9;break;
    case RAWKEY_RETURN:
        if(release&&!all)return 0;
        if(!all&&(!disambiguate||modifier==1))return -1;
        value=13;break;
    case RAWKEY_BACKSPACE:
        if(release&&!all)return 0;
        if(!all&&(!disambiguate||modifier==1))return -1;
        value=127;break;
    case RAWKEY_KP_0: value=57399;break;
    case RAWKEY_KP_1: value=57400;break;
    case RAWKEY_KP_2: value=57401;break;
    case RAWKEY_KP_3: value=57402;break;
    case RAWKEY_KP_4: value=57403;break;
    case RAWKEY_KP_5: value=57404;break;
    case RAWKEY_KP_6: value=57405;break;
    case RAWKEY_KP_7: value=57406;break;
    case RAWKEY_KP_8: value=57407;break;
    case RAWKEY_KP_9: value=57408;break;
    case RAWKEY_KP_DECIMAL: value=57409;break;
    case RAWKEY_KP_DIVIDE: value=57410;break;
    case RAWKEY_KP_MULTIPLY: value=57411;break;
    case RAWKEY_KP_MINUS: value=57412;break;
    case RAWKEY_KP_PLUS: value=57413;break;
    case RAWKEY_KP_ENTER: if(release&&!all)return 0;value=57414;break;
    case RAWKEY_UP: final='A';break;
    case RAWKEY_DOWN: final='B';break;
    case RAWKEY_RIGHT: final='C';break;
    case RAWKEY_LEFT: final='D';break;
    case RAWKEY_HOME: final='H';break;
    case RAWKEY_END: final='F';break;
    case RAWKEY_INSERT: value=2;final='~';break;
    case RAWKEY_DELETE: value=3;final='~';break;
    case RAWKEY_PAGEUP: value=5;final='~';break;
    case RAWKEY_PAGEDOWN: value=6;final='~';break;
    case RAWKEY_F1: final='P';break;
    case RAWKEY_F2: final='Q';break;
    case RAWKEY_F3: value=13;final='~';break;
    case RAWKEY_F4: final='S';break;
    case RAWKEY_F5: value=15;final='~';break;
    case RAWKEY_F6: value=17;final='~';break;
    case RAWKEY_F7: value=18;final='~';break;
    case RAWKEY_F8: value=19;final='~';break;
    case RAWKEY_F9: value=20;final='~';break;
    case RAWKEY_F10: value=21;final='~';break;
    case RAWKEY_F11: value=23;final='~';break;
    case RAWKEY_F12: value=24;final='~';break;
    case RAWKEY_SCRLOCK: value=57359;break;
    case RAWKEY_PRTSCREEN: value=57361;break;
    case RAWKEY_PAUSE: value=57362;break;
    case RAWKEY_LSHIFT: if(!all)return -1;value=57441;modifier=release?((modifier-1)&~1)+1:((modifier-1)|1)+1;break;
    case RAWKEY_RSHIFT: if(!all)return -1;value=57447;modifier=release?((modifier-1)&~1)+1:((modifier-1)|1)+1;break;
    case RAWKEY_CONTROL: if(!all)return -1;value=57442;modifier=release?((modifier-1)&~4)+1:((modifier-1)|4)+1;break;
    case RAWKEY_LALT: if(!all)return -1;value=57443;modifier=release?((modifier-1)&~2)+1:((modifier-1)|2)+1;break;
    case RAWKEY_RALT: if(!all)return -1;value=57449;modifier=release?((modifier-1)&~2)+1:((modifier-1)|2)+1;break;
    case RAWKEY_CAPSLOCK: if(!all)return -1;value=57358;break;
    case RAWKEY_NUMLOCK: if(!all)return -1;value=57360;break;
    default: {
        if (!release && !all && (!disambiguate || modifier<=2)) return -1;
        textual=TRUE;
        if((k->flags&16) && !release)
            textLength=con_keyboard_text(event,text,ConsoleDevice);
        struct InputEvent plain=*event; UBYTE mapped[8];
        plain.ie_Code = code;
        plain.ie_Qualifier &= ~(IEQUALIFIER_REPEAT|IEQUALIFIER_LSHIFT|IEQUALIFIER_RSHIFT|
            IEQUALIFIER_LALT|IEQUALIFIER_RALT|IEQUALIFIER_CONTROL);
        LONG n=RawKeyConvert(&plain,mapped,sizeof(mapped),NULL);
        if(n!=1 || mapped[0]<32 || (mapped[0]>=127 && mapped[0]<160))
        {
            if(!all || !textLength)return -1;
            value=0; /* A string mapping has text but no single base key. */
            break;
        }
        value=mapped[0];
        if ((k->flags&4) && (q&(IEQUALIFIER_LSHIFT|IEQUALIFIER_RSHIFT)))
        {
            plain.ie_Qualifier |= q&(IEQUALIFIER_LSHIFT|IEQUALIFIER_RSHIFT);
            n=RawKeyConvert(&plain,mapped,sizeof(mapped),NULL);
            if (n==1 && mapped[0]>=32 &&
                !(mapped[0]>=127 && mapped[0]<160) && mapped[0]!=value)
                alternate=mapped[0];
        }
        break;
    }
    }
    if(value>=57399 && value<=57414 && !release && !all && !disambiguate)return -1;
    if(value>=57399 && value<=57413 && (k->flags&16) && !release)
        textLength=con_keyboard_text(event,text,ConsoleDevice);
    ULONG n=0;out[n++]=27;out[n++]='[';
    if(value || textual || modifier!=1 || eventType || textLength)
        n+=con_keyboard_decimal(out+n,(value || textual)?value:1);
    if(alternate){out[n++]=':';n+=con_keyboard_decimal(out+n,alternate);}
    if(modifier!=1 || eventType || textLength){out[n++]=';';n+=con_keyboard_decimal(out+n,modifier);}
    if(eventType){out[n++]=':';out[n++]=release?'3':'2';}
    for(LONG i=0;i<textLength;i++){out[n++]=i?':':';';n+=con_keyboard_decimal(out+n,text[i]);}
    out[n++]=final;
    return n;
}
