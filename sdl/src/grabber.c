/**
 * \file grabber.c
 * \brief Screen recording/snapshots
 */
#include <X11/X.h>                     // for Drawable, ZPixmap
#include <X11/Xlib.h>                  // for XImage, XGetImage, AllPlanes, XCre..
#include <X11/Xutil.h>                 // for XDestroyImage, XGetPixel
#include <errno.h>                     // for EBUSY
#include <libavcodec/avcodec.h>        // for AVCodecContext, AVPacket, AVCodec
#include <libswresample/swresample.h>  // swr_free
#include <libavformat/avformat.h>      // for AVFormatContext, AVOutputFormat
#include <libavformat/avio.h>          // for avio_closep, avio_open, AVIO_FLAG_..
#include <libavutil/avutil.h>          // for AVMediaType::AVMEDIA_TYPE_VIDEO
#include <libavutil/dict.h>            // for AVDictionary, av_dict_copy, av_dic..
#include <libavutil/error.h>           // for av_err2str
#include <libavutil/frame.h>           // for AVFrame, av_frame_alloc, av_frame_..
#include <libavutil/pixfmt.h>          // for AVPixelFormat::AV_PIX_FMT_YUV420P
#include <libavutil/rational.h>        // for AVRational
#include <pthread.h>                   // for pthread_join, pthread_t, pthread_m..
#include <stdint.h>                    // for uint8_t
#include <stdio.h>                     // for NULL, fprintf, stderr, fclose, fopen
#include <stdlib.h>                    // for exit, free, malloc
#include <string.h>                    // for memset
#include <sys/select.h>                // for FD_ISSET, FD_SET, FD_ZERO, fd_set
#include <sys/stat.h>                  // for stat
#include <sys/time.h>                  // for timeval, gettimeofday
#include <time.h>                      // for timespec, time_t
#include <unistd.h>                    // for sleep, usleep

#include "amqp_listen.h"
#include "buffer_sizes.h"
#include "logger.h"
#include "recording.pb-c.h"
#include "sensors.h"
#include "socket.h"

#include "grabber.h"

#define LOG_TAG "grabber"

#define READ_BUFFER_SIZE 1024

/** \var extern int    g_width
    \brief Global variable for the window width
*/
extern int g_width;

/** \var extern int    g_height
    \brief Global variable for the window height
*/
extern int g_height;

/** \var extern void   *g_window_id ;
    \brief Global variable for the X window id
*/
extern void* g_window_id;

/** \var Display*         s_display;
    \brief static variable for Display
*/
static Display* s_display;

/** \var char*  s_path_results;
    \brief Static variable containing the recording result path
*/
static char* s_path_results;

void grabber_set_display(Display* display)
{
    s_display = display;
}

void grabber_set_path_results(char* results)
{
    s_path_results = results;
}

/*
    \brief To stop video recording \a fd.
    \param mtx mutex to stop video recording
*/
int needQuit(pthread_mutex_t* mtx)
{
    switch (pthread_mutex_trylock(mtx))
    {
    case 0: /* if we got the lock, unlock and return 1 (true) */
        LOGM("we got the lock, unlock and return 1 (true). -> NeedQuit Stop record");
        pthread_mutex_unlock(mtx);
        return 1;
    case EBUSY: /* return 0 (false) if the mutex was locked */
        return 0;
    }
    return 1;
}

static void log_packet(const AVFormatContext* fmt_ctx, const AVPacket* pkt)
{
    (void) fmt_ctx;
    (void) pkt;
    /*
    AVRational* time_base = &fmt_ctx->streams[pkt->stream_index]->time_base;

    printf("pts:%s pts_time:%s dts:%s dts_time:%s duration:%s duration_time:%s stream_index:%d\n",
           av_ts2str(pkt->pts), av_ts2timestr(pkt->pts, time_base),
           av_ts2str(pkt->dts), av_ts2timestr(pkt->dts, time_base),
           av_ts2str(pkt->duration), av_ts2timestr(pkt->duration, time_base),
           pkt->stream_index);*/
}

