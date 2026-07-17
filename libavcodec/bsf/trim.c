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

#include "libavutil/intreadwrite.h"
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

    int64_t duration_ts;

    int64_t pkt_idx;
} TrimContext;

static int64_t tb_to_samples(AVBSFContext *ctx, int64_t delta)
{
    return av_rescale_q(delta, ctx->time_base_in,
                        (AVRational){ 1, ctx->par_in->sample_rate });
}

/* For audio the packet timestamps and duration are left untouched, matching how
 * the decoder adjusts them once the skip samples side data is applied (see the
 * AV_PKT_DATA_SKIP_SAMPLES handling in decode.c). */
static int trim_audio(AVPacket *pkt, int64_t head, int64_t tail)
{
    uint8_t *side = av_packet_new_side_data(pkt, AV_PKT_DATA_SKIP_SAMPLES, 10);
    if (!side)
        return AVERROR(ENOMEM);

    AV_WL32(side,     head);
    AV_WL32(side + 4, tail);
    AV_WL8 (side + 8, 0);
    AV_WL8 (side + 9, 0);

    return 0;
}

static int trim_packet(AVBSFContext *ctx, AVPacket *pkt,
                       int64_t head, int64_t tail)
{
    TrimContext *s = ctx->priv_data;

    if (ctx->par_in->codec_type == AVMEDIA_TYPE_AUDIO &&
        ctx->par_in->sample_rate > 0 && ctx->time_base_in.num > 0)
        return trim_audio(pkt, tb_to_samples(ctx, head),
                          tb_to_samples(ctx, tail));

    if (head) {
        if (pkt->pts != AV_NOPTS_VALUE)
            pkt->pts = s->start_pts;
        if (pkt->dts != AV_NOPTS_VALUE)
            pkt->dts += head;
        pkt->duration -= head;
    }
    if (tail)
        pkt->duration -= tail;

    return 0;
}

static int trim_init(AVBSFContext *ctx)
{
    TrimContext *s = ctx->priv_data;
    int ts_set, pkt_set, endings;

    /* Timestamps (pts/dts/duration) and packet indices cannot be mixed, but
     * pts and dts may freely drive different terminals, e.g. start_dts +
     * end_pts. */
    ts_set  = s->start_pts != INT64_MIN || s->end_pts != INT64_MAX ||
              s->start_dts != INT64_MIN || s->end_dts != INT64_MAX ||
              s->duration_ts != 0;
    pkt_set = s->start_pkt != 0 || s->end_pkt != INT64_MAX;

    if (ts_set && pkt_set) {
        av_log(ctx, AV_LOG_ERROR, "timestamp and packet index ranges are "
               "mutually exclusive\n");
        return AVERROR(EINVAL);
    }

    endings = 0;
    if (s->end_pts != INT64_MAX) endings++;
    if (s->end_dts != INT64_MAX) endings++;
    if (s->duration_ts != 0) endings++;
    if (endings > 1) {
        av_log(ctx, AV_LOG_ERROR, "Only one of end_pts, end_dts or "
               "duration_ts can be used\n");
        return AVERROR(EINVAL);
    }

    /* Turn a duration into the matching end timestamp, so the range checks
     * below cover it too. */
    if (s->duration_ts) {
        if (s->duration_ts < 0) {
            av_log(ctx, AV_LOG_ERROR, "duration_ts must be positive\n");
            return AVERROR(EINVAL);
        }

        if (s->start_pts != INT64_MIN)
            s->end_pts = s->start_pts + s->duration_ts;
        else if (s->start_dts != INT64_MIN)
            s->end_dts = s->start_dts + s->duration_ts;
        else {
            av_log(ctx, AV_LOG_ERROR, "duration_ts requires start_pts or "
                   "start_dts to be set\n");
            return AVERROR(EINVAL);
        }
    }

    if (s->start_pts >= s->end_pts) {
        av_log(ctx, AV_LOG_ERROR, "start_pts (%"PRId64") must be smaller than "
               "end_pts (%"PRId64")\n", s->start_pts, s->end_pts);
        return AVERROR(EINVAL);
    }
    if (s->start_dts >= s->end_dts) {
        av_log(ctx, AV_LOG_ERROR, "start_dts (%"PRId64") must be smaller than "
               "end_dts (%"PRId64")\n", s->start_dts, s->end_dts);
        return AVERROR(EINVAL);
    }
    if (s->start_pkt >= s->end_pkt) {
        av_log(ctx, AV_LOG_ERROR, "start_pkt (%"PRId64") must be smaller than "
               "end_pkt (%"PRId64")\n", s->start_pkt, s->end_pkt);
        return AVERROR(EINVAL);
    }

    return 0;
}

static int trim_filter(AVBSFContext *ctx, AVPacket *pkt)
{
    TrimContext *s = ctx->priv_data;
    int64_t idx, head = 0, tail = 0;
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

    if (pkt->dts != AV_NOPTS_VALUE && pkt->dts >= s->end_dts) {
        av_packet_unref(pkt);
        return AVERROR_EOF;
    }

    if (pkt->dts != AV_NOPTS_VALUE && pkt->dts < s->start_dts) {
        av_packet_unref(pkt);
        return AVERROR(EAGAIN);
    }

    if (pkt->pts != AV_NOPTS_VALUE) {
        if (pkt->pts >= s->end_pts) {
            av_packet_unref(pkt);
            return AVERROR_EOF;
        }

        if (pkt->duration > 0) {
            /* The requested range may start or end inside this packet's
             * lifetime, in which case it is trimmed rather than dropped. */
            if (pkt->pts < s->start_pts) {
                head = av_sat_sub64(s->start_pts, pkt->pts);
                if (head >= pkt->duration) {
                    av_packet_unref(pkt);
                    return AVERROR(EAGAIN);
                }
            }
            /* pkt->pts < end_pts here, so the subtraction stays positive. */
            tail = pkt->duration - av_sat_sub64(s->end_pts, pkt->pts);
            if (tail < 0)
                tail = 0;
        } else if (pkt->pts < s->start_pts) {
            av_packet_unref(pkt);
            return AVERROR(EAGAIN);
        }
    }

    if (head || tail) {
        ret = trim_packet(ctx, pkt, head, tail);
        if (ret < 0) {
            av_packet_unref(pkt);
            return ret;
        }
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
    { "duration_ts", "duration to keep after start_pts/start_dts, in stream time base", OFFSET(duration_ts),
        AV_OPT_TYPE_INT64, { .i64 = 0 }, INT64_MIN, INT64_MAX, FLAGS },
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
    .init           = trim_init,
    .filter         = trim_filter,
};
