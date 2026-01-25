#include "../../include/argparser.h"
#include "../../include/wlr/creenhot.h"
#include "../../include/ffmpeg-converter.h"

#include <sys/mman.h>

#include <fcntl.h>
#include <unistd.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

const char* const SHM_FILENAME = "/creenhot_shm";

struct pointer_stuff {
    struct wl_surface* curface;    
    struct wl_buffer* buffer;

    uint32_t startx;
    uint32_t starty;
    uint32_t endx;
    uint32_t endy;

    bool started;
    bool ended;
    bool done;
};

void ptrenter(void* data, struct wl_pointer* pointer, uint32_t serial, struct wl_surface* surface, wl_fixed_t x, wl_fixed_t y) {
    struct pointer_stuff* ps = (struct pointer_stuff*) data;
    (void) surface;
    (void) x;
    (void) y;

    wl_pointer_set_cursor(pointer, serial, ps->curface, 0, 0);
    wl_surface_attach(ps->curface, ps->buffer, 0, 0);
    wl_surface_commit(ps->curface);
}

void ptrleave(void* data, struct wl_pointer* pointer, uint32_t serial, struct wl_surface* surface) {
    (void) data;
    (void) pointer;
    (void) serial;
    (void) surface;
}

void ptrframe(void* data, struct wl_pointer* pointer) {
    (void) data;
    (void) pointer;
}

void ptrmove(void* data, struct wl_pointer* pointer, uint32_t time, wl_fixed_t surface_x, wl_fixed_t surface_y) {
    (void) pointer;
    (void) time;

    struct pointer_stuff* ps = (struct pointer_stuff*) data;
    if(!ps->started) {
        ps->startx = wl_fixed_to_int(surface_x);
        ps->starty = wl_fixed_to_int(surface_y);
    }
    else {
        ps->endx = wl_fixed_to_int(surface_x);
        ps->endy = wl_fixed_to_int(surface_y);
    }
}

void ptrpress(void* data, struct wl_pointer* pointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t state) {
    (void) pointer;
    (void) serial;
    (void) time;
    (void) button;
    (void) state;

    struct pointer_stuff* ps = (struct pointer_stuff*) data;
    if(ps->started) {
        ps->ended = true;
    }
    else {
        ps->started = true;
    }
}

void Xsurface_ack(void* data, struct xdg_surface* Xsurface, uint32_t serial) {
    (void) data;
    xdg_surface_ack_configure(Xsurface, serial);
}

