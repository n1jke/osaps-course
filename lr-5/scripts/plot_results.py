#!/usr/bin/env python3
"""Build lab 5 plots and per-run summaries from one measurement series."""

import argparse
import csv
import math
import re
import statistics
import sys
from pathlib import Path

RESOURCE_SIZES = {"empty": 0, "small": 1024, "large": 1048576}
RESOURCE_NAMES = {**RESOURCE_SIZES, "missing": None}
FILE_PATTERN = re.compile(r"^(GET|HEAD)-(empty|small|large|missing)-(\d+)\.csv$")
FIELDS = {
    "run_id", "attempt", "host", "port", "method", "path", "http_status",
    "outcome", "error_stage", "connect_ms", "attempt_ms", "sent_http_bytes",
    "received_http_bytes", "body_bytes", "request_complete", "response_complete",
}


def read_run(path):
    match = FILE_PATTERN.fullmatch(path.name)
    if not match:
        raise ValueError(f"{path}: unexpected file name")
    method, resource, repeat_text = match.groups()
    with path.open(encoding="utf-8-sig", newline="") as stream:
        reader = csv.DictReader(stream)
        if not reader.fieldnames or not FIELDS.issubset(reader.fieldnames):
            raise ValueError(f"{path}: missing CSV columns")
        rows = list(reader)
    if not rows:
        raise ValueError(f"{path}: empty run")
    run_ids = {row["run_id"] for row in rows}
    peers = {(row["host"], row["port"]) for row in rows}
    if len(run_ids) != 1 or "" in run_ids or len(peers) != 1:
        raise ValueError(f"{path}: inconsistent run ID or server address")

    seen_attempts = set()
    valid = []
    errors = timeouts = requests = responses = sent = received = body = 0
    expected_status = "404" if resource == "missing" else "200"
    for row in rows:
        if row["method"] != method or row["path"] != f"/{resource}":
            raise ValueError(f"{path}: method or path differs from file name")
        try:
            attempt = int(row["attempt"])
            attempt_ms = float(row["attempt_ms"])
            connect_ms = float(row["connect_ms"]) if row["connect_ms"] else None
            sent_bytes = int(row["sent_http_bytes"])
            received_bytes = int(row["received_http_bytes"])
            body_bytes = int(row["body_bytes"])
        except (ValueError, TypeError) as error:
            raise ValueError(f"{path}: invalid numeric value") from error
        if (attempt <= 0 or attempt in seen_attempts
                or not math.isfinite(attempt_ms) or attempt_ms < 0
                or (connect_ms is not None and (not math.isfinite(connect_ms)
                    or connect_ms < 0 or connect_ms > attempt_ms + 0.000001))
                or min(sent_bytes, received_bytes, body_bytes) < 0
                or body_bytes > received_bytes):
            raise ValueError(f"{path}: invalid or inconsistent measurements")
        seen_attempts.add(attempt)
        if row["request_complete"] not in ("0", "1") or row["response_complete"] not in ("0", "1"):
            raise ValueError(f"{path}: invalid completion flag")
        requests += row["request_complete"] == "1"
        responses += row["response_complete"] == "1"
        sent += sent_bytes
        received += received_bytes
        body += body_bytes
        if row["outcome"] == "ok":
            if (row["http_status"] != expected_status or row["request_complete"] != "1"
                    or row["response_complete"] != "1"):
                raise ValueError(f"{path}: unexpected status or incomplete successful response")
            expected_body = (0 if method == "HEAD" else
                             10 if resource == "missing" else RESOURCE_SIZES[resource])
            if body_bytes != expected_body:
                raise ValueError(f"{path}: unexpected body size {body_bytes}, expected {expected_body}")
            valid.append((attempt_ms, received_bytes, body_bytes, connect_ms))
        elif row["outcome"] == "timeout":
            timeouts += 1
        elif row["outcome"] == "error":
            errors += 1
        else:
            raise ValueError(f"{path}: unknown outcome {row['outcome']!r}")

    elapsed = sum(item[0] for item in valid)
    connected = [item[3] for item in valid if item[3] is not None]
    return {
        "file": path.name, "run_id": next(iter(run_ids)), "host": next(iter(peers))[0],
        "port": next(iter(peers))[1], "method": method, "resource": resource,
        "resource_bytes": RESOURCE_NAMES[resource], "repeat": int(repeat_text),
        "attempts": len(rows), "requests": requests, "responses": responses,
        "successful": len(valid), "errors": errors, "timeouts": timeouts,
        "sent_http_bytes": sent, "received_http_bytes": received, "body_bytes": body,
        "mean_attempt_ms": statistics.mean(item[0] for item in valid) if valid else None,
        "mean_connect_ms": statistics.mean(connected) if connected else None,
        "mean_received_http_bytes": statistics.mean(item[1] for item in valid) if valid else None,
        "get_body_bytes_per_second": (sum(item[2] for item in valid) * 1000 / elapsed
                                      if method == "GET" and elapsed > 0 else None),
    }


