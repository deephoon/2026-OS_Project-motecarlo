#include "pipeline_mode.h"

#include "merge_queue.h"
#include "postprocess.h"
#include "preprocess.h"
#include "simulation.h"
#include "task_queue.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    TaskQueue *task_queue;
    const TaskBatch *batches;
    int batch_count;
    StageMetrics *metrics;
} PreprocessorArg;

typedef struct {
    const Config *cfg;
    TaskQueue *task_queue;
    MergeQueue *merge_queue;
    Result *final_partials;
    int *final_count;
    pthread_mutex_t *final_mutex;
    StageMetrics *metrics;
    pthread_mutex_t *metrics_mutex;
} PipelineWorkerArg;

typedef struct {
    MergeQueue *merge_queue;
    Result *global_result;
    StageMetrics *metrics;
} AggregatorArg;

static void add_metric_time(StageMetrics *metrics, pthread_mutex_t *mutex,
                            double *field, double value)
{
    pthread_mutex_lock(mutex);
    *field += value;
    metrics->processed_batches += 1;
    pthread_mutex_unlock(mutex);
}

static void *preprocessor_thread(void *arg_ptr)
{
    PreprocessorArg *arg = (PreprocessorArg *)arg_ptr;
    for (int i = 0; i < arg->batch_count; ++i) {
        task_queue_push(arg->task_queue, arg->batches[i]);
    }
    task_queue_close(arg->task_queue);
    return 0;
}

static void *pipeline_worker(void *arg_ptr)
{
    PipelineWorkerArg *arg = (PipelineWorkerArg *)arg_ptr;
    TaskBatch batch;

    while (1) {
        Result local;
        PartialResult partial;
        double start;
        double end;

        start = now_sec();
        if (!task_queue_pop(arg->task_queue, &batch)) {
            end = now_sec();
            add_metric_time(arg->metrics, arg->metrics_mutex,
                            &arg->metrics->t_sync, elapsed_sec(start, end));
            break;
        }
        end = now_sec();
        add_metric_time(arg->metrics, arg->metrics_mutex,
                        &arg->metrics->t_sync, elapsed_sec(start, end));

        start = now_sec();
        run_batch(arg->cfg, &batch, &local);
        end = now_sec();
        (void)end;

        if (arg->cfg->merge_mode == MERGE_INTERACTIVE) {
            partial.batch_id = batch.batch_id;
            partial.result = local;
            start = now_sec();
            merge_queue_push(arg->merge_queue, partial);
            end = now_sec();
            pthread_mutex_lock(arg->metrics_mutex);
            arg->metrics->t_sync += elapsed_sec(start, end);
            pthread_mutex_unlock(arg->metrics_mutex);
        } else {
            pthread_mutex_lock(arg->final_mutex);
            arg->final_partials[*arg->final_count] = local;
            *arg->final_count += 1;
            pthread_mutex_unlock(arg->final_mutex);
        }
    }
    return 0;
}

static void *aggregator_thread(void *arg_ptr)
{
    AggregatorArg *arg = (AggregatorArg *)arg_ptr;
    PartialResult partial;

    while (merge_queue_pop(arg->merge_queue, &partial)) {
        double start = now_sec();
        /* Aggregator is the only writer to global_result in interactive mode,
         * so no global result mutex is needed for this merge path. */
        result_merge(arg->global_result, &partial.result);
        arg->metrics->t_merge += elapsed_sec(start, now_sec());
    }
    return 0;
}

