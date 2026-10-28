/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    The "aros" VA driver, built into libva: VA-API on the gfx.hidd decode
    interface.

    A VA display is a screen's display object. Configs and contexts map
    onto moHidd_Display_QueryDecode / GetDecodeInterface; every
    vaCreateSurfaces() call creates one decode-target video bitmap through
    the display, with one buffer per surface it returns (FFmpeg asks for
    one surface at a time, so usually one bitmap per surface). Decoding is
    the driver's: this file only forwards VA buffers to the interface and
    resolves surface IDs for it. Images map buffers with LockBuffer, and
    vaPutSurfaceAROS() shows a surface as a client surface on a plane.
*/

#define __OOP_NOATTRBASES__

#include <aros/debug.h>
#include <aros/asmcall.h>
#include <exec/memory.h>
#include <graphics/gfx.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <utility/hooks.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/oop.h>

#include <stdlib.h>
#include <string.h>

/* Method bases: resolved once in the driver init (see va_aros_InitBases) */
static OOP_MethodID va_HiddBitMapBase;
static OOP_MethodID va_HiddDisplayBase;
#define HiddBitMapBase  va_HiddBitMapBase
#define HiddDisplayBase va_HiddDisplayBase

#include <hidd/gfx.h>
#include <aros/surfacetrack.h>

static OOP_AttrBase __IHidd_BitMap;
static OOP_AttrBase __IHidd_Display;

#include "sysdeps.h"
#include "va.h"
#include "va_backend.h"
#include "va_internal.h"
#include "va_aros.h"

#define VA_AROS_VENDOR  "AROS gfx.hidd VA driver"

#define MAX_PROFILES        16
#define MAX_ENTRYPOINTS     1
#define MAX_ATTRIBUTES      4
#define MAX_IMAGE_FORMATS   2
#define MAX_SUBPIC_FORMATS  1
#define MAX_DISPLAY_ATTRIBS 1

/* ID ranges of the object kinds, so a stray ID of another kind is caught */
#define ID_CONFIG   0x01000000
#define ID_CONTEXT  0x02000000
#define ID_SURFACE  0x04000000
#define ID_BUFFER   0x08000000
#define ID_IMAGE    0x0c000000
#define ID_MASK     0x00ffffff

/*****************************************************************************/

struct va_table
{
    APTR   *slots;
    ULONG   size;
    ULONG   base;
};

struct va_config
{
    VAProfile       profile;
    VAEntrypoint    entrypoint;
    ULONG           rtformat;
};

/* One vaCreateSurfaces() call: a decode-target video bitmap */
struct va_pool
{
    OOP_Object     *bm;
    ULONG           buffers;
    ULONG           refs;           /* surfaces still alive          */
    ULONG           videofmt;
    ULONG           width, height;
};

struct va_context;

struct va_surface
{
    struct va_pool     *pool;
    ULONG               index;      /* buffer of pool->bm            */
    APTR                iface;      /* session that decoded into it  */
    VAImageID           derived;    /* VA_INVALID_ID or the derived image */
};

struct va_context
{
    struct aros_va_driver *drv;
    struct va_config   *config;
    APTR                iface;      /* gfx.hidd decode interface     */
    struct Hook         resolve;
    VASurfaceID         target;     /* between Begin and EndPicture  */
    int                 width, height;
};

struct va_buffer
{
    VABufferType        type;
    unsigned int        size;
    unsigned int        num;
    UBYTE              *data;       /* NULL for a derived image's     */
    BOOL                owned;      /* data allocated by us           */
    BOOL                mapped;
    BOOL                written;    /* mapped for writing             */
    VAImageID           image;      /* image this buffer belongs to   */
};

struct va_image
{
    VAImage             image;
    VASurfaceID         surface;    /* derived: the surface it maps   */
    UBYTE              *locked;     /* derived: LockBuffer address    */
};

/* Windows surfaces are shown in */
struct va_present
{
    struct MinNode               node;
    struct aros_va_driver       *drv;
    struct SignalSemaphore       lock;
    struct Window               *window;
    struct HIDD_CompositeRequest req;
    struct Hook                  hook;
    struct SurfaceTrack          track;
    struct Rectangle             dst;   /* window relative, inside the inner area */
    BOOL                         fulldst;
    BOOL                         attached;
    volatile BOOL                onplane;
};

struct aros_va_driver
{
    struct SignalSemaphore  lock;
    struct Screen          *screen;
    OOP_Object             *display;
    OOP_Object             *screenbm;
    OOP_Class              *bmclass;    /* CLID_Hidd_BitMap for CreateObject */

    struct va_table         configs;
    struct va_table         contexts;
    struct va_table         surfaces;
    struct va_table         buffers;
    struct va_table         images;

    struct MinList          presents;
    struct SurfaceTrackCtx  stctx;
};

#define DRV(ctx)    ((struct aros_va_driver *)(ctx)->pDriverData)

/*****************************************************************************/

static ULONG va_table_add(struct va_table *t, APTR obj)
{
    ULONG i;

    for (i = 0; i < t->size; i++)
    {
        if (!t->slots[i])
        {
            t->slots[i] = obj;
            return t->base | i;
        }
    }

    if (t->size >= ID_MASK)
        return VA_INVALID_ID;

    {
        ULONG nsize = t->size ? t->size * 2 : 32;
        APTR *n = realloc(t->slots, nsize * sizeof(APTR));

        if (!n)
            return VA_INVALID_ID;
        memset(&n[t->size], 0, (nsize - t->size) * sizeof(APTR));
        t->slots = n;
        t->size = nsize;
    }
    t->slots[i] = obj;
    return t->base | i;
}

static APTR va_table_get(struct va_table *t, ULONG id)
{
    if ((id & ~ID_MASK) != t->base || (id & ID_MASK) >= t->size)
        return NULL;
    return t->slots[id & ID_MASK];
}

static void va_table_remove(struct va_table *t, ULONG id)
{
    if ((id & ~ID_MASK) == t->base && (id & ID_MASK) < t->size)
        t->slots[id & ID_MASK] = NULL;
}

static void va_table_free(struct va_table *t)
{
    free(t->slots);
    t->slots = NULL;
    t->size = 0;
}

/*****************************************************************************/

/* Profiles a hardware decoder may offer; QueryDecode says which it does */
static const VAProfile va_candidates[] =
{
    VAProfileMPEG2Simple,
    VAProfileMPEG2Main,
    VAProfileH264ConstrainedBaseline,
    VAProfileH264Main,
    VAProfileH264High,
    VAProfileVC1Simple,
    VAProfileVC1Main,
    VAProfileVC1Advanced,
    VAProfileJPEGBaseline,
    VAProfileVP8Version0_3,
    VAProfileHEVCMain,
    VAProfileHEVCMain10,
    VAProfileVP9Profile0,
    VAProfileVP9Profile2,
    VAProfileAV1Profile0,
};

#define NUM_CANDIDATES  (sizeof(va_candidates) / sizeof(va_candidates[0]))

static BOOL va_ProfileSupported(struct aros_va_driver *drv, VAProfile profile, struct HIDD_DecodeCaps *caps)
{
    ULONG i;

    for (i = 0; i < NUM_CANDIDATES; i++)
        if (va_candidates[i] == profile)
            break;
    if (i == NUM_CANDIDATES)
        return FALSE;

    return (HIDD_Display_QueryDecode(drv->display, profile, 0, 0, caps) & DCF_DECODE) ? TRUE : FALSE;
}

/* The VA_RT_FORMAT_* a profile decodes to */
static ULONG va_ProfileRTFormats(struct aros_va_driver *drv, VAProfile profile)
{
    struct HIDD_DecodeCaps caps;
    ULONG rt = 0;

    if (!va_ProfileSupported(drv, profile, &caps))
        return 0;
    if (caps.VideoFmtMask & (1 << vHidd_VideoFmt_NV12))
        rt |= VA_RT_FORMAT_YUV420;
    if (caps.VideoFmtMask & (1 << vHidd_VideoFmt_P010))
        rt |= VA_RT_FORMAT_YUV420_10;
    return rt;
}

static ULONG va_FourccToVideoFmt(unsigned int fourcc)
{
    switch (fourcc)
    {
    case VA_FOURCC_NV12: return vHidd_VideoFmt_NV12;
    case VA_FOURCC_P010: return vHidd_VideoFmt_P010;
    }
    return vHidd_VideoFmt_None;
}

static ULONG va_RTToVideoFmt(unsigned int rtformat)
{
    if (rtformat & VA_RT_FORMAT_YUV420)
        return vHidd_VideoFmt_NV12;
    if (rtformat & VA_RT_FORMAT_YUV420_10)
        return vHidd_VideoFmt_P010;
    return vHidd_VideoFmt_None;
}

static void va_ImageFormat(ULONG videofmt, VAImageFormat *fmt)
{
    memset(fmt, 0, sizeof(*fmt));
    fmt->byte_order = VA_LSB_FIRST;
    if (videofmt == vHidd_VideoFmt_P010)
    {
        fmt->fourcc = VA_FOURCC_P010;
        fmt->bits_per_pixel = 24;
    }
    else
    {
        fmt->fourcc = VA_FOURCC_NV12;
        fmt->bits_per_pixel = 12;
    }
}

/*****************************************************************************/

static VAStatus aros_Terminate(VADriverContextP ctx);

static VAStatus aros_QueryConfigProfiles(VADriverContextP ctx, VAProfile *profile_list, int *num_profiles)
{
    struct aros_va_driver *drv = DRV(ctx);
    ULONG i;
    int n = 0;

    for (i = 0; i < NUM_CANDIDATES; i++)
        if (va_ProfileSupported(drv, va_candidates[i], NULL))
            profile_list[n++] = va_candidates[i];

    *num_profiles = n;
    return VA_STATUS_SUCCESS;
}