def plot(runs, field, output, ylabel, title):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    fig, ax = plt.subplots(figsize=(8, 5))
    for method, color, marker in [("GET", "#176a55", "o"), ("HEAD", "#b45d2c", "s")]:
        points = []
        for resource, size in RESOURCE_SIZES.items():
            values = [run[field] for run in runs
                      if run["method"] == method and run["resource"] == resource
                      and run[field] is not None]
            if values:
                points.append((size, values))
        if not points:
            continue
        x = [list(RESOURCE_SIZES.values()).index(point[0]) for point in points]
        means = [statistics.mean(point[1]) for point in points]
        lower = [mean - min(point[1]) for mean, point in zip(means, points)]
        upper = [max(point[1]) - mean for mean, point in zip(means, points)]
        ax.errorbar(x, means, yerr=[lower, upper], label=method, color=color,
                    marker=marker, capsize=4, linewidth=1.8)
    if field == "mean_received_http_bytes":
        ax.set_yscale("log")
    ax.set_xticks(range(len(RESOURCE_SIZES)), ["0", "1 024", "1 048 576"])
    ax.set_xlabel("Размер ресурса, байт (отдельные конфигурации)")
    ax.set_ylabel(ylabel)
    ax.set_title(title)
    ax.grid(True, alpha=0.25)
    ax.legend(title="Метод")
    fig.tight_layout()
    fig.savefig(output, dpi=180)
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input_dir", type=Path, help="directory containing one series of CSV files")
    parser.add_argument("output_dir", type=Path, help="directory for plots and summary.csv")
    args = parser.parse_args()
    if not args.input_dir.is_dir():
        parser.error(f"input directory does not exist: {args.input_dir}")
    files = sorted(args.input_dir.glob("*.csv"))
    if not files:
        parser.error("no CSV files in input directory")
    try:
        import matplotlib  # Verify the dependency before creating output files.
    except ImportError:
        parser.exit(1, "error: install matplotlib with: python -m pip install matplotlib\n")
    try:
        runs = [read_run(path) for path in files]
        keys = [(r["method"], r["resource"], r["repeat"]) for r in runs]
        if len(keys) != len(set(keys)):
            raise ValueError("duplicate method, resource and repeat")
        if len({r["run_id"] for r in runs}) != len(runs):
            raise ValueError("repeated run ID across files")
        if len({(r["host"], r["port"]) for r in runs}) != 1:
            raise ValueError("server address differs within the series")
        for method in ("GET", "HEAD"):
            for resource in RESOURCE_NAMES:
                group = [r for r in runs if r["method"] == method and r["resource"] == resource]
                if len(group) != 3 or any(r["attempts"] != 100 for r in group):
                    raise ValueError(f"{method} /{resource}: expected 3 runs of 100 attempts")
        args.output_dir.mkdir(parents=True, exist_ok=True)
        summary_path = args.output_dir / "summary.csv"
        with summary_path.open("w", encoding="utf-8", newline="") as stream:
            writer = csv.DictWriter(stream, fieldnames=runs[0].keys())
            writer.writeheader()
            writer.writerows(runs)
        plot(runs, "mean_attempt_ms", args.output_dir / "attempt-time.png",
             "Среднее время клиентской операции, мс",
             "Время операции и размер ресурса")
        plot(runs, "mean_received_http_bytes", args.output_dir / "received-bytes.png",
             "Принято HTTP-байтов, среднее (лог. шкала)",
             "Объём ответа и размер ресурса")
        print(f"{args.input_dir}: {len(runs)} runs, {sum(r['attempts'] for r in runs)} attempts")
        print(f"Successful: {sum(r['successful'] for r in runs)}, "
              f"errors: {sum(r['errors'] for r in runs)}, timeouts: {sum(r['timeouts'] for r in runs)}")
        print(f"Plots and per-run summary: {args.output_dir}")
        print("404 runs are included in summary.csv and excluded from resource-size plots.")
        print("Error bars show the minimum and maximum of the three run averages.")
        if any(r["successful"] != r["attempts"] for r in runs):
            print("Warning: graphs use successful attempts only; inspect summary.csv for failed runs.", file=sys.stderr)
    except (OSError, ValueError) as error:
        parser.exit(1, f"error: {error}\n")


if __name__ == "__main__":
    main()
