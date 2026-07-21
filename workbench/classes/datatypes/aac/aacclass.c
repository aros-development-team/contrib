/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    aac.datatype - MPEG-2/4 Advanced Audio Coding datatype for AROS.

    Modelled on the mp3 datatype; decodes the entire AAC stream (raw
    ADTS or ADIF, including HE-AAC SBR/PS) to 16-bit signed
    native-endian PCM in OM_NEW using libfaad (faad2), then hands the
    resulting buffer to the parent sound.datatype via the standard
    SDTA_Sample attributes.  Multichannel (5.1) streams are down-mixed
    to stereo by the decoder.

    AAC audio inside an MP4 (ISO-BMFF) container - .m4a/.m4b files,
    and .aac files as written e.g. by ffmpeg without '-f adts' - is
    demuxed with faad2's mp4ff and decoded the same way.

    Also parses ID3v2 (2.2, 2.3, 2.4) and ID3v1 / ID3v1.1 tags (or the
    iTunes-style metadata of MP4 files) and exposes title, artist,
    album, year, copyright and comment via the standard DTA_ObjName /
    ObjAuthor / ObjAnnotation / ObjCopyright / ObjVersion datatypes
    attributes.
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

#include <neaacdec.h>
#include <mp4ff.h>

#include "debug.h"
#include "aacclass.h"

/* Open superclass */
ADD2LIBS("datatypes/sound.datatype", 0, struct Library *, SoundBase);

/**************************************************************************/

/* The rolling input buffer must always be able to hold a whole coded
 * frame; an ADTS frame is at most 8191 bytes, and faad wants
 * FAAD_MIN_STREAMSIZE (768) bytes per channel available on every call.
 */
#define AAC_INPUT_BUFFER_SIZE   65536
#define AAC_REFILL_THRESHOLD    16384
#define AAC_INITIAL_PCM_BYTES   (1 * 1024 * 1024)

#define AAC_STR_MAX             256
#define AAC_ANNOT_MAX           1024

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

struct AACDecodeCtx
{
    BPTR     aac_File;
    UBYTE   *aac_InBuf;

    /* Coded bytes still to be read from the file (excludes leading
     * ID3v2 tag and trailing ID3v1 tag). */
    LONG     aac_StreamLeft;

    /* Growing destination buffer of native-endian signed 16-bit PCM. */
    BYTE    *aac_PCM;
    ULONG    aac_PCMCapacity;
    ULONG    aac_PCMUsed;

    ULONG    aac_SampleRate;
    UWORD    aac_Channels;
    BOOL     aac_Error;

    /* Parsed ID3 metadata.  NULL when absent; otherwise points into
     * the per-object string pool.
     */
    STRPTR   aac_Title;
    STRPTR   aac_Artist;
    STRPTR   aac_Album;
    STRPTR   aac_Year;
    STRPTR   aac_Genre;
    STRPTR   aac_Track;
    STRPTR   aac_Comment;
    STRPTR   aac_Copyright;
};

static BOOL aac_EnsureCapacity(struct AACDecodeCtx *ctx, ULONG needed)
{
    ULONG newcap;
    BYTE *newbuf;

    if (needed <= ctx->aac_PCMCapacity)
        return TRUE;

    newcap = ctx->aac_PCMCapacity ? ctx->aac_PCMCapacity : AAC_INITIAL_PCM_BYTES;
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

    if (ctx->aac_PCM)
    {
        CopyMem(ctx->aac_PCM, newbuf, ctx->aac_PCMUsed);
        FreeVec(ctx->aac_PCM);
    }
    ctx->aac_PCM = newbuf;
    ctx->aac_PCMCapacity = newcap;
    return TRUE;
}

/* Append one decoded frame's worth of PCM to the destination buffer,
 * capturing the output format from the first frame that carries audio.
 */
