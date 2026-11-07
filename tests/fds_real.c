/* SPDX-License-Identifier: Apache-2.0 */
#include "../native/set.c"
#include "../native/watch.c"
#include <assert.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/wait.h>
static int count_fds(void) {
    DIR *directory = opendir("/proc/self/fd");
    assert(directory);
    int count = 0;
    while (readdir(directory))
        ++count;
    assert(closedir(directory) == 0);
    return count;
}
int main(int argc, char **argv) {
    if (argc == 3) {
        errno = 0;
        assert(fcntl(atoi(argv[2]), F_GETFD) == -1 && errno == EBADF);
        return 0;
    }
    char path[] = "/tmp/watch-fds-real-XXXXXX";
    assert(mkdtemp(path));
    int root = open(path, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    assert(root >= 0);
    int alias = dup(root);
    assert(alias >= 0);
    int64_t state[8] = {0}, map[512] = {0}, meta[8] = {0}, descriptors[2] = {root, alias};
    gw_ints owner = {state, 8}, mapping = {map, 512}, info = {meta, 8};
    uint8_t buffer[GW_BUFFER], name[GW_NAME];
    int initial = count_fds();
    int flags = fcntl(root, F_GETFD);
    for (int i = 0; i < 64; ++i) {
        assert(gw_set_open_fds(owner, mapping, (gw_const_ints){descriptors, 1}, info) == GW_OK);
        assert(gw_set_close(owner, mapping, info) == GW_OK);
        assert(fcntl(root, F_GETFD) == flags);
        assert(gw_set_open_fds(owner, mapping, (gw_const_ints){descriptors, 2}, info) == GW_SYSTEM);
        assert(meta[0] == EEXIST && meta[7] == 1 && !state[0]);
        assert(fcntl(root, F_GETFD) == flags && fcntl(alias, F_GETFD) >= 0);
    }
    assert(count_fds() == initial);
    assert(gw_set_open_fds(owner, mapping, (gw_const_ints){descriptors, 1}, info) == GW_OK);
    int watcher = (int)(state[0] - 1);
    assert((fcntl(watcher, F_GETFD) & FD_CLOEXEC) && (fcntl(watcher, F_GETFL) & O_NONBLOCK));
    pid_t child = fork();
    assert(child >= 0);
    if (!child) {
        char text[32];
        snprintf(text, sizeof text, "%d", watcher);
        execl(argv[0], argv[0], "exec", text, (char *)NULL);
        _exit(127);
    }
    int status;
    assert(waitpid(child, &status, 0) == child && WIFEXITED(status) && !WEXITSTATUS(status));
    assert(gw_set_close(owner, mapping, info) == GW_OK);
    child = fork();
    assert(child >= 0);
    if (!child) {
        assert(dup2(root, 0) == 0);
        int before = fcntl(0, F_GETFD);
        int64_t zero = 0;
        assert(gw_set_open_fds(owner, mapping, (gw_const_ints){&zero, 1}, info) == GW_OK);
        assert(gw_set_close(owner, mapping, info) == GW_OK && fcntl(0, F_GETFD) == before);
        assert(close(0) == 0);
        _exit(0);
    }
    assert(waitpid(child, &status, 0) == child && WIFEXITED(status) && !WEXITSTATUS(status));
    descriptors[0] = alias;
    assert(gw_set_open_fds(owner, mapping, (gw_const_ints){descriptors, 1}, info) == GW_OK);
    assert(close(alias) == 0);
    int file = openat(root, "after-close.goose", O_WRONLY | O_CREAT | O_CLOEXEC, 0600);
    assert(file >= 0 && close(file) == 0);
    assert(gw_set_next(owner, mapping, (gw_bytes){buffer, GW_BUFFER}, 1000,
                       (gw_bytes){name, GW_NAME}, info) == GW_OK);
    assert(meta[2] && meta[6] == 0 && (meta[3] & IN_CREATE));
    assert(gw_set_close(owner, mapping, info) == GW_OK);
    assert(unlinkat(root, "after-close.goose", 0) == 0);
    // An external directory anchor may defer self-deletion notifications.
    assert(mkdirat(root, "deleted", 0700) == 0);
    int held = openat(root, "deleted", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    assert(held >= 0);
    descriptors[0] = held;
    assert(gw_set_open_fds(owner, mapping, (gw_const_ints){descriptors, 1}, info) == GW_OK);
    assert(unlinkat(root, "deleted", AT_REMOVEDIR) == 0);
    struct stat deleted;
    assert(fstat(held, &deleted) == 0 && deleted.st_nlink == 0);
    assert(gw_set_next(owner, mapping, (gw_bytes){buffer, GW_BUFFER}, 0, (gw_bytes){name, GW_NAME},
                       info) == GW_OK);
    printf("held-directory deletion: ready=%lld mask=%lld nlink=0\n", (long long)meta[2],
           (long long)meta[3]);
    assert(gw_set_close(owner, mapping, info) == GW_OK && fcntl(held, F_GETFD) >= 0);
    assert(close(held) == 0 && close(root) == 0 && rmdir(path) == 0);
    assert(count_fds() == initial - 2);
    puts("fd-zero, caller lifetime, watch flags/exec, alias rollback and leak tests passed");
}