static VAStatus aros_QueryConfigEntrypoints(VADriverContextP ctx, VAProfile profile,
                                            VAEntrypoint *entrypoint_list, int *num_entrypoints)
{
    if (!va_ProfileSupported(DRV(ctx), profile, NULL))
    {
        *num_entrypoints = 0;
        return VA_STATUS_ERROR_UNSUPPORTED_PROFILE;
    }

    entrypoint_list[0] = VAEntrypointVLD;
    *num_entrypoints = 1;
    return VA_STATUS_SUCCESS;
}

static VAStatus aros_GetConfigAttributes(VADriverContextP ctx, VAProfile profile, VAEntrypoint entrypoint,
                                         VAConfigAttrib *attrib_list, int num_attribs)
{
    struct aros_va_driver *drv = DRV(ctx);
    int i;

    if (!va_ProfileSupported(drv, profile, NULL))
        return VA_STATUS_ERROR_UNSUPPORTED_PROFILE;
    if (entrypoint != VAEntrypointVLD)
        return VA_STATUS_ERROR_UNSUPPORTED_ENTRYPOINT;

    for (i = 0; i < num_attribs; i++)
    {
        switch (attrib_list[i].type)
        {
        case VAConfigAttribRTFormat:
            attrib_list[i].value = va_ProfileRTFormats(drv, profile);
            break;

        case VAConfigAttribDecSliceMode:
            attrib_list[i].value = VA_DEC_SLICE_MODE_NORMAL;
            break;

        default:
            attrib_list[i].value = VA_ATTRIB_NOT_SUPPORTED;
            break;
        }
    }
    return VA_STATUS_SUCCESS;
}

static VAStatus aros_CreateConfig(VADriverContextP ctx, VAProfile profile, VAEntrypoint entrypoint,
                                  VAConfigAttrib *attrib_list, int num_attribs, VAConfigID *config_id)
{
    struct aros_va_driver *drv = DRV(ctx);
    struct va_config *cfg;
    ULONG supported;
    int i;

    if (!va_ProfileSupported(drv, profile, NULL))
        return VA_STATUS_ERROR_UNSUPPORTED_PROFILE;
    if (entrypoint != VAEntrypointVLD)
        return VA_STATUS_ERROR_UNSUPPORTED_ENTRYPOINT;

    supported = va_ProfileRTFormats(drv, profile);
    cfg = calloc(1, sizeof(*cfg));
    if (!cfg)
        return VA_STATUS_ERROR_ALLOCATION_FAILED;
    cfg->profile    = profile;
    cfg->entrypoint = entrypoint;
    cfg->rtformat   = supported;    /* every format the profile decodes to: FFmpeg picks the stream's from the surface attributes */

    for (i = 0; i < num_attribs; i++)
    {
        if (attrib_list[i].type == VAConfigAttribRTFormat)
        {
            if (!(attrib_list[i].value & supported))
            {
                free(cfg);
                return VA_STATUS_ERROR_UNSUPPORTED_RT_FORMAT;
            }
            cfg->rtformat = attrib_list[i].value & supported;
        }
    }

    ObtainSemaphore(&drv->lock);
    *config_id = va_table_add(&drv->configs, cfg);
    ReleaseSemaphore(&drv->lock);

    if (*config_id == VA_INVALID_ID)
    {
        free(cfg);
        return VA_STATUS_ERROR_ALLOCATION_FAILED;
    }
    return VA_STATUS_SUCCESS;
}

static VAStatus aros_DestroyConfig(VADriverContextP ctx, VAConfigID config_id)
{
    struct aros_va_driver *drv = DRV(ctx);
    struct va_config *cfg;

    ObtainSemaphore(&drv->lock);
    cfg = va_table_get(&drv->configs, config_id);
    if (cfg)
        va_table_remove(&drv->configs, config_id);
    ReleaseSemaphore(&drv->lock);

    if (!cfg)
        return VA_STATUS_ERROR_INVALID_CONFIG;
    free(cfg);
    return VA_STATUS_SUCCESS;
}

static VAStatus aros_QueryConfigAttributes(VADriverContextP ctx, VAConfigID config_id, VAProfile *profile,
                                           VAEntrypoint *entrypoint, VAConfigAttrib *attrib_list, int *num_attribs)
{
    struct aros_va_driver *drv = DRV(ctx);
    struct va_config *cfg;

    ObtainSemaphore(&drv->lock);
    cfg = va_table_get(&drv->configs, config_id);
    if (cfg)
    {
        *profile    = cfg->profile;
        *entrypoint = cfg->entrypoint;
        attrib_list[0].type  = VAConfigAttribRTFormat;
        attrib_list[0].value = cfg->rtformat;
        *num_attribs = 1;
    }
    ReleaseSemaphore(&drv->lock);

    return cfg ? VA_STATUS_SUCCESS : VA_STATUS_ERROR_INVALID_CONFIG;
}

/*****************************************************************************/

/* drv->lock held */
static void va_PoolRelease(struct aros_va_driver *drv, struct va_pool *pool)
{
    if (--pool->refs)
        return;
    OOP_DisposeObject(pool->bm);
    free(pool);
}

/* drv->lock held. Unlocks a derived image's buffer if one is left. */
static void va_DestroyImageLocked(struct aros_va_driver *drv, VAImageID image_id);

static VAStatus aros_CreateSurfaces2(VADriverContextP ctx, unsigned int format, unsigned int width,
                                     unsigned int height, VASurfaceID *surfaces, unsigned int num_surfaces,
                                     VASurfaceAttrib *attrib_list, unsigned int num_attribs)
{
    struct aros_va_driver *drv = DRV(ctx);
    struct va_pool *pool;
    ULONG videofmt = va_RTToVideoFmt(format);
    unsigned int i;
    VAStatus status = VA_STATUS_SUCCESS;

    if (!num_surfaces || !width || !height)
        return VA_STATUS_ERROR_INVALID_PARAMETER;

    for (i = 0; i < num_attribs; i++)
    {
        if (!(attrib_list[i].flags & VA_SURFACE_ATTRIB_SETTABLE))
            continue;

        switch (attrib_list[i].type)
        {
        case VASurfaceAttribPixelFormat:
            videofmt = va_FourccToVideoFmt(attrib_list[i].value.value.i);
            break;

        case VASurfaceAttribMemoryType:
            if (attrib_list[i].value.value.i != VA_SURFACE_ATTRIB_MEM_TYPE_VA)
                return VA_STATUS_ERROR_UNSUPPORTED_MEMORY_TYPE;
            break;

        default:
            break;
        }
    }

    if (videofmt == vHidd_VideoFmt_None)
        return VA_STATUS_ERROR_UNSUPPORTED_RT_FORMAT;

    pool = calloc(1, sizeof(*pool));
    if (!pool)
        return VA_STATUS_ERROR_ALLOCATION_FAILED;

    {
        /* Width and height even: the chroma planes are half size */
        struct TagItem bmtags[] =
        {
            { aHidd_BitMap_Scanout,      TRUE                    },
            { aHidd_BitMap_DecodeTarget, TRUE                    },
            { aHidd_BitMap_Width,        (width + 1) & ~1        },
            { aHidd_BitMap_Height,       (height + 1) & ~1       },
            { aHidd_BitMap_Buffers,      num_surfaces            },
            { aHidd_BitMap_VideoFormat,  videofmt                },
            { aHidd_BitMap_Friend,       (IPTR)drv->screenbm     },
            { aHidd_BitMap_ColorSpace,   vHidd_ColorSpace_BT709  },
            { TAG_DONE,                  0                       }
        };
        IPTR val = 0;

        pool->bm = HIDD_Display_CreateObject(drv->display, drv->bmclass, bmtags);
        if (pool->bm)
            OOP_GetAttr(pool->bm, aHidd_BitMap_Buffers, &val);
        if (!pool->bm || val < num_surfaces)
        {
            D(bug("[VA] %s: decode-target bitmap %ux%u x%u refused\n", __func__, width, height, num_surfaces));
            if (pool->bm)
                OOP_DisposeObject(pool->bm);
            free(pool);
            return VA_STATUS_ERROR_ALLOCATION_FAILED;
        }
    }
    pool->buffers  = num_surfaces;
    pool->videofmt = videofmt;
    pool->width    = width;
    pool->height   = height;

    ObtainSemaphore(&drv->lock);
    for (i = 0; i < num_surfaces; i++)
    {
        struct va_surface *surf = calloc(1, sizeof(*surf));

        if (surf)
        {
            surf->pool    = pool;
            surf->index   = i;
            surf->derived = VA_INVALID_ID;
            surfaces[i] = va_table_add(&drv->surfaces, surf);
            if (surfaces[i] != VA_INVALID_ID)
            {
                pool->refs++;
                continue;
            }
            free(surf);
        }
        status = VA_STATUS_ERROR_ALLOCATION_FAILED;
        break;
    }

    if (status != VA_STATUS_SUCCESS)
    {
        unsigned int j;

        for (j = 0; j < i; j++)
        {
            free(va_table_get(&drv->surfaces, surfaces[j]));
            va_table_remove(&drv->surfaces, surfaces[j]);
            surfaces[j] = VA_INVALID_SURFACE;
        }
        pool->refs = 1;
        va_PoolRelease(drv, pool);
    }
    ReleaseSemaphore(&drv->lock);

    return status;
}

static VAStatus aros_CreateSurfaces(VADriverContextP ctx, int width, int height, int format,
                                    int num_surfaces, VASurfaceID *surfaces)
{
    return aros_CreateSurfaces2(ctx, format, width, height, surfaces, num_surfaces, NULL, 0);
}

