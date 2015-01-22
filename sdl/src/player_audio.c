/**
 * \file player_audio.c
 * \brief AiC audio player, reads an audio stream from the vm and transfers
 *  it to ffmpeg.
 *
 *  Based on ffmpeg API examples..
 */
#include <errno.h>                     // for ENOMEM
#include <libavcodec/avcodec.h>        // for AVCodecContext, AVPacket, AVCodec
#include <libavutil/avassert.h>        // for av_assert0
#include <libavutil/avutil.h>          // for AVMediaType::AVMEDIA_TYPE_AUDIO
#include <libavutil/channel_layout.h>  // for AV_CH_LAYOUT_STEREO, av_get_ch...
#include <libavutil/common.h>          // for FFMIN
#include <libavutil/dict.h>            // for AVDictionary, av_dict_copy
#include <libavutil/error.h>           // for AVERROR_EXIT, AVERROR, av_err2str
#include <libavutil/mem.h>             // for av_free, av_freep
#include <libavutil/rational.h>        // for AVRational
#include <libavutil/samplefmt.h>       // for AVSampleFormat::AV_SAMPLE_FMT_FLT
#include <libswresample/swresample.h>  // for swr_free, swr_init, SwrContext
#include <libswscale/swscale.h>        // for sws_freeContext
#include <stdint.h>                    // for uint8_t, uint64_t
#include <stdio.h>                     // for printf, NULL, fprintf, stderr
#include <stdlib.h>                    // for exit, calloc, free, malloc
#include <string.h>                    // for memcpy, memmove
#include <unistd.h>                    // sleep, close
#include <libavformat/avformat.h>      // for AVFormatContext, AVStream, AVO...
#include <libavformat/avio.h>          // for avio_closep, avio_open, AVIO_F...
#include <libavutil/audio_fifo.h>      // for av_audio_fifo_size, AVAudioFifo
#include <libavutil/frame.h>           // for AVFrame, av_frame_free, av_fra...
#include <libavutil/opt.h>             // for av_opt_set_int, av_opt_set_sam...

#include "socket.h"
#include "logger.h"
#include "config_env.h"

#define LOG_TAG "audio"

/** Port open on the VM */
#define ANDROIDINCLOUD_PCM_CLIENT_PORT 24296

const char* get_error_text(const int error)
{
    static char error_buffer[255];
    av_strerror(error, error_buffer, sizeof(error_buffer));
    return error_buffer;
}

/** Initialize one data packet for reading or writing. */
void init_packet(AVPacket* packet)
{
    av_init_packet(packet);
    /** Set the packet data and size so that it is recognized as being empty. */
    packet->data = NULL;
    packet->size = 0;
}

/** Initialize one audio frame for reading from the input file */
int init_input_frame(AVFrame** frame)
{
    if (!(*frame = av_frame_alloc()))
    {
        printf("Could not allocate input frame\n");
        return AVERROR(ENOMEM);
    }
    return 0;
}

/**
 * Initialize the audio resampler based on the input and output codec settings.
 * If the input and output sample formats differ, a conversion is required
 * libswresample takes care of this, but requires initialization.
 */
int init_resampler(AVCodecContext* input_codec_context, AVCodecContext* output_codec_context,
                   SwrContext** resample_context)
{
    int error;

    /**
     * Create a resampler context for the conversion.
     * Set the conversion parameters.
     * Default channel layouts based on the number of channels
     * are assumed for simplicity (they are sometimes not detected
     * properly by the demuxer and/or decoder).
     */
    *resample_context = swr_alloc_set_opts(
        NULL, av_get_default_channel_layout(output_codec_context->channels),
        output_codec_context->sample_fmt, output_codec_context->sample_rate,
        av_get_default_channel_layout(input_codec_context->channels),
        input_codec_context->sample_fmt, input_codec_context->sample_rate, 0, NULL);
    if (!*resample_context)
    {
        printf("Could not allocate resample context\n");
        return AVERROR(ENOMEM);
    }
    /**
    * Perform a sanity check so that the number of converted samples is
    * not greater than the number of samples to be converted.
    * If the sample rates differ, this case has to be handled differently
    */
    av_assert0(output_codec_context->sample_rate == input_codec_context->sample_rate);

    /** Open the resampler with the specified parameters. */
    if ((error = swr_init(*resample_context)) < 0)
    {
        printf("Could not open resample context\n");
        swr_free(resample_context);
        return error;
    }
    return 0;
}

