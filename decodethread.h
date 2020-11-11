#ifndef DECODETHREAD_H
#define DECODETHREAD_H

#include <QObject>

extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/pixdesc.h"
#include "libavutil/hwcontext.h"
#include "libavutil/opt.h"
#include "libavutil/avassert.h"
#include "libavutil/imgutils.h"
#include "libswscale/swscale.h"
}

#include <QPixmap>

class DecodeThread: public QObject
{
    Q_OBJECT

signals:
void rewardPixmap(QPixmap* pixmap);

public:
    DecodeThread();

    int hw_decoder_init(AVCodecContext *ctx, const enum AVHWDeviceType type);

    enum AVPixelFormat get_hw_format(const enum AVPixelFormat *pix_fmts);

    int decode_write(AVCodecContext *avctx, AVPacket *packet);

    void yuv420_to_rgb24(unsigned char *pY, unsigned char *pU, unsigned char *pV, unsigned char *pRGB24, int width, int height);

public slots:
    void decode();

private:
    QPixmap pixmap;
};

#endif // DECODETHREAD_H
