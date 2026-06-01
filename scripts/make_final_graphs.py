#!/usr/bin/env python3
import csv
import os
import sys


COLORS = {
    "blue": "#2563eb",
    "green": "#16a34a",
    "red": "#dc2626",
    "orange": "#ea580c",
    "purple": "#7c3aed",
    "gray": "#64748b",
    "light": "#e2e8f0",
    "dark": "#0f172a",
}


def load_rows(path):
    with open(path, newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f))


def row_by_case(rows):
    return {row["case_name"]: row for row in rows}


def f(row, key):
    try:
        return float(row.get(key, "0") or 0)
    except ValueError:
        return 0.0


def svg_header(width, height):
    return [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">',
        '<rect width="100%" height="100%" fill="white"/>',
        '<style>text{font-family:Arial,Helvetica,sans-serif;fill:#0f172a}.axis{stroke:#334155;stroke-width:1}.grid{stroke:#e2e8f0;stroke-width:1}.label{font-size:12px}.title{font-size:20px;font-weight:700}.subtitle{font-size:12px;fill:#475569}.tick{font-size:11px;fill:#475569}</style>',
    ]


def svg_footer():
    return ["</svg>"]


def write_svg(path, lines):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))


def bar_chart(path, title, subtitle, labels, series, y_label):
    width, height = 980, 560
    margin = {"left": 90, "right": 40, "top": 75, "bottom": 120}
    plot_w = width - margin["left"] - margin["right"]
    plot_h = height - margin["top"] - margin["bottom"]
    max_y = max([max(values) for _, values, _ in series] + [1.0]) * 1.15
    group_w = plot_w / len(labels)
    bar_gap = 8
    bar_w = max(10, (group_w - 24 - bar_gap * (len(series) - 1)) / len(series))
    lines = svg_header(width, height)
    lines.append(f'<text class="title" x="{margin["left"]}" y="34">{title}</text>')
    lines.append(f'<text class="subtitle" x="{margin["left"]}" y="55">{subtitle}</text>')
    for i in range(6):
        yv = max_y * i / 5
        y = margin["top"] + plot_h - (yv / max_y) * plot_h
        lines.append(f'<line class="grid" x1="{margin["left"]}" y1="{y:.1f}" x2="{width - margin["right"]}" y2="{y:.1f}"/>')
        lines.append(f'<text class="tick" x="{margin["left"] - 8}" y="{y + 4:.1f}" text-anchor="end">{yv:.2f}</text>')
    lines.append(f'<line class="axis" x1="{margin["left"]}" y1="{margin["top"]}" x2="{margin["left"]}" y2="{margin["top"] + plot_h}"/>')
    lines.append(f'<line class="axis" x1="{margin["left"]}" y1="{margin["top"] + plot_h}" x2="{width - margin["right"]}" y2="{margin["top"] + plot_h}"/>')
    lines.append(f'<text class="label" x="20" y="{margin["top"] + plot_h / 2:.1f}" transform="rotate(-90 20,{margin["top"] + plot_h / 2:.1f})">{y_label}</text>')
    for i, label in enumerate(labels):
        cx = margin["left"] + group_w * i + group_w / 2
        lines.append(f'<text class="tick" x="{cx:.1f}" y="{height - 72}" text-anchor="middle">{label}</text>')
        for j, (_, values, color) in enumerate(series):
            x = margin["left"] + group_w * i + 12 + j * (bar_w + bar_gap)
            h = (values[i] / max_y) * plot_h if max_y > 0 else 0
            y = margin["top"] + plot_h - h
            lines.append(f'<rect x="{x:.1f}" y="{y:.1f}" width="{bar_w:.1f}" height="{h:.1f}" rx="2" fill="{color}"/>')
            lines.append(f'<text class="tick" x="{x + bar_w / 2:.1f}" y="{y - 5:.1f}" text-anchor="middle">{values[i]:.2f}</text>')
    legend_x = margin["left"]
    legend_y = height - 36
    for name, _, color in series:
        lines.append(f'<rect x="{legend_x}" y="{legend_y - 11}" width="12" height="12" fill="{color}"/>')
        lines.append(f'<text class="label" x="{legend_x + 18}" y="{legend_y}">{name}</text>')
        legend_x += 160
    lines += svg_footer()
    write_svg(path, lines)