static int write_frame(AVFormatContext* fmt_ctx, const AVRational* time_base, AVStream* st,
                       AVPacket* pkt)
{
    /* rescale output packet timestamp values from codec to stream timebase */
    av_packet_rescale_ts(pkt, *time_base, st->time_base);
    pkt->stream_index = st->index;

    /* Write the compressed frame to the media file. */
    log_packet(fmt_ctx, pkt);
    return av_interleaved_write_frame(fmt_ctx, pkt);
}

/* Add an output stream. */
static void add_stream(OutputStream* ost, AVFormatContext* oc, AVCodec** codec,
                       enum AVCodecID codec_id)
{
    AVCodecContext* c;

    /* find the encoder */
    *codec = avcodec_find_encoder(codec_id);
    if (!(*codec))
    {
        fprintf(stderr, "Could not find encoder for '%s'\n", avcodec_get_name(codec_id));
        exit(1);
    }

    ost->st = avformat_new_stream(oc, *codec);
    if (!ost->st)
    {
        fprintf(stderr, "Could not allocate stream\n");
        exit(1);
    }
    ost->st->id = oc->nb_streams - 1;
    c = ost->st->codec;

    switch ((*codec)->type)
    {
    case AVMEDIA_TYPE_VIDEO:
        c->codec_id = codec_id;

        c->bit_rate = 400000;
        /* Resolution must be a multiple of two. */
        c->width = g_width;
        c->height = g_height;
        /* timebase: This is the fundamental unit of time (in seconds) in terms
         * of which frame timestamps are represented. For fixed-fps content,
         * timebase should be 1/framerate and timestamp increments should be
         * identical to 1. */
        ost->st->time_base = (AVRational){1, STREAM_FRAME_RATE};
        c->time_base = ost->st->time_base;

        c->gop_size = 12; /* emit one intra frame every twelve frames at most */
        c->pix_fmt = STREAM_PIX_FMT;
        if (c->codec_id == AV_CODEC_ID_MPEG2VIDEO)
        {
            /* just for testing, we also add B frames */
            c->max_b_frames = 2;
        }
        if (c->codec_id == AV_CODEC_ID_MPEG1VIDEO)
        {
            /* Needed to avoid using macroblocks in which some coeffs overflow.
             * This does not happen with normal video, it just happens here as
             * the motion of the chroma plane does not match the luma plane. */
            c->mb_decision = 2;
        }
        break;

    default:
        break;
    }

    /* Some formats want stream headers to be separate. */
    if (oc->oformat->flags & AVFMT_GLOBALHEADER)
        c->flags |= CODEC_FLAG_GLOBAL_HEADER;
}

/**************************************************************/
/* video output */

static AVFrame* alloc_picture(enum AVPixelFormat pix_fmt, int width, int height)
{
    AVFrame* picture;
    int ret;

    picture = av_frame_alloc();
    if (!picture)
        return NULL;

    picture->format = pix_fmt;
    picture->width = width;
    picture->height = height;

    /* allocate the buffers for the frame data */
    ret = av_frame_get_buffer(picture, 32);
    if (ret < 0)
    {
        fprintf(stderr, "Could not allocate frame data.\n");
        exit(1);
    }

    return picture;
}

static void open_video(AVCodec* codec, OutputStream* ost, AVDictionary* opt_arg)
{
    int ret;
    AVCodecContext* c = ost->st->codec;
    AVDictionary* opt = NULL;

    av_dict_copy(&opt, opt_arg, 0);

    /* open the codec */
    ret = avcodec_open2(c, codec, &opt);
    av_dict_free(&opt);
    if (ret < 0)
    {
        fprintf(stderr, "Could not open video codec: %s\n", av_err2str(ret));
        exit(1);
    }

    /* allocate and init a re-usable frame */
    ost->frame = alloc_picture(c->pix_fmt, c->width, c->height);
    if (!ost->frame)
    {
        fprintf(stderr, "Could not allocate video frame\n");
        exit(1);
    }

    /* If the output format is not YUV420P, then a temporary YUV420P
     * picture is needed too. It is then converted to the required
     * output format. */
    ost->tmp_frame = NULL;
    if (c->pix_fmt != AV_PIX_FMT_YUV420P)
    {
        ost->tmp_frame = alloc_picture(AV_PIX_FMT_YUV420P, c->width, c->height);
        if (!ost->tmp_frame)
        {
            fprintf(stderr, "Could not allocate temporary picture\n");
            exit(1);
        }
    }
}

