#ifndef FFMPEG_CONVERTER_H
#define FFMPEG_CONVERTER_H

#include <libavutil/avutil.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/pixdesc.h>

struct encoder_input {
    uint8_t* buf;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint32_t format;
};

struct encoder_params {
    uint32_t cimg_x;
    uint32_t cimg_y;
    uint32_t cimg_width;
    uint32_t cimg_height;
    enum AVPixelFormat dstfmt;
    enum AVCodecID ftype;
};

struct encoded_data {
    uint8_t* buf;
    int size;
};

int ffmpeg_encode(struct encoder_input rawimg, struct encoder_params params, struct encoded_data* data);

#endif
