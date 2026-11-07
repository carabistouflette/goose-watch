/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <stdint.h>
#include <sys/inotify.h>
static int calls;
static int counted_init(int flags) {
    ++calls;
    return inotify_init1(flags);
}
static int counted_add(int fd, const char *path, uint32_t mask) {
    ++calls;
    return inotify_add_watch(fd, path, mask);
}
#define GW_INIT counted_init
#define GW_ADD counted_add
#define GW_READ read
#define GW_POLL poll
#define GW_CLOSE close
#define GW_FSTAT MUST_NOT_CALL_FSTAT_WITHOUT_MASK_SUPPORT
#define GW_SET_NO_MASK_CREATE
#include "../native/set.c"
#include "../native/watch.c"
#include <assert.h>
int main(void) {
    int64_t state[8] = {0}, map[512] = {0}, meta[8] = {0}, offsets[2] = {0, 4};
    int code = (int)gw_set_open((gw_ints){state, 8}, (gw_ints){map, 512},
                                (gw_const_bytes){(const uint8_t *)"/tmp\0", 5},
                                (gw_ints){offsets, 2}, (gw_ints){meta, 8});
    assert(code == GW_SYSTEM && meta[0] == ENOTSUP && !state[0] && calls == 0);
    int64_t borrowed = 0;
    assert(gw_set_open_fds((gw_ints){state, 8}, (gw_ints){map, 512}, (gw_const_ints){&borrowed, 1},
                           (gw_ints){meta, 8}) == GW_SYSTEM);
    assert(meta[0] == ENOTSUP && !state[0] && calls == 0);
    assert(gw_open((gw_ints){state, 8}, (gw_const_bytes){(const uint8_t *)"/tmp", 4},
                   (gw_ints){meta, 6}) == GW_OK);
    assert(calls == 2);
    assert(gw_close((gw_ints){state, 8}, (gw_ints){meta, 6}) == GW_OK);
}
