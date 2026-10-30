#!/usr/bin/env python3
"""Build the two laboratory graphs from actual program CSV output."""

import argparse
import csv
import math
import statistics
import sys
from collections import defaultdict
from pathlib import Path


def read_runs(path, experiment, varying):
    groups = defaultdict(list)
    fixed = set()
    skipped = 0
    try:
        with path.open(encoding="utf-8-sig", newline="") as source:
            for row in csv.DictReader(source):
                if row["experiment"] != experiment:
                    continue
                if row["status"] != "ok" or int(row["violations"]) != 0 or int(row["timeouts"]) != 0:
                    skipped += 1
                    continue
                operations = int(row["operations"])
                percent = int(row["read_percent"])
                duration = int(row["read_ms"])
                seed = int(row["seed"])
                reads = operations * percent // 100
                writes = operations - reads
                if int(row["reader_done"]) != reads or int(row["writer_done"]) != writes:
                    raise ValueError("successful run has an unexpected operation count")
                values = {
                    "total_ms": float(row["total_ms"]),
                    "reader_wait_avg_ms": float(row["reader_wait_avg_ms"]) if reads else None,
                    "writer_wait_avg_ms": float(row["writer_wait_avg_ms"]) if writes else None,
                }
                if any(v is not None and (not math.isfinite(v) or v < 0) for v in values.values()):
                    raise ValueError("invalid timing value")
                key = percent if varying == "read_percent" else duration
                config = (operations, duration, seed) if varying == "read_percent" else (operations, percent, seed)
                fixed.add(config)
                groups[key].append(values)
    except (OSError, KeyError, TypeError, ValueError) as error:
        raise ValueError(f"{path}: {error}") from error

    if skipped:
        print(f"{path}: excluded {skipped} unsuccessful run(s)", file=sys.stderr)
    if not groups:
        raise ValueError(f"{path}: no successful runs for experiment {experiment}")
    if len(fixed) != 1:
        raise ValueError(f"{path}: fixed parameters differ within experiment {experiment}: {sorted(fixed)}")
    for key, runs in groups.items():
        if len(runs) < 3:
            raise ValueError(f"{path}: {varying}={key} has only {len(runs)} successful runs; need at least 3")
    return dict(sorted(groups.items())), fixed.pop()


def main():
    parser = argparse.ArgumentParser(description="Plot lab 4 measurements from CSV files.")
    parser.add_argument("--ratio", type=Path, required=True, help="CSV with experiment=ratio")
    parser.add_argument("--duration", type=Path, required=True, help="CSV with experiment=read_duration")
    parser.add_argument("--output-dir", type=Path, required=True, help="directory for PNG files")
    args = parser.parse_args()

    try:
        ratio, ratio_fixed = read_runs(args.ratio, "ratio", "read_percent")
        duration, duration_fixed = read_runs(args.duration, "read_duration", "read_ms")
        try:
            import matplotlib
            matplotlib.use("Agg")
            import matplotlib.pyplot as plt
        except ImportError as error:
            raise ValueError("matplotlib is required: python -m pip install matplotlib") from error

        args.output_dir.mkdir(parents=True, exist_ok=True)
        fig, ax = plt.subplots(figsize=(8, 5))
        for field, label in (("reader_wait_avg_ms", "Читатели"), ("writer_wait_avg_ms", "Писатели")):
            points = [(x, statistics.mean(run[field] for run in runs))
                      for x, runs in ratio.items() if all(run[field] is not None for run in runs)]
            if points:
                ax.plot([p[0] for p in points], [p[1] for p in points], marker="o", label=label)
        ax.set(xlabel="Доля операций чтения, %", ylabel="Среднее ожидание доступа, мс",
               title="Ожидание доступа и доля чтения")
        ax.grid(True, alpha=0.3)
        ax.legend()
        fig.tight_layout()
        fig.savefig(args.output_dir / "read-ratio-wait.png", dpi=180)
        plt.close(fig)

        fig, ax = plt.subplots(figsize=(8, 5))
        ax.plot(list(duration), [statistics.mean(run["total_ms"] for run in runs)
                                 for runs in duration.values()], marker="o")
        ax.set(xlabel="Заданная длительность чтения, мс", ylabel="Общее время эксперимента, мс",
               title="Время эксперимента и длительность чтения")
        ax.grid(True, alpha=0.3)
        fig.tight_layout()
        fig.savefig(args.output_dir / "read-duration-total.png", dpi=180)
        plt.close(fig)

        print(f"ratio: operations, read_ms, seed = {ratio_fixed}; runs = "
              + ", ".join(f"{x}%: {len(runs)}" for x, runs in ratio.items()))
        print(f"duration: operations, read_percent, seed = {duration_fixed}; runs = "
              + ", ".join(f"{x} ms: {len(runs)}" for x, runs in duration.items()))
        print(f"graphs saved to {args.output_dir}")
    except ValueError as error:
        parser.exit(1, f"error: {error}\n")


if __name__ == "__main__":
    main()
