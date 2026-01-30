#include "../include/select.h"

#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

struct pointer_stuff {
    struct wl_surface* cursor_surface;
    struct wl_buffer* cursor_buffer;

    uint32_t startx;
    uint32_t starty;
    uint32_t endx;
    uint32_t endy;

    bool started;
    bool ended;
    bool cancelled;
};

static void prepare_cursor(struct surface_image* curface) {
    uint32_t* pixbuf = (uint32_t*) curface->bytbuf;
    for(uint32_t i=0; i<curface->height; i++) {
        for(uint32_t j=0; j<curface->width; j++) {
            uint32_t L = i*curface->width + j;
            pixbuf[L] = 0;
            if(i == 15 || j == 15) 
                pixbuf[L] = 0x80FFFFFF;
        }
    }
}

static void Xsurface_ack(void* data, struct xdg_surface* Xsurface, uint32_t serial) {
    (void) data;
    xdg_surface_ack_configure(Xsurface, serial);
}

static void ptrenter(void* data, struct wl_pointer* pointer, uint32_t serial, struct wl_surface* surface, wl_fixed_t x, wl_fixed_t y) {
    (void) surface;
    (void) x;
    (void) y;

    struct pointer_stuff* ps = (struct pointer_stuff*) data;
    wl_pointer_set_cursor(pointer, serial, ps->cursor_surface, 15, 15);
    wl_surface_attach(ps->cursor_surface, ps->cursor_buffer, 0, 0);
    wl_surface_commit(ps->cursor_surface);
}

static void ptrleave(void* data, struct wl_pointer* pointer, uint32_t serial, struct wl_surface* surface) {
    (void) pointer;
    (void) serial;
    (void) surface;

    struct pointer_stuff* ps = (struct pointer_stuff*) data;
    ps->cancelled = true;
}

static void ptrmove(void* data, struct wl_pointer* pointer, uint32_t time, wl_fixed_t x, wl_fixed_t y) {
    (void) pointer;
    (void) time;

    struct pointer_stuff* ps = (struct pointer_stuff*) data;
    if(ps->started) {
        ps->endx = wl_fixed_to_int(x);
        ps->endy = wl_fixed_to_int(y);
    }
    else {
        ps->startx = wl_fixed_to_int(x);
        ps->starty = wl_fixed_to_int(y);
    }
}

static void ptrbutt(void* data, struct wl_pointer* pointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t state) {
    (void) pointer;
    (void) serial;
    (void) time;
    (void) button;
    (void) state;

    struct pointer_stuff* ps = (struct pointer_stuff*) data;
    if(ps->started)
        ps->ended = true;
    else
        ps->started = true;
}

static void noop_axis(void* data, struct wl_pointer* pointer, uint32_t time, uint32_t axis, wl_fixed_t value) {
    (void) data;
    (void) pointer;
    (void) time;
    (void) axis;
    (void) value;
}

static void noop_frame(void* data, struct wl_pointer* pointer) {
    (void) data;
    (void) pointer;
}

static void noop_axiss(void* data, struct wl_pointer* pointer, uint32_t axis_source) {
    (void) data;
    (void) pointer;
    (void) axis_source;
}

static void noop_axisst(void* data, struct wl_pointer* pointer, uint32_t time, uint32_t axis) {
    (void) data;
    (void) pointer;
    (void) time;
    (void) axis;
}

static void noop_axisd(void* data, struct wl_pointer* pointer, uint32_t axis, int32_t discrete) {
    (void) data;
    (void) pointer;
    (void) axis;
    (void) discrete;
}

static void noop_axisv(void* data, struct wl_pointer* pointer, uint32_t axis, int32_t value120) { 
    (void) data;
    (void) pointer;
    (void) axis;
    (void) value120;
}

static void noop_axisr(void* data, struct wl_pointer* pointer, uint32_t axis, uint32_t direction) {
    (void) data;
    (void) pointer;
    (void) axis;
    (void) direction;
}

struct selarea get_selection(struct selfaces* ifaces, struct surface_image* dispface) {
    dispface->buffer = NULL;
    dispface->wls = NULL;

    bool failed;

    struct surface_image curface = {
        .bytbuf = NULL,
        .buffer = NULL,
        .format = WL_SHM_FORMAT_ARGB8888,
        .width = 32,
        .height = 32,
        .stride = 32 * 4,
        .size = 32 * 32 * 4
    };

    if(ftruncate(ifaces->fildes, dispface->size + curface.size) < 0) {
        return (struct selarea) { 
            .failed = true
        };
    }

