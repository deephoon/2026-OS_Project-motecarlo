#!/usr/bin/env python3
import csv
import glob
import os
import re
import statistics


RAW_PATH = os.environ.get(
    "RAW_PATH",
    "results/csv/real_scaling/real_scaling_raw.csv",
)
SUMMARY_PATH = os.environ.get(
    "SUMMARY_PATH",
    "results/csv/real_scaling/real_scaling_summary.csv",
)
UTIL_DIR = os.environ.get(
    "UTIL_DIR",
    "results/csv/real_utilization",
)
UTIL_SUMMARY_PATH = os.environ.get(
    "UTIL_SUMMARY_PATH",
    "results/csv/real_utilization/real_utilization_summary.csv",
)


def mean(values):
    return statistics.fmean(values) if values else 0.0


def stdev(values):
    return statistics.stdev(values) if len(values) > 1 else 0.0


def parse_cpu_percent(path):
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            if "Percent of CPU this job got" in line:
                match = re.search(r"([0-9.]+)%", line)
                return float(match.group(1)) if match else 0.0
    return 0.0


def parse_mpstat(path, workers):
    per_core = {}
    if not os.path.exists(path):
        return []
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            fields = line.split()
            if len(fields) < 12 or not fields[1].isdigit():
                continue
            core = int(fields[1])
            if core >= workers:
                continue
            try:
                idle = float(fields[-1])
            except ValueError:
                continue
            per_core.setdefault(core, []).append(100.0 - idle)
    return [mean(per_core.get(core, [])) for core in range(workers)]


def analyze_scaling():
    with open(RAW_PATH, newline="", encoding="utf-8") as f:
        rows = list(csv.DictReader(f))
    grouped = {}
    for row in rows:
        grouped.setdefault(row["case_name"], []).append(row)
    seq_time = mean([float(row["time_total"]) for row in grouped["seq"]])
    summary = []
    for case_name, case_rows in sorted(grouped.items()):
        workers = int(case_rows[0]["workers"])
        times = [float(row["time_total"]) for row in case_rows]
        cpu = [float(row["cpu_percent"]) for row in case_rows]
        avg_time = mean(times)
        speedup = seq_time / avg_time if avg_time else 0.0
        summary.append({
            "case_name": case_name,
            "mode": case_rows[0]["mode"],
            "workers": workers,
            "repeats": len(case_rows),
            "avg_time": f"{avg_time:.6f}",
            "median_time": f"{statistics.median(times):.6f}",
            "min_time": f"{min(times):.6f}",
            "stdev_time": f"{stdev(times):.6f}",
            "ideal_time": f"{seq_time / workers:.6f}",
            "speedup": f"{speedup:.3f}",
            "efficiency": f"{speedup / workers:.3f}",
            "avg_cpu_percent": f"{mean(cpu):.1f}",
            "avg_util_per_worker": f"{mean(cpu) / workers:.1f}",
            "valid_all": "1" if all(row["valid"] == "1" for row in case_rows) else "0",
            "checksum_count": len({row["checksum"] for row in case_rows}),
        })
    os.makedirs(os.path.dirname(SUMMARY_PATH), exist_ok=True)
    with open(SUMMARY_PATH, "w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(
            f,
            fieldnames=list(summary[0].keys()),
            lineterminator="\n",
        )
        writer.writeheader()
        writer.writerows(summary)


def analyze_utilization():
    rows = []
    for time_path in sorted(glob.glob(os.path.join(UTIL_DIR, "time_*.txt"))):
        case_name = os.path.basename(time_path)[5:-4]
        sim_path = os.path.join(UTIL_DIR, f"sim_{case_name}.csv")
        mpstat_path = os.path.join(UTIL_DIR, f"mpstat_{case_name}.txt")
        with open(sim_path, newline="", encoding="utf-8") as f:
            sim_rows = list(csv.DictReader(f))
        sim = sim_rows[0]
        mode = sim["mode"]
        workers = (
            int(sim["processes"]) * int(sim["threads"])
            if mode == "hybrid"
            else int(sim["processes"])
            if mode == "process"
            else int(sim["threads"])
        )
        per_core = parse_mpstat(mpstat_path, workers)
        cpu = parse_cpu_percent(time_path)
        rows.append({
            "case_name": case_name,
            "workers": workers,
            "time_total": sim["time_total"],
            "cpu_percent": f"{cpu:.1f}",
            "util_per_worker": f"{cpu / workers:.1f}",
            "mpstat_core_avg": "|".join(f"{value:.1f}" for value in per_core),
            "mpstat_min_core": f"{min(per_core) if per_core else 0.0:.1f}",
            "mpstat_max_core": f"{max(per_core) if per_core else 0.0:.1f}",
            "mpstat_stdev": f"{stdev(per_core):.2f}",
            "valid": sim["valid"],
            "checksum": sim["checksum"],
        })
    os.makedirs(os.path.dirname(UTIL_SUMMARY_PATH), exist_ok=True)
    with open(UTIL_SUMMARY_PATH, "w", newline="", encoding="utf-8") as f:
        fieldnames = list(rows[0].keys()) if rows else []
        writer = csv.DictWriter(f, fieldnames=fieldnames, lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)


if __name__ == "__main__":
    analyze_scaling()
    analyze_utilization()
    print(f"wrote {SUMMARY_PATH}")
    print(f"wrote {UTIL_SUMMARY_PATH}")
