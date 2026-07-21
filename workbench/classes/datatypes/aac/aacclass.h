/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Private definitions for aac.datatype.
*/

#ifndef AACCLASS_H
#define AACCLASS_H

#include <exec/types.h>

/* Instance data attached to every aac.datatype object.  Holds the
 * memory pool used to store strings extracted from ID3 tags so we can
 * free them in one go on OM_DISPOSE.
 */
struct AAC_Data
{
    APTR aacd_StringPool;
};

#endif /* AACCLASS_H */
