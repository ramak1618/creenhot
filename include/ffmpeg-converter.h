#ifndef FFMPEG_CONVERTER_H
#define FFMPEG_CONVERTER_H

#include <libavutil/avutil.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/pixdesc.h>

int ffmpeg_encode(uint8_t* imgbuf, uint32_t width, uint32_t height, uint32_t stride, uint32_t format, enum AVPixelFormat dstfmt, enum AVCodecID ftype, uint8_t** outbuf, int* outsize);

#endif
