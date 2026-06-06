#!/usr/bin/env python3
import csv
import math
import statistics
import sys
from collections import defaultdict


def to_float(row, key):
    try:
        return float(row.get(key, "0") or 0)
    except ValueError:
        return 0.0


def to_int(row, key):
    try:
        return int(float(row.get(key, "0") or 0))
    except ValueError:
        return 0


def worker_count(row):
    mode = row.get("mode", "")
    if mode == "seq":
        return 1
    if mode == "process":
        return max(1, to_int(row, "processes"))
    if mode == "hybrid":
        return max(1, to_int(row, "processes") * to_int(row, "threads"))
    return max(1, to_int(row, "threads"))


def mean(values):
    return statistics.fmean(values) if values else 0.0


def stdev(values):
    return statistics.stdev(values) if len(values) > 1 else 0.0


def group_key(row):
    return (
        row.get("trials", ""),
        row.get("steps", ""),
        row.get("workload", ""),
        row.get("skew_factor", ""),
        row.get("pre_work", ""),
        row.get("post_work", ""),
        row.get("profile", "default"),
        row.get("inner_work", "0"),
    )


def main():
    if len(sys.argv) != 3:
        print("usage: analyze_results.py RAW_CSV ANALYZED_CSV", file=sys.stderr)
        return 2

    raw_path, analyzed_path = sys.argv[1:]
    with open(raw_path, newline="", encoding="utf-8") as f:
        rows = list(csv.DictReader(f))

    groups = defaultdict(list)
    baselines = {}
    baseline_checksums = {}
    for row in rows:
        key = (group_key(row), row.get("case_name", ""))
        groups[key].append(row)
        if row.get("case_name") == "seq":
            base_key = group_key(row)
            baselines.setdefault(base_key, []).append(to_float(row, "time_total"))
            baseline_checksums.setdefault(base_key, row.get("checksum", ""))

    baseline_avg = {key: mean(values) for key, values in baselines.items()}

    analyzed_rows = []
    for (base_key, case_name), case_rows in sorted(groups.items()):
        times = [to_float(r, "time_total") for r in case_rows]
        pre = [to_float(r, "time_pre") for r in case_rows]
        compute = [to_float(r, "time_compute") for r in case_rows]
        sync = [to_float(r, "time_sync") for r in case_rows]
        ipc = [to_float(r, "time_ipc") for r in case_rows]
        merge = [to_float(r, "time_merge") for r in case_rows]
        post = [to_float(r, "time_post") for r in case_rows]
        seq_frac = [to_float(r, "sequential_fraction_estimate") for r in case_rows]
        throughput = [to_float(r, "throughput_batches_per_sec") for r in case_rows]
        avg_time = mean(times)
        speedup = baseline_avg.get(base_key, 0.0) / avg_time if avg_time > 0 else 0.0
        workers = worker_count(case_rows[0])
        efficiency = speedup / workers if workers > 0 else 0.0
        checksums = {r.get("checksum", "") for r in case_rows}
        baseline_checksum = baseline_checksums.get(base_key, "")
        matches_baseline = len(checksums) == 1 and next(iter(checksums)) == baseline_checksum

        analyzed_rows.append({
            "case_name": case_name,
            "repeats": len(case_rows),
            "mode": case_rows[0].get("mode", ""),
            "sync": case_rows[0].get("sync", ""),
            "merge": case_rows[0].get("merge", ""),
            "processes": case_rows[0].get("processes", ""),
            "threads": case_rows[0].get("threads", ""),
            "ipc": case_rows[0].get("ipc", ""),
            "workers": workers,
            "trials": base_key[0],
            "steps": base_key[1],
            "workload": base_key[2],
            "skew_factor": base_key[3],
            "pre_work": base_key[4],
            "post_work": base_key[5],
            "profile": base_key[6],
            "inner_work": base_key[7],
            "avg_time_total": f"{avg_time:.6f}",
            "min_time_total": f"{min(times) if times else 0.0:.6f}",
            "stdev_time_total": f"{stdev(times):.6f}",
            "avg_time_pre": f"{mean(pre):.6f}",
            "avg_time_compute": f"{mean(compute):.6f}",
            "avg_time_sync": f"{mean(sync):.6f}",
            "avg_time_ipc": f"{mean(ipc):.6f}",
            "avg_time_merge": f"{mean(merge):.6f}",
            "avg_time_post": f"{mean(post):.6f}",
            "speedup_vs_seq_avg": f"{speedup:.3f}",
            "efficiency_vs_seq_avg": f"{efficiency:.3f}",
            "avg_sequential_fraction": f"{mean(seq_frac):.3f}",
            "avg_throughput_batches_per_sec": f"{mean(throughput):.1f}",
            "avg_lock_wait_count": f"{mean([to_float(r, 'lock_wait_count') for r in case_rows]):.1f}",
            "avg_cond_wait_count": f"{mean([to_float(r, 'cond_wait_count') for r in case_rows]):.1f}",
            "avg_queue_push_count": f"{mean([to_float(r, 'queue_push_count') for r in case_rows]):.1f}",
            "avg_queue_pop_count": f"{mean([to_float(r, 'queue_pop_count') for r in case_rows]):.1f}",
            "avg_ipc_read_count": f"{mean([to_float(r, 'ipc_read_count') for r in case_rows]):.1f}",
            "avg_ipc_bytes": f"{mean([to_float(r, 'ipc_bytes') for r in case_rows]):.1f}",
            "valid_all": "1" if all(r.get("valid") == "1" for r in case_rows) else "0",
            "checksum_count": len(checksums),
            "matches_seq_checksum": "1" if matches_baseline else "0",
        })

    fieldnames = list(analyzed_rows[0].keys()) if analyzed_rows else []
    with open(analyzed_path, "w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames, lineterminator="\n")
        writer.writeheader()
        writer.writerows(analyzed_rows)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
