/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    Private definitions for mp3.datatype.
*/

#ifndef MP3CLASS_H
#define MP3CLASS_H

#include <exec/types.h>

/* Instance data attached to every mp3.datatype object.  Holds the
 * memory pool used to store strings extracted from ID3 tags so we can
 * free them in one go on OM_DISPOSE.
 */
struct MP3_Data
{
    APTR mp3d_StringPool;
};

#endif /* MP3CLASS_H */
