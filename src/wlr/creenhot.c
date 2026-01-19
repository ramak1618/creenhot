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
    uint8_t* imgbuf = NULL;
    FILE* outfile = NULL;
 
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
   
    struct zwlr_screencopy_frame_v1* frame;
    if(args.mode == CREENHOT_MODE_FULLSCREEN) {
        frame = zwlr_screencopy_manager_v1_capture_output(interfaces.screencopy_manager, 0 /* cursor hide */, interfaces.output);
    }
    else {
        fprintf(stderr, "not yet impl.!");
        goto exit;
    }

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

    if(!img.done || img.failed) 
        goto exit;

    
    int imgsize;
    int encoder_result = ffmpeg_encode(img.bytbuf, img.width, img.height, img.stride, img.format, args.fmt, args.ftype, &imgbuf, &imgsize);
    if(encoder_result < 0) {
        fprintf(stderr, "Error while encoding image!\n");
        goto exit;
    }

    outfile = fopen(args.filename, "wb");
    fwrite(imgbuf, 1, imgsize, outfile);
exit:
    if(outfile) 
        fclose(outfile);

    if(imgbuf) 
        free(imgbuf);

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
