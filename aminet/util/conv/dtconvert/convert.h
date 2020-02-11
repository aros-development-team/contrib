
#ifndef DTCONVERT_CONVERT_H
#define DTCONVERT_CONVERT_H 1

/*
**
**  $VER: convert.h 1.8 (20.3.98)
**  datatypes.library/Examples/DTConvert
**
**  Converts file into another format using datatypes
**
**  header for converter routines
**
**  Written 1996-1998 by Roland 'Gizzy' Mainz
**
*/

#ifndef EXEC_TYPES_H
#include <exec/types.h>
#endif /* !EXEC_TYPES_H */

/*****************************************************************************/
/* sound.datatype defines */

typedef UBYTE SAMPLE8;  /*  8 bit sample data */
typedef UWORD SAMPLE16; /* 16 bit sample data */

/* Used in struct VoiceHeader... */
#define VOLUME_UNITY (0x10000UL) /* Unity = Fixed 1.0 = maximum volume */

#ifdef SOUNDDATATYPE_V41_SUPPORT

/* Source object for writesoundclass (see ConvertSound below) */
#define WRITESOUNDA_SourceObject (DTA_UserData)

#endif /* SOUNDDATATYPE_V41_SUPPORT */

/*****************************************************************************/
/* picture.datatype (/animation.datatype) defines */

/* from <iffp/ilbm.h>*/
/* BMHD flags */

/* Advisory that 8 significant bits-per-gun have been stored in CMAP
 * i.e. that the CMAP is definitely not 4-bit values shifted left.
 * This bit will disable nibble examination by color loading routine.
 */
#define BMHDB_CMAPOK (7U)
#define BMHDF_CMAPOK (1U << BMHDB_CMAPOK)

/*****************************************************************************/
/* animation.datatype defines */

/* Source object for writeanimclass (see convert.c/ConvertAnimation ) */
#define WRITEANIMA_SourceObject (DTA_UserData)

/*****************************************************************************/

#endif /* !DTCONVERT_CONVERT_H */


