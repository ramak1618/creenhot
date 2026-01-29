#ifndef FFMPEG_CONVERTER_H
#define FFMPEG_CONVERTER_H

#include <stdbool.h>

#include <libavutil/avutil.h>                                                                                                                      
#include <libavcodec/avcodec.h>                                                                                                                    
#include <libswscale/swscale.h>                                                                                                                    
#include <libavutil/pixdesc.h>  

struct scale_t {
    // src
    uint8_t* srcbuf;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint32_t shmfmt;

    uint32_t sx;
    uint32_t sy;

    uint32_t srcBpp;
    enum AVPixelFormat scrfmt;

    // dst
    uint8_t* dstbuf;
    uint32_t dstBpp;
    enum AVPixelFormat dstfmt;

    // flag
    bool failed;
};

struct encoder_t {
    uint8_t* imgbuf;
    uint8_t* encbuf;
    uint32_t encsize;
    enum AVCodecID ftype;
    enum AVPixelFormat fmt;
    uint32_t width;
    uint32_t height;
    uint32_t bpp;

    bool failed;
};

void ffmpeg_scale(struct scale_t* img);
void ffmpeg_encode(struct encoder_t* img);

#endif
