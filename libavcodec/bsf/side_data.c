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

/**
 * @file
 * Side data bitstream filter -- remove or modify the side data of packets.
 */

#include <stdint.h>
#include <string.h>

#include "libavcodec/bsf.h"
#include "libavcodec/bsf_internal.h"
#include "libavcodec/packet_internal.h"

#include "libavutil/intreadwrite.h"
#include "libavutil/opt.h"

typedef struct SideDataContext {
    const AVClass *class;

    char *remove;
    uint64_t remove_mask;   ///< bit i set: remove the side data of type i

    int64_t skip_samples[2];    ///< skip, discard; -1 to leave as is
    int skip_reasons[2];        ///< skip reason, discard reason; -1 to leave as is
} SideDataContext;

static int side_data_init(AVBSFContext *ctx)
{
    SideDataContext *s = ctx->priv_data;

    static_assert(AV_PKT_DATA_NB <= 64, "remove_mask has a bit per type");

    for (const char *cur = s->remove; cur && *cur; ) {
        size_t len = strcspn(cur, "+");
        int type = ff_packet_side_data_from_handle(cur, len);

        if (type < 0) {
            av_log(ctx, AV_LOG_ERROR, "Unknown side data type '%.*s'\n", (int)len, cur);
            return AVERROR(EINVAL);
        }
        s->remove_mask |= UINT64_C(1) << type;

        cur += len + !!cur[len];
    }

    return 0;
}

/**
 * Set a field of the skip samples side data: the two counts are 32 bit little
 * endian at 0 and 4, the two reasons one byte at 8 and 9.
 * Absent side data is only created for a value that is not 0.
 */
static int set_skip_samples(AVPacket *pkt, int offset, int size, int64_t value)
{
    size_t sd_size;
    uint8_t *sd = av_packet_get_side_data(pkt, AV_PKT_DATA_SKIP_SAMPLES, &sd_size);

    if (!sd) {
        if (!value)
            return 0;
        sd = av_packet_new_side_data(pkt, AV_PKT_DATA_SKIP_SAMPLES, 10);
        if (!sd)
            return AVERROR(ENOMEM);
        memset(sd, 0, 10);
    } else if (sd_size < 10) {
        return AVERROR_INVALIDDATA;
    }

    if (size == 4)
        AV_WL32(sd + offset, value);
    else
        sd[offset] = value;

    return 0;
}

static int side_data_filter(AVBSFContext *ctx, AVPacket *pkt)
{
    SideDataContext *s = ctx->priv_data;
    int ret = ff_bsf_get_packet_ref(ctx, pkt);

    if (ret < 0)
        return ret;

    for (int i = 0; s->remove_mask && i < AV_PKT_DATA_NB; i++) {
        if (s->remove_mask & (UINT64_C(1) << i))
            av_packet_side_data_remove(pkt->side_data, &pkt->side_data_elems, i);
    }

    for (int i = 0; i < 2 && ret >= 0; i++) {
        if (s->skip_samples[i] >= 0)
            ret = set_skip_samples(pkt, 4 * i, 4, s->skip_samples[i]);
        if (s->skip_reasons[i] >= 0 && ret >= 0)
            ret = set_skip_samples(pkt, 8 + i, 1, s->skip_reasons[i]);
    }
    if (ret < 0)
        av_packet_unref(pkt);

    return ret;
}

#define OFFSET(x) offsetof(SideDataContext, x)
#define FLAGS (AV_OPT_FLAG_VIDEO_PARAM | AV_OPT_FLAG_AUDIO_PARAM | AV_OPT_FLAG_SUBTITLE_PARAM | AV_OPT_FLAG_BSF_PARAM)
static const AVOption options[] = {
    { "remove", "side data types to remove, joined with '+'", OFFSET(remove), AV_OPT_TYPE_STRING, { .str = NULL }, 0, 0, FLAGS },
    { "skip_samples.skip",           "set the samples to skip at the start of the packet",      OFFSET(skip_samples[0]), AV_OPT_TYPE_INT64, { .i64 = -1 }, -1, UINT32_MAX, FLAGS },
    { "skip_samples.discard",        "set the samples to discard at the end of the packet",     OFFSET(skip_samples[1]), AV_OPT_TYPE_INT64, { .i64 = -1 }, -1, UINT32_MAX, FLAGS },
    { "skip_samples.skip_reason",    "set the reason of the skip",                              OFFSET(skip_reasons[0]), AV_OPT_TYPE_INT,   { .i64 = -1 }, -1, 255,        FLAGS },
    { "skip_samples.discard_reason", "set the reason of the discard",                           OFFSET(skip_reasons[1]), AV_OPT_TYPE_INT,   { .i64 = -1 }, -1, 255,        FLAGS },
    { NULL },
};

static const AVClass side_data_class = {
    .class_name = "side_data bsf",
    .item_name  = av_default_item_name,
    .option     = options,
    .version    = LIBAVUTIL_VERSION_INT,
};

const FFBitStreamFilter ff_side_data_bsf = {
    .p.name         = "side_data",
    .p.priv_class   = &side_data_class,
    .priv_data_size = sizeof(SideDataContext),
    .init           = side_data_init,
    .filter         = side_data_filter,
};
