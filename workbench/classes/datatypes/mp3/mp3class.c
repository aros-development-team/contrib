/*
    Copyright (C) 2025-2026, The AROS Development Team. All rights reserved.

    mp3.datatype - MPEG Layer III audio datatype for AROS.

    Modelled on the 8svx and wav datatypes; decodes the entire MP3 stream
    to 16-bit signed native-endian PCM in OM_NEW using libmad, then hands
    the resulting buffer to the parent sound.datatype via the standard
    SDTA_Sample attributes.

    Also parses ID3v2 (2.2, 2.3, 2.4) and ID3v1 / ID3v1.1 tags and
    exposes title, artist, album, year, copyright and comment via the
    standard DTA_ObjName / ObjAuthor / ObjAnnotation / ObjCopyright /
    ObjVersion datatypes attributes.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dostags.h>
#include <intuition/imageclass.h>
#include <intuition/icclass.h>
#include <intuition/gadgetclass.h>
#include <intuition/cghooks.h>
#include <datatypes/datatypesclass.h>
#include <datatypes/soundclass.h>
#include <datatypes/soundclassext.h>

#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/utility.h>
#include <proto/datatypes.h>

#include <aros/symbolsets.h>

#include <mad.h>

#include "debug.h"
#include "mp3class.h"

/* Open superclass */
ADD2LIBS("datatypes/sound.datatype", 0, struct Library *, SoundBase);

/**************************************************************************/

#define MP3_INPUT_BUFFER_SIZE   16384
#define MP3_INITIAL_PCM_BYTES   (1 * 1024 * 1024)

#define MP3_STR_MAX             256
#define MP3_ANNOT_MAX           1024

/* Standard ID3v1 (and Winamp-extended) genre table.  Values outside
 * this table are reported as "Unknown".
 */
static CONST_STRPTR const id3_genres[] =
{
    "Blues", "Classic Rock", "Country", "Dance", "Disco", "Funk",
    "Grunge", "Hip-Hop", "Jazz", "Metal", "New Age", "Oldies",
    "Other", "Pop", "R&B", "Rap", "Reggae", "Rock",
    "Techno", "Industrial", "Alternative", "Ska", "Death Metal",
    "Pranks", "Soundtrack", "Euro-Techno", "Ambient", "Trip-Hop",
    "Vocal", "Jazz+Funk", "Fusion", "Trance", "Classical",
    "Instrumental", "Acid", "House", "Game", "Sound Clip",
    "Gospel", "Noise", "Alt. Rock", "Bass", "Soul", "Punk",
    "Space", "Meditative", "Instrumental Pop", "Instrumental Rock",
    "Ethnic", "Gothic", "Darkwave", "Techno-Industrial", "Electronic",
    "Pop-Folk", "Eurodance", "Dream", "Southern Rock", "Comedy",
    "Cult", "Gangsta", "Top 40", "Christian Rap", "Pop/Funk",
    "Jungle", "Native American", "Cabaret", "New Wave",
    "Psychedelic", "Rave", "Showtunes", "Trailer", "Lo-Fi",
    "Tribal", "Acid Punk", "Acid Jazz", "Polka", "Retro",
    "Musical", "Rock & Roll", "Hard Rock"
};
#define NUM_ID3_GENRES  ((sizeof(id3_genres) / sizeof(id3_genres[0])))

/**************************************************************************/

struct MP3DecodeCtx
{
    BPTR     mp3_File;
    UBYTE   *mp3_InBuf;
    LONG     mp3_InBufFill;
    BOOL     mp3_EOF;

    /* Growing destination buffer of native-endian signed 16-bit PCM. */
    BYTE    *mp3_PCM;
    ULONG    mp3_PCMCapacity;
    ULONG    mp3_PCMUsed;

    ULONG    mp3_SampleRate;
    UWORD    mp3_Channels;
    BOOL     mp3_Error;

    /* Parsed ID3 metadata.  NULL when absent; otherwise points into
     * the per-object string pool.
     */
    STRPTR   mp3_Title;
    STRPTR   mp3_Artist;
    STRPTR   mp3_Album;
    STRPTR   mp3_Year;
    STRPTR   mp3_Genre;
    STRPTR   mp3_Track;
    STRPTR   mp3_Comment;
    STRPTR   mp3_Copyright;
};

/**************************************************************************/
/* libmad sample scale & callbacks                                        */
/**************************************************************************/

