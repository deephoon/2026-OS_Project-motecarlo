#!/usr/bin/env python3
import csv
import glob
import os
import re
import statistics


RAW_PATH = os.environ.get("RAW_PATH", "results/csv/ideal/ideal_strong_raw.csv")
STRONG_SUMMARY = os.environ.get(
    "STRONG_SUMMARY",
    "results/csv/ideal/ideal_strong_summary.csv",
)
UTIL_SUMMARY = os.environ.get(
    "UTIL_SUMMARY",
    "results/csv/ideal/ideal_utilization_summary.csv",
)
CORE_UTIL_DIR = os.environ.get("CORE_UTIL_DIR", "results/csv/core_util")


def mean(values):
    return statistics.fmean(values) if values else 0.0


def stdev(values):
    return statistics.stdev(values) if len(values) > 1 else 0.0


def parse_float(value):
    try:
        return float(value)
    except (TypeError, ValueError):
        return 0.0


def parse_cpu_percent_from_time(path):
    if not os.path.exists(path):
        return 0.0
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            if "Percent of CPU this job got" in line:
                match = re.search(r"([0-9.]+)%", line)
                if match:
                    return float(match.group(1))
    return 0.0


def analyze_strong():
    os.makedirs(os.path.dirname(STRONG_SUMMARY), exist_ok=True)
    if not os.path.exists(RAW_PATH):
        with open(STRONG_SUMMARY, "w", newline="", encoding="utf-8") as f:
            writer = csv.writer(f, lineterminator="\n")
            writer.writerow([
                "threads", "avg_time", "min_time", "stdev_time", "ideal_time",
                "speedup", "efficiency", "avg_cpu_percent", "avg_util_per_core",
            ])
        return []

    with open(RAW_PATH, newline="", encoding="utf-8") as f:
        rows = list(csv.DictReader(f))

    grouped = {}
    for row in rows:
        threads = int(row["threads"])
        grouped.setdefault(threads, []).append(row)

    t1 = mean([parse_float(r["time_total"]) for r in grouped.get(1, [])])
    summary = []
    for threads in sorted(grouped):
        case_rows = grouped[threads]
        times = [parse_float(r["time_total"]) for r in case_rows]
        cpus = [parse_float(r["cpu_percent"]) for r in case_rows]
        avg_time = mean(times)
        speedup = t1 / avg_time if avg_time > 0 else 0.0
        efficiency = speedup / threads if threads > 0 else 0.0
        avg_cpu = mean(cpus)
        summary.append({
            "threads": threads,
            "avg_time": f"{avg_time:.9f}",
            "min_time": f"{min(times) if times else 0.0:.9f}",
            "stdev_time": f"{stdev(times):.9f}",
            "ideal_time": f"{(t1 / threads) if threads > 0 else 0.0:.9f}",
            "speedup": f"{speedup:.3f}",
            "efficiency": f"{efficiency:.3f}",
            "avg_cpu_percent": f"{avg_cpu:.1f}",
            "avg_util_per_core": f"{(avg_cpu / threads) if threads > 0 else 0.0:.1f}",
        })

    with open(STRONG_SUMMARY, "w", newline="", encoding="utf-8") as f:
        fieldnames = list(summary[0].keys()) if summary else [
            "threads", "avg_time", "min_time", "stdev_time", "ideal_time",
            "speedup", "efficiency", "avg_cpu_percent", "avg_util_per_core",
        ]
        writer = csv.DictWriter(f, fieldnames=fieldnames, lineterminator="\n")
        writer.writeheader()
        writer.writerows(summary)
    return summary


def judgement(util):
    if util >= 90.0:
        return "near ideal"
    if util >= 70.0:
        return "partial saturation"
    return "under-utilized"


def analyze_utilization():
    os.makedirs(os.path.dirname(UTIL_SUMMARY), exist_ok=True)
    grouped = {}
    patterns = [
        os.path.join(CORE_UTIL_DIR, "time_threads*.txt"),
        os.path.join(CORE_UTIL_DIR, "time_ideal_*.txt"),
    ]
    paths = []
    for pattern in patterns:
        paths.extend(glob.glob(pattern))
    for path in sorted(set(paths)):
        match = re.search(r"time_threads([0-9]+)\.txt$", path)
        if not match:
            match = re.search(r"time_ideal_([0-9]+)\.txt$", path)
        if not match:
            continue
        threads = int(match.group(1))
        cpu_percent = parse_cpu_percent_from_time(path)
        grouped.setdefault(threads, []).append(cpu_percent)

    rows = []
    for threads in sorted(grouped):
        cpu_percent = mean(grouped[threads])
        util = cpu_percent / threads if threads > 0 else 0.0
        rows.append({
            "threads": threads,
            "avg_cpu_percent": f"{cpu_percent:.1f}",
            "avg_util_per_core": f"{util:.1f}",
            "judgement": judgement(util),
        })

    with open(UTIL_SUMMARY, "w", newline="", encoding="utf-8") as f:
        fieldnames = ["threads", "avg_cpu_percent", "avg_util_per_core", "judgement"]
        writer = csv.DictWriter(f, fieldnames=fieldnames, lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)
    return rows


def main():
    analyze_strong()
    analyze_utilization()
    print(f"wrote {STRONG_SUMMARY}")
    print(f"wrote {UTIL_SUMMARY}")


if __name__ == "__main__":
    main()
