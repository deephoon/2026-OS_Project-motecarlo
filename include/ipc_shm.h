#ifndef IPC_SHM_H
#define IPC_SHM_H

#include "result.h"

typedef struct {
    Result *slots;
    int *done;
    int process_count;
} ShmResultTable;

int shm_result_table_create(ShmResultTable *table, int process_count);
void shm_result_table_destroy(ShmResultTable *table);
void shm_write_result(ShmResultTable *table, int slot, const Result *result);
int shm_read_result(ShmResultTable *table, int slot, Result *out);

#endif
