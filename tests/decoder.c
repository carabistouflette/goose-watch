/* SPDX-License-Identifier: Apache-2.0 */
#include "../native/watch.c"
#include <assert.h>
#include <stdio.h>
int main(void) {
    uint8_t storage[1024] = {0}, name[256] = {0};
    uint8_t *data = storage + 1; /* deliberately unaligned */
    int64_t meta[6] = {0};
    size_t consumed = 0;
    struct inotify_event header = {.wd = 7, .mask = IN_CREATE, .cookie = UINT32_MAX, .len = 256};
    memcpy(data, &header, sizeof header);
    memset(data + sizeof header, 0xff, 255);
    assert(gw_decode(data, sizeof header + 256, 7, (gw_bytes){name, 256}, (gw_ints){meta, 6},
                     &consumed) == GW_OK);
    assert(consumed == sizeof header + 256 && meta[5] == 255 && meta[4] == UINT32_MAX &&
           name[254] == 255);
    for (size_t size = 0; size < sizeof header + 256; ++size)
        assert(gw_decode(data, size, 7, (gw_bytes){name, 256}, (gw_ints){meta, 6}, &consumed) ==
               GW_CORRUPT);
    data[sizeof header + 255] = 1;
    assert(gw_decode(data, 272, 7, (gw_bytes){name, 256}, (gw_ints){meta, 6}, &consumed) ==
           GW_CORRUPT);
    data[sizeof header + 255] = 0;
    data[sizeof header] = '/';
    assert(gw_decode(data, 272, 7, (gw_bytes){name, 256}, (gw_ints){meta, 6}, &consumed) ==
           GW_CORRUPT);
    header.len = UINT32_MAX;
    memcpy(data, &header, sizeof header);
    assert(gw_decode(data, 1023, 7, (gw_bytes){name, 256}, (gw_ints){meta, 6}, &consumed) ==
           GW_CORRUPT);
    header.len = 0;
    header.wd = 8;
    memcpy(data, &header, sizeof header);
    assert(gw_decode(data, 16, 7, (gw_bytes){name, 256}, (gw_ints){meta, 6}, &consumed) ==
           GW_CORRUPT);
    header.wd = -1;
    header.mask = IN_Q_OVERFLOW;
    memcpy(data, &header, sizeof header);
    assert(gw_decode(data, 16, 7, (gw_bytes){name, 256}, (gw_ints){meta, 6}, &consumed) == GW_OK);
    const char *bad[] = {"", "a", "/a/", "/a//b", "/a/.", "/a/..", "/./a", "/../a"};
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; ++i)
        assert(!gw_path((gw_const_bytes){(const uint8_t *)bad[i], (int64_t)strlen(bad[i])}));
    assert(gw_path((gw_const_bytes){(const uint8_t *)"/", 1}));
    assert(gw_path((gw_const_bytes){(const uint8_t *)"/a/.../b", 8}));
    puts("unaligned, malformed, maximum-byte-name and normalized-path tests passed");
}
