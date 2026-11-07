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
static int init_error, add_error, close_error, poll_error, read_error;
static int closes, polls, reads, fd_value, poll_result = 1;
static short poll_events = POLLIN;
static uint8_t payload[65536];
static size_t payload_size;
static int test_init(int flags) {
    assert(flags == (IN_NONBLOCK | IN_CLOEXEC));
    if (init_error) {
        errno = init_error;
        return -1;
    }
    return fd_value;
}
static int test_add(int fd, const char *path, uint32_t mask) {
    assert(fd == fd_value && path[0] == '/');
    assert((mask & (IN_ONLYDIR | IN_DONT_FOLLOW)) == (IN_ONLYDIR | IN_DONT_FOLLOW));
    if (add_error) {
        errno = add_error;
        return -1;
    }
    return 7;
}
static int test_close(int fd) {
    assert(fd == fd_value);
    ++closes;
    if (close_error) {
        errno = close_error;
        return -1;
    }
    return 0;
}
static int test_poll(struct pollfd *item, nfds_t count, int timeout) {
    assert(count == 1 && item->fd == fd_value && timeout >= 0 && timeout <= 60000);
    ++polls;
    if (poll_error) {
        errno = poll_error;
        return -1;
    }
    item->revents = poll_events;
    return poll_result;
}
static ssize_t test_read(int fd, void *out, size_t size) {
    assert(fd == fd_value && size >= payload_size);
    ++reads;
    if (read_error) {
        errno = read_error;
        return -1;
    }
    memcpy(out, payload, payload_size);
    return (ssize_t)payload_size;
}
#define GW_INIT test_init
#define GW_ADD test_add
#define GW_CLOSE test_close
#define GW_POLL test_poll
#define GW_READ test_read
#include "../native/watch.c"
static int64_t state[8], meta[6];
static uint8_t buffer[GW_BUFFER], name[GW_NAME];
static gw_ints owner = {state, 8}, info = {meta, 6};
static int open_owner(void) {
    return (int)gw_open(owner, (gw_const_bytes){(const uint8_t *)"/tmp", 4}, info);
}
static int next(void) {
    return (int)gw_next(owner, (gw_bytes){buffer, sizeof buffer}, 0, (gw_bytes){name, sizeof name},
                        info);
}
static void record(int wd, uint32_t mask, uint32_t cookie, const char *text) {
    struct inotify_event event = {.wd = wd, .mask = mask, .cookie = cookie, .len = text ? 16 : 0};
    memcpy(payload + payload_size, &event, sizeof event);
    payload_size += sizeof event;
    if (text) {
        memset(payload + payload_size, 0, 16);
        memcpy(payload + payload_size, text, strlen(text));
        payload_size += 16;
    }
}
int main(void) {
    assert(gw_next(owner, (gw_bytes){buffer, sizeof buffer}, 0, (gw_bytes){name, sizeof name},
                   info) == GW_STATE);
    assert(gw_open((gw_ints){NULL, 8}, (gw_const_bytes){NULL, 0}, info) == GW_INPUT);
    assert(gw_close(owner, (gw_ints){NULL, 0}) == GW_INPUT);
    init_error = EMFILE;
    assert(open_owner() == GW_SYSTEM && meta[0] == EMFILE && state[0] == 0);
    init_error = 0;
    add_error = ENOTDIR;
    close_error = EINTR;
    assert(open_owner() == GW_SYSTEM && meta[0] == ENOTDIR && meta[1] == EINTR && closes == 1 &&
           !state[0]);
    add_error = 0;
    close_error = 0;
    assert(open_owner() == GW_OK && state[0] == 1); /* descriptor zero */
    assert(open_owner() == GW_STATE && state[0] == 1);
    poll_result = 0;
    assert(next() == GW_OK && !meta[2]);
    poll_result = 1;
    poll_error = EINTR;
    assert(next() == GW_INTERRUPTED && meta[0] == EINTR);
    poll_error = 0;
    const short errors[] = {POLLNVAL, POLLERR, POLLHUP};
    for (size_t i = 0; i < 3; ++i) {
        poll_events = errors[i];
        assert(next() == GW_SYSTEM && meta[0] == (i ? EIO : EBADF));
    }
    poll_events = 0;
    assert(next() == GW_OK && !meta[2]);
    poll_events = POLLIN;
    read_error = EAGAIN;
    assert(next() == GW_OK && !meta[2]);
    read_error = EINTR;
    assert(next() == GW_INTERRUPTED && meta[0] == EINTR);
    read_error = EIO;
    assert(next() == GW_SYSTEM && meta[0] == EIO);
    read_error = 0;
    record(7, IN_MOVED_FROM, UINT32_MAX, "before");
    record(7, IN_MOVED_TO, UINT32_MAX, "after");
    record(-1, IN_Q_OVERFLOW, 0, NULL);
    record(7, IN_IGNORED, 0, NULL);
    int old_reads = reads, old_polls = polls;
    assert(next() == GW_OK && meta[2] && meta[3] == IN_MOVED_FROM && meta[4] == UINT32_MAX &&
           meta[5] == 6);
    assert(!memcmp(name, "before", 6));
    assert(next() == GW_OK && meta[3] == IN_MOVED_TO && !memcmp(name, "after", 5));
    assert(next() == GW_OK && meta[3] == IN_Q_OVERFLOW && !meta[5]);
    assert(next() == GW_OK && meta[3] == IN_IGNORED);
    assert(next() == GW_END && !meta[2]);
    assert(reads == old_reads + 1 && polls == old_polls + 1); /* syscall-free tail */
    close_error = EINTR;
    int old_closes = closes;
    assert(gw_close(owner, info) == GW_INTERRUPTED && !state[0]);
    assert(gw_close(owner, info) == GW_OK && closes == old_closes + 1);
    close_error = 0;
    assert(open_owner() == GW_OK);
    payload_size = 1;
    assert(next() == GW_CORRUPT && !meta[2]);
    old_reads = reads;
    old_polls = polls;
    assert(next() == GW_CORRUPT && reads == old_reads && polls == old_polls);
    assert(gw_close(owner, info) == GW_OK && open_owner() == GW_OK);
    payload_size = 0;
    assert(next() == GW_CORRUPT); /* impossible EOF */
    assert(gw_close(owner, info) == GW_OK && open_owner() == GW_OK);
    state[2] = -1;
    assert(next() == GW_CORRUPT);
    assert(gw_close(owner, info) == GW_OK);
    assert(open_owner() == GW_OK);
    payload_size = 0;
    for (uint32_t i = 0; i < 2048; ++i)
        record(7, IN_CREATE, i, "item");
    assert(payload_size == GW_BUFFER);
    for (int round = 0; round < 2; ++round) {
        old_reads = reads;
        old_polls = polls;
        for (int i = 0; i < 2048; ++i)
            assert(next() == GW_OK && meta[4] == i && meta[3] == IN_CREATE);
        assert(reads == old_reads + 1 && polls == old_polls + 1);
    }
    assert(gw_close(owner, info) == GW_OK);
    puts("fault, overflow, cookie, poison, fd-zero and syscall-count tests passed");
}
