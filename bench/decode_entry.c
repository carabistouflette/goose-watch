/* SPDX-License-Identifier: Apache-2.0 */
/* Separate translation unit: the caller cannot fold away the decoder. No LTO. */
#include "../native/watch.c"
size_t gw_benchmark_decode(const uint8_t *data, size_t size, uint8_t *name, int64_t *meta) {
    size_t consumed = 0;
    int code = gw_decode(data, size, 7, (gw_bytes){name, 256}, (gw_ints){meta, 6}, &consumed);
    return code == GW_OK ? consumed : 0;
}
