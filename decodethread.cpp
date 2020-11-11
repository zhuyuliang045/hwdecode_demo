#include "decodethread.h"

#include <stdio.h>
#include <QImage>
#include <QDebug>

static AVBufferRef *hw_device_ctx = nullptr;
static enum AVPixelFormat hw_pix_fmt;
static FILE *output_file = nullptr;

void DecodeThread::yuv420_to_rgb24(unsigned char *pY, unsigned char *pU, unsigned char *pV, unsigned char *pRGB24, int width, int height)
{
    int y, u, v;
    int off_set = 0;
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            y = i * width + j;
            v = (i / 4) * width + j / 2;
            u = (i / 4) * width + j / 2;

            int R = (pY[y] - 16) + 1.370805 * (pV[u] - 128);
            int G = (pY[y] - 16) - 0.69825 * (pV[u] - 128) - 0.33557 * (pU[v] - 128);
            int B = (pY[y] - 16) + 1.733221 * (pU[v] - 128);

            R = R < 255 ? R : 255;
            G = G < 255 ? G : 255;
            B = B < 255 ? B : 255;

            R = R < 0 ? 0 : R;
            G = G < 0 ? 0 : G;
            B = B < 0 ? 0 : B;

            pRGB24[off_set++] = (unsigned char)R;
            pRGB24[off_set++] = (unsigned char)G;
            pRGB24[off_set++] = (unsigned char)B;
        }
    }
}

int DecodeThread::hw_decoder_init(AVCodecContext *ctx, const enum AVHWDeviceType type)
{
    int err = 0;

    if ((err = av_hwdevice_ctx_create(&hw_device_ctx, type,
                                      nullptr, nullptr, 0)) < 0) {
        fprintf(stderr, "Failed to create specified HW device.\n");
        return err;
    }
    ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);

    return err;
}

enum AVPixelFormat DecodeThread::get_hw_format(const enum AVPixelFormat *pix_fmts)
{
    const enum AVPixelFormat *p;

    for (p = pix_fmts; *p != -1; p++) {
        if (*p == hw_pix_fmt)
            return *p;
    }

    fprintf(stderr, "Failed to get HW surface format.\n");
    return AV_PIX_FMT_NONE;
}

int DecodeThread::decode_write(AVCodecContext *avctx, AVPacket *packet)
{
    AVFrame *frame = nullptr;
    AVFrame *sw_frame = nullptr;
    AVFrame *tmp_frame = nullptr;
    uint8_t *buffer = nullptr;
    int size;
    int ret = 0;
    SwsContext *sws_context = nullptr;
    AVFrame* rgbFrame = nullptr;
    unsigned char * rgb24Buffer;
    //QPixmap pixmap;

    ret = avcodec_send_packet(avctx, packet);
    if (ret < 0) {
        fprintf(stderr, "Error during decoding\n");
        return ret;
    }

    while (1) {
        if (!(frame = /*(AVFrame*)malloc(sizeof(*frame))*/av_frame_alloc()) || !(sw_frame = av_frame_alloc())) {
            fprintf(stderr, "Can not alloc frame\n");
            ret = AVERROR(ENOMEM);
        }

//        memset(frame, 0, sizeof(*frame));

//        if (frame->extended_data != frame->data)
//        av_freep(&frame->extended_data);

//        frame->extended_data = NULL;
//        frame->pts                   =
//        frame->pkt_dts               = AV_NOPTS_VALUE;
//        frame->pkt_pts               = AV_NOPTS_VALUE;
//        frame->best_effort_timestamp = AV_NOPTS_VALUE;
//        frame->pkt_duration        = 0;
//        frame->pkt_pos             = -1;
//        frame->pkt_size            = -1;
//        frame->key_frame           = 1;
//        frame->sample_aspect_ratio = (AVRational){ 0, 1 };
//        frame->format              = -1; /* unknown */
//        frame->extended_data       = frame->data;
//        frame->color_primaries     = AVCOL_PRI_UNSPECIFIED;
//        frame->color_trc           = AVCOL_TRC_UNSPECIFIED;
//        frame->colorspace          = AVCOL_SPC_UNSPECIFIED;
//        frame->color_range         = AVCOL_RANGE_UNSPECIFIED;
//        frame->chroma_location     = AVCHROMA_LOC_UNSPECIFIED;
//        frame->flags               = 0;

        ret = avcodec_receive_frame(avctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            av_frame_free(&frame);
            av_frame_free(&sw_frame);
            return 0;
        } else if (ret < 0) {
            fprintf(stderr, "Error while decoding\n");
        }

        qDebug() << avctx->frame_number;
        if (frame->format == hw_pix_fmt) {
            /* retrieve data from GPU to CPU */
            if ((ret = av_hwframe_transfer_data(sw_frame, frame, 0)) < 0) {
                fprintf(stderr, "Error transferring the data to system memory\n");
            }
            tmp_frame = sw_frame;
        } else
            tmp_frame = frame;

        size = av_image_get_buffer_size((AVPixelFormat)tmp_frame->format, tmp_frame->width, tmp_frame->height, 1);
        buffer = (uint8_t*)av_malloc(size);
        if (!buffer) {
            fprintf(stderr, "Can not alloc buffer\n");
            ret = AVERROR(ENOMEM);
        }
        ret = av_image_copy_to_buffer(buffer, size,
                                      (const uint8_t * const *)tmp_frame->data,
                                      (const int *)tmp_frame->linesize, (AVPixelFormat)tmp_frame->format,
                                      tmp_frame->width, tmp_frame->height, 1);
        if (ret < 0) {
            fprintf(stderr, "Can not copy image to buffer\n");
        }

        //sws_context = sws_getContext(tmp_frame->width, tmp_frame->height, hw_pix_fmt, tmp_frame->width, tmp_frame->height, AV_PIX_FMT_YUV420P, SWS_BILINEAR, 0, 0, 0);
        sws_context = sws_getContext(avctx->width, avctx->height, (AVPixelFormat)tmp_frame->format, avctx->width, avctx->height, AV_PIX_FMT_YUV420P, SWS_GAUSS, NULL, NULL, NULL);

        rgbFrame = av_frame_alloc();
        if (NULL == rgbFrame)
        {
            return false;
        }
        rgbFrame->height = tmp_frame->height;
        rgbFrame->width = tmp_frame->width;
        rgbFrame->format = AV_PIX_FMT_YUV420P;
        if (av_frame_get_buffer(rgbFrame, 0) < 0)
        {
            fprintf(stderr, "Could not allocate audio data buffers\n");
            av_frame_free(&rgbFrame);
            rgbFrame = NULL;
            return false;
        }

        rgb24Buffer = (unsigned char*)malloc(avctx->width*avctx->height*3);

        sws_scale(sws_context, tmp_frame->data, tmp_frame->linesize, 0, tmp_frame->height, rgbFrame->data, rgbFrame->linesize);

        yuv420_to_rgb24(rgbFrame->data[0], rgbFrame->data[1], rgbFrame->data[2], rgb24Buffer, rgbFrame->width, rgbFrame->height);

        QImage tmpImg(rgb24Buffer, rgbFrame->width, rgbFrame->height, QImage::Format_RGB888);
        QImage tmpImg1 = tmpImg.scaled(1920,1080);

        pixmap = QPixmap::fromImage(tmpImg1);

        emit rewardPixmap(&pixmap);

        free(rgb24Buffer);
        av_frame_free(&rgbFrame);
        sws_freeContext(sws_context);
        av_frame_free(&frame);
        //free(frame);
        av_frame_free(&sw_frame);
        av_freep(&buffer);
    }
}

