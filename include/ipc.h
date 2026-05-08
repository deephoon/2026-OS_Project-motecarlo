#ifndef IPC_H
#define IPC_H

#include "result.h"

#include <stddef.h>

int ipc_write_full(int fd, const void *buf, size_t size);
int ipc_read_full(int fd, void *buf, size_t size);
int ipc_write_result(int fd, const Result *result);
int ipc_read_result(int fd, Result *result);

#endif
