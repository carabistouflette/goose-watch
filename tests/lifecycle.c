/* SPDX-License-Identifier: Apache-2.0 */
#include "../native/watch.c"
#include <assert.h>
#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/wait.h>
static int descriptors(void) {
    DIR *dir = opendir("/proc/self/fd");
    assert(dir);
    int count = 0;
    while (readdir(dir))
        ++count;
    assert(closedir(dir) == 0);
    return count;
}
static void caught(int signum) { (void)signum; }
int main(int argc, char **argv) {
    if (argc == 3) { /* exec must drop the watch, not merely mark its flag */
        int fd = atoi(argv[2]);
        errno = 0;
        assert(fcntl(fd, F_GETFD) == -1 && errno == EBADF);
        return 0;
    }
    char path[] = "/tmp/goose-watch-XXXXXX";
    assert(mkdtemp(path));
    int64_t state[8] = {0}, meta[6] = {0};
    uint8_t buffer[GW_BUFFER], name[GW_NAME];
    gw_ints owner = {state, 8}, info = {meta, 6};
    gw_const_bytes directory = {(uint8_t *)path, (int64_t)strlen(path)};
    int before = descriptors();
    for (int i = 0; i < 256; ++i) {
        assert(gw_open(owner, directory, info) == GW_OK);
        assert(gw_close(owner, info) == GW_OK);
    }
    assert(descriptors() == before);
    for (int i = 0; i < 64; ++i) {
        assert(gw_open(owner, (gw_const_bytes){(const uint8_t *)"/dev/null", 9}, info) ==
               GW_SYSTEM);
        assert(!state[0]);
    }
    assert(descriptors() == before);
    assert(gw_open(owner, directory, info) == GW_OK);
    int64_t second_state[8] = {0};
    gw_ints second = {second_state, 8};
    assert(gw_open(second, directory, info) == GW_OK);
    assert(second_state[0] != state[0]);
    assert(gw_close(second, info) == GW_OK);
    assert(gw_next(owner, (gw_bytes){buffer, sizeof buffer}, 0, (gw_bytes){name, sizeof name},
                   info) == GW_OK);
    int fd = (int)(state[0] - 1);
    assert(fcntl(fd, F_GETFL) & O_NONBLOCK);
    assert(fcntl(fd, F_GETFD) & FD_CLOEXEC);
    pid_t child = fork();
    assert(child >= 0);
    if (!child) {
        char value[32];
        snprintf(value, sizeof value, "%d", fd);
        execl(argv[0], argv[0], "exec", value, (char *)NULL);
        _exit(127);
    }
    int status = 0;
    assert(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    struct sigaction action = {0};
    action.sa_handler = caught;
    assert(sigemptyset(&action.sa_mask) == 0);
    assert(sigaction(SIGALRM, &action, NULL) == 0);
    struct itimerval timer = {.it_interval = {.tv_sec = 0, .tv_usec = 10000},
                              .it_value = {.tv_sec = 0, .tv_usec = 10000}};
    assert(setitimer(ITIMER_REAL, &timer, NULL) == 0);
    assert(gw_next(owner, (gw_bytes){buffer, sizeof buffer}, 1000, (gw_bytes){name, sizeof name},
                   info) == GW_INTERRUPTED);
    struct itimerval stopped = {0};
    assert(setitimer(ITIMER_REAL, &stopped, NULL) == 0);
    assert(meta[0] == EINTR && !meta[2]);
    assert(gw_close(owner, info) == GW_OK && descriptors() == before);
    assert(rmdir(path) == 0);
    puts("real fd leak, close-on-exec and signal interruption tests passed");
}