static VAStatus aros_DestroySurfaces(VADriverContextP ctx, VASurfaceID *surface_list, int num_surfaces)
{
    struct aros_va_driver *drv = DRV(ctx);
    VAStatus status = VA_STATUS_SUCCESS;
    struct va_present *vp;
    int i;

    ObtainSemaphore(&drv->lock);
    for (i = 0; i < num_surfaces; i++)
    {
        struct va_surface *surf = va_table_get(&drv->surfaces, surface_list[i]);

        if (!surf)
        {
            status = VA_STATUS_ERROR_INVALID_SURFACE;
            continue;
        }

        if (surf->iface)
        {
            ULONG c;

            /* The decode into it must be over before the memory can go */
            for (c = 0; c < drv->contexts.size; c++)
            {
                struct va_context *vc = drv->contexts.slots[c];
                if (vc && vc->iface == surf->iface)
                {
                    HIDD_Decode_Sync(surf->iface, surf->pool->bm, surf->index, ~0);
                    break;
                }
            }
        }
        if (surf->derived != VA_INVALID_ID)
            va_DestroyImageLocked(drv, surf->derived);

        /* A window still showing the bitmap keeps it until it shows another */
        ForeachNode(&drv->presents, vp)
        {
            if (vp->attached && vp->req.BitMap == surf->pool->bm && surf->pool->refs == 1)
            {
                HIDD_Display_DetachSurface(drv->display, &vp->req);
                vp->attached = FALSE;
                vp->onplane = FALSE;
                st_Reset(&vp->track);
                vp->req.Shape = NULL;
            }
        }

        va_table_remove(&drv->surfaces, surface_list[i]);
        va_PoolRelease(drv, surf->pool);
        free(surf);
    }
    ReleaseSemaphore(&drv->lock);

    return status;
}

static VAStatus aros_QuerySurfaceAttributes(VADriverContextP ctx, VAConfigID config, VASurfaceAttrib *attrib_list,
                                           unsigned int *num_attribs)
{
    struct aros_va_driver *drv = DRV(ctx);
    struct HIDD_DecodeCaps caps;
    struct va_config *cfg;
    VASurfaceAttrib attribs[8];
    unsigned int n = 0;

    ObtainSemaphore(&drv->lock);
    cfg = va_table_get(&drv->configs, config);
    ReleaseSemaphore(&drv->lock);
    if (!cfg)
        return VA_STATUS_ERROR_INVALID_CONFIG;

    memset(&caps, 0, sizeof(caps));
    va_ProfileSupported(drv, cfg->profile, &caps);
    memset(attribs, 0, sizeof(attribs));

    if ((cfg->rtformat & VA_RT_FORMAT_YUV420) && (caps.VideoFmtMask & (1 << vHidd_VideoFmt_NV12)))
    {
        attribs[n].type = VASurfaceAttribPixelFormat;
        attribs[n].flags = VA_SURFACE_ATTRIB_GETTABLE | VA_SURFACE_ATTRIB_SETTABLE;
        attribs[n].value.type = VAGenericValueTypeInteger;
        attribs[n++].value.value.i = VA_FOURCC_NV12;
    }
    if ((cfg->rtformat & VA_RT_FORMAT_YUV420_10) && (caps.VideoFmtMask & (1 << vHidd_VideoFmt_P010)))
    {
        attribs[n].type = VASurfaceAttribPixelFormat;
        attribs[n].flags = VA_SURFACE_ATTRIB_GETTABLE | VA_SURFACE_ATTRIB_SETTABLE;
        attribs[n].value.type = VAGenericValueTypeInteger;
        attribs[n++].value.value.i = VA_FOURCC_P010;
    }

    attribs[n].type = VASurfaceAttribMemoryType;
    attribs[n].flags = VA_SURFACE_ATTRIB_GETTABLE | VA_SURFACE_ATTRIB_SETTABLE;
    attribs[n].value.type = VAGenericValueTypeInteger;
    attribs[n++].value.value.i = VA_SURFACE_ATTRIB_MEM_TYPE_VA;

    if (caps.MaxWidth)
    {
        attribs[n].type = VASurfaceAttribMaxWidth;
        attribs[n].flags = VA_SURFACE_ATTRIB_GETTABLE;
        attribs[n].value.type = VAGenericValueTypeInteger;
        attribs[n++].value.value.i = caps.MaxWidth;

        attribs[n].type = VASurfaceAttribMaxHeight;
        attribs[n].flags = VA_SURFACE_ATTRIB_GETTABLE;
        attribs[n].value.type = VAGenericValueTypeInteger;
        attribs[n++].value.value.i = caps.MaxHeight;
    }
    if (caps.MinWidth)
    {
        attribs[n].type = VASurfaceAttribMinWidth;
        attribs[n].flags = VA_SURFACE_ATTRIB_GETTABLE;
        attribs[n].value.type = VAGenericValueTypeInteger;
        attribs[n++].value.value.i = caps.MinWidth;

        attribs[n].type = VASurfaceAttribMinHeight;
        attribs[n].flags = VA_SURFACE_ATTRIB_GETTABLE;
        attribs[n].value.type = VAGenericValueTypeInteger;
        attribs[n++].value.value.i = caps.MinHeight;
    }

    if (!attrib_list)
    {
        *num_attribs = n;
        return VA_STATUS_SUCCESS;
    }
    if (*num_attribs < n)
    {
        *num_attribs = n;
        return VA_STATUS_ERROR_MAX_NUM_EXCEEDED;
    }
    memcpy(attrib_list, attribs, n * sizeof(VASurfaceAttrib));
    *num_attribs = n;
    return VA_STATUS_SUCCESS;
}

static VAStatus aros_GetSurfaceAttributes(VADriverContextP ctx, VAConfigID config, VASurfaceAttrib *attrib_list,
                                         unsigned int num_attribs)
{
    return VA_STATUS_ERROR_UNIMPLEMENTED;
}

/*****************************************************************************/

/*
 * The decode interface's resolve hook: a VA surface ID to the (bitmap,
 * buffer) it lives in. Called by the driver from within the decode calls
 * we make (drv->lock is ours then) or from its own tasks.
 */
AROS_UFH3(static IPTR, va_ResolveHook,
    AROS_UFHA(struct Hook *, hook, A0),
    AROS_UFHA(APTR, iface, A2),
    AROS_UFHA(struct HIDD_DecodeSurfaceMsg *, msg, A1))
{
    AROS_USERFUNC_INIT

    struct aros_va_driver *drv = hook->h_Data;
    struct va_surface *surf;
    IPTR found = FALSE;

    ObtainSemaphore(&drv->lock);
    surf = va_table_get(&drv->surfaces, msg->SurfaceID);
    if (surf)
    {
        msg->BitMap = surf->pool->bm;
        msg->Buffer = surf->index;
        found = TRUE;
    }
    ReleaseSemaphore(&drv->lock);

    return found;

    AROS_USERFUNC_EXIT
}

static VAStatus aros_CreateContext(VADriverContextP ctx, VAConfigID config_id, int picture_width,
                                   int picture_height, int flag, VASurfaceID *render_targets,
                                   int num_render_targets, VAContextID *context)
{
    struct aros_va_driver *drv = DRV(ctx);
    struct va_config *cfg;
    struct va_context *vc;

    ObtainSemaphore(&drv->lock);
    cfg = va_table_get(&drv->configs, config_id);
    ReleaseSemaphore(&drv->lock);
    if (!cfg)
        return VA_STATUS_ERROR_INVALID_CONFIG;

    vc = calloc(1, sizeof(*vc));
    if (!vc)
        return VA_STATUS_ERROR_ALLOCATION_FAILED;

    vc->drv    = drv;
    vc->config = cfg;
    vc->width  = picture_width;
    vc->height = picture_height;
    vc->target = VA_INVALID_SURFACE;
    vc->resolve.h_Entry = (HOOKFUNC)AROS_ASMSYMNAME(va_ResolveHook);
    vc->resolve.h_Data  = drv;

    vc->iface = HIDD_Display_GetDecodeInterface(drv->display, cfg->profile, picture_width, picture_height,
                                                &vc->resolve, 0);
    if (!vc->iface)
    {
        free(vc);
        return VA_STATUS_ERROR_HW_BUSY;
    }

    ObtainSemaphore(&drv->lock);
    *context = va_table_add(&drv->contexts, vc);
    ReleaseSemaphore(&drv->lock);

    if (*context == VA_INVALID_ID)
    {
        HIDD_Display_FreeDecodeInterface(drv->display, vc->iface);
        free(vc);
        return VA_STATUS_ERROR_ALLOCATION_FAILED;
    }
    return VA_STATUS_SUCCESS;
}

static VAStatus aros_DestroyContext(VADriverContextP ctx, VAContextID context)
{
    struct aros_va_driver *drv = DRV(ctx);
    struct va_context *vc;
    ULONG i;

    ObtainSemaphore(&drv->lock);
    vc = va_table_get(&drv->contexts, context);
    if (vc)
    {
        va_table_remove(&drv->contexts, context);

        /* Surfaces it decoded no longer have a session to sync with */
        for (i = 0; i < drv->surfaces.size; i++)
        {
            struct va_surface *surf = drv->surfaces.slots[i];
            if (surf && surf->iface == vc->iface)
                surf->iface = NULL;
        }
    }
    ReleaseSemaphore(&drv->lock);

    if (!vc)
        return VA_STATUS_ERROR_INVALID_CONTEXT;

    HIDD_Display_FreeDecodeInterface(drv->display, vc->iface);
    free(vc);
    return VA_STATUS_SUCCESS;
}

/*****************************************************************************/

