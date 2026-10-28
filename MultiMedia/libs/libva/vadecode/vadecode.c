/*
    Copyright (C) 2026, The AROS Development Team. All rights reserved.

    vadecode: decode a video file with FFmpeg through VA-API (the screen's
    hardware decoder) or in software, and print a checksum of every frame.
    The two runs must give the same checksums; the hardware run reads the
    decoded surfaces back (vaGetImage through FFmpeg's hwframe transfer).

    vadecode [-sw] [-n frames] [-q] [-dump N file] [-show] file
      -sw   software decoding (the reference)
      -n N  stop after N frames
      -q    only the summary line
      -dump N file  write output frame N's Y, U and V planes (8-bit, no padding) to file
      -dumpall file write every output frame that way, one after the other (for codecs whose
            hardware and software output may differ by rounding: compare with a tolerance)
      -show show the decoded surfaces in a window on the video plane (VA-API only),
            paced at the stream's frame rate; no readback or checksums unless -md5
      -md5  with -show: read back and checksum as well
    vadecode -cut N out file          copy the first N video packets of file into out (no re-encode;
                                      container from out's extension); for making short test clips
    vadecode -transcode ENC N out file  decode file in software, encode N frames with FFmpeg's encoder
                                      ENC (e.g. mpeg2video) into out
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/time.h>

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext.h>
#include <libavutil/imgutils.h>
#include <libavutil/md5.h>
#include <libavutil/pixdesc.h>
#include <libavutil/hwcontext_vaapi.h>

#include <va/va.h>
#include <va/va_aros.h>
#include <intuition/intuition.h>
#include <proto/intuition.h>
#include <proto/dos.h>

static enum AVPixelFormat get_hw_format(AVCodecContext *ctx, const enum AVPixelFormat *fmts)
{
    const enum AVPixelFormat *p;

    for (p = fmts; *p != AV_PIX_FMT_NONE; p++)
        if (*p == AV_PIX_FMT_VAAPI)
            return *p;
    fprintf(stderr, "vadecode: the decoder offers no VA-API surface format\n");
    return AV_PIX_FMT_NONE;
}

/*
 * Canonical 4:2:0 8-bit checksum: Y rows, then U rows, then V rows,
 * without padding, so yuv420p (software) and nv12 (hardware) frames of the
 * same picture hash alike.
 */
/*
 * 10-bit pictures: P010 (hardware, value in the high 10 bits of 16) and
 * yuv420p10le (software, value in the low 10 bits) reduce to the same
 * canonical bytes: Y, U, V planes of 16-bit little-endian values in the low bits.
 */
static int frame_md5_10(const AVFrame *f, struct AVMD5 *md5)
{
    int w = f->width, h = f->height, y, x, plane;
    uint16_t *tmp = malloc(w * 2);

    if (!tmp)
        return -1;
    for (y = 0; y < h; y++)
    {
        const uint16_t *row = (const uint16_t *)(f->data[0] + (size_t)y * f->linesize[0]);

        for (x = 0; x < w; x++)
            tmp[x] = (f->format == AV_PIX_FMT_P010LE) ? (row[x] >> 6) : row[x];
        av_md5_update(md5, (uint8_t *)tmp, w * 2);
    }
    for (plane = 0; plane < 2; plane++)
        for (y = 0; y < h / 2; y++)
        {
            if (f->format == AV_PIX_FMT_P010LE)
            {
                const uint16_t *row = (const uint16_t *)(f->data[1] + (size_t)y * f->linesize[1]);

                for (x = 0; x < w / 2; x++)
                    tmp[x] = row[2 * x + plane] >> 6;
            }
            else
                memcpy(tmp, f->data[1 + plane] + (size_t)y * f->linesize[1 + plane], w);
            av_md5_update(md5, (uint8_t *)tmp, w);
        }
    free(tmp);
    return 0;
}