static inline signed int mp3_scale(mad_fixed_t sample)
{
    sample += (1L << (MAD_F_FRACBITS - 16));

    if (sample >= MAD_F_ONE)
        sample = MAD_F_ONE - 1;
    else if (sample < -MAD_F_ONE)
        sample = -MAD_F_ONE;

    return sample >> (MAD_F_FRACBITS + 1 - 16);
}

static BOOL mp3_EnsureCapacity(struct MP3DecodeCtx *ctx, ULONG needed)
{
    ULONG newcap;
    BYTE *newbuf;

    if (needed <= ctx->mp3_PCMCapacity)
        return TRUE;

    newcap = ctx->mp3_PCMCapacity ? ctx->mp3_PCMCapacity : MP3_INITIAL_PCM_BYTES;
    while (newcap < needed)
    {
        ULONG doubled = newcap << 1;
        if (doubled <= newcap)
            return FALSE;
        newcap = doubled;
    }

    newbuf = AllocVec(newcap, MEMF_PUBLIC);
    if (!newbuf)
        return FALSE;

    if (ctx->mp3_PCM)
    {
        CopyMem(ctx->mp3_PCM, newbuf, ctx->mp3_PCMUsed);
        FreeVec(ctx->mp3_PCM);
    }
    ctx->mp3_PCM = newbuf;
    ctx->mp3_PCMCapacity = newcap;
    return TRUE;
}

static enum mad_flow mp3_Input(void *data, struct mad_stream *stream)
{
    struct MP3DecodeCtx *ctx = (struct MP3DecodeCtx *)data;
    LONG     remaining = 0;
    LONG     readlen;

    if (ctx->mp3_EOF)
        return MAD_FLOW_STOP;

    if (stream->next_frame)
    {
        remaining = stream->bufend - stream->next_frame;
        memmove(ctx->mp3_InBuf, stream->next_frame, remaining);
    }

    readlen = Read(ctx->mp3_File,
                   ctx->mp3_InBuf + remaining,
                   MP3_INPUT_BUFFER_SIZE - remaining);
    if (readlen < 0)
    {
        D(bug("[mp3.dt] Input: read error\n"));
        ctx->mp3_Error = TRUE;
        return MAD_FLOW_STOP;
    }
    if (readlen == 0)
    {
        ctx->mp3_EOF = TRUE;
        if (remaining == 0)
            return MAD_FLOW_STOP;
        if (remaining + MAD_BUFFER_GUARD <= MP3_INPUT_BUFFER_SIZE)
        {
            memset(ctx->mp3_InBuf + remaining, 0, MAD_BUFFER_GUARD);
            readlen = MAD_BUFFER_GUARD;
        }
    }

    ctx->mp3_InBufFill = remaining + readlen;
    mad_stream_buffer(stream, ctx->mp3_InBuf, ctx->mp3_InBufFill);

    return MAD_FLOW_CONTINUE;
}

static enum mad_flow mp3_Output(void *data, struct mad_header const *header,
                                struct mad_pcm *pcm)
{
    struct MP3DecodeCtx *ctx = (struct MP3DecodeCtx *)data;
    unsigned int nchannels = pcm->channels;
    unsigned int nsamples  = pcm->length;
    mad_fixed_t const *left_ch  = pcm->samples[0];
    mad_fixed_t const *right_ch = pcm->samples[1];
    ULONG bytes_needed;
    WORD *out;

    if (ctx->mp3_SampleRate == 0)
    {
        ctx->mp3_SampleRate = pcm->samplerate;
        ctx->mp3_Channels   = nchannels;
        D(bug("[mp3.dt] Output: rate %lu, channels %u\n",
              ctx->mp3_SampleRate, ctx->mp3_Channels));
    }

    bytes_needed = ctx->mp3_PCMUsed
                 + (ULONG)nsamples * ctx->mp3_Channels * sizeof(WORD);

    if (!mp3_EnsureCapacity(ctx, bytes_needed))
    {
        D(bug("[mp3.dt] Output: out of memory\n"));
        ctx->mp3_Error = TRUE;
        return MAD_FLOW_STOP;
    }

    out = (WORD *)(ctx->mp3_PCM + ctx->mp3_PCMUsed);

