/* Copyright (C) 2026, The AROS Development Team. All rights reserved. */
#ifndef CONSOLENG_CLIPBOARD_PREFS_H
#define CONSOLENG_CLIPBOARD_PREFS_H
/* Version 1 remains a copy boolean. Version 2 uses copy/read bits without
 * changing the public record size or any command number. */
static inline BOOL con_clipboard_prefs_valid(const struct ConsolengClipboardPrefs *p)
{
    return p->cbp_Size == sizeof(*p) &&
        ((p->cbp_Version == 1 && p->cbp_Enabled <= 1) ||
         (p->cbp_Version == 2 && p->cbp_Enabled <= 3));
}
/* The existing private word keeps copy-disabled in bit 0; bit 1 permits read.
 * No unit layout change is needed. Zero retains the copy-only default. */
static inline ULONG con_clipboard_policy(const struct ConsolengClipboardPrefs *p)
{
    return (!(p->cbp_Enabled & 1)) | (p->cbp_Version == 2 ? p->cbp_Enabled & 2 : 0);
}
#endif