/** Initialize a FIFO buffer for the audio samples to be encoded. */
int init_fifo(AVAudioFifo** fifo, AVCodecContext* output_codec_context)
{
    /** Create the FIFO buffer based on the specified output sample format. */
    if (!(*fifo = av_audio_fifo_alloc(output_codec_context->sample_fmt,
                                      output_codec_context->channels, 1)))
    {
        printf("Could not allocate FIFO\n");
        return AVERROR(ENOMEM);
    }
    return 0;
}

/** Decode one audio frame from the input file. */
int decode_audio_frame(AVFrame* frame, AVFormatContext* input_format_context,
                       AVCodecContext* input_codec_context, int* data_present, int* finished)
{
    /** Packet used for temporary storage. */
    AVPacket input_packet;
    int error;
    init_packet(&input_packet);

    /** Read one audio frame from the input file into a temporary packet. */
    if ((error = av_read_frame(input_format_context, &input_packet)) < 0)
    {
        /** If we are at the end of the file, flush the decoder below. */
        if (error == AVERROR_EOF)
            *finished = 1;
        else
        {
            printf("Could not read frame (error '%s')\n", get_error_text(error));
            return error;
        }
    }

    /**
     * Decode the audio frame stored in the temporary packet.
     * The input audio stream decoder is used to do this.
     * If we are at the end of the file, pass an empty packet to the decoder
     * to flush it.
     */
    if ((error = avcodec_decode_audio4(input_codec_context, frame, data_present, &input_packet)) <
        0)
    {
        printf("Could not decode frame (error '%s')\n", get_error_text(error));
        av_free_packet(&input_packet);
        return error;
    }

    /**
     * If the decoder has not been flushed completely, we are not finished,
     * so that this function has to be called again.
     */
    if (*finished && *data_present)
        *finished = 0;
    av_free_packet(&input_packet);
    return 0;
}

/**
 * Initialize a temporary storage for the specified number of audio samples.
 * The conversion requires temporary storage due to the different format.
 * The number of audio samples to be allocated is specified in frame_size.
 */
int init_converted_samples(uint8_t*** converted_input_samples, AVCodecContext* output_codec_context,
                           int frame_size)
{
    int error;

    /**
     * Allocate as many pointers as there are audio channels.
     * Each pointer will later point to the audio samples of the corresponding
     * channels (although it may be NULL for interleaved formats).
     */
    if (!(*converted_input_samples =
              calloc(output_codec_context->channels, sizeof(**converted_input_samples))))
    {
        printf("Could not allocate converted input sample pointers\n");
        return AVERROR(ENOMEM);
    }

    /**
     * Allocate memory for the samples of all channels in one consecutive
     * block for convenience.
     */
    if ((error = av_samples_alloc(*converted_input_samples, NULL, output_codec_context->channels,
                                  frame_size, output_codec_context->sample_fmt, 0)) < 0)
    {
        fprintf(stderr, "Could not allocate converted input samples (error '%s')\n",
                get_error_text(error));
        av_freep(&(*converted_input_samples)[0]);
        free(*converted_input_samples);
        return error;
    }
    return 0;
}

/**
 * Convert the input audio samples into the output sample format.
 * The conversion happens on a per-frame basis, the size of which is specified
 * by frame_size.
 */
int convert_samples(const uint8_t** input_data, uint8_t** converted_data, const int frame_size,
                    SwrContext* resample_context)
{
    int error;

    /** Convert the samples using the resampler. */
    if ((error =
             swr_convert(resample_context, converted_data, frame_size, input_data, frame_size)) < 0)
    {
        printf("Could not convert input samples (error '%s')\n", get_error_text(error));
        return error;
    }

    return 0;
}