int main(int argc, char** argv) {
    struct wl_display* display = NULL;
    struct wl_registry* registry = NULL;
    struct ifaces interfaces = {
        .output = NULL,
        .shm = NULL,
        .seat = NULL,
        .compositor = NULL,
        .wm_base = NULL,
        .failed = false,
        .using_select = false
    }; 
    struct zwlr_screencopy_frame_v1* frame = NULL;
    struct image img = {
        .fildes = -1,
        .rawbuf = NULL,
        .bytbuf = NULL,
        .shm = NULL,
        .shm_pool = NULL,
        .buffer = NULL,
        .format = 0,
        .width = 0,
        .height = 0,
        .stride = 0,
        .imgsize = 0,
        .done = false,
        .failed = false
    };
 
    struct arg_t args = parse_args(argc, argv);
    if(!args.sensible) {
        fprintf(stderr, "args error!");
        return 0;
    }
    if(args.mode == CREENHOT_MODE_SELECT) {
        interfaces.using_select = true;
    }

    display = wl_display_connect(NULL);
    if(!display) {
        fprintf(stderr, "Could not connect to display!\n");
        goto exit;
    }

    registry = wl_display_get_registry(display);
    if(!registry) {
        fprintf(stderr, "Could not get registry\n");
        goto exit;
    }

    struct wl_registry_listener registry_listener = {
        .global = registry_bind_callback,
        .global_remove = noop_registry_remove
    };

    wl_registry_add_listener(registry, &registry_listener, &interfaces);


    (void) wl_display_roundtrip(display);
    if(interfaces.failed) {
        goto exit;
    }

    img.shm = interfaces.shm;
   
    frame = zwlr_screencopy_manager_v1_capture_output(interfaces.screencopy_manager, 0 /* cursor hide */, interfaces.output);

    struct zwlr_screencopy_frame_v1_listener frame_listener = {
        .buffer = frame_buffer_prepare,
        .flags = noop_frame_flags,
        .ready = frame_done,
        .failed = frame_failed,
        .damage = noop_frame_damage,
        .linux_dmabuf = noop_frame_linux_dmabuf,
        .buffer_done = noop_frame_buffer_done
    };
    zwlr_screencopy_frame_v1_add_listener(frame, &frame_listener, &img);

    (void) wl_display_roundtrip(display);
    while(!img.done || img.failed) {
        if(wl_display_dispatch(display) == -1) {
            fprintf(stderr, "unknown error!\n");
            goto exit;
        }
    }

    struct encoder_input rawimg = {
        .buf = img.bytbuf,
        .width = img.width,
        .height = img.height,
        .stride = img.stride,
        .format = img.format
    };

    struct encoder_params params;
    if(args.mode == CREENHOT_MODE_FULLSCREEN) {
        params = (struct encoder_params) {
            .cimg_x = 0,
            .cimg_y = 0,
            .cimg_width = img.width,
            .cimg_height = img.height,
            .dstfmt = args.fmt,
            .ftype = args.ftype
        };
    }
    else if(args.mode == CREENHOT_MODE_REGION) {
        params = (struct encoder_params) {
            .cimg_x = args.cimg_x,
            .cimg_y = args.cimg_y,
            .cimg_width = args.cimg_width,
            .cimg_height = args.cimg_height,
            .dstfmt = args.fmt,
            .ftype = args.ftype
        };
    }
    else if(args.mode == CREENHOT_MODE_SELECT) {
        // IF IT WORKS IT WORKS!!
        wl_buffer_destroy(img.buffer);
        wl_shm_pool_destroy(img.shm_pool);
        munmap(img.rawbuf, img.imgsize);

        uint32_t cursor_format = WL_SHM_FORMAT_ARGB8888;
        uint32_t cursor_width = 32;
        uint32_t cursor_height = 32;
        uint32_t cursor_stride = cursor_width * 4;

        ftruncate(img.fildes, img.stride*img.height + cursor_stride*cursor_height);
        img.rawbuf = mmap(NULL, img.stride*img.height + cursor_stride*cursor_height, PROT_READ | PROT_WRITE, MAP_SHARED, img.fildes, 0);
        img.shm_pool = wl_shm_create_pool(img.shm, img.fildes, img.stride*img.height + cursor_stride*cursor_height);
        struct wl_buffer* surface_buffer = wl_shm_pool_create_buffer(img.shm_pool, 0, img.width, img.height, img.stride, img.format);
        struct wl_buffer* cursor_buffer  = wl_shm_pool_create_buffer(img.shm_pool, img.imgsize, cursor_width, cursor_height, cursor_stride, cursor_format);
       
        for(uint32_t i=0; i<cursor_stride*cursor_height; i++) {
            ((uint8_t*)(img.rawbuf)) [img.stride*img.height + i] = 255;
        }

        struct wl_surface* curface = wl_compositor_create_surface(interfaces.compositor);
        struct wl_surface* surface = wl_compositor_create_surface(interfaces.compositor);
        struct xdg_surface* Xsurface = xdg_wm_base_get_xdg_surface(interfaces.wm_base, surface);
        struct xdg_surface_listener Xsurface_listener = {
            .configure = Xsurface_ack
        };
        xdg_surface_add_listener(Xsurface, &Xsurface_listener, NULL);

        struct wl_pointer* pointer = wl_seat_get_pointer(interfaces.seat);
        struct wl_pointer_listener pointer_listener = {
            .enter = ptrenter,
            .leave = ptrleave,
            .motion = ptrmove,
            .button = ptrpress,
            .axis = NULL,
            .frame = ptrframe,
            .axis_source = NULL, 
            .axis_stop = NULL,
            .axis_discrete = NULL,
            .axis_value120 = NULL,
            .axis_relative_direction = NULL
        };
        struct pointer_stuff ps = {
            .curface = curface,
            .buffer = cursor_buffer,
            .started = false,
            .ended = false,
            .done = false
        };

        wl_pointer_add_listener(pointer, &pointer_listener, &ps);
        struct xdg_toplevel* toplevel = xdg_surface_get_toplevel(Xsurface);
        (void) toplevel;
        xdg_toplevel_set_fullscreen(toplevel, interfaces.output);
        wl_surface_commit(surface);

        wl_display_roundtrip(display);
        wl_surface_attach(surface, surface_buffer, 0, 0);
        wl_surface_commit(surface);
        wl_display_roundtrip(display);


        wl_display_roundtrip(display);
        while(wl_display_dispatch(display) && !ps.ended);
        

        params = (struct encoder_params) {
            .cimg_x = ps.startx,
            .cimg_y = ps.starty,
            .cimg_width = ps.endx - ps.startx,
            .cimg_height = ps.endy - ps.starty,
            .dstfmt = args.fmt,
            .ftype = args.ftype
        };

    }


    struct encoded_data imgdata = {
        .buf = NULL,
        .size = 0
    };

    int encoder_result = ffmpeg_encode(rawimg, params, &imgdata);
    if(encoder_result < 0) {
        fprintf(stderr, "Error while encoding image!\n");
        goto exit;
    }
    FILE* outfile = fopen(args.filename, "wb");
    fwrite(imgdata.buf, 1, imgdata.size, outfile);

    if(imgdata.buf)
        free(imgdata.buf);
    fclose(outfile);
exit:
    if(img.buffer)
        wl_buffer_destroy(img.buffer);
    if(img.shm_pool)
        wl_shm_pool_destroy(img.shm_pool);
    if(img.bytbuf) 
        free(img.bytbuf);
    if(img.rawbuf)
        munmap(img.rawbuf, img.imgsize);
    if(img.fildes) {
        shm_unlink(SHM_FILENAME);
        close(img.fildes);
    }
    
    if(frame)
        zwlr_screencopy_frame_v1_destroy(frame);
    if(interfaces.output) 
        wl_output_destroy(interfaces.output);
    if(interfaces.shm)
        wl_shm_destroy(interfaces.shm);
    if(interfaces.seat)
        wl_seat_destroy(interfaces.seat);
    if(interfaces.compositor) 
        wl_compositor_destroy(interfaces.compositor);
    if(interfaces.wm_base)
        xdg_wm_base_destroy(interfaces.wm_base);
    if(interfaces.screencopy_manager)
        zwlr_screencopy_manager_v1_destroy(interfaces.screencopy_manager);

    if(registry) wl_registry_destroy(registry);
    if(display) wl_display_disconnect(display);
    return 0;
}