static BOOL aac_AppendFrame(struct AACDecodeCtx *ctx,
                            const NeAACDecFrameInfo *fi, const void *frame)
{
    ULONG bytes;

    if (fi->samples == 0 || !frame)
        return TRUE;

    bytes = (ULONG)fi->samples * sizeof(WORD);

    if (ctx->aac_SampleRate == 0)
    {
        ctx->aac_SampleRate = fi->samplerate;
        ctx->aac_Channels   = fi->channels;
        D(bug("[aac.dt] output: rate %lu, channels %u\n",
              ctx->aac_SampleRate, ctx->aac_Channels));
    }

    if (!aac_EnsureCapacity(ctx, ctx->aac_PCMUsed + bytes))
    {
        D(bug("[aac.dt] out of memory\n"));
        ctx->aac_Error = TRUE;
        return FALSE;
    }

    CopyMem((APTR)frame, ctx->aac_PCM + ctx->aac_PCMUsed, bytes);
    ctx->aac_PCMUsed += bytes;
    return TRUE;
}

/* Top up the input buffer.  'fill' bytes starting at 'pos' are still
 * unconsumed; they are moved to the buffer start and as much coded
 * stream data as fits (and remains) is appended.  Returns the new fill
 * level, or -1 on read error.
 */
static LONG aac_RefillInput(struct AACDecodeCtx *ctx, LONG pos, LONG fill)
{
    LONG remaining = fill - pos;
    LONG space, want, readlen;

    if (remaining > 0 && pos > 0)
        memmove(ctx->aac_InBuf, ctx->aac_InBuf + pos, remaining);
    if (remaining < 0)
        remaining = 0;

    space = AAC_INPUT_BUFFER_SIZE - remaining;
    want  = (ctx->aac_StreamLeft < space) ? ctx->aac_StreamLeft : space;

    if (want > 0)
    {
        readlen = Read(ctx->aac_File, ctx->aac_InBuf + remaining, want);
        if (readlen < 0)
            return -1;
        ctx->aac_StreamLeft -= readlen;
        if (readlen == 0)
            ctx->aac_StreamLeft = 0;
        remaining += readlen;
    }

    return remaining;
}

/**************************************************************************/
/* ID3 tag parsing                                                        */
/**************************************************************************/

static STRPTR aac_PoolDupBytes(APTR pool, const UBYTE *src, LONG len)
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

static void aac_TrimRight(STRPTR s)
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
static STRPTR aac_DecodeText(APTR pool, const UBYTE *body, LONG len)
{
    UBYTE encoding;
    const UBYTE *text;
    LONG textlen;
    UBYTE tmp[AAC_STR_MAX];
    LONG outlen = 0;

    if (!body || len < 1) return NULL;

    encoding = body[0];
    text = body + 1;
    textlen = len - 1;

    if (textlen <= 0) return NULL;

    if (encoding == 0 || encoding == 3)
    {
        LONG i;
        for (i = 0; i < textlen && outlen < AAC_STR_MAX - 1; i++)
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

        for (i = start; i + 1 < textlen && outlen < AAC_STR_MAX - 1; i += 2)
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

    return aac_PoolDupBytes(pool, tmp, outlen);
}

static STRPTR aac_GenreFromV1(APTR pool, UBYTE g)
{
    CONST_STRPTR name;
    if (g >= NUM_ID3_GENRES) return NULL;
    name = id3_genres[g];
    return aac_PoolDupBytes(pool, (const UBYTE *)name, strlen(name));
}

/* ID3v2 genre frame may be "Rock", "(17)", "(17)Rock" or "17". */
static STRPTR aac_NormaliseGenre(APTR pool, CONST_STRPTR raw)
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
                return aac_PoolDupBytes(pool, (const UBYTE *)p, strlen(p));
            return aac_GenreFromV1(pool, (UBYTE)n);
        }
    }
    else if (raw[0] >= '0' && raw[0] <= '9')
    {
        LONG n = 0;
        const char *p = raw;
        while (*p >= '0' && *p <= '9') { n = n * 10 + (*p - '0'); p++; }
        if (*p == '\0')
            return aac_GenreFromV1(pool, (UBYTE)n);
    }

    return aac_PoolDupBytes(pool, (const UBYTE *)raw, strlen(raw));
}

