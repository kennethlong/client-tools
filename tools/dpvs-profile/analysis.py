"""Phase 10 DPVS profiling - CSV aggregator + verdict emitter. Locked by .planning/phases/10-dpvs-culling-experiment/10-CONTEXT.md D-04, D-09, D-10, D-11.

Reads per-frame CSV files produced by the in-engine DpvsProfileInstrumentation
(Wave 1, plan 02) and emits:

1. A markdown-formatted summary table covering median/p95/p99/max/stdev for the
   metrics gpu_us, cpu_qpc_us, profiler_dpvs_us, total_frame_ms grouped by
   condition (on / off) -- ready to paste into the verdict doc.
2. A final stdout line of the form `verdict = remove` or `verdict = keep`,
   applying the D-10 threshold rule and the D-11 default-to-keep tiebreaker.

The script is stdlib-only (csv, statistics, glob, argparse, sys, os) and
requires no project-specific configuration beyond an optional --csv-dir.

Decision-ID trail:
- D-04: Statistics = median, p95, p99, max, stdev per condition x metric.
- D-09: Primary metric = total_frame_ms (median + p95).
- D-10: Threshold rule -- off median+p95 BOTH <= on -> "remove"; any
        regression on either -> "keep".
- D-11: Inconclusive / mixed / missing-data -> default "keep".
"""

from __future__ import annotations

import argparse
import csv
import glob
import os
import statistics
import sys
from typing import Dict, List, Optional, Tuple

# CSV header contract -- must match the DpvsProfileInstrumentation writer
# (planned in Wave 1, plan 02). Defined in 10-01-PLAN.md <interfaces>.
EXPECTED_HEADER = [
    "frame_no",
    "wall_ms_iso",
    "run_label",
    "dpvs_occlusion_flag",
    "gpu_us",
    "cpu_qpc_us",
    "profiler_dpvs_us",
    "total_frame_ms",
    "visible_object_count",
    "draw_call_count",
]

# Metrics aggregated for the summary table. Order matters: rows are printed in
# this order, on-row first then off-row, per metric. D-04.
METRICS = ["gpu_us", "cpu_qpc_us", "profiler_dpvs_us", "total_frame_ms"]

# Primary verdict metric per D-09.
PRIMARY_METRIC = "total_frame_ms"