static VAStatus aros_CreateBuffer(VADriverContextP ctx, VAContextID context, VABufferType type,
                                  unsigned int size, unsigned int num_elements, void *data, VABufferID *buf_id)
{
    struct aros_va_driver *drv = DRV(ctx);
    struct va_buffer *buf;

    if (!size || !num_elements)
        return VA_STATUS_ERROR_INVALID_PARAMETER;
    if ((ULONG)size * num_elements / num_elements != size)
        return VA_STATUS_ERROR_ALLOCATION_FAILED;

    buf = calloc(1, sizeof(*buf));
    if (!buf)
        return VA_STATUS_ERROR_ALLOCATION_FAILED;

    buf->type  = type;
    buf->size  = size;
    buf->num   = num_elements;
    buf->image = VA_INVALID_ID;
    buf->owned = TRUE;
    buf->data  = malloc((size_t)size * num_elements);
    if (!buf->data)
    {
        free(buf);
        return VA_STATUS_ERROR_ALLOCATION_FAILED;
    }
    if (data)
        memcpy(buf->data, data, (size_t)size * num_elements);

    ObtainSemaphore(&drv->lock);
    *buf_id = va_table_add(&drv->buffers, buf);
    ReleaseSemaphore(&drv->lock);

    if (*buf_id == VA_INVALID_ID)
    {
        free(buf->data);
        free(buf);
        return VA_STATUS_ERROR_ALLOCATION_FAILED;
    }
    return VA_STATUS_SUCCESS;
}

static VAStatus aros_BufferSetNumElements(VADriverContextP ctx, VABufferID buf_id, unsigned int num_elements)
{
    struct aros_va_driver *drv = DRV(ctx);
    struct va_buffer *buf;
    VAStatus status = VA_STATUS_SUCCESS;

    ObtainSemaphore(&drv->lock);
    buf = va_table_get(&drv->buffers, buf_id);
    if (!buf)
        status = VA_STATUS_ERROR_INVALID_BUFFER;
    else if (buf->mapped || !buf->owned || num_elements > buf->num)
        status = VA_STATUS_ERROR_INVALID_PARAMETER;
    else
        buf->num = num_elements;
    ReleaseSemaphore(&drv->lock);

    return status;
}

static VAStatus aros_MapBuffer2(VADriverContextP ctx, VABufferID buf_id, void **pbuf, uint32_t flags)
{
    struct aros_va_driver *drv = DRV(ctx);
    struct va_buffer *buf;
    VAStatus status = VA_STATUS_SUCCESS;

    ObtainSemaphore(&drv->lock);
    buf = va_table_get(&drv->buffers, buf_id);
    if (!buf || !buf->data)
        status = VA_STATUS_ERROR_INVALID_BUFFER;
    else
    {
        buf->mapped = TRUE;
        if (!flags || (flags & VA_MAPBUFFER_FLAG_WRITE))
            buf->written = TRUE;
        *pbuf = buf->data;
    }
    ReleaseSemaphore(&drv->lock);

    return status;
}

static VAStatus aros_MapBuffer(VADriverContextP ctx, VABufferID buf_id, void **pbuf)
{
    return aros_MapBuffer2(ctx, buf_id, pbuf, 0);
}

static VAStatus aros_UnmapBuffer(VADriverContextP ctx, VABufferID buf_id)
{
    struct aros_va_driver *drv = DRV(ctx);
    struct va_buffer *buf;
    VAStatus status = VA_STATUS_SUCCESS;

    ObtainSemaphore(&drv->lock);
    buf = va_table_get(&drv->buffers, buf_id);
    if (!buf)
        status = VA_STATUS_ERROR_INVALID_BUFFER;
    else if (!buf->mapped)
        status = VA_STATUS_ERROR_OPERATION_FAILED;
    else
        buf->mapped = FALSE;
    ReleaseSemaphore(&drv->lock);

    return status;
}

/* drv->lock held */
static void va_DestroyBufferLocked(struct aros_va_driver *drv, VABufferID buffer_id)
{
    struct va_buffer *buf = va_table_get(&drv->buffers, buffer_id);

    if (!buf)
        return;
    va_table_remove(&drv->buffers, buffer_id);
    if (buf->owned)
        free(buf->data);
    free(buf);
}

static VAStatus aros_DestroyBuffer(VADriverContextP ctx, VABufferID buffer_id)
{
    struct aros_va_driver *drv = DRV(ctx);
    struct va_buffer *buf;
    VAStatus status = VA_STATUS_SUCCESS;

    ObtainSemaphore(&drv->lock);
    buf = va_table_get(&drv->buffers, buffer_id);
    if (!buf)
        status = VA_STATUS_ERROR_INVALID_BUFFER;
    else if (buf->image != VA_INVALID_ID)
        status = VA_STATUS_ERROR_OPERATION_FAILED;   /* goes with its image */
    else
        va_DestroyBufferLocked(drv, buffer_id);
    ReleaseSemaphore(&drv->lock);

    return status;
}

static VAStatus aros_BufferInfo(VADriverContextP ctx, VABufferID buf_id, VABufferType *type,
                                unsigned int *size, unsigned int *num_elements)
{
    struct aros_va_driver *drv = DRV(ctx);
    struct va_buffer *buf;

    ObtainSemaphore(&drv->lock);
    buf = va_table_get(&drv->buffers, buf_id);
    if (buf)
    {
        *type = buf->type;
        *size = buf->size;
        *num_elements = buf->num;
    }
    ReleaseSemaphore(&drv->lock);

    return buf ? VA_STATUS_SUCCESS : VA_STATUS_ERROR_INVALID_BUFFER;
}

/*****************************************************************************/

static VAStatus aros_BeginPicture(VADriverContextP ctx, VAContextID context, VASurfaceID render_target)
{
    struct aros_va_driver *drv = DRV(ctx);
    struct va_context *vc;
    struct va_surface *surf;
    VAStatus status = VA_STATUS_SUCCESS;

    ObtainSemaphore(&drv->lock);
    vc = va_table_get(&drv->contexts, context);
    surf = va_table_get(&drv->surfaces, render_target);
    if (!vc)
        status = VA_STATUS_ERROR_INVALID_CONTEXT;
    else if (!surf)
        status = VA_STATUS_ERROR_INVALID_SURFACE;
    else if (surf->derived != VA_INVALID_ID)
        status = VA_STATUS_ERROR_SURFACE_BUSY;     /* mapped by the CPU */
    else if (!HIDD_Decode_BeginFrame(vc->iface, surf->pool->bm, surf->index))
        status = VA_STATUS_ERROR_DECODING_ERROR;
    else
        vc->target = render_target;
    ReleaseSemaphore(&drv->lock);

    return status;
}

static VAStatus aros_RenderPicture(VADriverContextP ctx, VAContextID context, VABufferID *buffers, int num_buffers)
{
    struct aros_va_driver *drv = DRV(ctx);
    struct va_context *vc;
    VAStatus status = VA_STATUS_SUCCESS;
    int i;

    ObtainSemaphore(&drv->lock);
    vc = va_table_get(&drv->contexts, context);
    if (!vc)
        status = VA_STATUS_ERROR_INVALID_CONTEXT;
    else if (vc->target == VA_INVALID_SURFACE)
        status = VA_STATUS_ERROR_OPERATION_FAILED;

    for (i = 0; (status == VA_STATUS_SUCCESS) && (i < num_buffers); i++)
    {
        struct va_buffer *buf = va_table_get(&drv->buffers, buffers[i]);
        BOOL ok;

        if (!buf || !buf->owned)
        {
            status = VA_STATUS_ERROR_INVALID_BUFFER;
            break;
        }

        if (buf->type == VASliceDataBufferType)
            ok = HIDD_Decode_SubmitBitstream(vc->iface, buf->data, buf->size * buf->num);
        else
            ok = HIDD_Decode_SubmitParams(vc->iface, buf->type, buf->data, buf->size, buf->num);

        if (!ok)
            status = VA_STATUS_ERROR_UNSUPPORTED_BUFFERTYPE;
    }
    ReleaseSemaphore(&drv->lock);

    return status;
}

static VAStatus aros_EndPicture(VADriverContextP ctx, VAContextID context)
{
    struct aros_va_driver *drv = DRV(ctx);
    struct va_context *vc;
    struct va_surface *surf;
    VAStatus status = VA_STATUS_SUCCESS;

    ObtainSemaphore(&drv->lock);
    vc = va_table_get(&drv->contexts, context);
    if (!vc)
        status = VA_STATUS_ERROR_INVALID_CONTEXT;
    else if (vc->target == VA_INVALID_SURFACE)
        status = VA_STATUS_ERROR_OPERATION_FAILED;
    else
    {
        surf = va_table_get(&drv->surfaces, vc->target);
        if (!HIDD_Decode_EndFrame(vc->iface))
            status = VA_STATUS_ERROR_DECODING_ERROR;
        if (surf)
            surf->iface = vc->iface;
        vc->target = VA_INVALID_SURFACE;
    }
    ReleaseSemaphore(&drv->lock);

    return status;
}

/* drv->lock held: wait for the decode into a surface */
static ULONG va_SyncLocked(struct va_surface *surf, ULONG timeoutms)
{
    if (!surf->iface)
        return vHidd_DecodeStatus_Ready;
    return HIDD_Decode_Sync(surf->iface, surf->pool->bm, surf->index, timeoutms);
}

static VAStatus aros_SyncSurface2(VADriverContextP ctx, VASurfaceID surface, uint64_t timeout_ns)
{
    struct aros_va_driver *drv = DRV(ctx);
    struct va_surface *surf;
    ULONG result = vHidd_DecodeStatus_Ready;
    ULONG timeoutms;

    if (timeout_ns == VA_TIMEOUT_INFINITE)
        timeoutms = ~0;
    else if (timeout_ns / 1000000 >= 0xfffffffe)
        timeoutms = 0xfffffffe;
    else
        timeoutms = (ULONG)(timeout_ns / 1000000);

    ObtainSemaphore(&drv->lock);
    surf = va_table_get(&drv->surfaces, surface);
    if (surf)
        result = va_SyncLocked(surf, timeoutms);
    ReleaseSemaphore(&drv->lock);

    if (!surf)
        return VA_STATUS_ERROR_INVALID_SURFACE;
    switch (result)
    {
    case vHidd_DecodeStatus_Ready: return VA_STATUS_SUCCESS;
    case vHidd_DecodeStatus_Busy:  return VA_STATUS_ERROR_TIMEDOUT;
    }
    return VA_STATUS_ERROR_DECODING_ERROR;
}