/** Add converted input audio samples to the FIFO buffer for later processing. */
int add_samples_to_fifo(AVAudioFifo* fifo, uint8_t** converted_input_samples, const int frame_size)
{
    int error;

    /**
     * Make the FIFO as large as it needs to be to hold both,
     * the old and the new samples.
     */
    if ((error = av_audio_fifo_realloc(fifo, av_audio_fifo_size(fifo) + frame_size)) < 0)
    {
        printf("Could not reallocate FIFO\n");
        return error;
    }

    /** Store the new samples in the FIFO buffer. */
    if (av_audio_fifo_write(fifo, (void**) converted_input_samples, frame_size) < frame_size)
    {
        printf("Could not write data to FIFO\n");
        return AVERROR_EXIT;
    }
    return 0;
}

/**
 * Initialize one input frame for writing to the output file.
 * The frame will be exactly frame_size samples large.
 */
int init_output_frame(AVFrame** frame, AVCodecContext* output_codec_context, int frame_size)
{
    int error;

    /** Create a new frame to store the audio samples. */
    if (!(*frame = av_frame_alloc()))
    {
        printf("Could not allocate output frame\n");
        return AVERROR_EXIT;
    }

    /**
     * Set the frame's parameters, especially its size and format.
     * av_frame_get_buffer needs this to allocate memory for the
     * audio samples of the frame.
     * Default channel layouts based on the number of channels
     * are assumed for simplicity.
     */
    (*frame)->nb_samples = frame_size;
    (*frame)->channel_layout = output_codec_context->channel_layout;
    (*frame)->format = output_codec_context->sample_fmt;
    (*frame)->sample_rate = output_codec_context->sample_rate;

    /**
     * Allocate the samples of the created frame. This call will make
     * sure that the audio frame can hold as many samples as specified.
     */
    if ((error = av_frame_get_buffer(*frame, 0)) < 0)
    {
        printf("Could allocate output frame samples (error '%s')\n", get_error_text(error));
        av_frame_free(frame);
        return error;
    }

    return 0;
}

/** Global timestamp for the audio frames */
static int64_t pts = 0;

/** Encode one frame worth of audio to the output file. */
int encode_audio_frame(AVFrame* frame, AVFormatContext* output_format_context,
                       AVCodecContext* output_codec_context, int* data_present)
{
    /** Packet used for temporary storage. */
    AVPacket output_packet;
    int error;
    init_packet(&output_packet);

    /** Set a timestamp based on the sample rate for the container. */
    if (frame)
    {
        frame->pts = pts;
        pts += frame->nb_samples;
    }

    /**
     * Encode the audio frame and store it in the temporary packet.
     * The output audio stream encoder is used to do this.
     */
    if ((error = avcodec_encode_audio2(output_codec_context, &output_packet, frame, data_present)) <
        0)
    {
        printf("Could not encode frame (error '%s')\n", get_error_text(error));
        av_free_packet(&output_packet);
        return error;
    }

    /** Write one audio frame from the temporary packet to the output file. */
    if (*data_present)
    {
        if ((error = av_write_frame(output_format_context, &output_packet)) < 0)
        {
            printf("Could not write frame (error '%s')\n", get_error_text(error));
            av_free_packet(&output_packet);
            return error;
        }

        av_free_packet(&output_packet);
    }

    return 0;
}

/**
 * Load one audio frame from the FIFO buffer, encode and write it to the
 * output file.
 */
int load_encode_and_write(AVAudioFifo* fifo, AVFormatContext* output_format_context,
                          AVCodecContext* output_codec_context)
{
    /** Temporary storage of the output samples of the frame written to the file. */
    AVFrame* output_frame;
    /**
     * Use the maximum number of possible samples per frame.
     * If there is less than the maximum possible frame size in the FIFO
     * buffer use this number. Otherwise, use the maximum possible frame size
     */
    const int frame_size = FFMIN(av_audio_fifo_size(fifo), output_codec_context->frame_size);
    int data_written;

    /** Initialize temporary storage for one output frame. */
    if (init_output_frame(&output_frame, output_codec_context, frame_size))
        return AVERROR_EXIT;

    /**
     * Read as many samples from the FIFO buffer as required to fill the frame.
     * The samples are stored in the frame temporarily.
     */
    if (av_audio_fifo_read(fifo, (void**) output_frame->data, frame_size) < frame_size)
    {
        printf("Could not read data from FIFO\n");
        av_frame_free(&output_frame);
        return AVERROR_EXIT;
    }

    /** Encode one frame worth of audio samples. */
    if (encode_audio_frame(output_frame, output_format_context, output_codec_context,
                           &data_written))
    {
        av_frame_free(&output_frame);
        return AVERROR_EXIT;
    }
    av_frame_free(&output_frame);
    return 0;
}

