#ifndef SCREENCOPY_H
#define SCREENCOPY_H

#include <stdbool.h>

#include <wayland-client.h>
#include "../../build/wayscanned/wlr-screencopy-unstable-v1.h"   

struct image {
    uint32_t format;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint32_t size;

    bool failed;
    uint8_t* buf;
};

struct image screencopy(int fildes, struct wl_display* display, struct wl_output* output, struct wl_shm* shm, struct zwlr_screencopy_manager_v1* screencopy_manager);


#endif