static VAStatus aros_SyncSurface(VADriverContextP ctx, VASurfaceID render_target)
{
    return aros_SyncSurface2(ctx, render_target, VA_TIMEOUT_INFINITE);
}

static VAStatus aros_QuerySurfaceStatus(VADriverContextP ctx, VASurfaceID render_target, VASurfaceStatus *status)
{
    struct aros_va_driver *drv = DRV(ctx);
    struct va_surface *surf;
    ULONG result = vHidd_DecodeStatus_Ready;

    ObtainSemaphore(&drv->lock);
    surf = va_table_get(&drv->surfaces, render_target);
    if (surf && surf->iface)
        result = HIDD_Decode_Status(surf->iface, surf->pool->bm, surf->index);
    ReleaseSemaphore(&drv->lock);

    if (!surf)
        return VA_STATUS_ERROR_INVALID_SURFACE;
    *status = (result == vHidd_DecodeStatus_Busy) ? VASurfaceRendering : VASurfaceReady;
    return VA_STATUS_SUCCESS;
}

static VAStatus aros_QuerySurfaceError(VADriverContextP ctx, VASurfaceID render_target, VAStatus error_status,
                                       void **error_info)
{
    return VA_STATUS_ERROR_UNIMPLEMENTED;
}

/*****************************************************************************/

static VAStatus aros_QueryImageFormats(VADriverContextP ctx, VAImageFormat *format_list, int *num_formats)
{
    va_ImageFormat(vHidd_VideoFmt_NV12, &format_list[0]);
    va_ImageFormat(vHidd_VideoFmt_P010, &format_list[1]);
    *num_formats = 2;
    return VA_STATUS_SUCCESS;
}

/* Layout of an image of the given format; FALSE for formats we lack */
static BOOL va_ImageLayout(VAImage *img, ULONG videofmt, int width, int height, ULONG pitch, ULONG uvoffset)
{
    ULONG bpp = (videofmt == vHidd_VideoFmt_P010) ? 2 : 1;
    ULONG cw = (width + 1) & ~1;
    ULONG ch = (height + 1) / 2;

    if (!pitch)
        pitch = cw * bpp;
    if (!uvoffset)
        uvoffset = pitch * height;

    va_ImageFormat(videofmt, &img->format);
    img->width      = width;
    img->height     = height;
    img->num_planes = 2;
    img->pitches[0] = pitch;
    img->pitches[1] = pitch;
    img->offsets[0] = 0;
    img->offsets[1] = uvoffset;
    img->data_size  = uvoffset + pitch * ch;
    return TRUE;
}

static VAStatus aros_CreateImage(VADriverContextP ctx, VAImageFormat *format, int width, int height, VAImage *image)
{
    struct aros_va_driver *drv = DRV(ctx);
    ULONG videofmt = va_FourccToVideoFmt(format->fourcc);
    struct va_image *vi;
    struct va_buffer *buf;

    if (videofmt == vHidd_VideoFmt_None)
        return VA_STATUS_ERROR_INVALID_IMAGE_FORMAT;
    if (width <= 0 || height <= 0)
        return VA_STATUS_ERROR_INVALID_PARAMETER;

    vi = calloc(1, sizeof(*vi));
    buf = calloc(1, sizeof(*buf));
    if (!vi || !buf)
    {
        free(vi);
        free(buf);
        return VA_STATUS_ERROR_ALLOCATION_FAILED;
    }

    va_ImageLayout(&vi->image, videofmt, width, height, 0, 0);
    vi->surface = VA_INVALID_SURFACE;

    buf->type  = VAImageBufferType;
    buf->size  = vi->image.data_size;
    buf->num   = 1;
    buf->owned = TRUE;
    buf->data  = malloc(buf->size);
    if (!buf->data)
    {
        free(vi);
        free(buf);
        return VA_STATUS_ERROR_ALLOCATION_FAILED;
    }

    ObtainSemaphore(&drv->lock);
    vi->image.image_id = va_table_add(&drv->images, vi);
    vi->image.buf = (vi->image.image_id != VA_INVALID_ID) ? va_table_add(&drv->buffers, buf) : VA_INVALID_ID;
    if (vi->image.buf == VA_INVALID_ID)
    {
        if (vi->image.image_id != VA_INVALID_ID)
            va_table_remove(&drv->images, vi->image.image_id);
        ReleaseSemaphore(&drv->lock);
        free(buf->data);
        free(buf);
        free(vi);
        return VA_STATUS_ERROR_ALLOCATION_FAILED;
    }
    buf->image = vi->image.image_id;
    *image = vi->image;
    ReleaseSemaphore(&drv->lock);

    return VA_STATUS_SUCCESS;
}

/*
 * The image maps the surface's buffer directly: it stays locked
 * (LockBuffer) until the image is destroyed.
 */
static VAStatus aros_DeriveImage(VADriverContextP ctx, VASurfaceID surface, VAImage *image)
{
    struct aros_va_driver *drv = DRV(ctx);
    struct va_surface *surf;
    struct va_image *vi = NULL;
    struct va_buffer *buf = NULL;
    VAStatus status = VA_STATUS_SUCCESS;
    UBYTE *addr = NULL;
    ULONG pitch = 0, offsets[2] = { 0, 0 };

    ObtainSemaphore(&drv->lock);
    surf = va_table_get(&drv->surfaces, surface);
    if (!surf)
        status = VA_STATUS_ERROR_INVALID_SURFACE;
    else if (surf->derived != VA_INVALID_ID)
        status = VA_STATUS_ERROR_SURFACE_BUSY;
    else
    {
        if (va_SyncLocked(surf, ~0) == vHidd_DecodeStatus_Error)
            D(bug("[VA] %s: surface %08x decoded with an error\n", __func__, surface));

        vi = calloc(1, sizeof(*vi));
        buf = calloc(1, sizeof(*buf));
        if (!vi || !buf)
            status = VA_STATUS_ERROR_ALLOCATION_FAILED;
        else if (!HIDD_BM_LockBuffer(surf->pool->bm, surf->index, &addr, &pitch, offsets))
            status = VA_STATUS_ERROR_OPERATION_FAILED;
    }

    if (status == VA_STATUS_SUCCESS)
    {
        va_ImageLayout(&vi->image, surf->pool->videofmt, surf->pool->width, surf->pool->height,
                       pitch, offsets[0]);
        vi->surface = surface;
        vi->locked  = addr;

        buf->type  = VAImageBufferType;
        buf->size  = vi->image.data_size;
        buf->num   = 1;
        buf->data  = addr;
        buf->owned = FALSE;

        vi->image.image_id = va_table_add(&drv->images, vi);
        vi->image.buf = (vi->image.image_id != VA_INVALID_ID) ? va_table_add(&drv->buffers, buf) : VA_INVALID_ID;
        if (vi->image.buf == VA_INVALID_ID)
        {
            if (vi->image.image_id != VA_INVALID_ID)
                va_table_remove(&drv->images, vi->image.image_id);
            HIDD_BM_UnlockBuffer(surf->pool->bm, surf->index, NULL);
            status = VA_STATUS_ERROR_ALLOCATION_FAILED;
        }
        else
        {
            buf->image = vi->image.image_id;
            surf->derived = vi->image.image_id;
            *image = vi->image;
            vi = NULL;
            buf = NULL;
        }
    }
    ReleaseSemaphore(&drv->lock);

    free(vi);
    free(buf);
    return status;
}

/* drv->lock held */
static void va_DestroyImageLocked(struct aros_va_driver *drv, VAImageID image_id)
{
    struct va_image *vi = va_table_get(&drv->images, image_id);
    struct va_buffer *buf;

    if (!vi)
        return;

    buf = va_table_get(&drv->buffers, vi->image.buf);
    if (vi->locked)
    {
        struct va_surface *surf = va_table_get(&drv->surfaces, vi->surface);

        if (surf)
        {
            /* Nothing written: an empty dirty rectangle, no upload */
            struct Rectangle none = { 0, 0, -1, -1 };

            HIDD_BM_UnlockBuffer(surf->pool->bm, surf->index, (buf && buf->written) ? NULL : &none);
            surf->derived = VA_INVALID_ID;
        }
    }
    if (buf)
        buf->image = VA_INVALID_ID;
    va_DestroyBufferLocked(drv, vi->image.buf);
    va_table_remove(&drv->images, image_id);
    free(vi);
}

static VAStatus aros_DestroyImage(VADriverContextP ctx, VAImageID image)
{
    struct aros_va_driver *drv = DRV(ctx);
    struct va_image *vi;

    ObtainSemaphore(&drv->lock);
    vi = va_table_get(&drv->images, image);
    if (vi)
        va_DestroyImageLocked(drv, image);
    ReleaseSemaphore(&drv->lock);

    return vi ? VA_STATUS_SUCCESS : VA_STATUS_ERROR_INVALID_IMAGE;
}

static VAStatus aros_SetImagePalette(VADriverContextP ctx, VAImageID image, unsigned char *palette)
{
    return VA_STATUS_ERROR_UNIMPLEMENTED;
}

/* Copy a rectangle between a locked surface buffer and an image (4:2:0, two planes) */
static void va_CopyPlanes(UBYTE *dst, const ULONG *dstpitch, const ULONG *dstoff, int dx, int dy,
                          const UBYTE *src, const ULONG *srcpitch, const ULONG *srcoff, int sx, int sy,
                          int width, int height, ULONG bpp)
{
    int row;
    ULONG bytes = width * bpp;

    for (row = 0; row < height; row++)
        memcpy(dst + dstoff[0] + (dy + row) * dstpitch[0] + dx * bpp,
               src + srcoff[0] + (sy + row) * srcpitch[0] + sx * bpp, bytes);

    /* Interleaved chroma: half the rows, same bytes per row (even origin) */
    bytes = ((width + 1) & ~1) * bpp;
    for (row = 0; row < (height + 1) / 2; row++)
        memcpy(dst + dstoff[1] + (dy / 2 + row) * dstpitch[1] + (dx & ~1) * bpp,
               src + srcoff[1] + (sy / 2 + row) * srcpitch[1] + (sx & ~1) * bpp, bytes);
}