/** Write the trailer of the output file container. */
int write_output_file_trailer(AVFormatContext* output_format_context)
{
    int error;
    if ((error = av_write_trailer(output_format_context)) < 0)
    {
        printf("Could not write output file trailer (error '%s')\n", get_error_text(error));
        return error;
    }
    return 0;
}

// a wrapper around a single output AVStream
typedef struct OutputStream
{
    AVStream* st;

    /* pts of the next frame that will be generated */
    int64_t next_pts;
    int samples_count;

    AVFrame* frame;
    AVFrame* tmp_frame;

    float t, tincr, tincr2;

    struct SwsContext* sws_ctx;
    struct SwrContext* swr_ctx;
} OutputStream;

#if 0
static void log_packet(const AVFormatContext* fmt_ctx, const AVPacket* pkt)
{
    AVRational* time_base = &fmt_ctx->streams[pkt->stream_index]->time_base;

    printf("pts:%s pts_time:%s dts:%s dts_time:%s duration:%s duration_time:%s stream_index:%d\n",
           av_ts2str(pkt->pts), av_ts2timestr(pkt->pts, time_base), av_ts2str(pkt->dts),
           av_ts2timestr(pkt->dts, time_base), av_ts2str(pkt->duration),
           av_ts2timestr(pkt->duration, time_base), pkt->stream_index);
}
#endif

/* Add an output stream. */
static void add_stream(OutputStream* ost, AVFormatContext* oc, AVCodec** codec,
                       enum AVCodecID codec_id)
{
    AVCodecContext* c;
    int i;

    /* find the encoder */
    *codec = avcodec_find_encoder(codec_id);
    if (!(*codec))
    {
        printf("Could not find encoder for '%s'\n", avcodec_get_name(codec_id));
        exit(1);
    }

    ost->st = avformat_new_stream(oc, *codec);
    if (!ost->st)
    {
        printf("Could not allocate stream\n");
        exit(1);
    }
    ost->st->id = oc->nb_streams - 1;
    c = ost->st->codec;
    switch ((*codec)->type)
    {
    case AVMEDIA_TYPE_AUDIO:
        c->sample_fmt = (*codec)->sample_fmts ? (*codec)->sample_fmts[0] : AV_SAMPLE_FMT_FLT;
        c->bit_rate = 64000;
        c->sample_rate = 44100;
        if ((*codec)->supported_samplerates)
        {
            c->sample_rate = (*codec)->supported_samplerates[0];
            for (i = 0; (*codec)->supported_samplerates[i]; i++)
            {
                if ((*codec)->supported_samplerates[i] == 44100)
                    c->sample_rate = 44100;
            }
        }
        c->channels = av_get_channel_layout_nb_channels(c->channel_layout);
        c->channel_layout = AV_CH_LAYOUT_STEREO;
        if ((*codec)->channel_layouts)
        {
            c->channel_layout = (*codec)->channel_layouts[0];
            for (i = 0; (*codec)->channel_layouts[i]; i++)
            {
                if ((*codec)->channel_layouts[i] == AV_CH_LAYOUT_STEREO)
                    c->channel_layout = AV_CH_LAYOUT_STEREO;
            }
        }
        ost->st->time_base = (AVRational){1, c->sample_rate};
        break;

    default:
        break;
    }

    /* Some formats want stream headers to be separate. */
    // if (oc->oformat->flags & AVFMT_GLOBALHEADER)
    c->flags |= CODEC_FLAG_GLOBAL_HEADER;
}

/**************************************************************/
/* audio output */