static int frame_md5(const AVFrame *f, uint8_t out[16])
{
    struct AVMD5 *md5 = av_md5_alloc();
    int w = f->width, h = f->height, y, x;
    uint8_t *tmp;

    if (!md5)
        return -1;
    av_md5_init(md5);
    if (f->format == AV_PIX_FMT_P010LE || f->format == AV_PIX_FMT_YUV420P10LE)
    {
        int r = frame_md5_10(f, md5);

        av_md5_final(md5, out);
        av_free(md5);
        return r;
    }
    for (y = 0; y < h; y++)
        av_md5_update(md5, f->data[0] + (size_t)y * f->linesize[0], w);
    tmp = malloc(w / 2);
    if (!tmp)
    {
        av_free(md5);
        return -1;
    }
    if (f->format == AV_PIX_FMT_NV12)
    {
        int plane;

        for (plane = 0; plane < 2; plane++)
            for (y = 0; y < h / 2; y++)
            {
                const uint8_t *row = f->data[1] + (size_t)y * f->linesize[1];

                for (x = 0; x < w / 2; x++)
                    tmp[x] = row[2 * x + plane];
                av_md5_update(md5, tmp, w / 2);
            }
    }
    else if (f->format == AV_PIX_FMT_YUV420P || f->format == AV_PIX_FMT_YUVJ420P)
    {
        for (y = 0; y < h / 2; y++)
            av_md5_update(md5, f->data[1] + (size_t)y * f->linesize[1], w / 2);
        for (y = 0; y < h / 2; y++)
            av_md5_update(md5, f->data[2] + (size_t)y * f->linesize[2], w / 2);
    }
    else
    {
        free(tmp);
        av_free(md5);
        return -2;
    }
    free(tmp);
    av_md5_final(md5, out);
    av_free(md5);
    return 0;
}

/* Y, then U, then V, each unpadded: the same bytes for yuv420p and nv12 */
static void frame_dump(const AVFrame *f, const char *path, int append)
{
    FILE *out = fopen(path, append ? "ab" : "wb");
    int w = f->width, h = f->height, y, x, plane;
    uint8_t *tmp;

    if (!out)
    {
        fprintf(stderr, "vadecode: cannot write %s\n", path);
        return;
    }
    for (y = 0; y < h; y++)
        fwrite(f->data[0] + (size_t)y * f->linesize[0], 1, w, out);
    tmp = malloc(w / 2);
    if (f->format == AV_PIX_FMT_NV12 && tmp)
    {
        for (plane = 0; plane < 2; plane++)
            for (y = 0; y < h / 2; y++)
            {
                const uint8_t *row = f->data[1] + (size_t)y * f->linesize[1];

                for (x = 0; x < w / 2; x++)
                    tmp[x] = row[2 * x + plane];
                fwrite(tmp, 1, w / 2, out);
            }
    }
    else if (f->format == AV_PIX_FMT_YUV420P || f->format == AV_PIX_FMT_YUVJ420P)
    {
        for (plane = 1; plane <= 2; plane++)
            for (y = 0; y < h / 2; y++)
                fwrite(f->data[plane] + (size_t)y * f->linesize[plane], 1, w / 2, out);
    }
    free(tmp);
    fclose(out);
    printf("vadecode: frame %dx%d written to %s\n", w, h, path);
}

