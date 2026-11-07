/* SPDX-License-Identifier: Apache-2.0 */
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
    if (size <= GW_PATH)
        (void)gw_path((gw_const_bytes){data, (int64_t)size});
    return 0;
}
