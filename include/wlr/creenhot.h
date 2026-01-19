#ifndef CREENHOT_H
#define CREENHOT_H

#include <stdint.h>
#include <stdbool.h>

#include <wayland-client.h>

#include "../../build/wayscanned/wlr-screencopy-unstable-v1.h"
#include "../../build/wayscanned/xdg-shell.h"


struct ifaces {
    struct wl_output* output;
    struct wl_shm* shm;
    struct wl_seat* seat;
    struct wl_compositor* compositor;
    struct xdg_wm_base* wm_base;
    struct zwlr_screencopy_manager_v1* screencopy_manager;
    bool failed;
    bool using_select;
};

struct image {
    int fildes;
    void* rawbuf;
    uint8_t* bytbuf;

    struct wl_shm* shm;
    struct wl_shm_pool* shm_pool;
    struct wl_buffer* buffer;

    uint32_t format;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint32_t imgsize;

    bool done;
    bool failed;
};

void registry_bind_callback(void* data, struct wl_registry* registry, uint32_t name, const char* iface, uint32_t version);

void xdg_surface_configure(void* data, struct xdg_surface* surface, uint32_t serial);

void frame_buffer_prepare(void* data, struct zwlr_screencopy_frame_v1* frame, uint32_t format, uint32_t width, uint32_t height, uint32_t stride);
void frame_done(void* data, struct zwlr_screencopy_frame_v1* frame, uint32_t tv_sec_hi, uint32_t tv_sec_lo, uint32_t tv_nsec);
void frame_failed(void* data, struct zwlr_screencopy_frame_v1* frame);

//noops
static void noop_registry_remove(void* data, struct wl_registry* registry, uint32_t name) {
    (void) data;
    (void) registry;
    (void) name;
}

static void noop_frame_flags(void* data, struct zwlr_screencopy_frame_v1* frame, uint32_t flags) {
    (void) data;
    (void) frame;
    (void) flags;
}

static void noop_frame_damage(void* data, struct zwlr_screencopy_frame_v1* frame, uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    (void) data;
    (void) frame;
    (void) x;
    (void) y;
    (void) width;
    (void) height;
}

static void noop_frame_linux_dmabuf(void* data, struct zwlr_screencopy_frame_v1* frame, uint32_t format, uint32_t width, uint32_t height) {
    (void) data;
    (void) frame;
    (void) format;
    (void) width;
    (void) height;
}

static void noop_frame_buffer_done(void* data, struct zwlr_screencopy_frame_v1* frame) {
    (void) data;
    (void) frame;
}

#endif
