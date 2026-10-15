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

#include <stdio.h>
#include <string.h>

#include "libavcodec/packet.h"
#include "libavcodec/packet_internal.h"

/*
 * Walk every side data type: each needs a handle, which must find the
 * type again, so that no two types share one. The handles are printed and
 * compared with the reference, since a released handle must not change.
 */
int main(void)
{
    int ret = 0;

    for (int type = 0; type < AV_PKT_DATA_NB; type++) {
        const char *handle = ff_packet_side_data_handle(type);
        int found;

        printf("%2d: %s\n", type, handle);

        found = ff_packet_side_data_from_handle(handle, strlen(handle));
        if (found != type) {
            printf("%2d: handle found type %d\n", type, found);
            ret = 1;
        }
    }

    if (ff_packet_side_data_from_handle("", 0) >= 0 ||
        ff_packet_side_data_from_handle("no_such_type", 12) >= 0 ||
        ff_packet_side_data_from_handle("skip_samples", 4) >= 0) {
        printf("unknown handle found a type\n");
        ret = 1;
    }

    return ret;
}