def _parse_args(argv: Optional[List[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Phase 10 DPVS profiling CSV aggregator + verdict emitter "
            "(per CONTEXT.md D-04, D-09, D-10, D-11)."
        ),
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    parser.add_argument(
        "--csv-dir",
        default="D:/Code/swg-client-v2/stage/dpvs-profile/",
        help="Directory containing per-pass CSV files (*.csv).",
    )
    parser.add_argument(
        "--scene",
        default="mosEisley",
        help=(
            "Scene tag used by the capture protocol (informational; "
            "verdict is still computed on whatever run_label values are seen)."
        ),
    )
    return parser.parse_args(argv)


def _read_csv_rows(path: str) -> List[Dict[str, str]]:
    """Read a single CSV file. Warn and skip if the header is wrong."""
    try:
        with open(path, "r", encoding="utf-8", newline="") as fh:
            reader = csv.reader(fh)
            try:
                header = next(reader)
            except StopIteration:
                sys.stderr.write(f"WARNING: empty CSV (no header): {path}\n")
                return []

            if header != EXPECTED_HEADER:
                sys.stderr.write(
                    f"WARNING: header mismatch -- skipping {path}\n"
                    f"  expected: {EXPECTED_HEADER}\n"
                    f"  got     : {header}\n"
                )
                return []

            rows: List[Dict[str, str]] = []
            for raw in reader:
                if len(raw) != len(EXPECTED_HEADER):
                    # Tolerate ragged trailing rows but skip them.
                    continue
                rows.append(dict(zip(EXPECTED_HEADER, raw)))
            return rows
    except OSError as exc:
        sys.stderr.write(f"WARNING: cannot read {path}: {exc}\n")
        return []


def _load_all_rows(csv_dir: str) -> List[Dict[str, str]]:
    pattern = os.path.join(csv_dir, "*.csv")
    files = sorted(glob.glob(pattern))
    if not files:
        sys.stderr.write(
            f"WARNING: no CSV files found under {csv_dir} "
            f"(pattern: {pattern})\n"
        )
    all_rows: List[Dict[str, str]] = []
    for path in files:
        all_rows.extend(_read_csv_rows(path))
    return all_rows


def _condition_for_label(label: str) -> Optional[str]:
    """Map a run_label to its on/off pool. Suffix match, case-insensitive.

    Per the run-label naming convention in test-protocol.md:
        mosEisley-pass<1|2|3>-<on|off>
    """
    if not label:
        return None
    lower = label.strip().lower()
    if lower.endswith("-on"):
        return "on"
    if lower.endswith("-off"):
        return "off"
    return None


def _safe_float(text: str) -> Optional[float]:
    """Parse a cell. Blank means S_FALSE from GetData per RESEARCH.md Pitfall 1."""
    if text is None:
        return None
    s = text.strip()
    if s == "":
        return None
    try:
        return float(s)
    except ValueError:
        return None


def _percentile(values: List[float], pct: float) -> Optional[float]:
    """Compute percentile via statistics.quantiles(n=100). pct in [0, 100].

    Returns None if values is empty. For pct==100, returns max(). For pct==50,
    returns median (more stable for tiny n than quantiles).
    """
    if not values:
        return None
    if pct >= 100.0:
        return max(values)
    if pct == 50.0:
        return statistics.median(values)
    if len(values) < 2:
        return values[0]
    # statistics.quantiles with n=100 returns 99 cut points; index 94 -> p95,
    # index 98 -> p99.
    try:
        cuts = statistics.quantiles(values, n=100)
    except statistics.StatisticsError:
        return values[0]
    idx = int(round(pct)) - 1
    idx = max(0, min(idx, len(cuts) - 1))
    return cuts[idx]


def _stdev(values: List[float]) -> Optional[float]:
    if len(values) < 2:
        return None
    try:
        return statistics.stdev(values)
    except statistics.StatisticsError:
        return None


def _summarize(values: List[float]) -> Dict[str, Optional[float]]:
    return {
        "median": statistics.median(values) if values else None,
        "p95": _percentile(values, 95.0),
        "p99": _percentile(values, 99.0),
        "max": max(values) if values else None,
        "stdev": _stdev(values),
        "n": float(len(values)),
    }


def _fmt(value: Optional[float], decimals: int = 2) -> str:
    if value is None:
        return "-"
    if abs(value) >= 1000.0:
        return f"{value:,.{decimals}f}"
    return f"{value:.{decimals}f}"


def _print_summary_table(
    summaries: Dict[str, Dict[str, Dict[str, Optional[float]]]],
) -> None:
    """summaries[condition][metric] -> {median, p95, p99, max, stdev, n}."""
    print("| condition | metric            | median | p95 | p99 | max | stdev | n_frames |")
    print("|-----------|-------------------|--------|-----|-----|-----|-------|----------|")
    # Per D-09, total_frame_ms is the primary metric -- print it first.
    metric_order = [PRIMARY_METRIC] + [m for m in METRICS if m != PRIMARY_METRIC]
    for metric in metric_order:
        for condition in ("on", "off"):
            cell = summaries.get(condition, {}).get(metric)
            if cell is None:
                row_median = row_p95 = row_p99 = row_max = row_stdev = "-"
                row_n = "0"
            else:
                row_median = _fmt(cell["median"])
                row_p95 = _fmt(cell["p95"])
                row_p99 = _fmt(cell["p99"])
                row_max = _fmt(cell["max"])
                row_stdev = _fmt(cell["stdev"])
                n_val = cell["n"]
                row_n = str(int(n_val)) if n_val is not None else "0"
            print(
                f"| {condition:<9} | {metric:<17} | {row_median} | {row_p95} | "
                f"{row_p99} | {row_max} | {row_stdev} | {row_n} |"
            )


def _compute_verdict(
    summaries: Dict[str, Dict[str, Dict[str, Optional[float]]]],
) -> str:
    """Apply D-10 threshold + D-11 inconclusive default.

    Returns "verdict = remove" or "verdict = keep" (exact string).
    """
    on = summaries.get("on", {}).get(PRIMARY_METRIC)
    off = summaries.get("off", {}).get(PRIMARY_METRIC)

    # D-11: missing data -> default keep.
    if not on or not off:
        return "verdict = keep"
    if on["n"] == 0 or off["n"] == 0:
        return "verdict = keep"

    on_median = on["median"]
    on_p95 = on["p95"]
    off_median = off["median"]
    off_p95 = off["p95"]

    if (
        on_median is None
        or on_p95 is None
        or off_median is None
        or off_p95 is None
    ):
        # D-11: missing stats -> default keep.
        return "verdict = keep"

    # D-10: off.median <= on.median AND off.p95 <= on.p95 -> remove; else keep.
    if off_median <= on_median and off_p95 <= on_p95:
        return "verdict = remove"
    return "verdict = keep"


def main(argv: Optional[List[str]] = None) -> int:
    args = _parse_args(argv)

    rows = _load_all_rows(args.csv_dir)

    # Group rows by condition (on / off) using the run_label suffix per D-10.
    # Labels not matching -on / -off are reported and ignored.
    pools: Dict[str, Dict[str, List[float]]] = {"on": {}, "off": {}}
    for metric in METRICS:
        pools["on"][metric] = []
        pools["off"][metric] = []

    unknown_labels: Dict[str, int] = {}
    for row in rows:
        condition = _condition_for_label(row.get("run_label", ""))
        if condition is None:
            label = row.get("run_label", "<blank>")
            unknown_labels[label] = unknown_labels.get(label, 0) + 1
            continue
        for metric in METRICS:
            val = _safe_float(row.get(metric))
            if val is not None:
                pools[condition][metric].append(val)

    if unknown_labels:
        sys.stderr.write(
            "WARNING: rows with unrecognised run_label suffix (skipped):\n"
        )
        for label, count in sorted(unknown_labels.items()):
            sys.stderr.write(f"  {label!r}: {count} rows\n")

    summaries: Dict[str, Dict[str, Dict[str, Optional[float]]]] = {
        "on": {},
        "off": {},
    }
    for condition in ("on", "off"):
        for metric in METRICS:
            summaries[condition][metric] = _summarize(pools[condition][metric])

    # Print markdown table to stdout for paste into the verdict doc.
    _print_summary_table(summaries)

    # Emit verdict line as the FINAL line of stdout per D-10 / D-11.
    print(_compute_verdict(summaries))
    return 0


if __name__ == "__main__":
    sys.exit(main())
