#include "../../include/argparser.h"
#include "../../include/wlr/creenhot.h"
#include "../../include/wlr/screencopy.h"
#include "../../include/select.h"
#include "../../include/ffmpeg-converter.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <stdio.h>

const char* const SHM_FILENAME = "/creenhot_shm";

int main(int argc, char* argv[]) {
    struct arg_t args = parse_args(argc, argv);
    if(!args.sensible) {
        fprintf(stderr, "args error!\n");
        return 0;
    }

    struct wl_display* display = NULL;
    struct wl_registry* registry = NULL;

    display = wl_display_connect(NULL);
    if(!display) {
        fprintf(stderr, "display error!\n");
        goto way_destroy;
    }
    registry = wl_display_get_registry(display);
    if(!registry) {
        fprintf(stderr, "registry error\n");
        goto way_destroy;
    }
 
    struct ifaces interfaces = {
        .output = NULL,
        .shm = NULL,
        .seat = NULL,
        .compositor = NULL,
        .wm_base = NULL,
        .screencopy_manager = NULL,
        .failed = false,
        .using_select = args.mode == CREENHOT_MODE_SELECT
    };

    struct wl_registry_listener registry_listener = {
        .global = registry_bind_callback,
        .global_remove = noop_registry_remove
    };
    wl_registry_add_listener(registry, &registry_listener, &interfaces);

    (void) wl_display_roundtrip(display);
    if(interfaces.failed) 
        goto ifaces_destroy;

    int fildes = shm_open(SHM_FILENAME, O_RDWR | O_CREAT, 0600);
    if(fildes == -1) {
        fprintf(stderr, "SHM file fail!\n");
        goto ifaces_destroy;
    }
    struct image img = screencopy(fildes, display, interfaces.output, interfaces.shm, interfaces.screencopy_manager); 
    

    struct scale_in sclsrc;
    switch(args.mode) {
        case CREENHOT_MODE_FULLSCREEN:
            sclsrc = (struct scale_in) {
                .buf = img.buf,
                .width = img.width,
                .height = img.height,
                .stride = img.stride,
                .shmfmt = img.format,
                .dstfmt = args.fmt,
                .sx = 0,
                .sy = 0,
            };
            break;
        case CREENHOT_MODE_REGION:
            if(args.cimg_x + args.cimg_width > img.width || args.cimg_y + args.cimg_height > img.height) {
                fprintf(stderr, "Out of bounds pixels requested: Got rectangle (%d, %d, %d, %d) for screen dimensions(%d, %d)\n", args.cimg_x, args.cimg_y, args.cimg_width, args.cimg_height, img.width, img.height);
                free(img.buf);
                goto shm_clean;
            }
            sclsrc = (struct scale_in) {
                .buf = img.buf,
                .width = args.cimg_width,
                .height = args.cimg_height,
                .stride = img.stride,
                .shmfmt = img.format,
                .dstfmt = args.fmt,
                .sx = args.cimg_x,
                .sy = args.cimg_y
            };
            break;
        case CREENHOT_MODE_SELECT: {
            struct selfaces sel_ifaces = {
                .display = display,
                .output = interfaces.output,
                .fildes = fildes,
                .shm = interfaces.shm,
                .compositor = interfaces.compositor,
                .seat = interfaces.seat,
                .wm_base = interfaces.wm_base
            };

            struct scale_in xrgb_scl_in = {
                .buf = img.buf,
                .width = img.width,
                .height = img.height,
                .stride = img.stride,
                .shmfmt = img.format,
                .dstfmt = AV_PIX_FMT_BGR0, //eq. WL_SHM_FORMAT_XRGB8888, which is almost certainly guarenteed to be supported by general compositors
                .sx = 0,
                .sy = 0,
            };
            struct scale_out xrgbd_img = ffmpeg_scale(&xrgb_scl_in);
            if(xrgbd_img.failed) {
                fprintf(stderr, "Error while converting pix fmts!\n");
                free(img.buf);
                goto shm_clean;
            }
            struct surface_image dispimg = {
                .bytbuf = xrgbd_img.buf,
                .buffer = NULL,
                .format = WL_SHM_FORMAT_XRGB8888,
                .width = img.width,
                .height = img.height,
                .stride = xrgbd_img.Bpp * img.width,
                .size = xrgbd_img.Bpp * img.width * img.height
            };
            struct selarea region = get_selection(&sel_ifaces, &dispimg);
            free(xrgbd_img.buf);
            if(region.failed) {
                fprintf(stderr, "Error while selection!\n");
                free(img.buf);
                goto shm_clean;
            }
            if(region.cancelled) {
                free(img.buf);
                goto shm_clean;
            }

            sclsrc = (struct scale_in) {
                .buf = img.buf,
                .width = region.width,
                .height = region.height,
                .stride = img.stride,
                .shmfmt = img.format,
                .dstfmt = args.fmt,
                .sx = region.x,
                .sy = region.y
            };
            break;
        }
        default:
            /// UNREACHABLE
            free(img.buf);
            goto shm_clean;
    }

    struct scale_out scldimg = ffmpeg_scale(&sclsrc);
    free(sclsrc.buf);
    if(scldimg.failed) {
        fprintf(stderr, "scale error!!\n");
        goto shm_clean;
    }

    struct encoder_t encimg = {
        .imgbuf = scldimg.buf,
        .ftype = args.ftype,
        .fmt = args.fmt,
        .width = sclsrc.width,
        .height = sclsrc.height,
        .bpp = scldimg.Bpp * 8
    };
    ffmpeg_encode(&encimg);
    free(encimg.imgbuf);
    if(encimg.failed) {
        fprintf(stderr, "encode error!!\n");
        goto shm_clean;
    }

    FILE* file = fopen(args.filename, "wb");
    if(!file) {
        fprintf(stderr, "Could not open file: %s\n", args.filename);
        goto buf_free;
    }

    fwrite(encimg.encbuf, 1, encimg.encsize, file);
    fclose(file);

buf_free:
    free(encimg.encbuf);
shm_clean:
    if(fildes != -1) {
        shm_unlink(SHM_FILENAME);
        close(fildes);
    }
ifaces_destroy:
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

way_destroy:
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