/* Prepare a dummy image. */
static void fill_yuv_image(AVFrame* pict, int width, int height)
{
    int x, y, ret;

    /* when we pass a frame to the encoder, it may keep a reference to it
     * internally;
     * make sure we do not overwrite it here
     */
    ret = av_frame_make_writable(pict);
    if (ret < 0)
        exit(1);

    /////////////////////////////////////////////////////
    XImage* image =
        XGetImage(s_display, (Drawable) g_window_id, 0, 0, width, height, AllPlanes, ZPixmap);

    // unsigned char *array = new unsigned char[width * height * 3];
    unsigned long red_mask = image->red_mask;
    unsigned long green_mask = image->green_mask;
    unsigned long blue_mask = image->blue_mask;

    unsigned long pixel;

    unsigned char blue;   //= pixel & blue_mask;
    unsigned char green;  //= (pixel & green_mask) >> 8;
    unsigned char red;    //= (pixel & red_mask) >> 16;

    // Y
    for (y = 0; y < height; y++)
    {
        for (x = 0; x < width; x++)
        {
            pixel = XGetPixel(image, x, y);
            blue = pixel & blue_mask;
            green = (pixel & green_mask) >> 8;
            red = (pixel & red_mask) >> 16;
            pict->data[0][y * pict->linesize[0] + x] =
                RGB2Y(red, green, blue);  // *(img+ x + y + i * 3);
            pict->data[1][(y / 2) * pict->linesize[1] + (x / 2)] = RGB2U(red, green, blue);
            pict->data[2][(y / 2) * pict->linesize[2] + (x / 2)] = RGB2V(red, green, blue);
        }
    }

    XDestroyImage(image);
}

static AVFrame* get_video_frame(OutputStream* ost, void* arg)
{
    AVCodecContext* c = ost->st->codec;

    /* check if we want to generate more frames
    if (av_compare_ts(ost->next_pts, ost->st->codec->time_base,
                      STREAM_DURATION, (AVRational){ 1, 1 }) >= 0)
        return NULL;*/
    pthread_mutex_t* mx = arg;
    while (needQuit(mx))
    {
        return NULL;
    }

    if (c->pix_fmt != AV_PIX_FMT_YUV420P)
    {
        /* as we only generate a YUV420P picture, we must convert it
         * to the codec pixel format if needed */
        if (!ost->sws_ctx)
        {
            ost->sws_ctx = sws_getContext(c->width, c->height, AV_PIX_FMT_YUV420P, c->width,
                                          c->height, c->pix_fmt, SCALE_FLAGS, NULL, NULL, NULL);
            if (!ost->sws_ctx)
            {
                fprintf(stderr, "Could not initialize the conversion context\n");
                exit(1);
            }
        }
        fill_yuv_image(ost->tmp_frame, c->width, c->height);
        sws_scale(ost->sws_ctx, (const uint8_t* const*) ost->tmp_frame->data,
                  ost->tmp_frame->linesize, 0, c->height, ost->frame->data, ost->frame->linesize);
    }
    else
    {
        fill_yuv_image(ost->frame, c->width, c->height);
    }

    ost->frame->pts = ost->next_pts++;

    return ost->frame;
}

/*
 * encode one video frame and send it to the muxer
 * return 1 when encoding is finished, 0 otherwise
 */