/* -cut: stream-copy the first n video packets into a new container */
static int clip_cut(const char *in, const char *out, int n)
{
    AVFormatContext *ifmt = NULL, *ofmt = NULL;
    AVStream *ost;
    AVPacket *pkt = av_packet_alloc();
    int vs, ret, count = 0;

    if ((ret = avformat_open_input(&ifmt, in, NULL, NULL)) < 0 || (ret = avformat_find_stream_info(ifmt, NULL)) < 0)
        goto fail;
    vs = av_find_best_stream(ifmt, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (vs < 0) { ret = vs; goto fail; }
    if ((ret = avformat_alloc_output_context2(&ofmt, NULL, NULL, out)) < 0)
        goto fail;
    ost = avformat_new_stream(ofmt, NULL);
    if (!ost || (ret = avcodec_parameters_copy(ost->codecpar, ifmt->streams[vs]->codecpar)) < 0)
        goto fail;
    ost->codecpar->codec_tag = 0;
    ost->time_base = ifmt->streams[vs]->time_base;
    ost->avg_frame_rate = ifmt->streams[vs]->avg_frame_rate;
    if (!(ofmt->oformat->flags & AVFMT_NOFILE) && (ret = avio_open(&ofmt->pb, out, AVIO_FLAG_WRITE)) < 0)
        goto fail;
    if ((ret = avformat_write_header(ofmt, NULL)) < 0)
        goto fail;
    while (count < n && av_read_frame(ifmt, pkt) >= 0)
    {
        if (pkt->stream_index == vs)
        {
            av_packet_rescale_ts(pkt, ifmt->streams[vs]->time_base, ost->time_base);
            pkt->stream_index = 0;
            pkt->pos = -1;
            if ((ret = av_interleaved_write_frame(ofmt, pkt)) < 0)
                goto fail;
            count++;
        }
        av_packet_unref(pkt);
    }
    av_write_trailer(ofmt);
    printf("vadecode: %d packets copied to %s\n", count, out);
    ret = 0;
fail:
    if (ret < 0)
        fprintf(stderr, "vadecode: cut failed: %s\n", av_err2str(ret));
    av_packet_free(&pkt);
    if (ofmt && !(ofmt->oformat->flags & AVFMT_NOFILE))
        avio_closep(&ofmt->pb);
    avformat_free_context(ofmt);
    avformat_close_input(&ifmt);
    return ret < 0 ? 10 : 0;
}

/* -transcode: software decode, encode n frames with a built-in FFmpeg encoder */
static int clip_transcode(const char *encname, const char *in, const char *out, int n)
{
    AVFormatContext *ifmt = NULL, *ofmt = NULL;
    AVCodecContext *dec = NULL, *enc = NULL;
    const AVCodec *dc = NULL, *ec;
    AVStream *ost;
    AVPacket *pkt = av_packet_alloc(), *opkt = av_packet_alloc();
    AVFrame *fr = av_frame_alloc();
    int vs, ret, count = 0, eof = 0;

    ec = avcodec_find_encoder_by_name(encname);
    if (!ec) { fprintf(stderr, "vadecode: no encoder %s\n", encname); return 10; }
    if ((ret = avformat_open_input(&ifmt, in, NULL, NULL)) < 0 || (ret = avformat_find_stream_info(ifmt, NULL)) < 0)
        goto fail;
    vs = av_find_best_stream(ifmt, AVMEDIA_TYPE_VIDEO, -1, -1, &dc, 0);
    if (vs < 0) { ret = vs; goto fail; }
    dec = avcodec_alloc_context3(dc);
    avcodec_parameters_to_context(dec, ifmt->streams[vs]->codecpar);
    if ((ret = avcodec_open2(dec, dc, NULL)) < 0)
        goto fail;
    enc = avcodec_alloc_context3(ec);
    enc->width = dec->width & ~1;
    enc->height = dec->height & ~1;
    enc->pix_fmt = AV_PIX_FMT_YUV420P;
    enc->time_base = av_inv_q(ifmt->streams[vs]->avg_frame_rate.num ? ifmt->streams[vs]->avg_frame_rate : (AVRational){ 25, 1 });
    enc->framerate = av_inv_q(enc->time_base);
    enc->gop_size = 12;
    enc->max_b_frames = 2;
    enc->bit_rate = 4000000;
    if ((ret = avformat_alloc_output_context2(&ofmt, NULL, NULL, out)) < 0)
        goto fail;
    if (ofmt->oformat->flags & AVFMT_GLOBALHEADER)
        enc->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    if ((ret = avcodec_open2(enc, ec, NULL)) < 0)
        goto fail;
    ost = avformat_new_stream(ofmt, NULL);
    avcodec_parameters_from_context(ost->codecpar, enc);
    ost->time_base = enc->time_base;
    if (!(ofmt->oformat->flags & AVFMT_NOFILE) && (ret = avio_open(&ofmt->pb, out, AVIO_FLAG_WRITE)) < 0)
        goto fail;
    if ((ret = avformat_write_header(ofmt, NULL)) < 0)
        goto fail;
    while (!eof)
    {
        if (count < n && av_read_frame(ifmt, pkt) >= 0)
        {
            if (pkt->stream_index == vs)
                avcodec_send_packet(dec, pkt);
            av_packet_unref(pkt);
        }
        else
        {
            avcodec_send_packet(dec, NULL);
            eof = 1;
        }
        while (avcodec_receive_frame(dec, fr) >= 0)
        {
            if (count < n && fr->format == AV_PIX_FMT_YUV420P && fr->width >= enc->width && fr->height >= enc->height)
            {
                fr->pts = count++;
                fr->pict_type = AV_PICTURE_TYPE_NONE;
                avcodec_send_frame(enc, fr);
                while (avcodec_receive_packet(enc, opkt) >= 0)
                {
                    av_packet_rescale_ts(opkt, enc->time_base, ost->time_base);
                    opkt->stream_index = 0;
                    av_interleaved_write_frame(ofmt, opkt);
                }
            }
            av_frame_unref(fr);
        }
    }
    avcodec_send_frame(enc, NULL);
    while (avcodec_receive_packet(enc, opkt) >= 0)
    {
        av_packet_rescale_ts(opkt, enc->time_base, ost->time_base);
        opkt->stream_index = 0;
        av_interleaved_write_frame(ofmt, opkt);
    }
    av_write_trailer(ofmt);
    printf("vadecode: %d frames encoded with %s to %s\n", count, encname, out);
    ret = 0;
fail:
    if (ret < 0)
        fprintf(stderr, "vadecode: transcode failed: %s\n", av_err2str(ret));
    av_frame_free(&fr);
    av_packet_free(&pkt);
    av_packet_free(&opkt);
    avcodec_free_context(&dec);
    avcodec_free_context(&enc);
    if (ofmt && !(ofmt->oformat->flags & AVFMT_NOFILE))
        avio_closep(&ofmt->pb);
    avformat_free_context(ofmt);
    avformat_close_input(&ifmt);
    return ret < 0 ? 10 : 0;
}

static double now_s(void)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

/*
 * The display driver decodes on the caller's task: its channel and object
 * setup runs deep inside the kernel-side driver, on this program's stack,
 * on top of FFmpeg's own needs. Run on a 1 MB stack whatever the shell gave.
 */
#include <exec/tasks.h>
#include <proto/exec.h>

#define VADECODE_STACK  (1024 * 1024)

static int real_main(int argc, char **argv);

int main(int argc, char **argv)
{
    struct Task *me = FindTask(NULL);
    IPTR have = (IPTR)me->tc_SPUpper - (IPTR)me->tc_SPLower;

    if (have < VADECODE_STACK)
    {
        struct StackSwapStruct sss;
        struct StackSwapArgs args;
        void *stack = AllocMem(VADECODE_STACK, MEMF_ANY);
        IPTR rc;

        if (stack)
        {
            sss.stk_Lower = stack;
            sss.stk_Upper = (APTR)((IPTR)stack + VADECODE_STACK);
            sss.stk_Pointer = sss.stk_Upper;
            args.Args[0] = (IPTR)argc;
            args.Args[1] = (IPTR)argv;
            rc = NewStackSwap(&sss, real_main, &args);
            FreeMem(stack, VADECODE_STACK);
            return (int)rc;
        }
    }
    return real_main(argc, argv);
}

static int real_main(int argc, char **argv)
{
    const char *file = NULL;
    int sw = 0, quiet = 0, maxframes = 0, i, dumpframe = -1, show = 0, md5too = 0;
    const char *dumppath = NULL;
    struct Window *win = NULL;
    VADisplay vadpy = NULL;
    double frametime = 0, nextshow = 0;
    AVFormatContext *fmt = NULL;
    AVCodecContext *ctx = NULL;
    const AVCodec *codec = NULL;
    AVBufferRef *hwdev = NULL;
    AVPacket *pkt;
    AVFrame *frame, *swframe;
    int stream = -1, ret, frames = 0, errors = 0, done = 0;
    double t0, t1, ta, tdecode = 0, treadback = 0, tsum = 0;

    if (argc == 5 && !strcmp(argv[1], "-cut"))
        return clip_cut(argv[4], argv[3], atoi(argv[2]));
    if (argc == 6 && !strcmp(argv[1], "-transcode"))
        return clip_transcode(argv[2], argv[5], argv[4], atoi(argv[3]));
    for (i = 1; i < argc; i++)
    {
        if (!strcmp(argv[i], "-sw"))
            sw = 1;
        else if (!strcmp(argv[i], "-q"))
            quiet = 1;
        else if (!strcmp(argv[i], "-n") && (i + 1 < argc))
            maxframes = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-dump") && (i + 2 < argc))
        {
            dumpframe = atoi(argv[++i]);
            dumppath = argv[++i];
        }
        else if (!strcmp(argv[i], "-dumpall") && (i + 1 < argc))
        {
            dumpframe = -2;
            dumppath = argv[++i];
        }
        else if (!strcmp(argv[i], "-show"))
            show = 1;
        else if (!strcmp(argv[i], "-md5"))
            md5too = 1;
        else
            file = argv[i];
    }
    if (!file)
    {
        fprintf(stderr, "usage: vadecode [-sw] [-n frames] [-q] file\n");
        return 10;
    }

    ret = avformat_open_input(&fmt, file, NULL, NULL);
    if (ret < 0)
    {
        fprintf(stderr, "vadecode: cannot open %s (%s)\n", file, av_err2str(ret));
        return 10;
    }
    if (avformat_find_stream_info(fmt, NULL) < 0)
    {
        fprintf(stderr, "vadecode: no stream information\n");
        return 10;
    }
    stream = av_find_best_stream(fmt, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0);
    if (stream < 0 || !codec)
    {
        fprintf(stderr, "vadecode: no video stream with a decoder\n");
        return 10;
    }
    ctx = avcodec_alloc_context3(codec);
    if (!ctx || avcodec_parameters_to_context(ctx, fmt->streams[stream]->codecpar) < 0)
    {
        fprintf(stderr, "vadecode: decoder context setup failed\n");
        return 10;
    }
    if (!sw)
    {
        ret = av_hwdevice_ctx_create(&hwdev, AV_HWDEVICE_TYPE_VAAPI, NULL, NULL, 0);
        if (ret < 0)
        {
            fprintf(stderr, "vadecode: no VA-API device on this screen (%s)\n", av_err2str(ret));
            return 10;
        }
        ctx->hw_device_ctx = av_buffer_ref(hwdev);
        ctx->get_format = get_hw_format;
    }
    else
        ctx->thread_count = 1;
    if (avcodec_open2(ctx, codec, NULL) < 0)
    {
        fprintf(stderr, "vadecode: cannot open the %s decoder\n", codec->name);
        return 10;
    }
    printf("vadecode: %s %dx%d, %s decoding\n", codec->name, ctx->width, ctx->height, sw ? "software" : "VA-API");
    if (show && !sw)
    {
        AVHWDeviceContext *dctx = (AVHWDeviceContext *)hwdev->data;
        AVVAAPIDeviceContext *vactx = dctx->hwctx;
        AVRational fr = fmt->streams[stream]->avg_frame_rate;

        vadpy = vactx->display;
        frametime = (fr.num > 0 && fr.den > 0) ? (double)fr.den / fr.num : 1.0 / 25.0;
        win = OpenWindowTags(NULL,
            WA_InnerWidth, ctx->width, WA_InnerHeight, ctx->height,
            WA_Title, (IPTR)"vadecode", WA_DragBar, TRUE, WA_DepthGadget, TRUE, WA_CloseGadget, TRUE,
            WA_IDCMP, IDCMP_CLOSEWINDOW, WA_Activate, TRUE, WA_SmartRefresh, TRUE,
            TAG_DONE);
        if (!win)
        {
            fprintf(stderr, "vadecode: cannot open the window\n");
            show = 0;
        }
        else
            printf("vadecode: showing at %.2f fps, close the window to stop\n", 1.0 / frametime);
    }
    else
        show = 0;

    pkt = av_packet_alloc();
    frame = av_frame_alloc();
    swframe = av_frame_alloc();
    t0 = now_s();
    while (!done)
    {
        ret = av_read_frame(fmt, pkt);
        if (ret < 0)
        {
            /* end of file: drain the decoder */
            avcodec_send_packet(ctx, NULL);
            done = 1;
        }
        else if (pkt->stream_index != stream)
        {
            av_packet_unref(pkt);
            continue;
        }
        else
        {
            ta = now_s();
            ret = avcodec_send_packet(ctx, pkt);
            tdecode += now_s() - ta;
            av_packet_unref(pkt);
            if (ret < 0)
            {
                fprintf(stderr, "vadecode: send_packet: %s\n", av_err2str(ret));
                errors++;
                continue;
            }
        }
        for (;;)
        {
            const AVFrame *out;
            uint8_t md5[16];

            ta = now_s();
            ret = avcodec_receive_frame(ctx, frame);
            tdecode += now_s() - ta;
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                break;
            if (ret < 0)
            {
                fprintf(stderr, "vadecode: receive_frame: %s\n", av_err2str(ret));
                errors++;
                break;
            }
            out = frame;
            if (show && frame->format == AV_PIX_FMT_VAAPI)
            {
                VASurfaceID surf = (VASurfaceID)(uintptr_t)frame->data[3];
                VARectangle src = { 0, 0, frame->width, frame->height };
                VARectangle dst = { 0, 0, frame->width, frame->height };
                struct IntuiMessage *imsg;
                VAStatus st;
                double t;

                /* pace to the stream rate */
                t = now_s();
                if (nextshow == 0)
                    nextshow = t;
                while ((t = now_s()) < nextshow)
                    Delay(1);
                nextshow += frametime;
                st = vaPutSurfaceAROS(vadpy, surf, win, &src, &dst, 0);
                if (st != VA_STATUS_SUCCESS)
                {
                    fprintf(stderr, "vadecode: frame %d: vaPutSurface: %s\n", frames, vaErrorStr(st));
                    errors++;
                }
                while ((imsg = (struct IntuiMessage *)GetMsg(win->UserPort)))
                {
                    if (imsg->Class == IDCMP_CLOSEWINDOW)
                        done = 1;
                    ReplyMsg((struct Message *)imsg);
                }
                if (!md5too)
                {
                    frames++;
                    av_frame_unref(frame);
                    if (done || (maxframes && frames >= maxframes))
                    {
                        done = 1;
                        break;
                    }
                    continue;
                }
            }
            if (frame->format == AV_PIX_FMT_VAAPI)
            {
                av_frame_unref(swframe);
                ta = now_s();
                ret = av_hwframe_transfer_data(swframe, frame, 0);
                treadback += now_s() - ta;
                if (ret < 0)
                {
                    fprintf(stderr, "vadecode: frame %d: readback failed (%s)\n", frames, av_err2str(ret));
                    errors++;
                    av_frame_unref(frame);
                    frames++;
                    continue;
                }
                out = swframe;
            }
            if (dumppath && ((frames == dumpframe) || (dumpframe == -2)))
                frame_dump(out, dumppath, (dumpframe == -2) && (frames > 0));
            ta = now_s();
            if (frame_md5(out, md5) == 0)
            {
                tsum += now_s() - ta;
                if (!quiet)
                {
                    printf("%5d %s ", frames, av_get_pix_fmt_name(out->format));
                    for (i = 0; i < 16; i++)
                        printf("%02x", md5[i]);
                    printf("\n");
                }
            }
            else
                fprintf(stderr, "vadecode: frame %d: unsupported format %s\n", frames, av_get_pix_fmt_name(out->format));
            frames++;
            av_frame_unref(frame);
            if (maxframes && frames >= maxframes)
            {
                done = 1;
                break;
            }
        }
    }
    t1 = now_s();
    printf("vadecode: %d frames, %d errors, %.1f s (%.1f fps): decode %.1f s, readback %.1f s, checksum %.1f s\n",
        frames, errors, t1 - t0, (t1 > t0) ? frames / (t1 - t0) : 0.0, tdecode, treadback, tsum);

    if (win)
    {
        vaReleaseWindowAROS(vadpy, win);
        CloseWindow(win);
    }
    av_frame_free(&swframe);
    av_frame_free(&frame);
    av_packet_free(&pkt);
    avcodec_free_context(&ctx);
    av_buffer_unref(&hwdev);
    avformat_close_input(&fmt);
    return errors ? 5 : 0;
}
