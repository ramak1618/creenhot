#include "../include/argparser.h"

struct arg_t parse_args(int argc, char* argv[]) {
    if(argc == 2) {
        return (struct arg_t) {
            .mode = CREENHOT_MODE_FULLSCREEN,
            .fmt = AV_PIX_FMT_RGBA,
            .ftype = AV_CODEC_ID_PNG,
            .filename = argv[1],
            .sensible = true
        };
    }

    if(argc == 6) {
        return (struct arg_t) {
            .mode = CREENHOT_MODE_REGION,
            .fmt = AV_PIX_FMT_RGBA,
            .ftype = AV_CODEC_ID_PNG,
            .filename = argv[1],
            .cimg_x = atoi(argv[2]),
            .cimg_y = atoi(argv[3]),
            .cimg_width = atoi(argv[4]),
            .cimg_height = atoi(argv[5]),
            .sensible = true
        };
    }

    return (struct arg_t) {
        .sensible = false
    };
}
