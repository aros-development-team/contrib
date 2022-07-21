// Scalos_rev.h
// $Date$
// $Revision$

#ifndef	SCALOSREV_H
#define SCALOSREV_H


#include <Year.h>

#define VERSION			41
#define REVISION		10
#define	VERS_REV_STR            STR(VERSION) "." STR(REVISION)
#define	RELEASE			"1.9"
#define	BUILDNR			STR(SVN_VERSION)
#define DATE			"21.07.2022"
#define VERSTAG			"\00$VER: Scalos " VERS_REV_STR " release " RELEASE " (" DATE ") " "build " BUILDNR " " COMPILER_STRING
#define	COPYRIGHT		"\n © 1999" CURRENTYEAR " The Scalos Team\n"

#endif /* SCALOSREV_H */

