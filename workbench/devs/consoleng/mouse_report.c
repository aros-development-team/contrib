/* Copyright (C) 2026, The AROS Development Team. All rights reserved. */
#include "console_gcc.h"
#include <string.h>

static ULONG mouse_decimal(UBYTE *out, ULONG value)
{
    UBYTE reverse[10]; ULONG n=0, i;
    do { reverse[n++]='0'+value%10; value/=10; } while (value);
    for (i=0;i<n;i++) out[i]=reverse[n-i-1];
    return n;
}

/* Zero-based cells. Legacy stops at222; DEC1005 encodes through2014.
 * The button byte stays unchanged; only coordinates use UTF-8. */
ULONG con_mouse_encode(BOOL sgr, BOOL utf8, ULONG button, BOOL release, LONG x, LONG y,
    UBYTE *out, ULONG capacity)
{
    UBYTE report[40]; ULONG n;
    if (x<0 || y<0 || x>0x7ffffffeL || y>0x7ffffffeL || button>127) return 0;
    report[0]=27; report[1]='[';
    if (sgr)
    {
        report[2]='<'; n=3;
        n+=mouse_decimal(report+n,button); report[n++]=';';
        n+=mouse_decimal(report+n,(ULONG)x+1); report[n++]=';';
        n+=mouse_decimal(report+n,(ULONG)y+1); report[n++]=release?'m':'M';
    }
    else
    {
        LONG limit = utf8 ? 2015 : 223;
        if (x>=limit || y>=limit) return 0;
        report[2]='M'; report[3]=32+(release?((button&28)|3):button); n=4;
        LONG coordinates[2] = { x, y };
        for (ULONG axis=0; axis<2; axis++)
        {
            ULONG value = 33 + coordinates[axis];
            if (utf8 && value >= 128)
            {
                report[n++] = 0xc0 | (value >> 6);
                report[n++] = 0x80 | (value & 63);
            }
            else report[n++] = value;
        }
    }
    if (!out || n>capacity) return 0;
    memcpy(out,report,n); return n;
}
