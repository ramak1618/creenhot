#include "../include/ffmpeg-converter.h"

#include <wayland-client.h>
#include <string.h>

int SHM2AV_pix_fmt(uint32_t shmfmt, enum AVPixelFormat* into) {
    switch(shmfmt) {
        case WL_SHM_FORMAT_BGRX8888:
            *into = AV_PIX_FMT_0RGB;
            return 0;
        case WL_SHM_FORMAT_XRGB8888:
            *into = AV_PIX_FMT_BGR0;
            return 0;
        case WL_SHM_FORMAT_RGBX8888:
            *into = AV_PIX_FMT_0BGR;
            return 0;
        case WL_SHM_FORMAT_XBGR8888:
            *into = AV_PIX_FMT_RGB0;
            return 0;
        case WL_SHM_FORMAT_BGRA8888:
            *into = AV_PIX_FMT_ARGB;
            return 0;
        case WL_SHM_FORMAT_ARGB8888:
            *into = AV_PIX_FMT_BGRA;
            return 0;
        case WL_SHM_FORMAT_RGBA8888:
            *into = AV_PIX_FMT_ABGR;
            return 0;
        case WL_SHM_FORMAT_ABGR8888:
            *into = AV_PIX_FMT_RGBA;
            return 0;
        default:
            return -1;
    }
}

int ffmpeg_encode(uint8_t* bytbuf, uint32_t width, uint32_t height, uint32_t stride, uint32_t format, enum AVPixelFormat dstfmt, enum AVCodecID filetype, uint8_t** outbuf, int* outsize) {
    int call_result = 0;

    enum AVPixelFormat scrfmt;
    call_result = SHM2AV_pix_fmt(format, &scrfmt);
    if(call_result < 0) 
        goto exit;

    struct SwsContext* swsctx = sws_getContext(width, height, scrfmt, width, height, dstfmt, SWS_POINT, NULL, NULL, NULL);
    if(!swsctx) {
        call_result = -1;
        goto exit;
    }

    const AVPixFmtDescriptor* dstfmtdes = av_pix_fmt_desc_get(dstfmt);
    int bpp = av_get_padded_bits_per_pixel(dstfmtdes);

    const uint8_t* srcplanes[AV_NUM_DATA_POINTERS];
    int srcstrides[AV_NUM_DATA_POINTERS];
    uint8_t* dstplanes[AV_NUM_DATA_POINTERS];
    int dststrides[AV_NUM_DATA_POINTERS];
    
    srcplanes[0] = bytbuf;
    srcstrides[0] = stride;
    dstplanes[0] = malloc(width*(bpp/8)*height);
    if(!dstplanes[0]) {
        call_result = -1;
        goto free_swsctx;
    }
    dststrides[0] = width*(bpp/8);

    for(uint32_t i=1; i<AV_NUM_DATA_POINTERS; i++) {
        srcplanes[i] = NULL;
        srcstrides[i] = 0;
        dstplanes[i] = NULL;
        dststrides[i] = 0;
    } 

    call_result = sws_scale(swsctx, srcplanes, srcstrides, 0, height, dstplanes, dststrides);
    if(call_result < 0)
         goto free_plane;

    const AVCodec* codec = avcodec_find_encoder(filetype);
    if(!codec) {
        call_result = -1;
        goto free_plane;
    }

    AVCodecContext* encctx = avcodec_alloc_context3(codec);
    if(!encctx) {
        call_result = -1;
        goto free_plane;
    }

    encctx->width = width;
    encctx->height = height;
    encctx->pix_fmt = dstfmt;
    encctx->time_base = (AVRational) {1, 1};

    call_result = avcodec_open2(encctx, codec, NULL);
    if(call_result < 0)
        goto exit;
    AVFrame* frame = av_frame_alloc();
    if(!frame) {
        call_result = -1;
        goto free_encctx;
    }

    frame->width = encctx->width;
    frame->height = encctx->height;
    frame->format = encctx->pix_fmt;

    call_result = av_frame_get_buffer(frame, bpp);
    if(call_result < 0) goto free_frame;

    
    frame->data[0] = dstplanes[0];
    avcodec_send_frame(encctx, frame);
    if(call_result < 0) goto free_frame;
   
   
    size_t capacity = 10;
    size_t idx = 0;
    AVPacket** pkts = malloc(capacity * sizeof(AVPacket*));
    int imgsize = 0;
    do {
        pkts[idx] = av_packet_alloc();
        call_result = avcodec_receive_packet(encctx, pkts[idx]);
        if(call_result == AVERROR(EAGAIN) || call_result == AVERROR_EOF) {
            break;
        }
        else if(call_result < 0) {
            goto free_pkts;
        }

        imgsize += pkts[idx]->size;
        idx++;
        if(idx == capacity) {
            capacity *= 2;
            AVPacket** tmp = realloc(pkts, capacity*sizeof(AVPacket*));
            if(!tmp) {
                call_result = -1;
                goto free_pkts;
            }
            pkts = tmp;
        }

    } while(call_result >= 0);
    call_result = 0;
    *outbuf = malloc(imgsize);
    
    int p = 0;
    for(size_t i=0; i<idx; i++) {
        memcpy((*outbuf + p), pkts[i]->data, pkts[i]->size);
        p += pkts[i]->size;
    }

    *outsize = imgsize;

free_pkts:
    for(size_t i=0; i<idx; i++)
        av_packet_free(&pkts[idx]);
    free(pkts);

free_frame:
    av_frame_free(&frame);

free_encctx:
    avcodec_free_context(&encctx);

free_plane:
    free(dstplanes[0]);

free_swsctx:
    sws_freeContext(swsctx);

exit:
    return call_result;
}
