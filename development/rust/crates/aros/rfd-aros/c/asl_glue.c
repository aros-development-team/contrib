/*
 * ASL file and drawer requesters for the AROS stand-in of the `rfd` crate.
 *
 * `rfd_asl_request` shows a requester and writes the chosen paths to `out`,
 * separated by newlines; it returns the number of paths (0: cancelled or
 * nothing fits). `save` selects save mode, `multi` multi-selection, `drawers`
 * a drawer-only requester. `dir`, `file` and `pattern` may be empty.
 */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/asl.h>
#include <libraries/asl.h>
#include <stdint.h>
#include <string.h>

static void append_path(const char *drawer, const char *name, char *out, int32_t outlen, int32_t *count)
{
    char path[1024];

    strncpy(path, drawer ? drawer : "", sizeof path - 1);
    path[sizeof path - 1] = 0;
    if (name && name[0] && !AddPart((STRPTR)path, (CONST_STRPTR)name, sizeof path))
        return;
    if (strlen(out) + strlen(path) + 2 > (size_t)outlen)
        return;
    if (*count)
        strcat(out, "\n");
    strcat(out, path);
    (*count)++;
}

int32_t rfd_asl_request(int32_t save, int32_t multi, int32_t drawers, const char *title,
                        const char *dir, const char *file, const char *pattern, char *out, int32_t outlen)
{
    struct FileRequester *req;
    int32_t count = 0;

    if (!out || outlen < 2)
        return 0;
    out[0] = 0;
    req = AllocAslRequestTags(ASL_FileRequest,
        ASLFR_TitleText, (IPTR)(title && title[0] ? title : (drawers ? "Select drawer" : "Select file")),
        ASLFR_InitialDrawer, (IPTR)(dir ? dir : ""),
        ASLFR_InitialFile, (IPTR)(file ? file : ""),
        ASLFR_DoSaveMode, save ? TRUE : FALSE,
        ASLFR_DoMultiSelect, (multi && !save && !drawers) ? TRUE : FALSE,
        ASLFR_DrawersOnly, drawers ? TRUE : FALSE,
        ASLFR_DoPatterns, (pattern && pattern[0] && !drawers) ? TRUE : FALSE,
        ASLFR_InitialPattern, (IPTR)((pattern && pattern[0]) ? pattern : "#?"),
        ASLFR_RejectIcons, TRUE,
        TAG_DONE);
    if (!req)
        return 0;
    if (AslRequestTags(req, TAG_DONE)) {
        if (drawers) {
            if (req->fr_Drawer && req->fr_Drawer[0])
                append_path((const char *)req->fr_Drawer, "", out, outlen, &count);
        } else if (multi && !save && req->fr_NumArgs > 0) {
            LONG i;
            for (i = 0; i < req->fr_NumArgs; i++)
                append_path((const char *)req->fr_Drawer, (const char *)req->fr_ArgList[i].wa_Name, out, outlen, &count);
        } else if (req->fr_File && req->fr_File[0]) {
            append_path((const char *)req->fr_Drawer, (const char *)req->fr_File, out, outlen, &count);
        }
    }
    FreeAslRequest(req);
    return count;
}

/*
 * A message requester (intuition.library EasyRequest). `gadgets` is the
 * gadget text with '|' separators ("Yes|No|Cancel"). Returns the EasyRequest
 * result: 1 for the leftmost gadget, 2 for the next, ... and 0 for the
 * rightmost one; -1 when the requester could not be shown.
 */
#include <proto/intuition.h>
#include <intuition/intuition.h>

int32_t rfd_message_request(const char *title, const char *text, const char *gadgets)
{
    struct EasyStruct es;

    es.es_StructSize = sizeof(struct EasyStruct);
    es.es_Flags = 0;
    es.es_Title = (CONST_STRPTR)((title && title[0]) ? title : "Maolan");
    es.es_TextFormat = (CONST_STRPTR)"%s";
    es.es_GadgetFormat = (CONST_STRPTR)((gadgets && gadgets[0]) ? gadgets : "OK");
    return (int32_t)EasyRequest(NULL, &es, NULL, (IPTR)(text ? text : ""));
}