static AVFrame* alloc_audio_frame(enum AVSampleFormat sample_fmt, uint64_t channel_layout,
                                  int sample_rate, int nb_samples)
{
    AVFrame* frame = av_frame_alloc();

    if (!frame)
    {
        printf("Error allocating an audio frame\n");
        exit(1);
    }

    frame->format = sample_fmt;
    frame->channel_layout = channel_layout;
    frame->sample_rate = sample_rate;
    frame->nb_samples = nb_samples;

    if (nb_samples)
    {
        int ret;
        ret = av_frame_get_buffer(frame, 0);
        if (ret < 0)
        {
            printf("Error allocating an audio buffer\n");
            exit(1);
        }
    }

    return frame;
}

static void open_audio(AVCodec* codec, OutputStream* ost, AVDictionary* opt_arg)
{
    AVCodecContext* c;
    int nb_samples;
    int ret;
    AVDictionary* opt = NULL;

    c = ost->st->codec;

    /* open it */
    av_dict_copy(&opt, opt_arg, 0);
    ret = avcodec_open2(c, codec, &opt);
    av_dict_free(&opt);
    if (ret < 0)
    {
        printf("Could not open audio codec: %s\n", av_err2str(ret));
        exit(1);
    }

    /* init signal generator */
    // ost->t     = 0;
    // ost->tincr = 2 * M_PI * 550.0 / c->sample_rate;
    /* increment frequency by 110 Hz per second */
    // ost->tincr2 = 2 * M_PI * 550.0 / c->sample_rate / c->sample_rate;

    nb_samples = c->frame_size;

    ost->frame = alloc_audio_frame(c->sample_fmt, c->channel_layout, c->sample_rate, nb_samples);
    ost->tmp_frame =
        alloc_audio_frame(AV_SAMPLE_FMT_FLT, c->channel_layout, c->sample_rate, nb_samples);

    /* create resampler context */
    ost->swr_ctx = swr_alloc();
    if (!ost->swr_ctx)
    {
        printf("Could not allocate resampler context\n");
        exit(1);
    }

    /* set options */
    av_opt_set_int(ost->swr_ctx, "in_channel_count", c->channels, 0);
    av_opt_set_int(ost->swr_ctx, "in_sample_rate", c->sample_rate, 0);
    av_opt_set_sample_fmt(ost->swr_ctx, "in_sample_fmt", AV_SAMPLE_FMT_FLT, 0);
    av_opt_set_int(ost->swr_ctx, "out_channel_count", c->channels, 0);
    av_opt_set_int(ost->swr_ctx, "out_sample_rate", c->sample_rate, 0);
    av_opt_set_sample_fmt(ost->swr_ctx, "out_sample_fmt", AV_SAMPLE_FMT_FLT, 0);

    /* initialize the resampling context */
    if (swr_init(ost->swr_ctx) < 0)
    {
        printf("Failed to initialize the resampling context\n");
        exit(1);
    }
}

