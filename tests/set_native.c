/* SPDX-License-Identifier: Apache-2.0 */
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <errno.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/inotify.h>
#include <unistd.h>
static int inits, adds, closes, polls, reads, fail_add = -1, init_error, close_error, poll_error,
                                              read_error;
static int add_error = EEXIST;
static int poll_result = 1;
static short poll_events = POLLIN;
static int ids[256];
static uint8_t payload[65536];
static size_t payload_size;
static int fake_init(int flags) {
    assert(flags == (IN_NONBLOCK | IN_CLOEXEC));
    ++inits;
    if (init_error) {
        errno = init_error;
        return -1;
    }
    return 0;
}
static int fake_add(int fd, const char *path, uint32_t mask) {
    assert(fd == 0 && path[0] == '/');
    assert(mask & IN_MASK_CREATE);
    int i = adds++;
    if (i == fail_add) {
        errno = add_error;
        return -1;
    }
    return ids[i];
}
static int fake_close(int fd) {
    assert(fd == 0);
    ++closes;
    if (close_error) {
        errno = close_error;
        return -1;
    }
    return 0;
}
static int fake_poll(struct pollfd *item, nfds_t count, int timeout) {
    assert(item->fd == 0 && count == 1 && timeout >= 0);
    ++polls;
    if (poll_error) {
        errno = poll_error;
        return -1;
    }
    item->revents = poll_events;
    return poll_result;
}
static ssize_t fake_read(int fd, void *out, size_t size) {
    assert(fd == 0 && size >= payload_size);
    ++reads;
    if (read_error) {
        errno = read_error;
        return -1;
    }
    memcpy(out, payload, payload_size);
    return (ssize_t)payload_size;
}
#define GW_INIT fake_init
#define GW_ADD fake_add
#define GW_CLOSE fake_close
#define GW_POLL fake_poll
#define GW_READ fake_read
#include "../native/set.c"
#include "../native/watch.c"
static int64_t state[8], map[512], meta[8], offsets[512];
static uint8_t packed[1048576], buffer[65536], name[256];
static gw_ints owner = {state, 8}, mapping = {map, 512}, info = {meta, 8};
static int open_paths(const char **paths, int count) {
    size_t used = 0;
    adds = 0;
    for (int i = 0; i < count; ++i) {
        size_t len = strlen(paths[i]);
        offsets[i * 2] = (int64_t)used;
        offsets[i * 2 + 1] = (int64_t)len;
        memcpy(packed + used, paths[i], len + 1);
        used += len + 1;
    }
    return (int)gw_set_open(owner, mapping, (gw_const_bytes){packed, (int64_t)used},
                            (gw_ints){offsets, count * 2}, info);
}
static int next(void) {
    return (int)gw_set_next(owner, mapping, (gw_bytes){buffer, sizeof buffer}, 0,
                            (gw_bytes){name, sizeof name}, info);
}
static void record(int wd, uint32_t mask, uint32_t cookie, const char *text) {
    struct inotify_event event = {.wd = wd, .mask = mask, .cookie = cookie, .len = text ? 16 : 0};
    assert(payload_size + sizeof event + event.len <= sizeof payload);
    memcpy(payload + payload_size, &event, sizeof event);
    payload_size += sizeof event;
    if (text) {
        memset(payload + payload_size, 0, 16);
        memcpy(payload + payload_size, text, strlen(text));
        payload_size += 16;
    }
}
static void terminal(uint32_t mask) {
    payload_size = 0;
    record(mask == IN_Q_OVERFLOW ? -1 : 5, mask, 0, NULL);
    record(5, IN_CREATE, 42, "after");
    memcpy(buffer, payload, payload_size);
    memset(state, 0, sizeof state);
    state[0] = 1;
    state[1] = 1;
    state[3] = (int64_t)payload_size;
    map[0] = 5;
    map[1] = 0;
    int old_reads = reads, old_polls = polls;
    assert(next() == GW_OK && meta[3] == mask && meta[6] == (mask == IN_Q_OVERFLOW ? -1 : 0));
    assert(next() == GW_END && !meta[2] && meta[6] == -1);
    assert(reads == old_reads && polls == old_polls);
    assert(gw_set_close(owner, mapping, info) == GW_OK);
    // Identical records preserve alpha.1 trailing delivery and overflow continuation.
    memcpy(buffer, payload, payload_size);
    state[0] = 1;
    state[1] = 5;
    state[3] = (int64_t)payload_size;
    gw_ints legacy = {meta, 6};
    assert(gw_next(owner, (gw_bytes){buffer, sizeof buffer}, 0, (gw_bytes){name, sizeof name},
                   legacy) == GW_OK &&
           meta[3] == mask);
    assert(gw_next(owner, (gw_bytes){buffer, sizeof buffer}, 0, (gw_bytes){name, sizeof name},
                   legacy) == GW_OK &&
           meta[3] == IN_CREATE);
    poll_result = 0;
    int code = (int)gw_next(owner, (gw_bytes){buffer, sizeof buffer}, 0,
                            (gw_bytes){name, sizeof name}, legacy);
    assert(code == (mask == IN_IGNORED ? GW_END : GW_OK));
    poll_result = 1;
    assert(gw_close(owner, legacy) == GW_OK);
}
int main(void) {
    for (int i = 0; i < 256; ++i)
        ids[i] = 256 - i;
    const char *paths[] = {"/first", "/second", "/third"};
    int old_inits = inits;
    assert(gw_set_open(owner, mapping, (gw_const_bytes){packed, 1}, (gw_ints){offsets, 0}, info) ==
           GW_INPUT);
    assert(gw_set_open(owner, mapping, (gw_const_bytes){packed, 1}, (gw_ints){offsets, 514},
                       info) == GW_INPUT);
    const char *bad[] = {"/same", "/same"};
    assert(open_paths(bad, 2) == GW_INPUT && meta[7] == 1 && inits == old_inits);
    const char *invalid[] = {"/first", "/a/.."};
    assert(open_paths(invalid, 2) == GW_INPUT && meta[7] == 1 && inits == old_inits);
    // Malformed private packed ABI must also fail before acquiring resources.
    const int64_t extremes[] = {INT64_MIN, INT64_MAX};
    for (size_t i = 0; i < 2; ++i) {
        offsets[0] = extremes[i];
        offsets[1] = 1;
        assert(gw_set_open(owner, mapping, (gw_const_bytes){packed, 4}, (gw_ints){offsets, 2},
                           info) == GW_INPUT &&
               inits == old_inits);
        offsets[0] = 0;
        offsets[1] = extremes[i];
        assert(gw_set_open(owner, mapping, (gw_const_bytes){packed, 4}, (gw_ints){offsets, 2},
                           info) == GW_INPUT &&
               inits == old_inits);
    }
    memcpy(packed, "/okX", 4);
    offsets[0] = 0;
    offsets[1] = 3;
    assert(gw_set_open(owner, mapping, (gw_const_bytes){packed, 4}, (gw_ints){offsets, 2}, info) ==
               GW_INPUT &&
           inits == old_inits); // Missing terminator.
    packed[3] = 0;
    packed[4] = 0;
    assert(gw_set_open(owner, mapping, (gw_const_bytes){packed, 5}, (gw_ints){offsets, 2}, info) ==
               GW_INPUT &&
           inits == old_inits); // Unclaimed trailing byte.
    packed[0] = '/';
    packed[1] = 0;
    packed[2] = 'x';
    packed[3] = 0;
    offsets[0] = 0;
    offsets[1] = 3;
    assert(gw_set_open(owner, mapping, (gw_const_bytes){packed, 4}, (gw_ints){offsets, 2}, info) ==
               GW_INPUT &&
           inits == old_inits);
    init_error = EMFILE;
    assert(open_paths(paths, 3) == GW_SYSTEM && meta[0] == EMFILE && meta[7] == -1);
    init_error = 0;
    for (int fail = 0; fail < 3; ++fail) {
        fail_add = fail;
        close_error = EINTR;
        int old = closes;
        assert(open_paths(paths, 3) == GW_SYSTEM && meta[0] == EEXIST && meta[1] == EINTR &&
               meta[7] == fail && !state[0] && closes == old + 1);
    }
    close_error = 0;
    fail_add = 0;
    add_error = EINVAL; // Older kernels reject the mandatory mask; never downgrade.
    int prior_closes = closes;
    assert(open_paths(paths, 3) == GW_SYSTEM && meta[0] == EINVAL && meta[7] == 0 && !state[0] &&
           closes == prior_closes + 1 && adds == 1);
    fail_add = -1;
    add_error = EEXIST;
    ids[0] = INT_MAX;
    ids[1] = 0;
    ids[2] = 41;
    assert(open_paths(paths, 3) == GW_OK && state[0] == 1 && state[1] == 3 && meta[7] == -1);
    assert(gs_lookup(mapping, 3, INT_MAX) == 0 && gs_lookup(mapping, 3, 0) == 1 &&
           gs_lookup(mapping, 3, 41) == 2 && gs_lookup(mapping, 3, 42) == -1);
    int64_t saved_state[8], saved_map[512];
    memcpy(saved_state, state, sizeof state);
    memcpy(saved_map, map, sizeof map);
    assert(open_paths(paths, 3) == GW_STATE && !memcmp(saved_state, state, sizeof state) &&
           !memcmp(saved_map, map, sizeof map));
    payload_size = 0;
    record(0, IN_MOVED_FROM, UINT32_MAX, "before");
    record(41, IN_MOVED_TO, UINT32_MAX, "after");
    record(INT_MAX, IN_CREATE, 0, "first");
    int old_reads = reads, old_polls = polls;
    assert(next() == GW_OK && meta[6] == 1 && meta[4] == UINT32_MAX && meta[5] == 6);
    memcpy(saved_state, state, sizeof state);
    memcpy(saved_map, map, sizeof map);
    uint8_t saved_buffer[65536];
    memcpy(saved_buffer, buffer, sizeof buffer);
    assert(open_paths(paths, 3) == GW_STATE && !memcmp(saved_state, state, sizeof state) &&
           !memcmp(saved_map, map, sizeof map) && !memcmp(saved_buffer, buffer, sizeof buffer));
    assert(next() == GW_OK && meta[6] == 2 && meta[4] == UINT32_MAX);
    assert(next() == GW_OK && meta[6] == 0 && meta[3] == IN_CREATE);
    assert(reads == old_reads + 1 && polls == old_polls + 1);
    assert(gw_set_close(owner, mapping, info) == GW_OK);
    terminal(IN_IGNORED);
    terminal(IN_Q_OVERFLOW);
    ids[0] = 5;
    assert(open_paths(paths, 1) == GW_OK);
    for (int variant = 0; variant < 4; ++variant) {
        payload_size = 0;
        record(variant == 0   ? 99
               : variant == 1 ? 5
                              : -1,
               variant == 0 ? IN_CREATE : IN_Q_OVERFLOW, variant == 2 ? 1 : 0,
               variant == 3 ? "bad" : NULL);
        assert(next() == GW_CORRUPT && !meta[2] && meta[6] == -1 && state[5]);
        old_reads = reads;
        old_polls = polls;
        assert(next() == GW_CORRUPT && reads == old_reads && polls == old_polls);
        assert(gw_set_close(owner, mapping, info) == GW_OK && open_paths(paths, 1) == GW_OK);
    }
    poll_error = EINTR;
    assert(next() == GW_INTERRUPTED && meta[0] == EINTR);
    poll_error = 0;
    read_error = EAGAIN;
    assert(next() == GW_OK && !meta[2]);
    read_error = EINTR;
    assert(next() == GW_INTERRUPTED);
    read_error = 0;
    poll_events = POLLNVAL;
    assert(next() == GW_SYSTEM && meta[0] == EBADF);
    poll_events = POLLIN;
    close_error = EINTR;
    int old_closes = closes;
    assert(gw_set_close(owner, mapping, info) == GW_INTERRUPTED && !state[0]);
    assert(gw_set_close(owner, mapping, info) == GW_OK && closes == old_closes + 1);
    close_error = 0;
    // Every insertion and binary lookup at the full cap, with descending IDs.
    const char *many[256];
    char texts[256][20];
    for (int i = 0; i < 256; ++i) {
        snprintf(texts[i], sizeof texts[i], "/d%d", i);
        many[i] = texts[i];
        ids[i] = 256 - i;
    }
    for (int fail = 0; fail < 256; ++fail) {
        fail_add = fail;
        int old = closes;
        assert(open_paths(many, 256) == GW_SYSTEM && meta[7] == fail && meta[0] == EEXIST &&
               !state[0] && closes == old + 1);
    }
    fail_add = -1;
    assert(open_paths(many, 256) == GW_OK);
    for (int i = 0; i < 256; ++i)
        assert(gs_lookup(mapping, 256, ids[i]) == i);
    assert(gw_set_close(owner, mapping, info) == GW_OK);
    ids[0] = ids[1] = 9;
    assert(open_paths(paths, 2) == GW_SYSTEM && meta[0] == EEXIST && meta[7] == 1 && !state[0]);
    puts(
        "set transaction, mapping, terminal compatibility, poison, fd-zero and fault tests passed");
}