    if (ctx->mp3_Channels == 2)
    {
        while (nsamples--)
        {
            signed int l = mp3_scale(*left_ch++);
            signed int r;
            if (nchannels == 2)
                r = mp3_scale(*right_ch++);
            else
                r = l;
            *out++ = (WORD)l;
            *out++ = (WORD)r;
        }
    }
    else
    {
        while (nsamples--)
        {
            signed int l = mp3_scale(*left_ch++);
            if (nchannels == 2)
            {
                signed int r = mp3_scale(*right_ch++);
                l = (l + r) >> 1;
            }
            *out++ = (WORD)l;
        }
    }

    ctx->mp3_PCMUsed = bytes_needed;
    return MAD_FLOW_CONTINUE;
}

static enum mad_flow mp3_Error(void *data, struct mad_stream *stream,
                               struct mad_frame *frame)
{
    struct MP3DecodeCtx *ctx = (struct MP3DecodeCtx *)data;

    D(bug("[mp3.dt] decode error 0x%04x (%s)\n",
          stream->error, mad_stream_errorstr(stream)));

    if (MAD_RECOVERABLE(stream->error))
        return MAD_FLOW_CONTINUE;

    ctx->mp3_Error = TRUE;
    return MAD_FLOW_BREAK;
}

/**************************************************************************/
/* ID3 tag parsing                                                        */
/**************************************************************************/

static STRPTR mp3_PoolDupBytes(APTR pool, const UBYTE *src, LONG len)
{
    STRPTR dst;
    if (!pool || !src || len <= 0)
        return NULL;
    dst = AllocPooled(pool, len + 1);
    if (!dst)
        return NULL;
    CopyMem((APTR)src, dst, len);
    dst[len] = '\0';
    return dst;
}

static void mp3_TrimRight(STRPTR s)
{
    LONG n;
    if (!s) return;
    n = strlen(s);
    while (n > 0 && ((UBYTE)s[n - 1] <= ' '))
        s[--n] = '\0';
}

/* Decode an ID3v2 text-frame body.  body[0] is the encoding byte
 * (0=ISO-8859-1, 1=UTF-16+BOM, 2=UTF-16BE, 3=UTF-8).  We keep 8-bit
 * data byte-for-byte and downsample UTF-16 by taking the low byte of
 * each codepoint (non-Latin chars become '?').
 */
static STRPTR mp3_DecodeText(APTR pool, const UBYTE *body, LONG len)
{
    UBYTE encoding;
    const UBYTE *text;
    LONG textlen;
    UBYTE tmp[MP3_STR_MAX];
    LONG outlen = 0;

    if (!body || len < 1) return NULL;

    encoding = body[0];
    text = body + 1;
    textlen = len - 1;

    if (textlen <= 0) return NULL;

    if (encoding == 0 || encoding == 3)
    {
        LONG i;
        for (i = 0; i < textlen && outlen < MP3_STR_MAX - 1; i++)
        {
            UBYTE c = text[i];
            if (c == 0) break;
            tmp[outlen++] = c;
        }
    }
    else if (encoding == 1 || encoding == 2)
    {
        BOOL bigendian = (encoding == 2);
        LONG start = 0;
        LONG i;

        if (encoding == 1 && textlen >= 2)
        {
            if (text[0] == 0xFF && text[1] == 0xFE) { bigendian = FALSE; start = 2; }
            else if (text[0] == 0xFE && text[1] == 0xFF) { bigendian = TRUE; start = 2; }
        }

        for (i = start; i + 1 < textlen && outlen < MP3_STR_MAX - 1; i += 2)
        {
            UWORD u;
            if (bigendian)
                u = ((UWORD)text[i] << 8) | text[i + 1];
            else
                u = ((UWORD)text[i + 1] << 8) | text[i];
            if (u == 0) break;
            tmp[outlen++] = (u <= 0xFF) ? (UBYTE)u : (UBYTE)'?';
        }
    }
    else
    {
        return NULL;
    }

    while (outlen > 0 && ((UBYTE)tmp[outlen - 1] <= ' '))
        outlen--;

    return mp3_PoolDupBytes(pool, tmp, outlen);
}

static STRPTR mp3_GenreFromV1(APTR pool, UBYTE g)
{
    CONST_STRPTR name;
    if (g >= NUM_ID3_GENRES) return NULL;
    name = id3_genres[g];
    return mp3_PoolDupBytes(pool, (const UBYTE *)name, strlen(name));
}