static int write_video_frame(AVFormatContext* oc, OutputStream* ost, void* arg)
{
    int ret;
    AVCodecContext* c;
    AVFrame* frame;
    int got_packet = 0;

    c = ost->st->codec;

    frame = get_video_frame(ost, arg);

    if (oc->oformat->flags & AVFMT_RAWPICTURE)
    {
        /* a hack to avoid data copy with some raw video muxers */
        AVPacket pkt;
        av_init_packet(&pkt);

        if (!frame)
            return 1;

        pkt.flags |= AV_PKT_FLAG_KEY;
        pkt.stream_index = ost->st->index;
        pkt.data = (uint8_t*) frame;
        pkt.size = sizeof(AVPicture);

        pkt.pts = pkt.dts = frame->pts;
        av_packet_rescale_ts(&pkt, c->time_base, ost->st->time_base);

        ret = av_interleaved_write_frame(oc, &pkt);
    }
    else
    {
        AVPacket pkt = {0};
        av_init_packet(&pkt);

        /* encode the image */
        ret = avcodec_encode_video2(c, &pkt, frame, &got_packet);
        if (ret < 0)
        {
            fprintf(stderr, "Error encoding video frame: %s\n", av_err2str(ret));
            exit(1);
        }

        if (got_packet)
        {
            ret = write_frame(oc, &c->time_base, ost->st, &pkt);
        }
        else
        {
            ret = 0;
        }
    }

    if (ret < 0)
    {
        fprintf(stderr, "Error while writing video frame: %s\n", av_err2str(ret));
        exit(1);
    }

    return (frame || got_packet) ? 0 : 1;
}

static void close_stream(OutputStream* ost)
{
    avcodec_close(ost->st->codec);
    av_frame_free(&ost->frame);
    av_frame_free(&ost->tmp_frame);
    sws_freeContext(ost->sws_ctx);
    swr_free(&ost->swr_ctx);
}

/**************************************************************/
/* media file output */

int ffmpeg_grabber(void* arg)
{
    OutputStream video_st = {0};
    const char* filename;
    AVOutputFormat* fmt;
    AVFormatContext* oc;
    AVCodec* video_codec;
    int ret;
    int have_video = 0;
    int encode_video = 0;
    AVDictionary* opt = NULL;

    struct thread_args* args = (struct thread_args*) arg;

    filename = args->record_filename;
    av_dict_set(&opt, "author", "aic", 0);

    /* allocate the output media context */
    avformat_alloc_output_context2(&oc, NULL, NULL, filename);
    if (!oc)
    {
        LOGM("Could not deduce output format from file extension: using MPEG. %s",
             args->record_filename);
        avformat_alloc_output_context2(&oc, NULL, "mpeg", filename);
    }
    if (!oc)
        return 1;

    fmt = oc->oformat;

    /* Add the audio and video streams using the default format codecs
    * and initialize the codecs. */
    if (fmt->video_codec != AV_CODEC_ID_NONE)
    {
        add_stream(&video_st, oc, &video_codec, fmt->video_codec);
        have_video = 1;
        encode_video = 1;
    }

    /* Now that all the parameters are set, we can open
    * video codecs and allocate the necessary encode buffers. */
    if (have_video)
        open_video(video_codec, &video_st, opt);

    av_dump_format(oc, 0, filename, 1);

    /* open the output file, if needed */
    if (!(fmt->flags & AVFMT_NOFILE))
    {
        ret = avio_open(&oc->pb, filename, AVIO_FLAG_WRITE);
        if (ret < 0)
        {
            fprintf(stderr, "Could not open '%s': %s\n", filename, av_err2str(ret));
            return 1;
        }
    }

    /* Write the stream header, if any. */
    ret = avformat_write_header(oc, &opt);
    if (ret < 0)
    {
        fprintf(stderr, "Error occurred when opening output file: %s\n", av_err2str(ret));
        return 1;
    }

    while (encode_video)
    {
        encode_video = !write_video_frame(oc, &video_st, (void*) &args->mtx);
    }

    /* Write the trailer, if any. The trailer must be written before you
     * close the CodecContexts open when you wrote the header; otherwise
     * av_write_trailer() may try to use memory that was freed on
     * av_codec_close(). */
    av_write_trailer(oc);

    /* Close each codec. */
    if (have_video)
        close_stream(&video_st);

    if (!(fmt->flags & AVFMT_NOFILE))
        /* Close the output file. */
        avio_closep(&oc->pb);

    /* free the stream */
    avformat_free_context(oc);

    return 0;
}

