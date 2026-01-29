#include "../../include/wlr/screencopy.h"

#include <unistd.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <string.h>

struct internal_image {
    uint32_t format;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint32_t size;
    uint8_t* fbuf;
    
    int fildes;
    void* rawbuf;
    struct wl_shm* shm;
    struct wl_shm_pool* pool;
    struct wl_buffer* buffer;

    bool done;
    bool failed;
};

//cbacks
static void img_prep(void* data, struct zwlr_screencopy_frame_v1* frame, uint32_t format, uint32_t width, uint32_t height, uint32_t stride);
static void noop_flags(void* data, struct zwlr_screencopy_frame_v1* frame, uint32_t flags);
static void img_ready(void* data, struct zwlr_screencopy_frame_v1* frame, uint32_t tv_sec_hi, uint32_t tv_sec_lo, uint32_t tv_nsec);
static void img_failed(void* data, struct zwlr_screencopy_frame_v1* frame);
static void noop_damage(void* data, struct zwlr_screencopy_frame_v1* frame, uint32_t x, uint32_t y, uint32_t width, uint32_t height);
static void noop_dma(void* data, struct zwlr_screencopy_frame_v1* frame, uint32_t format, uint32_t width, uint32_t height);
static void noop_done(void* data, struct zwlr_screencopy_frame_v1* frame);

struct image screencopy(int fildes, struct wl_display* display, struct wl_output* output, struct wl_shm* shm, struct zwlr_screencopy_manager_v1* screencopy_manager) {
    struct internal_image img = {
        .format = 0,
        .width = 0,
        .height = 0,
        .stride = 0,
        .size = 0,
        .fbuf = NULL,
        .fildes = fildes,
        .rawbuf = NULL,
        .shm = shm,
        .pool = NULL,
        .buffer = NULL,
        .done = false,
        .failed = false
    };

    struct zwlr_screencopy_frame_v1* frame = zwlr_screencopy_manager_v1_capture_output(screencopy_manager, 0, output);
    if(!frame) {
        img.failed = true;
        goto exit;
    }
    struct zwlr_screencopy_frame_v1_listener frame_listener = {
        .buffer = img_prep,
        .flags = noop_flags,
        .ready = img_ready,
        .failed = img_failed,
        .damage = noop_damage,
        .linux_dmabuf = noop_dma,
        .buffer_done = noop_done
    };
    zwlr_screencopy_frame_v1_add_listener(frame, &frame_listener, &img);
    wl_display_roundtrip(display);

    while(!(img.done || img.failed)) {
        if(wl_display_dispatch(display) == -1) {
            img.failed = true;
            break;
        }
    }

exit:
    if(frame)
        zwlr_screencopy_frame_v1_destroy(frame);
    if(img.buffer)
        wl_buffer_destroy(img.buffer);
    if(img.pool) 
        wl_shm_pool_destroy(img.pool);
    if(img.rawbuf != NULL && img.rawbuf != MAP_FAILED) {
        munmap(img.rawbuf, img.size);
    }
    return (struct image) {
        .format = img.format,
        .width = img.width,
        .height = img.height,
        .stride = img.stride,
        .size = img.size,
        .failed = img.failed,
        .buf = img.fbuf
    };
}

static void img_prep(void* data, struct zwlr_screencopy_frame_v1* frame, uint32_t format, uint32_t width, uint32_t height, uint32_t stride) {
    struct internal_image* img = (struct internal_image*) data;
    img->format = format;
    img->width = width;
    img->height = height;
    img->stride = stride;
    img->size = stride * height;

    if(ftruncate(img->fildes, img->size) < 0) {
        img->failed = true;
        goto exit;
    }

    img->rawbuf = mmap(NULL, img->size, PROT_READ | PROT_WRITE, MAP_SHARED, img->fildes, 0);
    if(img->rawbuf == MAP_FAILED) {
        img->failed = true;
        goto exit;
    }

    img->pool = wl_shm_create_pool(img->shm, img->fildes, img->size);
    if(!img->pool) {
        img->failed = true;
        goto exit;
    }

    img->buffer = wl_shm_pool_create_buffer(img->pool, 0, img->width, img->height, img->stride, img->format);
    if(!img->buffer)
        img->failed = true;

    zwlr_screencopy_frame_v1_copy(frame, img->buffer);
exit:
    if(img->failed) {
        if(img->rawbuf != MAP_FAILED)
            munmap(img->rawbuf, img->size);
        if(img->pool)
            wl_shm_pool_destroy(img->pool);
    }
}

static void noop_flags(void* data, struct zwlr_screencopy_frame_v1* frame, uint32_t flags) {
    (void) data;
    (void) frame;
    (void) flags;
}

static void img_ready(void* data, struct zwlr_screencopy_frame_v1* frame, uint32_t tv_sec_hi, uint32_t tv_sec_lo, uint32_t tv_nsec) {
    (void) frame;
    (void) tv_sec_hi;
    (void) tv_sec_lo;
    (void) tv_nsec;

    struct internal_image* img = (struct internal_image*) data;
    img->fbuf = malloc(img->size);
    if(!img->fbuf) {
        img->failed = true;
        return;
    }

    memcpy(img->fbuf, img->rawbuf, img->size);

    img->done = true;
}

static void img_failed(void* data, struct zwlr_screencopy_frame_v1* frame) {
    (void) frame;
    struct internal_image* img = (struct internal_image*) data;
    img->failed = true;
}

static void noop_damage(void* data, struct zwlr_screencopy_frame_v1* frame, uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    (void) data;
    (void) frame;
    (void) x;
    (void) y;
    (void) width;
    (void) height;
}

static void noop_dma(void* data, struct zwlr_screencopy_frame_v1* frame, uint32_t format, uint32_t width, uint32_t height) {
    (void) data;
    (void) frame;
    (void) format;
    (void) width;
    (void) height;
}

static void noop_done(void* data, struct zwlr_screencopy_frame_v1* frame) {
    (void) data;
    (void) frame;
}
