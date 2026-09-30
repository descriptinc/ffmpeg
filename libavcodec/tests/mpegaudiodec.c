/*
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/*
 * Decodes MP2 frames whose sample rate changes mid-stream, as MPEG audio
 * files concatenated without re-encoding do, and checks that every decoded
 * frame carries the sample rate of its own header.
 */

#include <stdio.h>

#include "libavcodec/avcodec.h"
#include "libavutil/channel_layout.h"
#include "libavutil/frame.h"
#include "libavutil/samplefmt.h"

#define FRAMES_PER_RATE 3
#define MAX_PACKETS     16

typedef struct Encoded {
    AVPacket *pkt[MAX_PACKETS];
    int sample_rate[MAX_PACKETS];
    int nb;
} Encoded;

static int store(AVCodecContext *enc, Encoded *out)
{
    int ret;

    for (;;) {
        AVPacket *pkt = av_packet_alloc();
        if (!pkt)
            return AVERROR(ENOMEM);
        ret = avcodec_receive_packet(enc, pkt);
        if (ret < 0) {
            av_packet_free(&pkt);
            return ret == AVERROR(EAGAIN) || ret == AVERROR_EOF ? 0 : ret;
        }
        if (out->nb == MAX_PACKETS) {
            av_packet_free(&pkt);
            return AVERROR_BUG;
        }
        out->sample_rate[out->nb] = enc->sample_rate;
        out->pkt[out->nb++]       = pkt;
    }
}

static int encode(int sample_rate, Encoded *out)
{
    const AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_MP2);
    AVCodecContext *enc  = avcodec_alloc_context3(codec);
    AVFrame *frame       = av_frame_alloc();
    int ret;

    if (!codec || !enc || !frame) {
        ret = AVERROR(ENOMEM);
        goto end;
    }
    enc->sample_rate = sample_rate;
    enc->sample_fmt  = AV_SAMPLE_FMT_S16;
    enc->bit_rate    = 128000;
    av_channel_layout_default(&enc->ch_layout, 2);
    if ((ret = avcodec_open2(enc, codec, NULL)) < 0)
        goto end;

    frame->nb_samples  = enc->frame_size;
    frame->format      = enc->sample_fmt;
    frame->sample_rate = sample_rate;
    if ((ret = av_channel_layout_copy(&frame->ch_layout, &enc->ch_layout)) < 0 ||
        (ret = av_frame_get_buffer(frame, 0)) < 0)
        goto end;
    av_samples_set_silence(frame->extended_data, 0, frame->nb_samples, 2, enc->sample_fmt);

    for (int i = 0; i < FRAMES_PER_RATE; i++) {
        frame->pts = (int64_t)i * frame->nb_samples;
        if ((ret = avcodec_send_frame(enc, frame)) < 0 || (ret = store(enc, out)) < 0)
            goto end;
    }
    if ((ret = avcodec_send_frame(enc, NULL)) < 0)
        goto end;
    ret = store(enc, out);

end:
    av_frame_free(&frame);
    avcodec_free_context(&enc);
    return ret;
}

int main(void)
{
    const AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_MP2);
    AVCodecContext *dec  = NULL;
    AVFrame *frame       = av_frame_alloc();
    Encoded encoded      = { 0 };
    int ret, failures = 0;

    if (!codec || !frame)
        return 1;
    if (encode(44100, &encoded) < 0 || encode(48000, &encoded) < 0) {
        fprintf(stderr, "could not encode the test stream\n");
        return 1;
    }

    dec = avcodec_alloc_context3(codec);
    if (!dec || avcodec_open2(dec, codec, NULL) < 0)
        return 1;

    for (int i = 0; i < encoded.nb; i++) {
        if ((ret = avcodec_send_packet(dec, encoded.pkt[i])) < 0) {
            fprintf(stderr, "packet %d: send failed: %d\n", i, ret);
            return 1;
        }
        while ((ret = avcodec_receive_frame(dec, frame)) >= 0) {
            printf("packet %d: header %d Hz, frame %d Hz\n",
                   i, encoded.sample_rate[i], frame->sample_rate);
            if (frame->sample_rate != encoded.sample_rate[i])
                failures++;
            av_frame_unref(frame);
        }
    }

    for (int i = 0; i < encoded.nb; i++)
        av_packet_free(&encoded.pkt[i]);
    av_frame_free(&frame);
    avcodec_free_context(&dec);
    return failures ? 1 : 0;
}
