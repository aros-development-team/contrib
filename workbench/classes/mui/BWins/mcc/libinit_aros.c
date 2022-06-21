#include <aros/symbolsets.h>

#include "base.h"
#include "class.h"

/****************************************************************************/

struct Library         *lib_base;
struct Catalog         *lib_cat = NULL;

Class                  *lib_sgad = NULL;
struct MUI_CustomClass *lib_boopsi = NULL;
struct MUI_CustomClass *lib_contents = NULL;
struct MUI_CustomClass *lib_mcc = NULL;

/****************************************************************************/

BOOL LibInit(struct Library *lib)
{
    lib_base = lib;
    if (initSGad() && initBoopsi() && initContents())
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

    if (lib_sgad)
    {
        FreeClass(lib_sgad);
        lib_sgad = NULL;
    }

    if (lib_boopsi)
    {
        MUI_DeleteCustomClass(lib_boopsi);
        lib_boopsi = NULL;
    }

    if (lib_contents)
    {
        MUI_DeleteCustomClass(lib_contents);
        lib_contents = NULL;
    }
}

ADD2INITLIB(LibInit, 0);
ADD2EXPUNGELIB(LibExpunge, 0);

/****************************************************************************/