def stacked_stage_chart(path, title, subtitle, labels, rows):
    width, height = 1080, 600
    margin = {"left": 90, "right": 50, "top": 80, "bottom": 125}
    plot_w = width - margin["left"] - margin["right"]
    plot_h = height - margin["top"] - margin["bottom"]
    stages = [
        ("pre", "avg_time_pre", COLORS["purple"]),
        ("compute", "avg_time_compute", COLORS["blue"]),
        ("sync", "avg_time_sync", COLORS["orange"]),
        ("ipc", "avg_time_ipc", COLORS["gray"]),
        ("merge", "avg_time_merge", COLORS["green"]),
        ("post", "avg_time_post", COLORS["red"]),
    ]
    totals = [sum(f(row, key) for _, key, _ in stages) for row in rows]
    max_y = max(totals + [1.0]) * 1.12
    group_w = plot_w / len(labels)
    bar_w = min(58, group_w * 0.55)
    lines = svg_header(width, height)
    lines.append(f'<text class="title" x="{margin["left"]}" y="34">{title}</text>')
    lines.append(f'<text class="subtitle" x="{margin["left"]}" y="55">{subtitle}</text>')
    for i in range(6):
        yv = max_y * i / 5
        y = margin["top"] + plot_h - (yv / max_y) * plot_h
        lines.append(f'<line class="grid" x1="{margin["left"]}" y1="{y:.1f}" x2="{width - margin["right"]}" y2="{y:.1f}"/>')
        lines.append(f'<text class="tick" x="{margin["left"] - 8}" y="{y + 4:.1f}" text-anchor="end">{yv:.3f}</text>')
    lines.append(f'<line class="axis" x1="{margin["left"]}" y1="{margin["top"] + plot_h}" x2="{width - margin["right"]}" y2="{margin["top"] + plot_h}"/>')
    for i, row in enumerate(rows):
        x = margin["left"] + group_w * i + (group_w - bar_w) / 2
        y_cursor = margin["top"] + plot_h
        for stage_name, key, color in stages:
            value = f(row, key)
            h = (value / max_y) * plot_h if max_y > 0 else 0
            y_cursor -= h
            lines.append(f'<rect x="{x:.1f}" y="{y_cursor:.1f}" width="{bar_w:.1f}" height="{h:.1f}" fill="{color}"/>')
        lines.append(f'<text class="tick" x="{x + bar_w / 2:.1f}" y="{height - 76}" text-anchor="middle">{labels[i]}</text>')
    legend_x = margin["left"]
    legend_y = height - 38
    for stage_name, _, color in stages:
        lines.append(f'<rect x="{legend_x}" y="{legend_y - 11}" width="12" height="12" fill="{color}"/>')
        lines.append(f'<text class="label" x="{legend_x + 18}" y="{legend_y}">{stage_name}</text>')
        legend_x += 130
    lines += svg_footer()
    write_svg(path, lines)