static VAStatus aros_GetImage(VADriverContextP ctx, VASurfaceID surface, int x, int y,
                              unsigned int width, unsigned int height, VAImageID image)
{
    struct aros_va_driver *drv = DRV(ctx);
    struct va_surface *surf;
    struct va_image *vi;
    struct va_buffer *buf;
    VAStatus status = VA_STATUS_SUCCESS;

    ObtainSemaphore(&drv->lock);
    surf = va_table_get(&drv->surfaces, surface);
    vi = va_table_get(&drv->images, image);
    buf = vi ? va_table_get(&drv->buffers, vi->image.buf) : NULL;
    if (!surf)
        status = VA_STATUS_ERROR_INVALID_SURFACE;
    else if (!vi || !buf || vi->locked)
        status = VA_STATUS_ERROR_INVALID_IMAGE;
    else if (vi->image.format.fourcc != (surf->pool->videofmt == vHidd_VideoFmt_P010 ? VA_FOURCC_P010 : VA_FOURCC_NV12))
        status = VA_STATUS_ERROR_INVALID_IMAGE_FORMAT;
    else if (x < 0 || y < 0 || x + width > surf->pool->width || y + height > surf->pool->height ||
             width > vi->image.width || height > vi->image.height)
        status = VA_STATUS_ERROR_INVALID_PARAMETER;
    else if (surf->derived != VA_INVALID_ID)
        status = VA_STATUS_ERROR_SURFACE_BUSY;
    else
    {
        UBYTE *addr;
        ULONG pitch, offsets[2] = { 0, 0 };

        va_SyncLocked(surf, ~0);
        if (HIDD_BM_LockBuffer(surf->pool->bm, surf->index, &addr, &pitch, offsets))
        {
            struct Rectangle none = { 0, 0, -1, -1 };
            ULONG spitch[2] = { pitch, pitch };
            ULONG soff[2] = { 0, offsets[0] };

            va_CopyPlanes(buf->data, vi->image.pitches, vi->image.offsets, 0, 0,
                          addr, spitch, soff, x, y, width, height,
                          (surf->pool->videofmt == vHidd_VideoFmt_P010) ? 2 : 1);
            HIDD_BM_UnlockBuffer(surf->pool->bm, surf->index, &none);
        }
        else
            status = VA_STATUS_ERROR_OPERATION_FAILED;
    }
    ReleaseSemaphore(&drv->lock);

    return status;
}

static VAStatus aros_PutImage(VADriverContextP ctx, VASurfaceID surface, VAImageID image,
                              int src_x, int src_y, unsigned int src_width, unsigned int src_height,
                              int dest_x, int dest_y, unsigned int dest_width, unsigned int dest_height)
{
    struct aros_va_driver *drv = DRV(ctx);
    struct va_surface *surf;
    struct va_image *vi;
    struct va_buffer *buf;
    VAStatus status = VA_STATUS_SUCCESS;

    if (src_width != dest_width || src_height != dest_height)
        return VA_STATUS_ERROR_UNIMPLEMENTED;   /* no scaling here */

    ObtainSemaphore(&drv->lock);
    surf = va_table_get(&drv->surfaces, surface);
    vi = va_table_get(&drv->images, image);
    buf = vi ? va_table_get(&drv->buffers, vi->image.buf) : NULL;
    if (!surf)
        status = VA_STATUS_ERROR_INVALID_SURFACE;
    else if (!vi || !buf || vi->locked)
        status = VA_STATUS_ERROR_INVALID_IMAGE;
    else if (vi->image.format.fourcc != (surf->pool->videofmt == vHidd_VideoFmt_P010 ? VA_FOURCC_P010 : VA_FOURCC_NV12))
        status = VA_STATUS_ERROR_INVALID_IMAGE_FORMAT;
    else if (src_x < 0 || src_y < 0 || dest_x < 0 || dest_y < 0 ||
             src_x + src_width > vi->image.width || src_y + src_height > vi->image.height ||
             dest_x + dest_width > surf->pool->width || dest_y + dest_height > surf->pool->height)
        status = VA_STATUS_ERROR_INVALID_PARAMETER;
    else if (surf->derived != VA_INVALID_ID)
        status = VA_STATUS_ERROR_SURFACE_BUSY;
    else
    {
        UBYTE *addr;
        ULONG pitch, offsets[2] = { 0, 0 };

        va_SyncLocked(surf, ~0);
        if (HIDD_BM_LockBuffer(surf->pool->bm, surf->index, &addr, &pitch, offsets))
        {
            struct Rectangle dirty = { dest_x, dest_y, dest_x + dest_width - 1, dest_y + dest_height - 1 };
            ULONG dpitch[2] = { pitch, pitch };
            ULONG doff[2] = { 0, offsets[0] };

            va_CopyPlanes(addr, dpitch, doff, dest_x, dest_y,
                          buf->data, vi->image.pitches, vi->image.offsets, src_x, src_y,
                          src_width, src_height, (surf->pool->videofmt == vHidd_VideoFmt_P010) ? 2 : 1);
            HIDD_BM_UnlockBuffer(surf->pool->bm, surf->index, &dirty);
        }
        else
            status = VA_STATUS_ERROR_OPERATION_FAILED;
    }
    ReleaseSemaphore(&drv->lock);

    return status;
}

/*****************************************************************************/

/* No subpictures and no display attributes */

static VAStatus aros_QuerySubpictureFormats(VADriverContextP ctx, VAImageFormat *format_list,
                                            unsigned int *flags, unsigned int *num_formats)
{
    *num_formats = 0;
    return VA_STATUS_SUCCESS;
}

static VAStatus aros_CreateSubpicture(VADriverContextP ctx, VAImageID image, VASubpictureID *subpicture)
{
    return VA_STATUS_ERROR_UNIMPLEMENTED;
}

static VAStatus aros_DestroySubpicture(VADriverContextP ctx, VASubpictureID subpicture)
{
    return VA_STATUS_ERROR_INVALID_SUBPICTURE;
}

static VAStatus aros_SetSubpictureImage(VADriverContextP ctx, VASubpictureID subpicture, VAImageID image)
{
    return VA_STATUS_ERROR_INVALID_SUBPICTURE;
}

static VAStatus aros_SetSubpictureChromakey(VADriverContextP ctx, VASubpictureID subpicture,
                                            unsigned int chromakey_min, unsigned int chromakey_max,
                                            unsigned int chromakey_mask)
{
    return VA_STATUS_ERROR_INVALID_SUBPICTURE;
}

static VAStatus aros_SetSubpictureGlobalAlpha(VADriverContextP ctx, VASubpictureID subpicture, float global_alpha)
{
    return VA_STATUS_ERROR_INVALID_SUBPICTURE;
}

static VAStatus aros_AssociateSubpicture(VADriverContextP ctx, VASubpictureID subpicture,
                                         VASurfaceID *target_surfaces, int num_surfaces,
                                         short src_x, short src_y, unsigned short src_width,
                                         unsigned short src_height, short dest_x, short dest_y,
                                         unsigned short dest_width, unsigned short dest_height,
                                         unsigned int flags)
{
    return VA_STATUS_ERROR_INVALID_SUBPICTURE;
}

static VAStatus aros_DeassociateSubpicture(VADriverContextP ctx, VASubpictureID subpicture,
                                           VASurfaceID *target_surfaces, int num_surfaces)
{
    return VA_STATUS_ERROR_INVALID_SUBPICTURE;
}

static VAStatus aros_QueryDisplayAttributes(VADriverContextP ctx, VADisplayAttribute *attr_list, int *num_attributes)
{
    *num_attributes = 0;
    return VA_STATUS_SUCCESS;
}

static VAStatus aros_GetDisplayAttributes(VADriverContextP ctx, VADisplayAttribute *attr_list, int num_attributes)
{
    return VA_STATUS_ERROR_UNIMPLEMENTED;
}

static VAStatus aros_SetDisplayAttributes(VADriverContextP ctx, VADisplayAttribute *attr_list, int num_attributes)
{
    return VA_STATUS_ERROR_UNIMPLEMENTED;
}

/*****************************************************************************/

/*
 * Presenting: a decoded surface is a client surface of the window's
 * screen (CRF_PLANE_REQUIRED). Same bitmap as the one shown: SwapBuffers
 * to the surface's buffer; another bitmap: point the request at it.
 */

/* vp->lock held */
static void va_PushGeometry(struct va_present *vp)
{
    vp->req.Dest  = vp->track.Dest;
    vp->req.Shape = vp->track.Shape;

    if ((vp->req.Dest.MaxX - vp->req.Dest.MinX) != (vp->req.Src.MaxX - vp->req.Src.MinX) ||
        (vp->req.Dest.MaxY - vp->req.Dest.MinY) != (vp->req.Src.MaxY - vp->req.Src.MinY))
        vp->req.Flags |= CRF_SCALED;
    else
        vp->req.Flags &= ~CRF_SCALED;

    if (vp->attached)
        HIDD_Display_UpdateSurface(vp->drv->display, &vp->req);
}