void registry_bind_callback(void* data, struct wl_registry* registry, uint32_t name, const char* iface, uint32_t version) {
    struct ifaces* interfaces = (struct ifaces*) data;

    if(strcmp(iface, wl_output_interface.name) == 0) {
        interfaces->output = wl_registry_bind(registry, name, &wl_output_interface, version);
        if(!interfaces->output) {
            fprintf(stderr, "Could not bind output\n");
            interfaces->failed = true;
        } 
        return;
    }

    if(strcmp(iface, wl_shm_interface.name) == 0) {
        interfaces->shm = wl_registry_bind(registry, name, &wl_shm_interface, version);
        if(!interfaces->shm) {
            fprintf(stderr, "Could not bind shm\n");
            interfaces->failed = true;
        }
        return;
    }

   
    if(interfaces->using_select) {
        if(strcmp(iface, wl_seat_interface.name) == 0) {
            interfaces->seat = wl_registry_bind(registry, name, &wl_seat_interface, version);
                if(!interfaces->seat) {
                   fprintf(stderr, "Could not bind seat\n");
                   interfaces->failed = true;
                }
            return;
        }

        if(strcmp(iface, wl_compositor_interface.name) == 0) {
            interfaces->compositor = wl_registry_bind(registry, name, &wl_compositor_interface, version);
            if(!interfaces->compositor) {
                fprintf(stderr, "Could not bind compositor\n");
                interfaces->failed = true;
            }
            return;
        }

        if(strcmp(iface, xdg_wm_base_interface.name) == 0) {
            interfaces->wm_base = wl_registry_bind(registry, name, &xdg_wm_base_interface, version);
            if(!interfaces->wm_base) {
                fprintf(stderr, "Could not bind wm_base\n");
                interfaces->failed = true;
            }
            return;
        }
    }

    if(strcmp(iface, zwlr_screencopy_manager_v1_interface.name) == 0) {
        interfaces->screencopy_manager = wl_registry_bind(registry, name, &zwlr_screencopy_manager_v1_interface, version);
        if(!interfaces->screencopy_manager) {
            fprintf(stderr, "Could not bin screencopy manaher\n");
            interfaces->failed = true;
        }
        return;
    }
}

void frame_buffer_prepare(void* data, struct zwlr_screencopy_frame_v1* frame, uint32_t format, uint32_t width, uint32_t height, uint32_t stride) {
    struct image* img = (struct image*) data;

    img->format = format;
    img->width = width;
    img->height = height;
    img->stride = stride;
    img->imgsize = stride * height;

    img->fildes = shm_open(SHM_FILENAME, O_RDWR | O_CREAT, 0600);
    if(img->fildes == -1) {
        img->failed = true;
        goto exit;
    }


    int alloc_success = ftruncate(img->fildes, img->imgsize);
    if(alloc_success == -1) {
        img->failed = true;
        goto exit;
    }

    img->rawbuf = mmap(NULL, img->imgsize, PROT_READ | PROT_WRITE, MAP_SHARED, img->fildes, 0);
    if(img->rawbuf == MAP_FAILED) {
        img->failed = true;
        goto exit;
    }

    img->shm_pool = wl_shm_create_pool(img->shm, img->fildes, img->imgsize);
    if(!img->shm_pool) {
        img->failed = true;
        goto exit;
    }


    img->buffer = wl_shm_pool_create_buffer(img->shm_pool, 0, width, height, stride, format);
    if(!img->buffer) {
        img->failed = true;
        goto exit;
    }

    zwlr_screencopy_frame_v1_copy(frame, img->buffer);

exit:
    return;
}

void frame_done(void* data, struct zwlr_screencopy_frame_v1* frame, uint32_t tv_sec_hi, uint32_t tv_sec_lo, uint32_t tv_nsec) {
    (void) frame;
    (void) tv_sec_hi;
    (void) tv_sec_lo;
    (void) tv_nsec;

    struct image* img = (struct image*) data;

    img->bytbuf = malloc(img->imgsize);
    if(!img->bytbuf) 
        goto exit;
    memcpy(img->bytbuf, img->rawbuf, img->imgsize); 

exit:
   img->done = true; 
}

void frame_failed(void* data, struct zwlr_screencopy_frame_v1* frame) {
    struct image* img = (struct image*) data;
    (void) frame;

    fprintf(stderr, "failed to take screenshot!\n");
    img->done = true;
}
