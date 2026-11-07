/* SPDX-License-Identifier: Apache-2.0 */
#include "../native/set.c"
#include "../native/watch.c"
#include <assert.h>
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    uint8_t name[256];
    int64_t meta[6] = {0};
    size_t consumed = 0;
    int code = gw_decode(data, size, 7, (gw_bytes){name, 256}, (gw_ints){meta, 6}, &consumed);
    if (code == GW_OK) {
        assert(consumed <= size && consumed >= sizeof(struct inotify_event));
        assert(meta[2] == 1 && meta[5] >= 0 && meta[5] <= 255);
        assert(!memchr(name, '/', (size_t)meta[5]));
    } else {
        assert(code == GW_CORRUPT && !meta[2] && consumed == 0);
    }
    int64_t pairs[512] = {0}, output[8] = {0};
    int64_t count = size ? (int64_t)data[0] + 1 : 1;
    for (int64_t i = 0; i < count; ++i) {
        pairs[i * 2] = 7 + i * 13;
        pairs[i * 2 + 1] = count - 1 - i;
    }
    output[6] = -1;
    output[7] = -1;
    consumed = 0;
    code = gs_decode(data, size, (gw_ints){pairs, 512}, count, (gw_bytes){name, 256},
                     (gw_ints){output, 8}, &consumed);
    if (code == GW_OK) {
        assert(output[2] && consumed <= size && output[5] >= 0 && output[5] <= 255);
        if (output[3] & IN_Q_OVERFLOW)
            assert(output[6] == -1 && output[4] == 0 && output[5] == 0);
        else
            assert(output[6] >= 0 && output[6] < count);
    } else
        assert(code == GW_CORRUPT && !output[2] && output[6] == -1 && consumed == 0);
    if (size <= GW_PATH)
        (void)gw_path((gw_const_bytes){data, (int64_t)size});
    return 0;
}
