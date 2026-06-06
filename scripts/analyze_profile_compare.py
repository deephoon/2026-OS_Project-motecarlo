#!/usr/bin/env python3
import csv
import os
import statistics


RAW_PATH = os.environ.get(
    "RAW_PATH",
    "results/csv/profile_compare/profile_compare_raw.csv",
)
SUMMARY_PATH = os.environ.get(
    "SUMMARY_PATH",
    "results/csv/profile_compare/profile_compare_summary.csv",
)


def mean(values):
    return statistics.fmean(values) if values else 0.0


def stdev(values):
    return statistics.stdev(values) if len(values) > 1 else 0.0


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


def group_rows(rows):
    grouped = {}
    for row in rows:
        grouped.setdefault(row.get("case_name", ""), []).append(row)
    return grouped


def analyze():
    if not os.path.exists(RAW_PATH):
        raise SystemExit(f"missing {RAW_PATH}")
    with open(RAW_PATH, newline="", encoding="utf-8") as f:
        rows = list(csv.DictReader(f))

    grouped = group_rows(rows)
    profile_baselines = {}
    profile_checksums = {}
    for name, case_rows in grouped.items():
        first = case_rows[0]
        if first.get("mode") == "seq":
            profile = first.get("profile", "default")
            profile_baselines[profile] = mean([to_float(r, "time_total") for r in case_rows])
            checksums = {r.get("checksum", "") for r in case_rows}
            profile_checksums[profile] = next(iter(checksums)) if len(checksums) == 1 else ""

    summary = []
    for name, case_rows in sorted(grouped.items()):
        first = case_rows[0]
        profile = first.get("profile", "default")
        times = [to_float(r, "time_total") for r in case_rows]
        avg_time = mean(times)
        base = profile_baselines.get(profile, 0.0)
        speedup = base / avg_time if avg_time > 0 else 0.0
        workers = worker_count(first)
        checksums = {r.get("checksum", "") for r in case_rows}
        expected_checksum = profile_checksums.get(profile, "")
        checksum_match = len(checksums) == 1 and next(iter(checksums)) == expected_checksum
        summary.append({
            "case_name": name,
            "profile": profile,
            "mode": first.get("mode", ""),
            "processes": first.get("processes", ""),
            "threads": first.get("threads", ""),
            "ipc": first.get("ipc", ""),
            "workers": workers,
            "repeats": len(case_rows),
            "avg_time_total": f"{avg_time:.6f}",
            "min_time_total": f"{min(times) if times else 0.0:.6f}",
            "stdev_time_total": f"{stdev(times):.6f}",
            "speedup_vs_profile_seq": f"{speedup:.3f}",
            "efficiency": f"{(speedup / workers) if workers > 0 else 0.0:.3f}",
            "valid_all": "1" if all(r.get("valid") == "1" for r in case_rows) else "0",
            "checksum_match": "1" if checksum_match else "0",
            "inner_work": first.get("inner_work", "0"),
        })

    os.makedirs(os.path.dirname(SUMMARY_PATH), exist_ok=True)
    with open(SUMMARY_PATH, "w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(
            f,
            fieldnames=list(summary[0].keys()) if summary else [],
            lineterminator="\n",
        )
        writer.writeheader()
        writer.writerows(summary)
    return summary


def main():
    summary = analyze()
    print(f"wrote {SUMMARY_PATH}")
    print(f"analyzed {len(summary)} cases")


if __name__ == "__main__":
    main()