/* ID3v2 genre frame may be "Rock", "(17)", "(17)Rock" or "17". */
static STRPTR mp3_NormaliseGenre(APTR pool, CONST_STRPTR raw)
{
    if (!raw || !*raw) return NULL;

    if (raw[0] == '(')
    {
        LONG n = 0;
        const char *p = raw + 1;
        while (*p >= '0' && *p <= '9') { n = n * 10 + (*p - '0'); p++; }
        if (*p == ')')
        {
            p++;
            if (*p)
                return mp3_PoolDupBytes(pool, (const UBYTE *)p, strlen(p));
            return mp3_GenreFromV1(pool, (UBYTE)n);
        }
    }
    else if (raw[0] >= '0' && raw[0] <= '9')
    {
        LONG n = 0;
        const char *p = raw;
        while (*p >= '0' && *p <= '9') { n = n * 10 + (*p - '0'); p++; }
        if (*p == '\0')
            return mp3_GenreFromV1(pool, (UBYTE)n);
    }

    return mp3_PoolDupBytes(pool, (const UBYTE *)raw, strlen(raw));
}

/* Parse ID3v2 header+body, populate ctx fields, and return the total
 * tag size (header+payload) so the caller can position the decoder
 * past it.  Returns 0 when there's no tag we can parse.
 */
static ULONG mp3_ParseID3v2(struct MP3DecodeCtx *ctx, APTR pool, BPTR file)
{
    UBYTE   hdr[10];
    UBYTE  *body = NULL;
    ULONG   tagsize, pos;
    UBYTE   version, flags;
    LONG    framehdrsize;
    BOOL    syncsafe_sizes;

    Seek(file, 0, OFFSET_BEGINNING);
    if (Read(file, hdr, 10) != 10) goto fail;
    if (hdr[0] != 'I' || hdr[1] != 'D' || hdr[2] != '3') goto fail;
    if (hdr[3] == 0xFF || hdr[4] == 0xFF) goto fail;
    if ((hdr[6] | hdr[7] | hdr[8] | hdr[9]) & 0x80) goto fail;

    version = hdr[3];
    flags   = hdr[5];
    tagsize = ((ULONG)hdr[6] << 21)
            | ((ULONG)hdr[7] << 14)
            | ((ULONG)hdr[8] <<  7)
            | ((ULONG)hdr[9]);

    if (tagsize == 0 || tagsize > 16UL * 1024UL * 1024UL) goto fail;

    body = AllocVec(tagsize, MEMF_PUBLIC);
    if (!body) goto fail;
    if (Read(file, body, tagsize) != (LONG)tagsize) goto fail;

    pos = 0;
    if (flags & 0x40)
    {
        ULONG extsize;
        if (pos + 4 > tagsize) goto done;
        if (version >= 4)
            extsize = ((ULONG)body[pos] << 21) | ((ULONG)body[pos + 1] << 14)
                    | ((ULONG)body[pos + 2] << 7) | (ULONG)body[pos + 3];
        else
            extsize = ((ULONG)body[pos] << 24) | ((ULONG)body[pos + 1] << 16)
                    | ((ULONG)body[pos + 2] << 8) | (ULONG)body[pos + 3];
        pos += 4 + extsize;
        if (pos > tagsize) goto done;
    }

    framehdrsize   = (version == 2) ? 6 : 10;
    syncsafe_sizes = (version >= 4);

    while (pos + framehdrsize <= tagsize)
    {
        UBYTE *fh = body + pos;
        char fid[5];
        ULONG fsize;
        UBYTE *fbody;
        STRPTR *target = NULL;

        if (fh[0] == 0) break;

        if (version == 2)
        {
            fid[0] = fh[0]; fid[1] = fh[1]; fid[2] = fh[2];
            fid[3] = 0; fid[4] = 0;
            fsize = ((ULONG)fh[3] << 16) | ((ULONG)fh[4] << 8) | (ULONG)fh[5];
        }
        else
        {
            fid[0] = fh[0]; fid[1] = fh[1]; fid[2] = fh[2]; fid[3] = fh[3];
            fid[4] = 0;
            if (syncsafe_sizes
                && ((fh[4] | fh[5] | fh[6] | fh[7]) & 0x80) == 0)
            {
                fsize = ((ULONG)fh[4] << 21) | ((ULONG)fh[5] << 14)
                      | ((ULONG)fh[6] <<  7) | (ULONG)fh[7];
            }
            else
            {
                fsize = ((ULONG)fh[4] << 24) | ((ULONG)fh[5] << 16)
                      | ((ULONG)fh[6] <<  8) | (ULONG)fh[7];
            }
        }

        if (fsize == 0 || pos + framehdrsize + fsize > tagsize)
            break;

        fbody = body + pos + framehdrsize;

        if (!strcmp(fid, "TIT2") || !strcmp(fid, "TT2"))
            target = &ctx->mp3_Title;
        else if (!strcmp(fid, "TPE1") || !strcmp(fid, "TP1"))
            target = &ctx->mp3_Artist;
        else if (!strcmp(fid, "TALB") || !strcmp(fid, "TAL"))
            target = &ctx->mp3_Album;
        else if (!strcmp(fid, "TYER") || !strcmp(fid, "TDRC")
              || !strcmp(fid, "TYE"))
            target = &ctx->mp3_Year;
        else if (!strcmp(fid, "TRCK") || !strcmp(fid, "TRK"))
            target = &ctx->mp3_Track;
        else if (!strcmp(fid, "TCOP") || !strcmp(fid, "TCR"))
            target = &ctx->mp3_Copyright;
        else if (!strcmp(fid, "TCON") || !strcmp(fid, "TCO"))
        {
            STRPTR raw = mp3_DecodeText(pool, fbody, fsize);
            if (raw && !ctx->mp3_Genre)
                ctx->mp3_Genre = mp3_NormaliseGenre(pool, raw);
            pos += framehdrsize + fsize;
            continue;
        }
        else if (!strcmp(fid, "COMM") || !strcmp(fid, "COM"))
        {
            /* COMM body: enc(1) + lang(3) + descr (NUL-terminated) + text */
            if (fsize >= 5 && !ctx->mp3_Comment)
            {
                UBYTE enc = fbody[0];
                LONG  dstart = 4;
                LONG  i;
                BOOL  found = FALSE;

                if (enc == 1 || enc == 2)
                {
                    for (i = dstart; i + 1 < (LONG)fsize; i += 2)
                        if (fbody[i] == 0 && fbody[i + 1] == 0)
                        { dstart = i + 2; found = TRUE; break; }
                }
                else
                {
                    for (i = dstart; i < (LONG)fsize; i++)
                        if (fbody[i] == 0)
                        { dstart = i + 1; found = TRUE; break; }
                }

                if (found && dstart < (LONG)fsize)
                {
                    UBYTE tmp[MP3_STR_MAX];
                    LONG  rem = (LONG)fsize - dstart;
                    if (rem > MP3_STR_MAX - 1) rem = MP3_STR_MAX - 1;
                    tmp[0] = enc;
                    CopyMem(fbody + dstart, tmp + 1, rem);
                    ctx->mp3_Comment = mp3_DecodeText(pool, tmp, rem + 1);
                }
            }
            pos += framehdrsize + fsize;
            continue;
        }

        if (target && *target == NULL)
        {
            STRPTR decoded = mp3_DecodeText(pool, fbody, fsize);
            if (decoded) *target = decoded;
        }

        pos += framehdrsize + fsize;
    }

done:
    FreeVec(body);
    return 10 + tagsize;

fail:
    if (body) FreeVec(body);
    Seek(file, 0, OFFSET_BEGINNING);
    return 0;
}