void audiolive_decode_encode(AVFormatContext* oc, OutputStream* ost, unsigned char* inbuf,
                             int num_read, int size)
{
    avcodec_register_all();

    AVCodec* codec;
    AVCodecContext* codecContext = NULL;

    uint8_t memBuffer[size + FF_INPUT_BUFFER_PADDING_SIZE];
    AVPacket memPkt;
    AVFrame* decoded_frame_mem = NULL;

    av_init_packet(&memPkt);

    // printf("Audio decoding \n");

    /* find the mpeg audio decoder */
    codec = avcodec_find_decoder(AV_CODEC_ID_PCM_S16LE);
    if (!codec)
    {
        printf("codec not found\n");
        exit(1);
    }

    codecContext = avcodec_alloc_context3(codec);

    codecContext->bit_rate = 64000;
    codecContext->sample_fmt = AV_SAMPLE_FMT_S16;
    codecContext->sample_rate = 44100;  // select_sample_rate(codec);
    codecContext->channel_layout = AV_CH_LAYOUT_STEREO;
    codecContext->channels = av_get_channel_layout_nb_channels(codecContext->channel_layout);

    /* open it */
    if (avcodec_open2(codecContext, codec, NULL) < 0)
    {
        printf("could not open codec\n");
        exit(1);
    }

    /* decode until eof */
    memPkt.data = memBuffer;
    memPkt.size = num_read;
    memcpy(memBuffer, inbuf, num_read);

    AVCodecContext* output_codec_context = NULL;
    SwrContext* resample_context = NULL;
    uint8_t** converted_input_samples = NULL;
    AVAudioFifo* fifo = NULL;

    output_codec_context = ost->st->codec;
    init_fifo(&fifo, output_codec_context);
    init_resampler(codecContext, output_codec_context, &resample_context);

    while (memPkt.size > 0)
    {
        int got_frame_mem = 0;
        int length;

        if (!decoded_frame_mem)
        {
            if (!(decoded_frame_mem = av_frame_alloc()))
            {
                printf("out of memory\n");
                exit(1);
            }
        }
        else
        {
            av_frame_unref(decoded_frame_mem);
        }

        length = avcodec_decode_audio4(codecContext, decoded_frame_mem, &got_frame_mem, &memPkt);

        if (length < 0)
        {
            printf("Error while decoding\n");
            exit(1);
        }
        if (got_frame_mem)
        {
            av_samples_get_buffer_size(NULL, codecContext->channels, decoded_frame_mem->nb_samples,
                                       codecContext->sample_fmt, 1);

            output_codec_context->frame_size = decoded_frame_mem->nb_samples;

            /** Use the encoder's desired frame size for processing. */
            const int output_frame_size = output_codec_context->frame_size;
            int finished = 0;

            /**
             * Make sure that there is one frame worth of samples in the FIFO
             * buffer so that the encoder can do its work.
             * Since the decoder's and the encoder's frame size may differ, we
             * need to FIFO buffer to store as many frames worth of input samples
             * that they make up at least one frame worth of output samples.
             */
            while (av_audio_fifo_size(fifo) < output_frame_size)
            {
                // printf("Audio decoding  output_frame_size %d %d %d\n", output_frame_size,
                // av_audio_fifo_size(fifo) , finished);
                /**
                 * Decode one frame worth of audio samples, convert it to the
                 * output sample format and put it into the FIFO buffer.
                 */

                if (init_converted_samples(&converted_input_samples, output_codec_context,
                                           decoded_frame_mem->nb_samples))
                    printf("Audio decoding A\n");

                /**
                 * Convert the input samples to the desired output sample format.
                 * This requires a temporary storage provided by converted_input_samples.
                 */

                if (convert_samples((const uint8_t**) decoded_frame_mem->extended_data,
                                    converted_input_samples, decoded_frame_mem->nb_samples,
                                    resample_context))
                    printf("Audio decoding B\n");

                /** Add the converted input samples to the FIFO buffer for later processing. */
                if (add_samples_to_fifo(fifo, converted_input_samples,
                                        decoded_frame_mem->nb_samples))
                    printf("Audio decoding C\n");

                /**
                 * If we are at the end of the input file, we continue
                 * encoding the remaining audio samples to the output file.
                 */
                if (finished)
                    break;
            }

            /**
             * If we have enough samples for the encoder, we encode them.
             * At the end of the file, we pass the remaining samples to
             * the encoder.
             */

            while (av_audio_fifo_size(fifo) >= output_frame_size ||
                   (finished && av_audio_fifo_size(fifo) > 0))
                /**
                 * Take one frame worth of audio samples from the FIFO buffer,
                 * encode it and write it to the output file.
                 */
                // printf("Audio decoding  output_frame_size %d %d %d\n", output_frame_size,
                // av_audio_fifo_size(fifo) , finished);
                if (load_encode_and_write(fifo, oc, output_codec_context))
                    printf("Audio decoding D\n");
        }

        // write_output_file_trailer(oc);

        memPkt.size -= length;
        memPkt.data += length;

        if (memPkt.size < 4090)
        {
            /* Refill the input buffer, to avoid trying to decode
             * incomplete frames. Instead of this, one could also use
             * a parser, or use a proper container format through
             * libavformat. */
            memmove(inbuf, memPkt.data, memPkt.size);
            memPkt.data = inbuf;
            length = 0;  // size - memPkt.size;
            if (length > 0)
                memPkt.size += length;
        }
    }

    avcodec_close(codecContext);
    av_free(codecContext);
    // avcodec_free_frame(&decoded_frame_mem);
}