unsigned char* xgrabber()
{
    char T[30];
    char usec[4];
    time_t curtime;
    struct timeval tv;
    struct tm cur_localtime;
    gettimeofday(&tv, NULL);
    curtime = tv.tv_sec;
    localtime_r(&curtime, &cur_localtime);
    strftime(T, 30, "%Y/%d/%m %H:%M:%S", &cur_localtime);

    snprintf(usec, 4, "%ld", tv.tv_usec);

    unsigned char* img = NULL;
    int w, h;
    w = g_width;
    h = g_height;

    if (img)
        free(img);
    img = (unsigned char*) malloc(3 * w * h);
    if (!img)
        LOGE("xgrabber(): out of memory");
    memset(img, 0, sizeof(*img));

    char string1[BUF_SIZE];
    snprintf(string1, sizeof(string1), "%s%s", T, usec);

    XMapWindow(s_display, (Drawable) g_window_id);
    GC gc = XCreateGC(s_display, (Drawable) g_window_id, 0, 0);

    XFillRectangle(s_display, (Drawable) g_window_id, gc, 0, g_height, g_width, g_height + 100);

    XDrawString(s_display, (Drawable) g_window_id, gc, 5, g_height - 100 + 15, string1,
                strlen(string1));

    XImage* image = XGetImage(s_display, (Drawable) g_window_id, 0, 0, w, h, AllPlanes, ZPixmap);

    // unsigned char *array = new unsigned char[width * height * 3];
    unsigned long red_mask = image->red_mask;
    unsigned long green_mask = image->green_mask;
    unsigned long blue_mask = image->blue_mask;

    int x, y = 0;

    for (x = 0; x < w; x++)
    {
        for (y = 0; y < h; y++)
        {
            unsigned long pixel = XGetPixel(image, x, y);

            unsigned char blue = pixel & blue_mask;
            unsigned char green = (pixel & green_mask) >> 8;
            unsigned char red = (pixel & red_mask) >> 16;

            img[(x + w * y) * 3 + 0] = blue;
            img[(x + w * y) * 3 + 1] = green;
            img[(x + w * y) * 3 + 2] = red;
        }
    }

    XDestroyImage(image);

    /*FILE *avconv = NULL;

    avconv = popen("ffmpeg -y -f rawvideo -vcodec rawvideo -s 800x600 -pix_fmt rgb444be -r 25 -i -
    -vf vflip -an -c:v libx264 -preset slow test.mp4", "w");
    if (avconv)
        fwrite(img ,800*600*3 , 1, avconv);*/

    return img;
}

void grab_snapshot(char* snap_filename)
{
    FILE* f;
    unsigned char* img = NULL;
    int w, h;
    w = g_width;
    h = g_height;

    if (img)
        free(img);
    img = (unsigned char*) malloc(3 * w * h);
    if (!img)
        LOGE("grab_snapshot(): out of memory");
    memset(img, 0, sizeof(*img));

    XImage* image = XGetImage(s_display, (Drawable) g_window_id, 0, 0, w, h, AllPlanes, ZPixmap);

    // unsigned char *array = new unsigned char[width * height * 3];
    unsigned long red_mask = image->red_mask;
    unsigned long green_mask = image->green_mask;
    unsigned long blue_mask = image->blue_mask;

    int x, y = 0;

    for (x = 0; x < w; x++)
    {
        for (y = 0; y < h; y++)
        {
            unsigned long pixel = XGetPixel(image, x, y);

            unsigned char blue = pixel & blue_mask;
            unsigned char green = (pixel & green_mask) >> 8;
            unsigned char red = (pixel & red_mask) >> 16;

            img[(x + w * y) * 3 + 0] = blue;
            img[(x + w * y) * 3 + 1] = green;
            img[(x + w * y) * 3 + 2] = red;
        }
    }

    int filesize = 54 + 3 * g_width * g_height;
    unsigned char bmpfileheader[14] = {'B', 'M', 0, 0, 0, 0, 0, 0, 0, 0, 54, 0, 0, 0};
    unsigned char bmpinfoheader[40] = {40, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 24, 0};
    unsigned char bmppad[3] = {0, 0, 0};

    bmpfileheader[2] = (unsigned char) (filesize);
    bmpfileheader[3] = (unsigned char) (filesize >> 8);
    bmpfileheader[4] = (unsigned char) (filesize >> 16);
    bmpfileheader[5] = (unsigned char) (filesize >> 24);

    bmpinfoheader[4] = (unsigned char) (w);
    bmpinfoheader[5] = (unsigned char) (w >> 8);
    bmpinfoheader[6] = (unsigned char) (w >> 16);
    bmpinfoheader[7] = (unsigned char) (w >> 24);
    bmpinfoheader[8] = (unsigned char) (h);
    bmpinfoheader[9] = (unsigned char) (h >> 8);
    bmpinfoheader[10] = (unsigned char) (h >> 16);
    bmpinfoheader[11] = (unsigned char) (h >> 24);

    f = fopen(snap_filename, "wb");
    fwrite(bmpfileheader, 1, 14, f);
    fwrite(bmpinfoheader, 1, 40, f);

    int ii = 0;

    for (ii = 0; ii < g_height; ii++)
    {
        fwrite(img + (g_width * (g_height - ii - 1) * 3), 3, g_width, f);
        fwrite(bmppad, 1, (4 - (g_width * 3) % 4) % 4, f);
    }
    fclose(f);
    XDestroyImage(image);
    free(img);
}