/* Parse ID3v1 / ID3v1.1 from the last 128 bytes - only fills fields
 * still missing after ID3v2.  Leaves the file handle at end of file.
 */
static void mp3_ParseID3v1(struct MP3DecodeCtx *ctx, APTR pool, BPTR file)
{
    UBYTE tag[128];
    LONG  size;

    Seek(file, 0, OFFSET_END);
    size = Seek(file, 0, OFFSET_CURRENT);
    if (size < 128) return;

    if (Seek(file, -128, OFFSET_CURRENT) < 0) return;
    if (Read(file, tag, 128) != 128) return;
    if (tag[0] != 'T' || tag[1] != 'A' || tag[2] != 'G') return;

    if (!ctx->mp3_Title)
    {
        STRPTR s = mp3_PoolDupBytes(pool, tag + 3, 30);
        if (s) { mp3_TrimRight(s); if (*s) ctx->mp3_Title = s; }
    }
    if (!ctx->mp3_Artist)
    {
        STRPTR s = mp3_PoolDupBytes(pool, tag + 33, 30);
        if (s) { mp3_TrimRight(s); if (*s) ctx->mp3_Artist = s; }
    }
    if (!ctx->mp3_Album)
    {
        STRPTR s = mp3_PoolDupBytes(pool, tag + 63, 30);
        if (s) { mp3_TrimRight(s); if (*s) ctx->mp3_Album = s; }
    }
    if (!ctx->mp3_Year)
    {
        STRPTR s = mp3_PoolDupBytes(pool, tag + 93, 4);
        if (s) { mp3_TrimRight(s); if (*s) ctx->mp3_Year = s; }
    }
    if (!ctx->mp3_Comment)
    {
        LONG commlen = 30;
        /* ID3v1.1: if byte 125 == 0 and byte 126 != 0, the comment is
         * 28 bytes and byte 126 carries a track number. */
        if (tag[125] == 0 && tag[126] != 0)
        {
            commlen = 28;
            if (!ctx->mp3_Track)
            {
                char trk[8];
                snprintf(trk, sizeof(trk), "%u", (unsigned)tag[126]);
                ctx->mp3_Track = mp3_PoolDupBytes(pool,
                                                  (const UBYTE *)trk,
                                                  strlen(trk));
            }
        }
        {
            STRPTR s = mp3_PoolDupBytes(pool, tag + 97, commlen);
            if (s) { mp3_TrimRight(s); if (*s) ctx->mp3_Comment = s; }
        }
    }
    if (!ctx->mp3_Genre)
        ctx->mp3_Genre = mp3_GenreFromV1(pool, tag[127]);
}

