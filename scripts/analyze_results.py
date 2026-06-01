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
    )


def main():
    if len(sys.argv) != 4:
        print("usage: analyze_results.py RAW_CSV ANALYZED_CSV SUMMARY_MD", file=sys.stderr)
        return 2

    raw_path, analyzed_path, summary_path = sys.argv[1:]
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
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(analyzed_rows)

    fastest = sorted(analyzed_rows, key=lambda r: float(r["avg_time_total"]))[:5]
    risky = [
        r for r in analyzed_rows
        if r["valid_all"] != "1" or r["matches_seq_checksum"] != "1"
    ]
    seq_rows = [r for r in analyzed_rows if r["case_name"] == "seq"]
    seq_time = seq_rows[0]["avg_time_total"] if seq_rows else "n/a"

    with open(summary_path, "w", encoding="utf-8") as f:
        f.write("# Final Experiment Summary\n\n")
        f.write("## Conditions\n\n")
        if analyzed_rows:
            first = analyzed_rows[0]
            f.write(f"- trials: {first['trials']}\n")
            f.write(f"- steps: {first['steps']}\n")
            f.write(f"- repeats: {first['repeats']}\n")
            f.write(f"- workload: {first['workload']}\n")
            f.write(f"- skew_factor: {first['skew_factor']}\n")
            f.write(f"- pre_work: {first['pre_work']}\n")
            f.write(f"- post_work: {first['post_work']}\n")
            f.write(f"- sequential baseline avg: {seq_time}s\n\n")
        f.write("## OS26 Guide Coverage\n\n")
        f.write("| Requirement | Evidence in this run |\n")
        f.write("| --- | --- |\n")
        f.write("| sequential baseline | `seq` case |\n")
        f.write("| single/multi thread | `thread_1_reduce`, `thread_2_reduce`, `thread_4_reduce`, `thread_8_reduce` |\n")
        f.write("| child process | `process_1_pipe`, `process_2_pipe`, `process_4_pipe`, and shm variants |\n")
        f.write("| hybrid process + thread | `hybrid_2x2`, `hybrid_2x4`, `hybrid_4x2`, `hybrid_2x4_shm` |\n")
        f.write("| synchronization comparison | `thread_4_nosync`, `thread_4_mutex`, `thread_4_reduce` |\n")
        f.write("| pipeline and merge strategy | `pipeline_final_b1000`, `pipeline_interactive_b1000` |\n")
        f.write("| batch granularity | `pipeline_interactive_b100`, `pipeline_interactive_b1000`, `pipeline_interactive_b10000` |\n\n")
        f.write("## Fastest Cases By Average Time\n\n")
        f.write("| Case | Avg time | Speedup | Efficiency | Valid | Checksum matches seq |\n")
        f.write("| --- | ---: | ---: | ---: | ---: | ---: |\n")
        for r in fastest:
            f.write(
                f"| {r['case_name']} | {r['avg_time_total']} | {r['speedup_vs_seq_avg']} | "
                f"{r['efficiency_vs_seq_avg']} | {r['valid_all']} | {r['matches_seq_checksum']} |\n"
            )
        f.write("\n## Validation Risks\n\n")
        if risky:
            f.write("| Case | Valid all | Checksum count | Matches seq checksum |\n")
            f.write("| --- | ---: | ---: | ---: |\n")
            for r in risky:
                f.write(
                    f"| {r['case_name']} | {r['valid_all']} | {r['checksum_count']} | {r['matches_seq_checksum']} |\n"
                )
        else:
            f.write("- All measured cases were valid and matched the sequential checksum.\n")
        f.write("\n## Report Guidance\n\n")
        f.write("- Treat `thread_4_nosync` as a synchronization failure demonstration if checksum differs.\n")
        f.write("- Use `speedup_vs_seq_avg` and `efficiency_vs_seq_avg` from `final_analyzed.csv`, not the raw per-run zero placeholders.\n")
        f.write("- Explain `time_sync` as observed synchronization/IPC wait cost. For queue modes it can include accumulated worker wait time.\n")
        f.write("- Use large `TRIALS`, large `STEPS`, and at least 5 repeats for final report claims.\n")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
