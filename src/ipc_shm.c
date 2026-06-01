#define _GNU_SOURCE
#define _DARWIN_C_SOURCE

#include "ipc_shm.h"

#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#ifndef MAP_ANONYMOUS
#ifdef MAP_ANON
#define MAP_ANONYMOUS MAP_ANON
#endif
#endif

int shm_result_table_create(ShmResultTable *table, int process_count)
{
    size_t result_size;
    size_t done_size;

    if (table == 0 || process_count <= 0) {
        return -1;
    }

    result_size = sizeof(Result) * (size_t)process_count;
    done_size = sizeof(int) * (size_t)process_count;
    table->slots = mmap(0, result_size, PROT_READ | PROT_WRITE,
                        MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (table->slots == MAP_FAILED) {
        table->slots = 0;
        return -1;
    }

    table->done = mmap(0, done_size, PROT_READ | PROT_WRITE,
                       MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (table->done == MAP_FAILED) {
        munmap(table->slots, result_size);
        table->slots = 0;
        table->done = 0;
        return -1;
    }

    memset(table->slots, 0, result_size);
    memset(table->done, 0, done_size);
    table->process_count = process_count;
    return 0;
}

void shm_result_table_destroy(ShmResultTable *table)
{
    if (table == 0) {
        return;
    }
    if (table->slots != 0) {
        munmap(table->slots, sizeof(Result) * (size_t)table->process_count);
    }
    if (table->done != 0) {
        munmap(table->done, sizeof(int) * (size_t)table->process_count);
    }
    table->slots = 0;
    table->done = 0;
    table->process_count = 0;
}

void shm_write_result(ShmResultTable *table, int slot, const Result *result)
{
    if (table == 0 || result == 0 || slot < 0 || slot >= table->process_count) {
        return;
    }
    table->slots[slot] = *result;
    table->done[slot] = 1;
}

int shm_read_result(ShmResultTable *table, int slot, Result *out)
{
    if (table == 0 || out == 0 || slot < 0 || slot >= table->process_count) {
        return 0;
    }
    if (!table->done[slot]) {
        return 0;
    }
    *out = table->slots[slot];
    return 1;
}
