#include "ipc.h"

#include <errno.h>
#include <unistd.h>

int ipc_write_full(int fd, const void *buf, size_t size)
{
    const char *p = (const char *)buf;
    size_t done = 0;
    while (done < size) {
        ssize_t n = write(fd, p + done, size - done);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) return -1;
        done += (size_t)n;
    }
    return 0;
}

int ipc_read_full(int fd, void *buf, size_t size)
{
    char *p = (char *)buf;
    size_t done = 0;
    while (done < size) {
        ssize_t n = read(fd, p + done, size - done);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) return -1;
        done += (size_t)n;
    }
    return 0;
}

int ipc_write_result(int fd, const Result *result)
{
    return ipc_write_full(fd, result, sizeof(*result));
}

int ipc_read_result(int fd, Result *result)
{
    return ipc_read_full(fd, result, sizeof(*result));
}
