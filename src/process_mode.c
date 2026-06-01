#include "process_mode.h"

#include "ipc.h"
#include "ipc_shm.h"
#include "postprocess.h"
#include "preprocess.h"
#include "simulation.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

static void process_partition(long trials, int processes, int pid,
                              long *start_idx, long *end_idx)
{
    long base = trials / processes;
    long rem = trials % processes;
    long extra_before = pid < rem ? pid : rem;
    *start_idx = (long)pid * base + extra_before;
    *end_idx = *start_idx + base + (pid < rem ? 1 : 0);
}

int run_process_mode(const Config *cfg, Result *out, StageMetrics *metrics)
{
    int (*pipes)[2] = 0;
    pid_t *pids = 0;
    ShmResultTable shm_table = {0};
    int failed = 0;
    double start;
    double end;
    PostSummary summary;

    if (cfg == 0 || out == 0 || metrics == 0 || cfg->processes <= 0) {
        return -1;
    }

    metrics_init(metrics);
    metrics->t_total_start = now_sec();
    result_init(out);
    pids = calloc((size_t)cfg->processes, sizeof(*pids));
    if (cfg->ipc_mode == IPC_PIPE) {
        pipes = calloc((size_t)cfg->processes, sizeof(*pipes));
    } else if (shm_result_table_create(&shm_table, cfg->processes) != 0) {
        free(pids);
        return -1;
    }
    if ((cfg->ipc_mode == IPC_PIPE && pipes == 0) || pids == 0) {
        free(pipes);
        free(pids);
        shm_result_table_destroy(&shm_table);
        return -1;
    }

    start = now_sec();
    preprocess_run_extra_work(cfg, (int)((cfg->trials + cfg->batch_size - 1) / cfg->batch_size));
    end = now_sec();
    metrics->t_pre = elapsed_sec(start, end);

    for (int i = 0; i < cfg->processes; ++i) {
        if (cfg->ipc_mode == IPC_PIPE && pipe(pipes[i]) != 0) {
            failed = 1;
            break;
        }
        pids[i] = fork();
        if (pids[i] < 0) {
            failed = 1;
            break;
        }
        if (pids[i] == 0) {
            Result local;
            TaskBatch batch;
            Config child_cfg = *cfg;
            long s;
            long e;
            if (cfg->ipc_mode == IPC_PIPE) {
                close(pipes[i][0]);
            }
            process_partition(cfg->trials, cfg->processes, i, &s, &e);
            batch.batch_id = i;
            batch.start_idx = s;
            batch.end_idx = e;
            batch.base_seed = cfg->seed;
            batch.time_steps = cfg->time_steps;
            batch.difficulty_level = i % 3;
            run_batch(&child_cfg, &batch, &local);
            if (cfg->ipc_mode == IPC_PIPE) {
                ipc_write_result(pipes[i][1], &local);
                close(pipes[i][1]);
            } else {
                shm_write_result(&shm_table, i, &local);
            }
            _exit(0);
        }
        if (cfg->ipc_mode == IPC_PIPE) {
            close(pipes[i][1]);
        }
    }
    for (int i = 0; i < cfg->processes; ++i) {
        if (pids[i] > 0) {
            int status;
            double wait_start = now_sec();
            if (waitpid(pids[i], &status, 0) < 0 || !WIFEXITED(status) ||
                WEXITSTATUS(status) != 0) {
                failed = 1;
            }
            metrics->t_sync += elapsed_sec(wait_start, now_sec());
        }
    }
    if (cfg->ipc_mode == IPC_PIPE) {
        for (int i = 0; i < cfg->processes; ++i) {
            Result child_result;
            double read_start = now_sec();
            if (pids[i] > 0 && ipc_read_result(pipes[i][0], &child_result) == 0) {
                double merge_start;
                metrics->t_ipc += elapsed_sec(read_start, now_sec());
                metrics->ipc_read_count += 1;
                metrics->ipc_bytes += sizeof(child_result);
                merge_start = now_sec();
                result_merge(out, &child_result);
                metrics->t_merge += elapsed_sec(merge_start, now_sec());
            } else if (pids[i] > 0) {
                metrics->t_ipc += elapsed_sec(read_start, now_sec());
                failed = 1;
            }
            if (pipes[i][0] >= 0) {
                close(pipes[i][0]);
            }
        }
    } else if (cfg->ipc_mode == IPC_SHM) {
        for (int i = 0; i < cfg->processes; ++i) {
            Result child_result;
            double read_start = now_sec();
            if (shm_read_result(&shm_table, i, &child_result)) {
                double merge_start;
                metrics->t_ipc += elapsed_sec(read_start, now_sec());
                metrics->ipc_read_count += 1;
                metrics->ipc_bytes += sizeof(child_result);
                merge_start = now_sec();
                result_merge(out, &child_result);
                metrics->t_merge += elapsed_sec(merge_start, now_sec());
            } else {
                failed = 1;
            }
        }
    }
    end = now_sec();
    (void)end;

    start = now_sec();
    postprocess_finalize(out, cfg->trials, &summary);
    postprocess_run_extra_work(cfg, out);
    end = now_sec();
    metrics->t_post = elapsed_sec(start, end);
    metrics->processed_batches = cfg->processes;
    metrics->t_total_end = now_sec();
    metrics->t_compute = metrics_total(metrics) - metrics->t_pre -
                         metrics->t_sync - metrics->t_ipc - metrics->t_merge -
                         metrics->t_post;
    if (metrics->t_compute < 0.0) {
        metrics->t_compute = 0.0;
    }

    free(pipes);
    free(pids);
    shm_result_table_destroy(&shm_table);
    return failed ? -1 : 0;
}