/* The surface rectangle in layer coordinates */
static void va_PresentRect(struct va_present *vp, struct Rectangle *out)
{
    struct Rectangle inner;

    st_WindowInnerRect(vp->window, 0, 0, 0, 0, &inner);
    if (vp->fulldst)
    {
        *out = inner;
        return;
    }

    out->MinX = inner.MinX + vp->dst.MinX;
    out->MinY = inner.MinY + vp->dst.MinY;
    out->MaxX = inner.MinX + vp->dst.MaxX;
    out->MaxY = inner.MinY + vp->dst.MaxY;
    if (out->MaxX > inner.MaxX)
        out->MaxX = inner.MaxX;
    if (out->MaxY > inner.MaxY)
        out->MaxY = inner.MaxY;
    if (out->MaxX < out->MinX)
        out->MaxX = out->MinX;
    if (out->MaxY < out->MinY)
        out->MaxY = out->MinY;
}

AROS_UFH3(static IPTR, va_SurfaceHook,
    AROS_UFHA(struct Hook *, hook, A0),
    AROS_UFHA(OOP_Object *, display, A2),
    AROS_UFHA(struct HIDD_SurfaceHookMsg *, msg, A1))
{
    AROS_USERFUNC_INIT

    struct va_present *vp = hook->h_Data;
    struct Rectangle rect;

    if (!msg)
        return 0;

    switch (msg->Type)
    {
    case vHidd_SurfaceHook_PlaneGained:
        vp->onplane = TRUE;
        return 0;

    case vHidd_SurfaceHook_PlaneLost:
        vp->onplane = FALSE;
        return 0;

    case vHidd_SurfaceHook_Damage:
        break;

    default:
        return 0;
    }

    if (!AttemptSemaphore(&vp->lock))
    {
        st_SetStale(&vp->track);
        return 0;
    }

    if (vp->attached && vp->window)
    {
        va_PresentRect(vp, &rect);
        if (st_HookDamageRect(&vp->track, msg, &rect))
        {
            st_Changed(&vp->track);
            va_PushGeometry(vp);
        }
    }

    ReleaseSemaphore(&vp->lock);
    return 0;

    AROS_USERFUNC_EXIT
}

/* drv->lock held */
static void va_PresentEnd(struct aros_va_driver *drv, struct va_present *vp)
{
    ObtainSemaphore(&vp->lock);
    if (vp->attached)
        HIDD_Display_DetachSurface(drv->display, &vp->req);
    vp->attached = FALSE;
    ReleaseSemaphore(&vp->lock);

    Remove((struct Node *)&vp->node);
    st_Cleanup(&vp->track);
    free(vp);
}

static VAStatus aros_PutSurface(VADriverContextP ctx, VASurfaceID surface, void *draw,
                                short srcx, short srcy, unsigned short srcw, unsigned short srch,
                                short destx, short desty, unsigned short destw, unsigned short desth,
                                VARectangle *cliprects, unsigned int number_cliprects, unsigned int flags)
{
    struct aros_va_driver *drv = DRV(ctx);
    struct Window *window = draw;
    struct va_surface *surf;
    struct va_present *vp, *found = NULL;
    VAStatus status = VA_STATUS_SUCCESS;

    if (!window || window->WScreen != drv->screen)
        return VA_STATUS_ERROR_INVALID_PARAMETER;

    ObtainSemaphore(&drv->lock);

    surf = va_table_get(&drv->surfaces, surface);
    if (!surf)
    {
        ReleaseSemaphore(&drv->lock);
        return VA_STATUS_ERROR_INVALID_SURFACE;
    }

    ForeachNode(&drv->presents, vp)
    {
        if (vp->window == window)
        {
            found = vp;
            break;
        }
    }

    vp = found;
    if (!vp)
    {
        vp = calloc(1, sizeof(*vp));
        if (!vp || !st_Init(&vp->track, &drv->stctx))
        {
            free(vp);
            ReleaseSemaphore(&drv->lock);
            return VA_STATUS_ERROR_ALLOCATION_FAILED;
        }
        InitSemaphore(&vp->lock);
        vp->drv    = drv;
        vp->window = window;
        vp->hook.h_Entry = (HOOKFUNC)AROS_ASMSYMNAME(va_SurfaceHook);
        vp->hook.h_Data  = vp;
        AddTail((struct List *)&drv->presents, (struct Node *)&vp->node);
    }

    ObtainSemaphore(&vp->lock);

    /* Where in the window */
    {
        struct Rectangle dst;
        BOOL full = (destw == 0 || desth == 0);

        if (!full)
        {
            dst.MinX = destx;
            dst.MinY = desty;
            dst.MaxX = destx + destw - 1;
            dst.MaxY = desty + desth - 1;
        }
        if (full != vp->fulldst || (!full && memcmp(&dst, &vp->dst, sizeof(dst))))
        {
            vp->fulldst = full;
            if (!full)
                vp->dst = dst;
            st_SetStale(&vp->track);
        }
    }

    /* The decode must be complete before the plane scans it out */
    va_SyncLocked(surf, ~0);

    vp->req.Src.MinX = (srcw && srch) ? srcx : 0;
    vp->req.Src.MinY = (srcw && srch) ? srcy : 0;
    vp->req.Src.MaxX = (srcw && srch) ? srcx + srcw - 1 : surf->pool->width - 1;
    vp->req.Src.MaxY = (srcw && srch) ? srcy + srch - 1 : surf->pool->height - 1;

    if (!vp->attached)
    {
        struct Rectangle rect, dest;
        struct Region *shape = NULL;

        HIDD_BM_SwapBuffers(surf->pool->bm, surf->index, fHidd_Swap_NoWait);

        va_PresentRect(vp, &rect);
        if (!st_Compute(&vp->track, window, &rect, &dest, &shape, TRUE))
            status = VA_STATUS_ERROR_OPERATION_FAILED;
        else
        {
            st_Changed(&vp->track);
            vp->req.Next      = NULL;
            vp->req.BitMap    = surf->pool->bm;
            vp->req.Dest      = dest;
            vp->req.Shape     = shape;
            vp->req.Opacity   = 255;
            vp->req.AlphaMode = vHidd_CompAlpha_None;
            vp->req.Flags     = CRF_PLANE_REQUIRED;
            vp->req.ZOrder    = 0;
            vp->req.UserData  = NULL;
            if ((dest.MaxX - dest.MinX) != (vp->req.Src.MaxX - vp->req.Src.MinX) ||
                (dest.MaxY - dest.MinY) != (vp->req.Src.MaxY - vp->req.Src.MinY))
                vp->req.Flags |= CRF_SCALED;

            vp->onplane = FALSE;
            if (HIDD_Display_AttachSurface(drv->display, drv->screenbm, &vp->req, &vp->hook))
                vp->attached = TRUE;
            else
            {
                st_Reset(&vp->track);
                vp->req.Shape = NULL;
                status = VA_STATUS_ERROR_OPERATION_FAILED;
            }
        }
    }
    else
    {
        struct Rectangle rect;

        if (vp->req.BitMap == surf->pool->bm)
            HIDD_BM_SwapBuffers(surf->pool->bm, surf->index, 0);
        else
        {
            /* Front buffer first, so the new bitmap shows this frame from the start */
            HIDD_BM_SwapBuffers(surf->pool->bm, surf->index, fHidd_Swap_NoWait);
            vp->req.BitMap = surf->pool->bm;
            st_SetStale(&vp->track);
        }

        if (st_IsStale(&vp->track))
        {
            va_PresentRect(vp, &rect);
            if (st_Compute(&vp->track, window, &rect, NULL, NULL, TRUE))
                st_Changed(&vp->track);
            va_PushGeometry(vp);
        }
    }

    if (status == VA_STATUS_SUCCESS && !vp->onplane)
        status = VA_STATUS_ERROR_OPERATION_FAILED;

    ReleaseSemaphore(&vp->lock);
    ReleaseSemaphore(&drv->lock);

    return status;
}

/*****************************************************************************/

static VAStatus aros_Terminate(VADriverContextP ctx)
{
    struct aros_va_driver *drv = DRV(ctx);
    struct va_present *vp, *tmp;
    ULONG i;

    if (!drv)
        return VA_STATUS_SUCCESS;

    ObtainSemaphore(&drv->lock);

    ForeachNodeSafe(&drv->presents, vp, tmp)
        va_PresentEnd(drv, vp);

    for (i = 0; i < drv->images.size; i++)
        if (drv->images.slots[i])
            va_DestroyImageLocked(drv, ID_IMAGE | i);

    for (i = 0; i < drv->contexts.size; i++)
    {
        struct va_context *vc = drv->contexts.slots[i];
        if (vc)
        {
            HIDD_Display_FreeDecodeInterface(drv->display, vc->iface);
            free(vc);
        }
    }

    for (i = 0; i < drv->surfaces.size; i++)
    {
        struct va_surface *surf = drv->surfaces.slots[i];
        if (surf)
        {
            va_PoolRelease(drv, surf->pool);
            free(surf);
        }
    }

    for (i = 0; i < drv->buffers.size; i++)
    {
        struct va_buffer *buf = drv->buffers.slots[i];
        if (buf)
        {
            if (buf->owned)
                free(buf->data);
            free(buf);
        }
    }

    for (i = 0; i < drv->configs.size; i++)
        free(drv->configs.slots[i]);

    va_table_free(&drv->configs);
    va_table_free(&drv->contexts);
    va_table_free(&drv->surfaces);
    va_table_free(&drv->buffers);
    va_table_free(&drv->images);

    ReleaseSemaphore(&drv->lock);

    free(drv);
    ctx->pDriverData = NULL;
    return VA_STATUS_SUCCESS;
}

static BOOL va_aros_InitBases(void)
{
    if (!__IHidd_BitMap)
        __IHidd_BitMap = OOP_ObtainAttrBase(IID_Hidd_BitMap);
    if (!__IHidd_Display)
        __IHidd_Display = OOP_ObtainAttrBase(IID_Hidd_Display);
    if (!va_HiddBitMapBase)
        va_HiddBitMapBase = OOP_GetMethodID(IID_Hidd_BitMap, 0);
    if (!va_HiddDisplayBase)
        va_HiddDisplayBase = OOP_GetMethodID(IID_Hidd_Display, 0);

    return (__IHidd_BitMap && __IHidd_Display) ? TRUE : FALSE;
}

