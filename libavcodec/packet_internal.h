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

#ifndef AVCODEC_PACKET_INTERNAL_H
#define AVCODEC_PACKET_INTERNAL_H

#include <stdint.h>

#include "packet.h"

#define AVPACKET_IS_EMPTY(pkt) (!(pkt)->data && !(pkt)->side_data_elems)

int ff_side_data_set_prft(AVPacket *pkt, int64_t timestamp);

/**
 * Get the handle of a side data type: a short, stable name made of lower case
 * letters, digits and underscores, for use where a name has to be typed, such
 * as in the options of a filter.
 *
 * Every type below AV_PKT_DATA_NB has one, which the packet_side_data_handle
 * test checks. A handle that has been released must not change.
 *
 * @param type the side data type, which has to be valid
 * @return the handle
 */
const char *ff_packet_side_data_handle(enum AVPacketSideDataType type);

/**
 * Get the side data type that has a handle.
 *
 * @param handle the handle, not necessarily null terminated
 * @param len    length of the handle
 * @return the type, or a negative AVERROR code if no type has this handle
 */
int ff_packet_side_data_from_handle(const char *handle, size_t len);

#endif // AVCODEC_PACKET_INTERNAL_H
