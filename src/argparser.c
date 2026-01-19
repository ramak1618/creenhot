#include "../include/argparser.h"
//yet to impl.
struct arg_t parse_args(int argc, char* argv[]) {
    if(argc == 1) 
        return (struct arg_t) {
            .sensible = false
        };
    return (struct arg_t) {
        .mode = CREENHOT_MODE_FULLSCREEN,
        .fmt = AV_PIX_FMT_RGB24,
        .ftype = AV_CODEC_ID_PNG,
        .filename = argv[1],
        .sensible = true
    };
}