VAStatus __vaDriverInit_1_22(VADriverContextP ctx)
{
    struct VADriverVTable *vt = ctx->vtable;
    struct aros_va_driver *drv;
    struct Screen *screen = ctx->native_dpy;
    struct BitMap *bm;
    IPTR display = 0;

    if (ctx->display_type != VA_DISPLAY_AROS || !screen || !va_aros_InitBases())
        return VA_STATUS_ERROR_INVALID_DISPLAY;

    bm = screen->RastPort.BitMap;
    if (!bm || !IS_HIDD_BM(bm) || !HIDD_BM_OBJ(bm))
        return VA_STATUS_ERROR_INVALID_DISPLAY;
    OOP_GetAttr(HIDD_BM_OBJ(bm), aHidd_BitMap_Display, &display);
    if (!display)
        return VA_STATUS_ERROR_INVALID_DISPLAY;

    drv = calloc(1, sizeof(*drv));
    if (!drv)
        return VA_STATUS_ERROR_ALLOCATION_FAILED;

    InitSemaphore(&drv->lock);
    NEWLIST((struct List *)&drv->presents);
    drv->screen   = screen;
    drv->screenbm = HIDD_BM_OBJ(bm);
    drv->display  = (OOP_Object *)display;
    drv->bmclass  = OOP_FindClass(CLID_Hidd_BitMap);
    drv->configs.base  = ID_CONFIG;
    drv->contexts.base = ID_CONTEXT;
    drv->surfaces.base = ID_SURFACE;
    drv->buffers.base  = ID_BUFFER;
    drv->images.base   = ID_IMAGE;

    ctx->pDriverData            = drv;
    ctx->version_major          = VA_MAJOR_VERSION;
    ctx->version_minor          = VA_MINOR_VERSION;
    ctx->max_profiles           = MAX_PROFILES;
    ctx->max_entrypoints        = MAX_ENTRYPOINTS;
    ctx->max_attributes         = MAX_ATTRIBUTES;
    ctx->max_image_formats      = MAX_IMAGE_FORMATS;
    ctx->max_subpic_formats     = MAX_SUBPIC_FORMATS;
    ctx->max_display_attributes = MAX_DISPLAY_ATTRIBS;
    ctx->str_vendor             = VA_AROS_VENDOR;

    vt->vaTerminate                 = aros_Terminate;
    vt->vaQueryConfigProfiles       = aros_QueryConfigProfiles;
    vt->vaQueryConfigEntrypoints    = aros_QueryConfigEntrypoints;
    vt->vaGetConfigAttributes       = aros_GetConfigAttributes;
    vt->vaCreateConfig              = aros_CreateConfig;
    vt->vaDestroyConfig             = aros_DestroyConfig;
    vt->vaQueryConfigAttributes     = aros_QueryConfigAttributes;
    vt->vaCreateSurfaces            = aros_CreateSurfaces;
    vt->vaDestroySurfaces           = aros_DestroySurfaces;
    vt->vaCreateContext             = aros_CreateContext;
    vt->vaDestroyContext            = aros_DestroyContext;
    vt->vaCreateBuffer              = aros_CreateBuffer;
    vt->vaBufferSetNumElements      = aros_BufferSetNumElements;
    vt->vaMapBuffer                 = aros_MapBuffer;
    vt->vaUnmapBuffer               = aros_UnmapBuffer;
    vt->vaDestroyBuffer             = aros_DestroyBuffer;
    vt->vaBeginPicture              = aros_BeginPicture;
    vt->vaRenderPicture             = aros_RenderPicture;
    vt->vaEndPicture                = aros_EndPicture;
    vt->vaSyncSurface               = aros_SyncSurface;
    vt->vaQuerySurfaceStatus        = aros_QuerySurfaceStatus;
    vt->vaQuerySurfaceError         = aros_QuerySurfaceError;
    vt->vaPutSurface                = aros_PutSurface;
    vt->vaQueryImageFormats         = aros_QueryImageFormats;
    vt->vaCreateImage               = aros_CreateImage;
    vt->vaDeriveImage               = aros_DeriveImage;
    vt->vaDestroyImage              = aros_DestroyImage;
    vt->vaSetImagePalette           = aros_SetImagePalette;
    vt->vaGetImage                  = aros_GetImage;
    vt->vaPutImage                  = aros_PutImage;
    vt->vaQuerySubpictureFormats    = aros_QuerySubpictureFormats;
    vt->vaCreateSubpicture          = aros_CreateSubpicture;
    vt->vaDestroySubpicture         = aros_DestroySubpicture;
    vt->vaSetSubpictureImage        = aros_SetSubpictureImage;
    vt->vaSetSubpictureChromakey    = aros_SetSubpictureChromakey;
    vt->vaSetSubpictureGlobalAlpha  = aros_SetSubpictureGlobalAlpha;
    vt->vaAssociateSubpicture       = aros_AssociateSubpicture;
    vt->vaDeassociateSubpicture     = aros_DeassociateSubpicture;
    vt->vaQueryDisplayAttributes    = aros_QueryDisplayAttributes;
    vt->vaGetDisplayAttributes      = aros_GetDisplayAttributes;
    vt->vaSetDisplayAttributes      = aros_SetDisplayAttributes;
    vt->vaBufferInfo                = aros_BufferInfo;
    vt->vaGetSurfaceAttributes      = aros_GetSurfaceAttributes;
    vt->vaCreateSurfaces2           = aros_CreateSurfaces2;
    vt->vaQuerySurfaceAttributes    = aros_QuerySurfaceAttributes;
    vt->vaSyncSurface2              = aros_SyncSurface2;
    vt->vaMapBuffer2                = aros_MapBuffer2;

    return VA_STATUS_SUCCESS;
}

/*****************************************************************************/

static int va_DisplayContextIsValid(VADisplayContextP pDisplayContext)
{
    return pDisplayContext && pDisplayContext->pDriverContext &&
           pDisplayContext->pDriverContext->native_dpy;
}

static void va_DisplayContextDestroy(VADisplayContextP pDisplayContext)
{
    if (!pDisplayContext)
        return;
    /* opaque: the screen vaGetDisplayAROSPubScreen() locked */
    if (pDisplayContext->opaque)
        UnlockPubScreen(NULL, pDisplayContext->opaque);
    free(pDisplayContext->pDriverContext);
    free(pDisplayContext);
}

static VAStatus va_DisplayContextGetDriverNames(VADisplayContextP pDisplayContext, char **drivers,
                                                unsigned *num_drivers)
{
    if (!*num_drivers)
        return VA_STATUS_ERROR_UNKNOWN;
    drivers[0] = strdup("aros");
    *num_drivers = 1;
    return VA_STATUS_SUCCESS;
}

VADisplay vaGetDisplayAROS(struct Screen *screen)
{
    VADisplayContextP pDisplayContext;
    VADriverContextP pDriverContext;

    if (!screen)
        return NULL;

    pDisplayContext = va_newDisplayContext();
    if (!pDisplayContext)
        return NULL;

    pDisplayContext->vaIsValid        = va_DisplayContextIsValid;
    pDisplayContext->vaDestroy        = va_DisplayContextDestroy;
    pDisplayContext->vaGetDriverNames = va_DisplayContextGetDriverNames;

    pDriverContext = va_newDriverContext(pDisplayContext);
    if (!pDriverContext)
    {
        free(pDisplayContext);
        return NULL;
    }

    pDriverContext->native_dpy   = screen;
    pDriverContext->display_type = VA_DISPLAY_AROS;

    return (VADisplay)pDisplayContext;
}

VADisplay vaGetDisplayAROSPubScreen(const char *name)
{
    struct Screen *screen = LockPubScreen((CONST_STRPTR)name);
    VADisplay dpy;

    if (!screen)
        return NULL;

    dpy = vaGetDisplayAROS(screen);
    if (!dpy)
    {
        UnlockPubScreen(NULL, screen);
        return NULL;
    }
    ((VADisplayContextP)dpy)->opaque = screen;
    return dpy;
}

VAStatus vaPutSurfaceAROS(VADisplay dpy, VASurfaceID surface, struct Window *window,
                          const VARectangle *src, const VARectangle *dst, unsigned int flags)
{
    VADriverContextP ctx;

    if (!vaDisplayIsValid(dpy))
        return VA_STATUS_ERROR_INVALID_DISPLAY;
    ctx = CTX(dpy);
    if (!ctx->vtable || ctx->vtable->vaPutSurface != aros_PutSurface)
        return VA_STATUS_ERROR_INVALID_DISPLAY;

    return aros_PutSurface(ctx, surface, window,
                           src ? src->x : 0, src ? src->y : 0,
                           src ? src->width : 0, src ? src->height : 0,
                           dst ? dst->x : 0, dst ? dst->y : 0,
                           dst ? dst->width : 0, dst ? dst->height : 0,
                           NULL, 0, flags);
}

VAStatus vaReleaseWindowAROS(VADisplay dpy, struct Window *window)
{
    VADriverContextP ctx;
    struct aros_va_driver *drv;
    struct va_present *vp, *tmp;

    if (!vaDisplayIsValid(dpy))
        return VA_STATUS_ERROR_INVALID_DISPLAY;
    ctx = CTX(dpy);
    if (!ctx->vtable || ctx->vtable->vaPutSurface != aros_PutSurface)
        return VA_STATUS_ERROR_INVALID_DISPLAY;
    drv = DRV(ctx);

    ObtainSemaphore(&drv->lock);
    ForeachNodeSafe(&drv->presents, vp, tmp)
    {
        if (vp->window == window)
            va_PresentEnd(drv, vp);
    }
    ReleaseSemaphore(&drv->lock);

    return VA_STATUS_SUCCESS;
}