/*
 * encode one audio frame and send it to the muxer
 * return 1 when encoding is finished, 0 otherwise
 */
static int read_audio_frame(AVFormatContext* oc, OutputStream* ost, socket_t outsocket)
{
    unsigned char* buffer;
    int size = 65535;
    int num_read = 1;

    buffer = malloc(size);

    ///    FILE*   memoutfile = fopen("ffplay-f_s16le-ar_44100-ac_2.wav", "wb");
    do
    {
        num_read = recv(outsocket, buffer, size, 0);
        audiolive_decode_encode(oc, ost, buffer, num_read, size);
    } while (num_read > 0);
    ///    fclose(memoutfile);

    return 0;
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
void* aic_audioplayer(char* vmip)
{
    OutputStream audio_st = {0};
    const char* filename;
    AVOutputFormat* fmt;
    AVFormatContext* oc;
    AVCodec* audio_codec;
    int ret;
    int have_audio = 0;
    AVDictionary* opt = NULL;

    LOGI("avcodec - register all formats and codecs");
    av_register_all();
    avformat_network_init();

    filename = "http://ffserver:8090/audio.ffm";

    /* allocate the output media context */
    avformat_alloc_output_context2(&oc, NULL, "ffm", filename);

    if (!oc)
    {
        printf("Could not deduce output format from file extension: using OGG.\n");
        avformat_alloc_output_context2(&oc, NULL, "ogg", filename);
    }
    if (!oc)
        return NULL;

    fmt = oc->oformat;

    /* Add the audio and video streams using the default format codecs
     * and initialize the codecs. */
    if (fmt->audio_codec != AV_CODEC_ID_NONE)
    {
        add_stream(&audio_st, oc, &audio_codec, AV_CODEC_ID_VORBIS);
        have_audio = 1;
        LOGW("No audio stream");
    }

    /* Now that all the parameters are set, we can open the audio and
     * video codecs and allocate the necessary encode buffers. */
    if (have_audio)
        open_audio(audio_codec, &audio_st, opt);
    else
        return NULL;

    /* open the output file, if needed */
    if (!(fmt->flags & AVFMT_NOFILE))
    {
        ret = avio_open(&oc->pb, filename, AVIO_FLAG_WRITE);
        if (ret < 0)
        {
            printf("Could not open '%s': %s\n", filename, av_err2str(ret));
            return NULL;
        }
    }

    /* Write the stream header, if any. */
    ret = avformat_write_header(oc, &opt);
    if (ret < 0)
    {
        printf("Error occurred when opening output file: %s\n", av_err2str(ret));
        return NULL;
    }
    av_dump_format(oc, 0, filename, 1);

    socket_t outsocket = open_socket(vmip, ANDROIDINCLOUD_PCM_CLIENT_PORT);

    while (outsocket == -1)
    {
        LOGI(" B  outsocket -> %d", outsocket);
        close(outsocket);
        sleep(1);
        outsocket = open_socket(vmip, ANDROIDINCLOUD_PCM_CLIENT_PORT);
    }

    read_audio_frame(oc, &audio_st, outsocket);
    close(outsocket);

    /* Write the trailer, if any. The trailer must be written before you
     * close the CodecContexts open when you wrote the header; otherwise
     * av_write_trailer() may try to use memory that was freed on
     * av_codec_close(). */
    write_output_file_trailer(oc);

    /* Close each codec. */
    if (have_audio)
        close_stream(&audio_st);

    if (!(fmt->flags & AVFMT_NOFILE))
        /* Close the output file. */
        avio_closep(&oc->pb);

    /* free the stream */
    avformat_free_context(oc);

    return NULL;
}

int main()
{
    char* g_vmip = NULL;
    g_vmip = configvar_string("AIC_PLAYER_VM_HOST");
    aic_audioplayer(g_vmip);

    return 0;
}