int precv(void* arg)
{
    s_read_args* args = (struct read_args*) arg;

    while (1)
    {
        pthread_mutex_lock(&args->mtx);
        fd_set forread;

        FD_ZERO(&forread);
        FD_SET(args->sock, &forread);
        if (select(args->sock + 1, &forread, 0, 0, 0) == -1)
        {
            LOGW(" error select()");
            return 0;
        }
        if (FD_ISSET(args->sock, &forread))
        {
            args->len = 0;
            args->len =
                recv(args->sock, args->buffer, READ_BUFFER_SIZE * sizeof(*args->buffer), MSG_PEEK);
            if (args->len)
            {
                LOGW("select()1 %d", args->len);
                args->len = recv(args->sock, args->buffer, args->len, MSG_WAITFORONE);
                args->flagSnapRec = 1;
                LOGW("select()2 %d %s", args->len, args->buffer);
                pthread_cond_signal(&args->cond);
            }
            pthread_mutex_unlock(&args->mtx);
        }
        struct timespec duration = {0, 250000};
        nanosleep(&duration, NULL);
    }
    return 0;
}

void* pgrab(void* arg)
{
    struct timespec tv;
    // tv.tv_sec = 1;
    tv.tv_nsec = 100 * 1000000;  // 100ms

    s_thread_args grab_args;
    pthread_t pgrab_Thread;

    RecordingPayload* recData;

    struct stat st = {0};
    char base_path[BUF_SIZE] = "./log/";
    g_strlcpy(base_path, s_path_results, sizeof(base_path));
    LOGI("grabber base path: %s", base_path);

    if (stat(base_path, &st) == -1)
    {
        mkdir(base_path, 0700);
    }

    s_read_args* args = (struct read_args*) arg;

    pthread_mutex_lock(&args->mtx);
    while (1)
    {
        pthread_cond_timedwait(&args->cond, &args->mtx, &tv);
        if (args->len && args->flagSnapRec)
        {
            char str_path[BUF_SIZE];
            args->flagSnapRec = 0;
            LOGW("%d %s flagRecording=%d", args->len, args->buffer, args->flagRecording);
            recData = recording_payload__unpack(NULL, args->len, args->buffer);

            LOGM(" recData->recFilename=%s recData.startStop=%d ", recData->recfilename,
                 recData->startstop);
            if (!strncmp("video", recData->recfilename, 5))
            {
                snprintf(grab_args.record_filename, sizeof(str_path), "%s%s", base_path,
                         recData->recfilename);

                if (recData->startstop && !args->flagRecording)
                {
                    pthread_mutex_init(&grab_args.mtx, NULL);
                    pthread_mutex_lock(&grab_args.mtx);
                    pthread_create(&pgrab_Thread, NULL, (void*) &ffmpeg_grabber, &grab_args);
                    args->flagRecording = 1;
                }
                else if (!recData->startstop && args->flagRecording)
                {
                    pthread_mutex_unlock(&grab_args.mtx);
                    pthread_join(pgrab_Thread, NULL);
                    args->flagRecording = 0;
                }
            }
            else if (!strncmp("snap", recData->recfilename, 4) && recData->startstop == 2)
            {
                snprintf(str_path, sizeof(str_path), "%s%s", base_path, recData->recfilename);
                grab_snapshot(str_path);
            }  // end video/snap
        }
    }
    pthread_mutex_unlock(&args->mtx);
}

