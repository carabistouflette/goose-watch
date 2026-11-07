/* SPDX-License-Identifier: Apache-2.0 */
#define _POSIX_C_SOURCE 200809L
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/inotify.h>
extern size_t gw_benchmark_decode(const uint8_t *, size_t, uint8_t *, int64_t *);
#include <assert.h>
#include <stdio.h>
#include <time.h>
int main(void) {
    uint8_t data[32] = {0}, name[256];
    int64_t meta[6] = {0};
    size_t used = 0;
    struct inotify_event event = {.wd = 7, .mask = IN_CREATE, .cookie = 0, .len = 16};
    memcpy(data, &event, sizeof event);
    memcpy(data + sizeof event, "sample.goose", 13);
    uint64_t sum = 0;
    struct timespec start, end;
    assert(clock_gettime(CLOCK_MONOTONIC, &start) == 0);
    for (uint32_t i = 0; i < 1000000; ++i) {
        memcpy(data + 8, &i, 4);
        used = gw_benchmark_decode(data, sizeof data, name, meta);
        assert(used == sizeof data);
        sum += (uint64_t)meta[4] + (uint64_t)meta[5] + name[i % 12];
    }
    assert(clock_gettime(CLOCK_MONOTONIC, &end) == 0);
    int64_t elapsed = (end.tv_sec - start.tv_sec) * 1000000000LL + end.tv_nsec - start.tv_nsec;
    printf("%lld %llu\n", (long long)elapsed, (unsigned long long)sum);
}