static STRPTR mp3_BuildAnnotation(struct MP3DecodeCtx *ctx, APTR pool)
{
    char buf[MP3_ANNOT_MAX];
    LONG used = 0;
    BOOL any = FALSE;

    buf[0] = '\0';

    if (ctx->mp3_Album)
    {
        used += snprintf(buf + used, MP3_ANNOT_MAX - used,
                         "Album: %s", ctx->mp3_Album);
        any = TRUE;
    }
    if (ctx->mp3_Track && used < MP3_ANNOT_MAX - 1)
    {
        used += snprintf(buf + used, MP3_ANNOT_MAX - used,
                         "%sTrack: %s", any ? "\n" : "", ctx->mp3_Track);
        any = TRUE;
    }
    if (ctx->mp3_Genre && used < MP3_ANNOT_MAX - 1)
    {
        used += snprintf(buf + used, MP3_ANNOT_MAX - used,
                         "%sGenre: %s", any ? "\n" : "", ctx->mp3_Genre);
        any = TRUE;
    }
    if (ctx->mp3_Comment && used < MP3_ANNOT_MAX - 1)
    {
        used += snprintf(buf + used, MP3_ANNOT_MAX - used,
                         "%sComment: %s", any ? "\n" : "", ctx->mp3_Comment);
        any = TRUE;
    }

    if (!any) return NULL;
    if (used >= MP3_ANNOT_MAX) used = MP3_ANNOT_MAX - 1;
    return mp3_PoolDupBytes(pool, (const UBYTE *)buf, used);
}

/**************************************************************************/
/* OM_NEW                                                                  */
/**************************************************************************/

