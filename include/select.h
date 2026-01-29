#ifndef SELECT_H
#define SELECT_H

#include <stdbool.h>
#include <stdint.h>

#include <wayland-client.h>
#include "../build/wayscanned/xdg-shell.h"

struct selarea {
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;

    bool cancelled;
    bool failed;
};

struct selfaces {
    struct wl_display* display;
    struct wl_output* output;
    
    int fildes;
    struct wl_shm* shm;

    struct wl_compositor* compositor;
    struct wl_seat* seat;
    struct xdg_wm_base* wm_base;
};

struct surface_image {
    uint8_t* bytbuf;
    struct wl_buffer* buffer;
    struct wl_surface* wls;

    uint32_t format;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint32_t size;
};

//dispface->buffer & dispface->wls disshould NOT be created before calling.
struct selarea get_selection(struct selfaces* ifaces, struct surface_image* dispface);

#endif
