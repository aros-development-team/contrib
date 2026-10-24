/*
    Copyright © 2001, The AROS Development Team. All rights reserved.
    $Id$
*/

#include <exec/types.h>
#include <rexx/storage.h>
#include <proto/rexxsyslib.h>

#include "rexx.h"

/*****************************************************************************

    NAME
        BOOL IsReginaMsg()

    SYNOPSIS
        BOOL IsReginaMsg(struct RexxMsg *msg);

    FUNCTION
        Checks whether a Rexx message is associated with one of the
        thread-context values currently recorded by regina.library. The
        routine opens rexxsyslib.library, calls IsRexxMsg(), closes the
        library, and compares rm_Private2 with the stored TSD values.

    INPUTS
        msg - Non-NULL pointer to a readable RexxMsg. The message must
              remain valid throughout this call.

    RESULT
        TRUE when IsRexxMsg() accepts the message and rm_Private2 matches
        a value in the Regina context list; FALSE otherwise.

        Also returns FALSE if rexxsyslib.library cannot be opened.

    NOTES
        IsRexxMsg() checks the RexxSys message-identity pointer, not the
        contents of an arbitrary RexxMsg. This routine does not read
        rm_Private1 or validate the message contents beyond these tests.

        This routine traverses the context list without taking a lock.
        Its result is not guaranteed during concurrent changes to that
        list. A match does not confer ownership of the message or context.

    BUGS
        An invalid or NULL msg is unsafe: IsRexxMsg() dereferences the
        supplied pointer without checking it first.

        Cleanup clears a context node's TSD pointer without removing its
        node. Therefore a valid RexxSys message with rm_Private2 == NULL
        can be accepted as a Regina message if any cleared node remains
        in the list.

    SEE ALSO
        rexxsyslib.library/IsRexxMsg(), RexxStart()

*****************************************************************************/
BOOL IsReginaMsg (struct RexxMsg *msg)
{
    tsd_node_t *node;
    BOOL ok;
    struct Library *RexxSysBase;
    
    RexxSysBase = OpenLibrary("rexxsyslib.library", 0);
    if (RexxSysBase == NULL) return FALSE;
    ok = IsRexxMsg(msg);
    CloseLibrary(RexxSysBase);
    if (!ok) return FALSE;

    ok = FALSE;
    ForeachNode(__regina_tsdlist, node)
    {
	if (node->TSD == (tsd_t *)msg->rm_Private2)
	{
	    ok = TRUE;
	    break;
	}
    }

    return ok;
}
