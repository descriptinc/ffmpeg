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

#include "libavutil/opt.h"

#include "libavcodec/bsf.h"
#include "libavcodec/bsf_internal.h"

typedef struct TrimContext {
    const AVClass *class;

    int64_t start_pts;
    int64_t end_pts;
    int64_t start_dts;
    int64_t end_dts;
    int64_t start_pkt;
    int64_t end_pkt;

    int64_t pkt_idx;
} TrimContext;

static int trim_filter(AVBSFContext *ctx, AVPacket *pkt)
{
    TrimContext *s = ctx->priv_data;
    int64_t idx;
    int ret;

    ret = ff_bsf_get_packet_ref(ctx, pkt);
    if (ret < 0)
        return ret;

    idx = s->pkt_idx++;

    if (idx >= s->end_pkt) {
        av_packet_unref(pkt);
        return AVERROR_EOF;
    }

    if (idx < s->start_pkt) {
        av_packet_unref(pkt);
        return AVERROR(EAGAIN);
    }

    if ((pkt->pts != AV_NOPTS_VALUE && pkt->pts >= s->end_pts) ||
        (pkt->dts != AV_NOPTS_VALUE && pkt->dts >= s->end_dts)) {
        av_packet_unref(pkt);
        return AVERROR_EOF;
    }

    if ((pkt->pts != AV_NOPTS_VALUE && pkt->pts < s->start_pts) ||
        (pkt->dts != AV_NOPTS_VALUE && pkt->dts < s->start_dts)) {
        av_packet_unref(pkt);
        return AVERROR(EAGAIN);
    }

    return 0;
}

#define OFFSET(x) offsetof(TrimContext, x)
#define FLAGS (AV_OPT_FLAG_VIDEO_PARAM|AV_OPT_FLAG_AUDIO_PARAM|AV_OPT_FLAG_BSF_PARAM)
static const AVOption trim_options[] = {
    { "start_pts", "pts of the first packet that should be passed, in stream time base", OFFSET(start_pts),
        AV_OPT_TYPE_INT64, { .i64 = INT64_MIN }, INT64_MIN, INT64_MAX, FLAGS },
    { "end_pts", "pts of the first packet that should be dropped again, in stream time base", OFFSET(end_pts),
        AV_OPT_TYPE_INT64, { .i64 = INT64_MAX }, INT64_MIN, INT64_MAX, FLAGS },
    { "start_dts", "dts of the first packet that should be passed, in stream time base", OFFSET(start_dts),
        AV_OPT_TYPE_INT64, { .i64 = INT64_MIN }, INT64_MIN, INT64_MAX, FLAGS },
    { "end_dts", "dts of the first packet that should be dropped again, in stream time base", OFFSET(end_dts),
        AV_OPT_TYPE_INT64, { .i64 = INT64_MAX }, INT64_MIN, INT64_MAX, FLAGS },
    { "start_pkt", "index of the first packet that should be passed", OFFSET(start_pkt),
        AV_OPT_TYPE_INT64, { .i64 = 0 }, 0, INT64_MAX, FLAGS },
    { "end_pkt", "index of the first packet that should be dropped again", OFFSET(end_pkt),
        AV_OPT_TYPE_INT64, { .i64 = INT64_MAX }, 0, INT64_MAX, FLAGS },
    { NULL },
};

static const AVClass trim_class = {
    .class_name = "trim",
    .item_name  = av_default_item_name,
    .option     = trim_options,
    .version    = LIBAVUTIL_VERSION_INT,
};

const FFBitStreamFilter ff_trim_bsf = {
    .p.name         = "trim",
    .p.priv_class   = &trim_class,
    .priv_data_size = sizeof(TrimContext),
    .filter         = trim_filter,
};