static BOOL ReadMP3(Class *cl, Object *o)
{
    struct MP3_Data    *id = INST_DATA(cl, o);
    struct MP3DecodeCtx ctx;
    struct mad_decoder  decoder;
    IPTR                sourcetype = 0;
    BPTR                handle = BNULL;
    LONG                err;
    UBYTE               sampletype;
    ULONG               framecount;
    ULONG               id3v2size;
    STRPTR              annotation;

    D(bug("mp3.datatype/ReadMP3()\n"));

    memset(&ctx, 0, sizeof(ctx));

    if (!id->mp3d_StringPool)
        id->mp3d_StringPool = CreatePool(MEMF_PUBLIC | MEMF_CLEAR, 1024, 256);

    if (GetDTAttrs(o, DTA_SourceType, (IPTR)&sourcetype,
                      DTA_Handle,     (IPTR)&handle,
                      TAG_DONE) != 2)
    {
        SetIoErr(ERROR_OBJECT_NOT_FOUND);
        return FALSE;
    }

    if ((sourcetype != DTST_FILE) && (sourcetype != DTST_CLIPBOARD))
    {
        SetIoErr(ERROR_NOT_IMPLEMENTED);
        return FALSE;
    }

    if (!handle)
    {
        SetIoErr(ERROR_OBJECT_NOT_FOUND);
        return FALSE;
    }

    ctx.mp3_File  = handle;
    ctx.mp3_InBuf = AllocVec(MP3_INPUT_BUFFER_SIZE, MEMF_PUBLIC);
    if (!ctx.mp3_InBuf)
    {
        SetIoErr(ERROR_NO_FREE_STORE);
        return FALSE;
    }

    id3v2size = mp3_ParseID3v2(&ctx, id->mp3d_StringPool, handle);

    mp3_ParseID3v1(&ctx, id->mp3d_StringPool, handle);

    /* Rewind past the ID3v2 tag for libmad. */
    Seek(handle, id3v2size, OFFSET_BEGINNING);

    mad_decoder_init(&decoder, &ctx,
                     mp3_Input, NULL /* header */, NULL /* filter */,
                     mp3_Output, mp3_Error, NULL /* message */);

    err = mad_decoder_run(&decoder, MAD_DECODER_MODE_SYNC);

    mad_decoder_finish(&decoder);

    FreeVec(ctx.mp3_InBuf);
    ctx.mp3_InBuf = NULL;

    if (ctx.mp3_Error || ctx.mp3_PCMUsed == 0 || ctx.mp3_SampleRate == 0)
    {
        D(bug("[mp3.dt] decode failed (err=%ld used=%lu rate=%lu)\n",
              err, ctx.mp3_PCMUsed, ctx.mp3_SampleRate));
        if (ctx.mp3_PCM) FreeVec(ctx.mp3_PCM);
        SetIoErr(ERROR_OBJECT_WRONG_TYPE);
        return FALSE;
    }

    sampletype = (ctx.mp3_Channels == 2) ? SDTST_S16S : SDTST_M16S;
    framecount = ctx.mp3_PCMUsed
               / (sizeof(WORD) * ctx.mp3_Channels);

    D(bug("[mp3.dt] decoded %lu frames, %lu Hz, %s 16-bit\n",
          framecount, ctx.mp3_SampleRate,
          (ctx.mp3_Channels == 2) ? "stereo" : "mono"));
    D(bug("[mp3.dt] title='%s' artist='%s' album='%s' year='%s'\n",
          ctx.mp3_Title  ? ctx.mp3_Title  : (STRPTR)"",
          ctx.mp3_Artist ? ctx.mp3_Artist : (STRPTR)"",
          ctx.mp3_Album  ? ctx.mp3_Album  : (STRPTR)"",
          ctx.mp3_Year   ? ctx.mp3_Year   : (STRPTR)""));

    annotation = mp3_BuildAnnotation(&ctx, id->mp3d_StringPool);

    SetDTAttrs(o, NULL, NULL,
               DTA_ObjName,        (IPTR)(ctx.mp3_Title ? ctx.mp3_Title
                                                        : (STRPTR)"Unknown"),
               DTA_ObjAuthor,      (IPTR)ctx.mp3_Artist,
               DTA_ObjAnnotation,  (IPTR)annotation,
               DTA_ObjCopyright,   (IPTR)ctx.mp3_Copyright,
               DTA_ObjVersion,     (IPTR)ctx.mp3_Year,
               SDTA_Sample,        (IPTR)ctx.mp3_PCM,
               SDTA_SampleLength,  framecount,
               SDTA_SampleType,    sampletype,
               SDTA_SamplesPerSec, ctx.mp3_SampleRate,
               SDTA_Period,        709379UL * 5 / ctx.mp3_SampleRate,
               SDTA_Volume,        64,
               SDTA_Cycles,        1,
               TAG_DONE);

    return TRUE;
}

/**************************************************************************/

IPTR MP3__OM_NEW(Class *cl, Object *o, struct opSet *msg)
{
    IPTR retval;

    retval = DoSuperMethodA(cl, o, (Msg)msg);
    if (retval)
    {
        if (!ReadMP3(cl, (Object *)retval))
        {
            CoerceMethod(cl, (Object *)retval, OM_DISPOSE);
            retval = 0;
        }
    }

    return retval;
}

/**************************************************************************/

IPTR MP3__OM_DISPOSE(Class *cl, Object *o, Msg msg)
{
    struct MP3_Data *id = INST_DATA(cl, o);

    if (id->mp3d_StringPool)
    {
        DeletePool(id->mp3d_StringPool);
        id->mp3d_StringPool = NULL;
    }

    return DoSuperMethodA(cl, o, msg);
}

/**************************************************************************/
