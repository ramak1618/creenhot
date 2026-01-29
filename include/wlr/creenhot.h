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

void registry_bind_callback(void* data, struct wl_registry* registry, uint32_t name, const char* iface, uint32_t version);
static void noop_registry_remove(void* data, struct wl_registry* registry, uint32_t name) {
    (void) data;
    (void) registry;
    (void) name;
}

#endif
