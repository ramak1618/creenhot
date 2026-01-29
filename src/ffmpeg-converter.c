#include "../include/ffmpeg-converter.h"

#include <wayland-client.h>
#include <string.h>

static void init_av_pix_fmt(struct scale_t* img) {
    switch(img->shmfmt) {
       case WL_SHM_FORMAT_BGRX8888:
            img->scrfmt = AV_PIX_FMT_0RGB;
            img->srcBpp = 4;
            break;
       case WL_SHM_FORMAT_XRGB8888:
            img->scrfmt = AV_PIX_FMT_BGR0;
            img->srcBpp = 4;
            break;
       case WL_SHM_FORMAT_RGBX8888:
            img->scrfmt = AV_PIX_FMT_0BGR;
            img->srcBpp = 4;
            break;
       case WL_SHM_FORMAT_XBGR8888:
            img->scrfmt = AV_PIX_FMT_RGB0;
            img->srcBpp = 4;
            break;
       case WL_SHM_FORMAT_BGRA8888:
            img->scrfmt = AV_PIX_FMT_ARGB;
            img->srcBpp = 4;
            break;
       case WL_SHM_FORMAT_ARGB8888:
            img->scrfmt = AV_PIX_FMT_BGRA;
            img->srcBpp = 4;
            break;
       case WL_SHM_FORMAT_RGBA8888:
            img->scrfmt = AV_PIX_FMT_ABGR;
            img->srcBpp = 4;
            break;
       case WL_SHM_FORMAT_ABGR8888:
            img->scrfmt = AV_PIX_FMT_RGBA;
            img->srcBpp = 4;
            break;
       default:
            img->failed = true;
    }
}

void ffmpeg_scale(struct scale_t* img) {
    img->failed = false;
    init_av_pix_fmt(img);
    if(img->failed)
        return;

    const AVPixFmtDescriptor* dstfmtdes = av_pix_fmt_desc_get(img->dstfmt);
    img->dstBpp = av_get_padded_bits_per_pixel(dstfmtdes) / 8;

    const uint8_t* srcplanes[AV_NUM_DATA_POINTERS] = {0};
    int srcstrides[AV_NUM_DATA_POINTERS] = {0};
    uint8_t* dstplanes[AV_NUM_DATA_POINTERS] = {0};
    int dststrides[AV_NUM_DATA_POINTERS] = {0};

    srcplanes[0] = img->srcbuf + img->sy*img->stride + img->sx*img->srcBpp;
    srcstrides[0] = img->stride;

    dststrides[0] = img->dstBpp * img->width;
    dststrides[0] += (32 - dststrides[0]%32) % 32;

    dstplanes[0] = malloc(dststrides[0] * img->height);
    if(!dstplanes[0]) {
        img->failed = true;
        return;
    }

    struct SwsContext* swsctx = sws_getContext(img->width, img->height, img->scrfmt, img->width, img->height, img->dstfmt, SWS_POINT, NULL, NULL, NULL);
    if(!swsctx) {
        img->failed = true;
        goto exit;
    }

    int scale_success = sws_scale(swsctx, srcplanes, srcstrides, 0, img->height, dstplanes, dststrides);
    if(scale_success < 0) {
        img->failed = true;
        goto exit;
    }

    img->dstbuf = dstplanes[0];
exit:
    if(swsctx) 
        sws_freeContext(swsctx);
    if(img->failed)
        free(dstplanes[0]);
}

void ffmpeg_encode(struct encoder_t* img) {
    img->failed = false;

    const AVCodec* codec = avcodec_find_encoder(img->ftype);
    if(!codec) {
        img->failed = true;
        return;
    }

    AVCodecContext* encctx = avcodec_alloc_context3(codec);
    if(!encctx) {
        img->failed = true;
        return;
    }

    encctx->width = img->width;
    encctx->height = img->height;
    encctx->pix_fmt = img->fmt;
    encctx->time_base = (AVRational) {1, 1};
    if(avcodec_open2(encctx, codec, NULL) < 0) {
        img->failed = true;
        goto exit;
    }

    AVFrame* frame = av_frame_alloc();
    if(!frame) {
        img->failed = true;
        goto exit;
    }
    frame->width = encctx->width;
    frame->height = encctx->height;
    frame->format = encctx->pix_fmt;

    if(av_frame_get_buffer(frame, img->bpp) < 0) {
        img->failed = true;
        goto exit;
    }

    frame->data[0] = img->imgbuf;
    if(avcodec_send_frame(encctx, frame) < 0) {
        img->failed = true;
        goto exit;
    }

    size_t capacity = 10;
    size_t idx = 0;
    AVPacket** pkts = malloc(capacity * sizeof(AVPacket*));
    if(!pkts) {
        img->failed = true;
        goto exit;
    }
    int imgsize = 0;
    int result;
    do {
        pkts[idx] = av_packet_alloc();
        result = avcodec_receive_packet(encctx, pkts[idx]);
        if(result == AVERROR(EAGAIN) || result == AVERROR_EOF)
            break;
        else if(result < 0) {
            img->failed = true;
            goto exit;
        }

        imgsize += pkts[idx]->size;
        idx++;
        if(idx == capacity) {
            capacity *= 2;
            AVPacket** tmp = realloc(pkts, capacity*sizeof(AVPacket*));
            if(!tmp) {
                img->failed = true;
                goto exit;
            }
            pkts = tmp;
        }
    } while(result >= 0);

    img->encbuf = malloc(imgsize);
    if(!img->encbuf) {
        img->failed = true;
        goto exit;
    }

    int p=0;
    for(size_t i=0; i<idx; i++) {
        memcpy(img->encbuf + p, pkts[i]->data, pkts[i]->size);
        p += pkts[i]->size;
    }
    img->encsize = imgsize;
exit:
    if(pkts) {
        for(size_t i=0; i<idx; i++) 
            av_packet_free(&pkts[i]);
        free(pkts);
    }
    if(frame)
        av_frame_free(&frame);
    avcodec_free_context(&encctx);
}