    void* rawbuf = mmap(NULL, dispface->size + curface.size, PROT_READ | PROT_WRITE, MAP_SHARED, ifaces->fildes, 0);
    if(rawbuf == MAP_FAILED) {
        return (struct selarea) {
            .failed = true
        };
    }

    struct wl_shm_pool* pool = wl_shm_create_pool(ifaces->shm, ifaces->fildes, dispface->size + curface.size);
    if(!pool) {
        failed = true;
        goto exit;
    }


    dispface->buffer = wl_shm_pool_create_buffer(pool, 0, dispface->width, dispface->height, dispface->stride, dispface->format);
    if(!dispface->buffer) {
        failed = true;
        goto exit;
    }
    curface.buffer = wl_shm_pool_create_buffer(pool, dispface->size, curface.width, curface.height, curface.stride, curface.format);
    if(!curface.buffer) {
        failed = true;
        goto exit;
    }

    memcpy(rawbuf, dispface->bytbuf, dispface->size);
    curface.bytbuf = (uint8_t*)(rawbuf) + dispface->size;
    prepare_cursor(&curface);

    dispface->wls = wl_compositor_create_surface(ifaces->compositor);
    if(!dispface->wls) {
        failed = true;
        goto exit;
    }
    curface.wls = wl_compositor_create_surface(ifaces->compositor);
    if(!curface.wls) {
        failed = true;
        goto exit;
    }

    struct xdg_surface* Xsurface = xdg_wm_base_get_xdg_surface(ifaces->wm_base, dispface->wls);
    if(!Xsurface) {
        failed = true;
        goto exit;
    }
    struct xdg_surface_listener Xsurface_listener = {
        .configure = Xsurface_ack
    };
    xdg_surface_add_listener(Xsurface, &Xsurface_listener, NULL);

    struct wl_pointer* pointer = wl_seat_get_pointer(ifaces->seat);
    if(!pointer) {
        failed = true;
        goto X_destroy;
    }
    struct wl_pointer_listener pointer_listener = {
        .enter = ptrenter,
        .leave = ptrleave,
        .motion = ptrmove,
        .button = ptrbutt,
        .axis = noop_axis,
        .frame = noop_frame,
        .axis_source = noop_axiss,
        .axis_stop = noop_axisst,
        .axis_discrete = noop_axisd,
        .axis_value120 = noop_axisv,
        .axis_relative_direction = noop_axisr
    };
    struct pointer_stuff ps = {
        .cursor_surface = curface.wls,
        .cursor_buffer = curface.buffer,
        .started = false,
        .ended = false
    };
    wl_pointer_add_listener(pointer, &pointer_listener, &ps);

    struct xdg_toplevel* toplevel = xdg_surface_get_toplevel(Xsurface);
    xdg_toplevel_set_fullscreen(toplevel, ifaces->output);

    wl_surface_commit(dispface->wls);
    wl_display_roundtrip(ifaces->display);
    wl_surface_attach(dispface->wls, dispface->buffer, 0, 0);
    wl_surface_commit(dispface->wls);
    wl_display_roundtrip(ifaces->display);

    while(!ps.ended) {
        if(wl_display_dispatch(ifaces->display) == -1) {
            break;
        }

        if(ps.cancelled) {
            break;
        }
    }

    wl_pointer_destroy(pointer);
X_destroy:
    xdg_surface_destroy(Xsurface);
exit:
    if(curface.wls) 
        wl_surface_destroy(curface.wls);
    if(dispface->wls)
        wl_surface_destroy(dispface->wls);
    if(curface.buffer)
        wl_buffer_destroy(curface.buffer);
    if(dispface->buffer)
        wl_buffer_destroy(dispface->buffer);
    wl_shm_pool_destroy(pool);
    munmap(rawbuf, dispface->size + curface.size);

    if(failed) {
        return (struct selarea) {
            .failed = true
        };
    }

    if(ps.cancelled) {
        return (struct selarea) {
            .cancelled = true
        };
    }

    struct selarea region = {
        .cancelled = false,
        .failed = false
    };
    if(ps.startx < ps.endx) {
        region.x = ps.startx;
        region.width = ps.endx - ps.startx;
    }
    else {
        region.x = ps.endx;
        region.width = ps.startx - ps.endx;
    }

    if(ps.starty < ps.endy) {
        region.y = ps.starty;
        region.height = ps.endy - ps.starty;
    }
    else {
        region.y = ps.endy;
        region.height = ps.starty - ps.endy;
    }
    return region;
}
