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

#include "libavutil/opt.h"

typedef struct SideDataContext {
    const AVClass *class;

    char *remove;
    uint64_t remove_mask;   ///< bit i set: remove the side data of type i
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

    return 0;
}

#define OFFSET(x) offsetof(SideDataContext, x)
#define FLAGS (AV_OPT_FLAG_VIDEO_PARAM | AV_OPT_FLAG_AUDIO_PARAM | AV_OPT_FLAG_SUBTITLE_PARAM | AV_OPT_FLAG_BSF_PARAM)
static const AVOption options[] = {
    { "remove", "side data types to remove, joined with '+'", OFFSET(remove), AV_OPT_TYPE_STRING, { .str = NULL }, 0, 0, FLAGS },
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