/* Parse ID3v2 header+body, populate ctx fields, and return the total
 * tag size (header+payload) so the caller can position the decoder
 * past it.  Returns 0 when there's no tag we can parse.
 */
static ULONG aac_ParseID3v2(struct AACDecodeCtx *ctx, APTR pool, BPTR file)
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
            target = &ctx->aac_Title;
        else if (!strcmp(fid, "TPE1") || !strcmp(fid, "TP1"))
            target = &ctx->aac_Artist;
        else if (!strcmp(fid, "TALB") || !strcmp(fid, "TAL"))
            target = &ctx->aac_Album;
        else if (!strcmp(fid, "TYER") || !strcmp(fid, "TDRC")
              || !strcmp(fid, "TYE"))
            target = &ctx->aac_Year;
        else if (!strcmp(fid, "TRCK") || !strcmp(fid, "TRK"))
            target = &ctx->aac_Track;
        else if (!strcmp(fid, "TCOP") || !strcmp(fid, "TCR"))
            target = &ctx->aac_Copyright;
        else if (!strcmp(fid, "TCON") || !strcmp(fid, "TCO"))
        {
            STRPTR raw = aac_DecodeText(pool, fbody, fsize);
            if (raw && !ctx->aac_Genre)
                ctx->aac_Genre = aac_NormaliseGenre(pool, raw);
            pos += framehdrsize + fsize;
            continue;
        }
        else if (!strcmp(fid, "COMM") || !strcmp(fid, "COM"))
        {
            /* COMM body: enc(1) + lang(3) + descr (NUL-terminated) + text */
            if (fsize >= 5 && !ctx->aac_Comment)
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
                    UBYTE tmp[AAC_STR_MAX];
                    LONG  rem = (LONG)fsize - dstart;
                    if (rem > AAC_STR_MAX - 1) rem = AAC_STR_MAX - 1;
                    tmp[0] = enc;
                    CopyMem(fbody + dstart, tmp + 1, rem);
                    ctx->aac_Comment = aac_DecodeText(pool, tmp, rem + 1);
                }
            }
            pos += framehdrsize + fsize;
            continue;
        }

        if (target && *target == NULL)
        {
            STRPTR decoded = aac_DecodeText(pool, fbody, fsize);
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
 * still missing after ID3v2.  Returns the number of trailing bytes the
 * tag occupies (128 or 0) so the decoder can stop short of it.
 */
static LONG aac_ParseID3v1(struct AACDecodeCtx *ctx, APTR pool, BPTR file)
{
    UBYTE tag[128];
    LONG  size;

    Seek(file, 0, OFFSET_END);
    size = Seek(file, 0, OFFSET_CURRENT);
    if (size < 128) return 0;

    if (Seek(file, -128, OFFSET_CURRENT) < 0) return 0;
    if (Read(file, tag, 128) != 128) return 0;
    if (tag[0] != 'T' || tag[1] != 'A' || tag[2] != 'G') return 0;

    if (!ctx->aac_Title)
    {
        STRPTR s = aac_PoolDupBytes(pool, tag + 3, 30);
        if (s) { aac_TrimRight(s); if (*s) ctx->aac_Title = s; }
    }
    if (!ctx->aac_Artist)
    {
        STRPTR s = aac_PoolDupBytes(pool, tag + 33, 30);
        if (s) { aac_TrimRight(s); if (*s) ctx->aac_Artist = s; }
    }
    if (!ctx->aac_Album)
    {
        STRPTR s = aac_PoolDupBytes(pool, tag + 63, 30);
        if (s) { aac_TrimRight(s); if (*s) ctx->aac_Album = s; }
    }
    if (!ctx->aac_Year)
    {
        STRPTR s = aac_PoolDupBytes(pool, tag + 93, 4);
        if (s) { aac_TrimRight(s); if (*s) ctx->aac_Year = s; }
    }
    if (!ctx->aac_Comment)
    {
        LONG commlen = 30;
        /* ID3v1.1: if byte 125 == 0 and byte 126 != 0, the comment is
         * 28 bytes and byte 126 carries a track number. */
        if (tag[125] == 0 && tag[126] != 0)
        {
            commlen = 28;
            if (!ctx->aac_Track)
            {
                char trk[8];
                snprintf(trk, sizeof(trk), "%u", (unsigned)tag[126]);
                ctx->aac_Track = aac_PoolDupBytes(pool,
                                                  (const UBYTE *)trk,
                                                  strlen(trk));
            }
        }
        {
            STRPTR s = aac_PoolDupBytes(pool, tag + 97, commlen);
            if (s) { aac_TrimRight(s); if (*s) ctx->aac_Comment = s; }
        }
    }
    if (!ctx->aac_Genre)
        ctx->aac_Genre = aac_GenreFromV1(pool, tag[127]);

    return 128;
}

static STRPTR aac_BuildAnnotation(struct AACDecodeCtx *ctx, APTR pool)
{
    char buf[AAC_ANNOT_MAX];
    LONG used = 0;
    BOOL any = FALSE;

    buf[0] = '\0';

    if (ctx->aac_Album)
    {
        used += snprintf(buf + used, AAC_ANNOT_MAX - used,
                         "Album: %s", ctx->aac_Album);
        any = TRUE;
    }
    if (ctx->aac_Track && used < AAC_ANNOT_MAX - 1)
    {
        used += snprintf(buf + used, AAC_ANNOT_MAX - used,
                         "%sTrack: %s", any ? "\n" : "", ctx->aac_Track);
        any = TRUE;
    }
    if (ctx->aac_Genre && used < AAC_ANNOT_MAX - 1)
    {
        used += snprintf(buf + used, AAC_ANNOT_MAX - used,
                         "%sGenre: %s", any ? "\n" : "", ctx->aac_Genre);
        any = TRUE;
    }
    if (ctx->aac_Comment && used < AAC_ANNOT_MAX - 1)
    {
        used += snprintf(buf + used, AAC_ANNOT_MAX - used,
                         "%sComment: %s", any ? "\n" : "", ctx->aac_Comment);
        any = TRUE;
    }

    if (!any) return NULL;
    if (used >= AAC_ANNOT_MAX) used = AAC_ANNOT_MAX - 1;
    return aac_PoolDupBytes(pool, (const UBYTE *)buf, used);
}

/**************************************************************************/
/* Decode loop                                                            */
/**************************************************************************/

static BOOL aac_DecodeStream(struct AACDecodeCtx *ctx)
{
    NeAACDecHandle           dec;
    NeAACDecConfigurationPtr cfg;
    NeAACDecFrameInfo        fi;
    unsigned long            samplerate = 0;
    unsigned char            channels = 0;
    long                     skip;
    LONG                     fill, pos;
    BOOL                     ok = FALSE;

    dec = NeAACDecOpen();
    if (!dec)
        return FALSE;

    cfg = NeAACDecGetCurrentConfiguration(dec);
    cfg->outputFormat = FAAD_FMT_16BIT;
    cfg->downMatrix   = 1;      /* fold 5.1 down to stereo */
    NeAACDecSetConfiguration(dec, cfg);

    fill = aac_RefillInput(ctx, 0, 0);
    if (fill <= 0)
        goto out;

    skip = NeAACDecInit(dec, ctx->aac_InBuf, fill, &samplerate, &channels);
    if (skip < 0)
    {
        D(bug("[aac.dt] NeAACDecInit failed\n"));
        goto out;
    }
    pos = skip;

    D(bug("[aac.dt] init: rate %lu, channels %u, skip %ld\n",
          samplerate, channels, skip));

    for (;;)
    {
        void *frame;
        LONG  avail = fill - pos;

        if (avail < AAC_REFILL_THRESHOLD && ctx->aac_StreamLeft > 0)
        {
            fill = aac_RefillInput(ctx, pos, fill);
            if (fill < 0)
                goto out;
            pos = 0;
            avail = fill;
        }

        if (avail <= 0)
            break;

        frame = NeAACDecDecode(dec, &fi, ctx->aac_InBuf + pos, avail);

        if (fi.error)
        {
            /* Tolerate trailing garbage once some audio has been
             * decoded; otherwise the stream is not usable. */
            D(bug("[aac.dt] decode error %u (%s)\n", fi.error,
                  NeAACDecGetErrorMessage(fi.error)));
            break;
        }

        if (!aac_AppendFrame(ctx, &fi, frame))
            goto out;

        if (fi.bytesconsumed == 0 && fi.samples == 0)
            break;

        pos += fi.bytesconsumed;
        if (pos > fill)
            pos = fill;
    }

    ok = (ctx->aac_PCMUsed > 0 && ctx->aac_SampleRate != 0);

out:
    NeAACDecClose(dec);
    return ok;
}

/**************************************************************************/
/* MP4 (ISO-BMFF) container decode via mp4ff                              */
/**************************************************************************/

static uint32_t aac_MP4Read(void *udata, void *buffer, uint32_t length)
{
    struct AACDecodeCtx *ctx = (struct AACDecodeCtx *)udata;
    LONG r = Read(ctx->aac_File, buffer, (LONG)length);
    return (r < 0) ? 0 : (uint32_t)r;
}

static uint32_t aac_MP4Seek(void *udata, uint64_t position)
{
    struct AACDecodeCtx *ctx = (struct AACDecodeCtx *)udata;
    /* like fseek(): 0 on success */
    return (Seek(ctx->aac_File, (LONG)position, OFFSET_BEGINNING) < 0)
           ? (uint32_t)-1 : 0;
}

/* Find the first track whose decoder config libfaad accepts */
static LONG aac_MP4FindTrack(mp4ff_t *mp4)
{
    LONG i, numTracks = mp4ff_total_tracks(mp4);

    for (i = 0; i < numTracks; i++)
    {
        unsigned char *buff = NULL;
        unsigned int   buff_size = 0;
        mp4AudioSpecificConfig mp4ASC;

        mp4ff_get_decoder_config(mp4, i, &buff, &buff_size);
        if (buff)
        {
            char rc = NeAACDecAudioSpecificConfig(buff, buff_size, &mp4ASC);
            free(buff);
            if (rc < 0)
                continue;
            return i;
        }
    }

    return -1;
}

static void aac_MP4Meta(struct AACDecodeCtx *ctx, APTR pool, mp4ff_t *mp4)
{
    struct { STRPTR *target; int32_t (*get)(const mp4ff_t *, char **); }
    fields[] =
    {
        { &ctx->aac_Title,     mp4ff_meta_get_title   },
        { &ctx->aac_Artist,    mp4ff_meta_get_artist  },
        { &ctx->aac_Album,     mp4ff_meta_get_album   },
        { &ctx->aac_Year,      mp4ff_meta_get_date    },
        { &ctx->aac_Genre,     mp4ff_meta_get_genre   },
        { &ctx->aac_Track,     mp4ff_meta_get_track   },
        { &ctx->aac_Comment,   mp4ff_meta_get_comment },
        { NULL, NULL }
    };
    LONG i;

    for (i = 0; fields[i].target; i++)
    {
        char *value = NULL;
        if (fields[i].get(mp4, &value) && value)
        {
            if (*value && !*fields[i].target)
                *fields[i].target = aac_PoolDupBytes(pool,
                                                     (const UBYTE *)value,
                                                     strlen(value));
            free(value);
        }
    }
}

static BOOL aac_DecodeMP4(struct AACDecodeCtx *ctx, APTR pool)
{
    NeAACDecHandle           dec;
    NeAACDecConfigurationPtr cfg;
    mp4ff_callback_t         cb;
    mp4ff_t                 *mp4;
    unsigned char           *asc = NULL;
    unsigned int             asc_size = 0;
    unsigned long            samplerate = 0;
    unsigned char            channels = 0;
    LONG                     track, numSamples, i;
    BOOL                     ok = FALSE;

    memset(&cb, 0, sizeof(cb));
    cb.read      = aac_MP4Read;
    cb.seek      = aac_MP4Seek;
    cb.user_data = ctx;

    Seek(ctx->aac_File, 0, OFFSET_BEGINNING);

    mp4 = mp4ff_open_read(&cb);
    if (!mp4)
        return FALSE;

    dec = NeAACDecOpen();
    if (!dec)
    {
        mp4ff_close(mp4);
        return FALSE;
    }

    cfg = NeAACDecGetCurrentConfiguration(dec);
    cfg->outputFormat = FAAD_FMT_16BIT;
    cfg->downMatrix   = 1;      /* fold 5.1 down to stereo */
    NeAACDecSetConfiguration(dec, cfg);

    track = aac_MP4FindTrack(mp4);
    if (track < 0)
    {
        D(bug("[aac.dt] no decodable AAC track in MP4 container\n"));
        goto out;
    }

    mp4ff_get_decoder_config(mp4, track, &asc, &asc_size);
    if (!asc)
        goto out;

    if (NeAACDecInit2(dec, asc, asc_size, &samplerate, &channels) < 0)
    {
        D(bug("[aac.dt] NeAACDecInit2 failed\n"));
        free(asc);
        goto out;
    }
    free(asc);

    D(bug("[aac.dt] mp4: track %ld, rate %lu, channels %u\n",
          track, samplerate, channels));

    numSamples = mp4ff_num_samples(mp4, track);

    for (i = 0; i < numSamples; i++)
    {
        unsigned char    *sample = NULL;
        unsigned int      samplesize = 0;
        NeAACDecFrameInfo fi;
        void             *frame;

        if (!mp4ff_read_sample(mp4, track, i, &sample, &samplesize))
            break;

        frame = NeAACDecDecode(dec, &fi, sample, samplesize);
        free(sample);

        if (fi.error)
        {
            D(bug("[aac.dt] mp4 decode error %u (%s)\n", fi.error,
                  NeAACDecGetErrorMessage(fi.error)));
            break;
        }

        if (!aac_AppendFrame(ctx, &fi, frame))
            goto out;
    }

    ok = (ctx->aac_PCMUsed > 0 && ctx->aac_SampleRate != 0);

    if (ok)
        aac_MP4Meta(ctx, pool, mp4);

out:
    NeAACDecClose(dec);
    mp4ff_close(mp4);
    return ok;
}

/**************************************************************************/
/* OM_NEW                                                                  */
/**************************************************************************/

static BOOL ReadAAC(Class *cl, Object *o)
{
    struct AAC_Data    *id = INST_DATA(cl, o);
    struct AACDecodeCtx ctx;
    IPTR                sourcetype = 0;
    BPTR                handle = BNULL;
    UBYTE               sampletype;
    ULONG               framecount;
    ULONG               id3v2size;
    LONG                id3v1size;
    LONG                filesize;
    STRPTR              annotation;
    BOOL                ismp4 = FALSE;
    BOOL                decoded;

    D(bug("aac.datatype/ReadAAC()\n"));

    memset(&ctx, 0, sizeof(ctx));

    if (!id->aacd_StringPool)
        id->aacd_StringPool = CreatePool(MEMF_PUBLIC | MEMF_CLEAR, 1024, 256);

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

    ctx.aac_File = handle;

    /* MP4 (ISO-BMFF) container? First atom is size(4) + 'ftyp'. */
    {
        UBYTE hdr[8];
        Seek(handle, 0, OFFSET_BEGINNING);
        if (Read(handle, hdr, 8) == 8
            && hdr[4] == 'f' && hdr[5] == 't'
            && hdr[6] == 'y' && hdr[7] == 'p')
            ismp4 = TRUE;
    }

    if (ismp4)
    {
        D(bug("[aac.dt] MP4 container detected\n"));
        decoded = aac_DecodeMP4(&ctx, id->aacd_StringPool);
    }
    else
    {
        ctx.aac_InBuf = AllocVec(AAC_INPUT_BUFFER_SIZE, MEMF_PUBLIC);
        if (!ctx.aac_InBuf)
        {
            SetIoErr(ERROR_NO_FREE_STORE);
            return FALSE;
        }

        id3v2size = aac_ParseID3v2(&ctx, id->aacd_StringPool, handle);

        id3v1size = aac_ParseID3v1(&ctx, id->aacd_StringPool, handle);

        Seek(handle, 0, OFFSET_END);
        filesize = Seek(handle, 0, OFFSET_CURRENT);
        ctx.aac_StreamLeft = filesize - (LONG)id3v2size - id3v1size;

        /* Rewind past the ID3v2 tag for the decoder. */
        Seek(handle, id3v2size, OFFSET_BEGINNING);

        decoded = aac_DecodeStream(&ctx);
    }

    if (!decoded || ctx.aac_Error
        || ctx.aac_Channels == 0 || ctx.aac_Channels > 2)
    {
        D(bug("[aac.dt] decode failed (used=%lu rate=%lu channels=%u)\n",
              ctx.aac_PCMUsed, ctx.aac_SampleRate, ctx.aac_Channels));
        if (ctx.aac_InBuf) FreeVec(ctx.aac_InBuf);
        if (ctx.aac_PCM) FreeVec(ctx.aac_PCM);
        SetIoErr(ERROR_OBJECT_WRONG_TYPE);
        return FALSE;
    }

    if (ctx.aac_InBuf)
    {
        FreeVec(ctx.aac_InBuf);
        ctx.aac_InBuf = NULL;
    }

    sampletype = (ctx.aac_Channels == 2) ? SDTST_S16S : SDTST_M16S;
    framecount = ctx.aac_PCMUsed
               / (sizeof(WORD) * ctx.aac_Channels);

    D(bug("[aac.dt] decoded %lu frames, %lu Hz, %s 16-bit\n",
          framecount, ctx.aac_SampleRate,
          (ctx.aac_Channels == 2) ? "stereo" : "mono"));
    D(bug("[aac.dt] title='%s' artist='%s' album='%s' year='%s'\n",
          ctx.aac_Title  ? ctx.aac_Title  : (STRPTR)"",
          ctx.aac_Artist ? ctx.aac_Artist : (STRPTR)"",
          ctx.aac_Album  ? ctx.aac_Album  : (STRPTR)"",
          ctx.aac_Year   ? ctx.aac_Year   : (STRPTR)""));

    annotation = aac_BuildAnnotation(&ctx, id->aacd_StringPool);

    SetDTAttrs(o, NULL, NULL,
               DTA_ObjName,        (IPTR)(ctx.aac_Title ? ctx.aac_Title
                                                        : (STRPTR)"Unknown"),
               DTA_ObjAuthor,      (IPTR)ctx.aac_Artist,
               DTA_ObjAnnotation,  (IPTR)annotation,
               DTA_ObjCopyright,   (IPTR)ctx.aac_Copyright,
               DTA_ObjVersion,     (IPTR)ctx.aac_Year,
               SDTA_Sample,        (IPTR)ctx.aac_PCM,
               SDTA_SampleLength,  framecount,
               SDTA_SampleType,    sampletype,
               SDTA_SamplesPerSec, ctx.aac_SampleRate,
               SDTA_Period,        709379UL * 5 / ctx.aac_SampleRate,
               SDTA_Volume,        64,
               SDTA_Cycles,        1,
               TAG_DONE);

    return TRUE;
}

/**************************************************************************/

IPTR AAC__OM_NEW(Class *cl, Object *o, struct opSet *msg)
{
    IPTR retval;

    retval = DoSuperMethodA(cl, o, (Msg)msg);
    if (retval)
    {
        if (!ReadAAC(cl, (Object *)retval))
        {
            CoerceMethod(cl, (Object *)retval, OM_DISPOSE);
            retval = 0;
        }
    }

    return retval;
}

/**************************************************************************/

IPTR AAC__OM_DISPOSE(Class *cl, Object *o, Msg msg)
{
    struct AAC_Data *id = INST_DATA(cl, o);

    if (id->aacd_StringPool)
    {
        DeletePool(id->aacd_StringPool);
        id->aacd_StringPool = NULL;
    }

    return DoSuperMethodA(cl, o, msg);
}

/**************************************************************************/