DecodeThread::DecodeThread()
{

}

void DecodeThread::decode()
{
    AVFormatContext *input_ctx = nullptr;
    int video_stream, ret;
    AVStream *video = nullptr;
    AVCodecContext *decoder_ctx = nullptr;
    AVCodec *decoder = nullptr;
    AVPacket packet;
    enum AVHWDeviceType type;
    int i;

    type = av_hwdevice_find_type_by_name("vdpau");
    if (type == AV_HWDEVICE_TYPE_NONE) {
        fprintf(stderr, "Available device types:");
        while((type = av_hwdevice_iterate_types(type)) != AV_HWDEVICE_TYPE_NONE)
            fprintf(stderr, " %s", av_hwdevice_get_type_name(type));
        fprintf(stderr, "\n");
        return;
    }

    /* open the input file */
    if (avformat_open_input(&input_ctx, "3.mp4", nullptr, nullptr) != 0) {
        return;
    }

    if (avformat_find_stream_info(input_ctx, nullptr) < 0) {
        fprintf(stderr, "Cannot find input stream information.\n");
        return;
    }

    /* find the video stream information */
    ret = av_find_best_stream(input_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &decoder, 0);
    if (ret < 0) {
        fprintf(stderr, "Cannot find a video stream in the input file\n");
        return;
    }
    video_stream = ret;

    for (i = 0;; i++) {
        const AVCodecHWConfig *config = avcodec_get_hw_config(decoder, i);
        if (!config) {
            fprintf(stderr, "Decoder %s does not support device type %s.\n",
                    decoder->name, av_hwdevice_get_type_name(type));
            return;
        }
        if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
            config->device_type == type) {
            hw_pix_fmt = config->pix_fmt;
            break;
        }
    }

    if (!(decoder_ctx = avcodec_alloc_context3(decoder)))
        return;

    video = input_ctx->streams[video_stream];
    if (avcodec_parameters_to_context(decoder_ctx, video->codecpar) < 0)
        return;

    //decoder_ctx->get_format  = get_hw_format;

    if (hw_decoder_init(decoder_ctx, type) < 0)
        return;

    if ((ret = avcodec_open2(decoder_ctx, decoder, nullptr)) < 0) {
        fprintf(stderr, "Failed to open codec for stream #%u\n", video_stream);
        return;
    }

    /* actual decoding and dump the raw data */
    while (ret >= 0) {
        if ((ret = av_read_frame(input_ctx, &packet)) < 0)
            break;

        if (video_stream == packet.stream_index)
            ret = decode_write(decoder_ctx, &packet);

        av_packet_unref(&packet);
    }

    /* flush the decoder */
    packet.data = nullptr;
    packet.size = 0;
    ret = decode_write(decoder_ctx, &packet);
    av_packet_unref(&packet);

    if (output_file)
        fclose(output_file);
    avcodec_free_context(&decoder_ctx);
    avformat_close_input(&input_ctx);
    av_buffer_unref(&hw_device_ctx);

    return;
}
