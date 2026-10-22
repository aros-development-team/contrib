/* Copyright (C) 2026, The AROS Development Team. All rights reserved. */
#include <proto/keymap.h>
#include <string.h>
#include "console_gcc.h"

#include "binding_prefs.h"

BOOL con_bindings_valid(const struct ConsolengBindingPrefs *p) { return con_binding_prefs_valid(p); }
VOID con_bindings_defaults(struct ConsolengBindingPrefs *p) { con_binding_prefs_defaults(p); }
static BOOL con_binding_modifiers(ULONG binding,ULONG modifiers,ULONG context)
{
    return (binding & 31UL)==modifiers &&
        con_binding_context_matches(binding,context);
}
ULONG con_binding_action_in_context(const struct ConsolengBindingPrefs *p,
    const struct InputEvent *event, struct ConsoleBase *ConsoleDevice,ULONG context)
{
    if (event->ie_Class != IECLASS_RAWKEY || (event->ie_Code & IECODE_UP_PREFIX)) return 0;
    ULONG q=event->ie_Qualifier, mods=0;
    if (q & (IEQUALIFIER_LSHIFT|IEQUALIFIER_RSHIFT)) mods|=CONSOLENG_BIND_SHIFT;
    if (q & (IEQUALIFIER_LALT|IEQUALIFIER_RALT)) mods|=CONSOLENG_BIND_ALT;
    if (q & IEQUALIFIER_CONTROL) mods|=CONSOLENG_BIND_CTRL;
    if (q & IEQUALIFIER_LCOMMAND) mods|=CONSOLENG_BIND_LAMIGA;
    if (q & IEQUALIFIER_RCOMMAND) mods|=CONSOLENG_BIND_RAMIGA;
    /* Special keys work even when the keymap returns a multi-byte sequence
     * or no text. They deliberately override a remapped printable character. */
    if (p->bp_Version >= 2)
        for (ULONG i=0; i<p->bp_Count; i++)
            if (p->bp_Entries[i].kb_Key == (CONSOLENG_BIND_RAW | event->ie_Code) &&
                con_binding_modifiers(p->bp_Entries[i].kb_Modifiers,mods,context))
                return p->bp_Entries[i].kb_Action;
    struct InputEvent plain=*event;
    /* Retain numeric-pad identity; strip modifiers, locks and repeat. */
    plain.ie_Qualifier &= IEQUALIFIER_NUMERICPAD;
    UBYTE mapped[8];
    if (MapRawKey(&plain,mapped,sizeof(mapped),NULL) != 1) return 0;
    ULONG key=mapped[0];
    if (key >= 'A' && key <= 'Z') key += 'a'-'A';
    for (ULONG i=0; i<p->bp_Count; i++)
        if (p->bp_Entries[i].kb_Key == key && con_binding_modifiers(p->bp_Entries[i].kb_Modifiers,mods,context))
            return p->bp_Entries[i].kb_Action;
    return 0;
}
ULONG con_binding_action_in_mode(const struct ConsolengBindingPrefs *p,
    const struct InputEvent *event, struct ConsoleBase *ConsoleDevice,BOOL navigating)
{
    return con_binding_action_in_context(p,event,ConsoleDevice,navigating?CONSOLENG_BIND_NAVONLY:0);
}
ULONG con_binding_action(const struct ConsolengBindingPrefs *p,
    const struct InputEvent *event, struct ConsoleBase *ConsoleDevice)
{
    return con_binding_action_in_mode(p,event,ConsoleDevice,FALSE);
}
/* Releases belong to the original shortcut even after policy/modifier changes.
 * A fresh press clears stale state if focus loss discarded its release. */
BOOL con_binding_release(UBYTE *held, const struct InputEvent *event)
{
    ULONG code=event->ie_Code & 127, mask=1U << (code & 7);
    BOOL consumed=!!(held[code >> 3] & mask);
    if (event->ie_Code & IECODE_UP_PREFIX) {
        held[code >> 3] &= ~mask;
        return consumed;
    }
    if (event->ie_Qualifier & IEQUALIFIER_REPEAT) return consumed;
    held[code >> 3] &= ~mask;
    return FALSE;
}
VOID con_binding_hold(UBYTE *held, const struct InputEvent *event)
{
    ULONG code=event->ie_Code & 127;
    held[code >> 3] |= 1U << (code & 7);
}

/* Only an unchanged navigation binding can repeat. Clipboard/search actions
 * remain single-shot, and a policy change cannot turn a held key into paste. */
BOOL con_binding_repeat(ULONG original, ULONG current, const struct InputEvent *event)
{
    return event->ie_Class == IECLASS_RAWKEY &&
        !(event->ie_Code & IECODE_UP_PREFIX) &&
        (event->ie_Qualifier & IEQUALIFIER_REPEAT) && original == current &&
        ((current >= CONSOLENG_BIND_LINEUP && current <= CONSOLENG_BIND_BOTTOM) ||
         current == CONSOLENG_BIND_FONTLARGER || current == CONSOLENG_BIND_FONTSMALLER ||
         current == CONSOLENG_BIND_HALFUP || current == CONSOLENG_BIND_HALFDOWN);
}
