#include <aros/symbolsets.h>

#include <proto/exec.h>
#include <string.h>
#include "class.h"

/****************************************************************************/

struct Library         *lib_base;

struct Catalog         *lib_cat = NULL;

struct MUI_CustomClass *lib_shape = NULL;
struct MUI_CustomClass *lib_class = NULL;

/****************************************************************************/

BOOL LibInit(struct Library *lib)
{
    lib_base = lib;
    if (initShape())
    {
        initStrings();
        return TRUE;
    }
    return FALSE;
}

void LibExpunge(struct Library *lib)
{
    if (lib_cat)
    {
        CloseCatalog(lib_cat);
        lib_cat = NULL;
    }

    if (lib_shape)
    {
        MUI_DeleteCustomClass(lib_shape);
        lib_shape = NULL;
    }
}

ADD2INITLIB(LibInit, 0);
ADD2EXPUNGELIB(LibExpunge, 0);

/****************************************************************************/
