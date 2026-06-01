#include "hybrid_mode.h"

#include "ipc.h"
#include "ipc_shm.h"
#include "postprocess.h"
#include "preprocess.h"
#include "simulation.h"

#include <pthread.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct {
    const Config *cfg;
    long start_idx;
    long end_idx;
    Result local;
} HybridThreadArg;

static void *hybrid_thread_worker(void *arg_ptr)
{
    HybridThreadArg *arg = (HybridThreadArg *)arg_ptr;
    result_init(&arg->local);
    for (long i = arg->start_idx; i < arg->end_idx; ++i) {
        int collided = 0;
        RiskLevel risk = run_trial_for_index(arg->cfg, i, &collided);
        result_add_trial(&arg->local, risk, collided);
    }
    return 0;
}

static void hybrid_partition(long trials, int processes, int pid,
                             long *start_idx, long *end_idx)
{
    long base = trials / processes;
    long rem = trials % processes;
    long extra_before = pid < rem ? pid : rem;
    *start_idx = (long)pid * base + extra_before;
    *end_idx = *start_idx + base + (pid < rem ? 1 : 0);
}

static int run_child_thread_group(const Config *cfg, long start_idx, long end_idx,
                                  Result *out)
{
    pthread_t *threads = 0;
    HybridThreadArg *args = 0;
    long trials = end_idx - start_idx;
    int failed = 0;

    result_init(out);
    threads = calloc((size_t)cfg->threads, sizeof(*threads));
    args = calloc((size_t)cfg->threads, sizeof(*args));
    if (threads == 0 || args == 0) {
        free(threads);
        free(args);
        return -1;
    }

    for (int i = 0; i < cfg->threads; ++i) {
        long base = trials / cfg->threads;
        long rem = trials % cfg->threads;
        long extra_before = i < rem ? i : rem;
        args[i].cfg = cfg;
        args[i].start_idx = start_idx + (long)i * base + extra_before;
        args[i].end_idx = args[i].start_idx + base + (i < rem ? 1 : 0);
        if (pthread_create(&threads[i], 0, hybrid_thread_worker, &args[i]) != 0) {
            failed = 1;
            break;
        }
    }
    for (int i = 0; i < cfg->threads; ++i) {
        if (threads[i]) {
            pthread_join(threads[i], 0);
        }
    }
    if (!failed) {
        for (int i = 0; i < cfg->threads; ++i) {
            result_merge(out, &args[i].local);
        }
    }
    out->checksum = result_compute_checksum(out);
    free(threads);
    free(args);
    return failed ? -1 : 0;
}

int run_hybrid_mode(const Config *cfg, Result *out, StageMetrics *metrics)
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
            long s;
            long e;
            Result local;
            Config child_cfg = *cfg;
            if (cfg->ipc_mode == IPC_PIPE) {
                close(pipes[i][0]);
            }
            hybrid_partition(cfg->trials, cfg->processes, i, &s, &e);
            /* Simplified hybrid: each child owns a large simulation group and
             * runs a process-local pthread reduce over global trial indices.
             * The parent only merges IPC results. A later version can add
             * process-local queue scheduling. */
            run_child_thread_group(&child_cfg, s, e, &local);
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
    (void)start;
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