int run_pipeline_mode(const Config *cfg, Result *out, StageMetrics *metrics)
{
    TaskBatch *batches = 0;
    int batch_count = 0;
    TaskQueue task_queue = {0};
    MergeQueue merge_queue = {0};
    pthread_t preprocessor;
    pthread_t aggregator;
    pthread_t *workers = 0;
    PipelineWorkerArg *worker_args = 0;
    Result *final_partials = 0;
    int final_count = 0;
    pthread_mutex_t final_mutex;
    pthread_mutex_t metrics_mutex;
    int task_queue_ready = 0;
    int merge_queue_ready = 0;
    int final_mutex_ready = 0;
    int metrics_mutex_ready = 0;
    int failed = 0;
    double start;
    double end;
    PostSummary summary;
    double compute_wall_start;
    double compute_wall_end;

    if (cfg == 0 || out == 0 || metrics == 0) {
        return -1;
    }

    metrics_init(metrics);
    metrics->t_total_start = now_sec();
    result_init(out);

    start = now_sec();
    batches = create_batches(cfg, &batch_count);
    end = now_sec();
    metrics->t_pre = elapsed_sec(start, end);
    if (batches == 0 || batch_count <= 0) {
        return -1;
    }

    workers = (pthread_t *)calloc((size_t)cfg->threads, sizeof(*workers));
    worker_args = (PipelineWorkerArg *)calloc((size_t)cfg->threads, sizeof(*worker_args));
    final_partials = (Result *)calloc((size_t)batch_count, sizeof(*final_partials));
    if (workers == 0 || worker_args == 0 || final_partials == 0) {
        failed = 1;
        goto cleanup;
    }
    if (task_queue_init(&task_queue, cfg->queue_size) != 0) {
        failed = 1;
        goto cleanup;
    }
    task_queue_ready = 1;
    if (merge_queue_init(&merge_queue, cfg->queue_size) != 0) {
        failed = 1;
        goto cleanup;
    }
    merge_queue_ready = 1;
    if (pthread_mutex_init(&final_mutex, 0) != 0) {
        failed = 1;
        goto cleanup;
    }
    final_mutex_ready = 1;
    if (pthread_mutex_init(&metrics_mutex, 0) != 0) {
        failed = 1;
        goto cleanup;
    }
    metrics_mutex_ready = 1;

    compute_wall_start = now_sec();
    if (cfg->merge_mode == MERGE_INTERACTIVE) {
        AggregatorArg aggregator_arg;
        aggregator_arg.merge_queue = &merge_queue;
        aggregator_arg.global_result = out;
        aggregator_arg.metrics = metrics;
        if (pthread_create(&aggregator, 0, aggregator_thread, &aggregator_arg) != 0) {
            failed = 1;
            goto cleanup;
        }

        PreprocessorArg prep_arg;
        prep_arg.task_queue = &task_queue;
        prep_arg.batches = batches;
        prep_arg.batch_count = batch_count;
        prep_arg.metrics = metrics;
        if (pthread_create(&preprocessor, 0, preprocessor_thread, &prep_arg) != 0) {
            failed = 1;
            merge_queue_close(&merge_queue);
            pthread_join(aggregator, 0);
            goto cleanup;
        }

        for (int i = 0; i < cfg->threads; ++i) {
            worker_args[i].cfg = cfg;
            worker_args[i].task_queue = &task_queue;
            worker_args[i].merge_queue = &merge_queue;
            worker_args[i].final_partials = final_partials;
            worker_args[i].final_count = &final_count;
            worker_args[i].final_mutex = &final_mutex;
            worker_args[i].metrics = metrics;
            worker_args[i].metrics_mutex = &metrics_mutex;
            if (pthread_create(&workers[i], 0, pipeline_worker, &worker_args[i]) != 0) {
                failed = 1;
                task_queue_close(&task_queue);
                break;
            }
        }
        pthread_join(preprocessor, 0);
        for (int i = 0; i < cfg->threads; ++i) {
            if (workers[i]) {
                pthread_join(workers[i], 0);
            }
        }
        merge_queue_close(&merge_queue);
        pthread_join(aggregator, 0);
        compute_wall_end = now_sec();
        metrics->t_compute = elapsed_sec(compute_wall_start, compute_wall_end);
    } else {
        PreprocessorArg prep_arg;
        prep_arg.task_queue = &task_queue;
        prep_arg.batches = batches;
        prep_arg.batch_count = batch_count;
        prep_arg.metrics = metrics;
        pthread_create(&preprocessor, 0, preprocessor_thread, &prep_arg);
        for (int i = 0; i < cfg->threads; ++i) {
            worker_args[i].cfg = cfg;
            worker_args[i].task_queue = &task_queue;
            worker_args[i].merge_queue = &merge_queue;
            worker_args[i].final_partials = final_partials;
            worker_args[i].final_count = &final_count;
            worker_args[i].final_mutex = &final_mutex;
            worker_args[i].metrics = metrics;
            worker_args[i].metrics_mutex = &metrics_mutex;
            if (pthread_create(&workers[i], 0, pipeline_worker, &worker_args[i]) != 0) {
                failed = 1;
                task_queue_close(&task_queue);
                break;
            }
        }
        pthread_join(preprocessor, 0);
        for (int i = 0; i < cfg->threads; ++i) {
            if (workers[i]) {
                pthread_join(workers[i], 0);
            }
        }
        compute_wall_end = now_sec();
        metrics->t_compute = elapsed_sec(compute_wall_start, compute_wall_end);
        start = now_sec();
        for (int i = 0; i < final_count; ++i) {
            result_merge(out, &final_partials[i]);
        }
        end = now_sec();
        metrics->t_merge += elapsed_sec(start, end);
    }

    start = now_sec();
    postprocess_finalize(out, cfg->trials, &summary);
    postprocess_run_extra_work(cfg, out);
    end = now_sec();
    metrics->t_post = elapsed_sec(start, end);
    metrics->processed_batches = batch_count;
    metrics->t_total_end = now_sec();

cleanup:
    if (task_queue_ready) task_queue_destroy(&task_queue);
    if (merge_queue_ready) merge_queue_destroy(&merge_queue);
    if (final_mutex_ready) pthread_mutex_destroy(&final_mutex);
    if (metrics_mutex_ready) pthread_mutex_destroy(&metrics_mutex);
    free(workers);
    free(worker_args);
    free(final_partials);
    free_batches(batches);
    return failed ? -1 : 0;
}