void* grab_handler_sock(void* args)
{
    sensor_params* data;
    data = args;

    s_read_args* r_args = (struct read_args*) malloc(sizeof(s_read_args));

    socket_t player_fd;
    int flag_connect = 0;

    uint8_t* read_buffer = (uint8_t*) malloc(sizeof(uint8_t) * READ_BUFFER_SIZE);
    if (!r_args || !read_buffer)
        LOGE("grab_handler_sock(): out of memory");

    flag_connect = 0;
    data->flagRecording = 0;
    r_args->flagRecording = data->flagRecording;
    while (!flag_connect)
    {
        player_fd = open_socket(data->gvmip, PORT_GRAB);
        if (player_fd == SOCKET_ERROR)
        {
            close(player_fd);
            sleep(1);
        }
        else
        {
            LOGD("Connected to aicTest (TCP %d)", PORT_GRAB);
            flag_connect = 1;
        }
    }
    r_args->buffer = read_buffer;
    r_args->sock = player_fd;
    r_args->len = 0;

    pthread_t pread_Thread1, pread_Thread2;
    pthread_mutex_init(&r_args->mtx, NULL);
    pthread_create(&pread_Thread1, NULL, (void*) &precv, r_args);
    pthread_create(&pread_Thread2, NULL, (void*) &pgrab, r_args);

    return NULL;
}

void* grab_handler_amqp(void* args)
{
    amqp_envelope_t envelope;  // envelope.message.body = amqp_bytes_malloc ( ) ;
    sensor_params* data;

    data = args;
    RecordingPayload* recData;

    s_thread_args grab_args;

    struct stat st = {0};
    char base_path[BUF_SIZE] = "./log/";
    pthread_t pgrab_Thread;

    if (stat(base_path, &st) == -1)
    {
        mkdir(base_path, 0700);
    }

    amqp_connection_state_t conn;
    amqp_listen_retry(data->amqp_host, 5672, data->queue, &conn, 5);
    while (1)
    {
        int err_amqlisten = amqp_consume(&conn, &envelope);
        if (err_amqlisten == 0)
        {
            if (envelope.message.properties._flags & AMQP_BASIC_CONTENT_TYPE_FLAG)
            {
                recData = recording_payload__unpack(NULL, envelope.message.body.len,
                                                    envelope.message.body.bytes);
                LOGM(" recData->mpegFilename=%s recData.startStop=%d ", recData->recfilename,
                     recData->startstop);
                if (!strncmp("video", recData->recfilename, 5))
                {
                    snprintf(grab_args.record_filename, sizeof(grab_args.record_filename), "%s%s",
                             base_path, recData->recfilename);

                    if (recData->startstop && !data->flagRecording)
                    {
                        pthread_mutex_init(&grab_args.mtx, NULL);
                        pthread_mutex_lock(&grab_args.mtx);
                        pthread_create(&pgrab_Thread, NULL, (void*) &ffmpeg_grabber, &grab_args);
                        data->flagRecording = 1;
                    }
                    else if (!recData->startstop && data->flagRecording)
                    {
                        pthread_mutex_unlock(&grab_args.mtx);
                        pthread_join(pgrab_Thread, NULL);
                        data->flagRecording = 0;
                    }
                }
                else if (!strncmp("snap", recData->recfilename, 4) && recData->startstop == 2)
                {
                    char str_path[BUF_SIZE];
                    snprintf(str_path, sizeof(str_path), "%s%s", base_path, recData->recfilename);
                    grab_snapshot(str_path);
                }  // end video/snap
            }      // end ifenvelope
        }          // end if err_amqlisten
        sleep(1);
    }  // end while
}