def main():
    if len(sys.argv) not in (3, 4):
        print("usage: make_final_graphs.py FINAL_ANALYZED_CSV OUT_DIR [AMDahl_ANALYZED_CSV]", file=sys.stderr)
        return 2
    final_csv = sys.argv[1]
    out_dir = sys.argv[2]
    stress_csv = sys.argv[3] if len(sys.argv) == 4 else None
    rows = row_by_case(load_rows(final_csv))

    thread_cases = ["thread_1_reduce", "thread_2_reduce", "thread_4_reduce", "thread_8_reduce"]
    thread_labels = ["1 thread", "2 threads", "4 threads", "8 threads"]
    bar_chart(
        os.path.join(out_dir, "thread_speedup_efficiency.svg"),
        "Thread Scaling: Speedup and Efficiency",
        "Docker Ubuntu Linux, trials=1,000,000, repeats=5",
        thread_labels,
        [
            ("speedup", [f(rows[c], "speedup_vs_seq_avg") for c in thread_cases], COLORS["blue"]),
            ("efficiency", [f(rows[c], "efficiency_vs_seq_avg") for c in thread_cases], COLORS["green"]),
        ],
        "ratio",
    )

    sync_cases = ["thread_4_nosync", "thread_4_mutex", "thread_4_reduce"]
    bar_chart(
        os.path.join(out_dir, "sync_compare.svg"),
        "Synchronization Strategy Comparison",
        "nosync is fast-looking but invalid; reduce balances correctness and performance",
        ["nosync", "mutex", "reduce"],
        [
            ("avg time sec", [f(rows[c], "avg_time_total") for c in sync_cases], COLORS["orange"]),
            ("speedup", [f(rows[c], "speedup_vs_seq_avg") for c in sync_cases], COLORS["blue"]),
        ],
        "seconds / ratio",
    )

    ph_cases = ["process_1_pipe", "process_2_pipe", "process_4_pipe", "hybrid_2x2", "hybrid_2x4", "hybrid_4x2"]
    bar_chart(
        os.path.join(out_dir, "process_hybrid_compare.svg"),
        "Process and Hybrid Comparison",
        "fork/pipe overhead vs process+thread parallelism",
        ["p1", "p2", "p4", "h2x2", "h2x4", "h4x2"],
        [
            ("avg time sec", [f(rows[c], "avg_time_total") for c in ph_cases], COLORS["orange"]),
            ("speedup", [f(rows[c], "speedup_vs_seq_avg") for c in ph_cases], COLORS["blue"]),
        ],
        "seconds / ratio",
    )

    pipe_cases = ["pipeline_final_b1000", "pipeline_interactive_b100", "pipeline_interactive_b1000", "pipeline_interactive_b10000"]
    bar_chart(
        os.path.join(out_dir, "pipeline_merge_compare.svg"),
        "Pipeline Merge Strategy Comparison",
        "interactive merge enables mid-run aggregation but adds queue/aggregator overhead",
        ["final b1000", "int b100", "int b1000", "int b10000"],
        [
            ("avg time sec", [f(rows[c], "avg_time_total") for c in pipe_cases], COLORS["orange"]),
            ("speedup", [f(rows[c], "speedup_vs_seq_avg") for c in pipe_cases], COLORS["blue"]),
        ],
        "seconds / ratio",
    )

    stage_cases = ["seq", "thread_4_reduce", "thread_8_reduce", "pipeline_final_b1000", "pipeline_interactive_b1000"]
    stacked_stage_chart(
        os.path.join(out_dir, "stage_time_stacked.svg"),
        "Stage Time Breakdown",
        "pre / compute / sync / merge / post",
        ["seq", "t4", "t8", "pipe final", "pipe int"],
        [rows[c] for c in stage_cases],
    )

    if stress_csv:
        stress = row_by_case(load_rows(stress_csv))
        bar_chart(
            os.path.join(out_dir, "amdahl_stress_speedup.svg"),
            "Amdahl Stress: Speedup Is Capped By Sequential Work",
            "PRE_WORK=50,000,000, POST_WORK=10,000,000",
            thread_labels,
            [
                ("base speedup", [f(rows[c], "speedup_vs_seq_avg") for c in thread_cases], COLORS["blue"]),
                ("stress speedup", [f(stress[c], "speedup_vs_seq_avg") for c in thread_cases], COLORS["red"]),
            ],
            "speedup",
        )
        stacked_stage_chart(
            os.path.join(out_dir, "amdahl_stress_stage_time.svg"),
            "Amdahl Stress Stage Time",
            "larger pre/post sequential stages visibly limit speedup",
            ["seq", "t4", "t8", "pipe final", "pipe int"],
            [stress[c] for c in stage_cases],
        )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
