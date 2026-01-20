#ifndef ARGPARSER_H
#define ARGPARSER_H

#include <stdbool.h>
#include <libavutil/pixfmt.h>

#include <libavcodec/avcodec.h>

enum creenhot_mode {
    CREENHOT_MODE_FULLSCREEN,
    CREENHOT_MODE_REGION,
    CREENHOT_MODE_SELECT
};

struct arg_t {
    enum creenhot_mode mode;
    enum AVPixelFormat fmt;
    enum AVCodecID ftype;
    const char* filename;


    uint32_t cimg_x;
    uint32_t cimg_y;
    uint32_t cimg_width;
    uint32_t cimg_height;

    bool sensible;
};


struct arg_t parse_args(int argc, char* argv[]);

#endif
