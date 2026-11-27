#!/usr/bin/env python3
"""Summarize the nine measured runs and build standalone SVG plots."""

import argparse
import csv
from decimal import Decimal
from pathlib import Path
from statistics import mean
from xml.etree.ElementTree import Element, SubElement, ElementTree

RATES = (10, 50, 100)
DURATION_MS = 30_000
COLORS = ("#185a93", "#ae4818", "#648037")


def read_run(path, hz, repeat):
    with path.open(encoding="utf-8-sig", newline="") as stream:
        rows = list(csv.DictReader(stream))
    if not rows or not rows[0]["run_id"]:
        raise ValueError(f"{path}: empty run")
    run_id = rows[0]["run_id"]
    keys = set()
    intervals = set()
    messages = timer = test = 0
    for row in rows:
        start = Decimal(row["interval_start_ms"])
        end = Decimal(row["interval_end_ms"])
        message_id = row["message_id"]
        count = int(row["count"])
        ticks = int(row["study_timer_count"])
        key = (start, message_id)
        if (
            row["run_id"] != run_id
            or int(row["target_hz"]) != hz
            or int(row["period_ms"]) != 1000 // hz
            or row["status"] != "completed"
            or Decimal(row["observed_ms"]) != DURATION_MS
            or Decimal(row["requested_ms"]) != DURATION_MS
            or start not in range(0, DURATION_MS, 1000)
            or end != start + 1000
            or key in keys
            or not 0 <= ticks <= count
            or (message_id != "275" and ticks)
            or (not message_id and count)
        ):
            raise ValueError(f"{path}: inconsistent measurement row {key}")
        keys.add(key)
        intervals.add(start)
        messages += count
        timer += ticks
        if message_id == "32769":
            test += count
    if len(intervals) != DURATION_MS // 1000:
        raise ValueError(f"{path}: missing intervals")
    return {
        "file": path.name, "run_id": run_id, "target_hz": hz, "repeat": repeat,
        "observed_ms": DURATION_MS, "messages": messages, "timer_messages": timer,
        "other_messages": messages - timer, "test_messages": test,
        "actual_hz": timer / (DURATION_MS / 1000),
    }


def write_table(path, rows):
    with path.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)


def plot(path, title, ylabel, maximum, step, draw):
    svg = Element("svg", xmlns="http://www.w3.org/2000/svg", width="900", height="490",
                  viewBox="0 0 900 490", **{"font-family": "Arial, sans-serif"})
    SubElement(svg, "title").text = title

    def shape(tag, **attrs):
        return SubElement(svg, tag, {key.replace("_", "-"): str(value)
                                    for key, value in attrs.items()})

    def text(x, y, label, size=15, anchor="start"):
        shape("text", x=x, y=y, fill="#162b3d", font_size=size,
              text_anchor=anchor).text = str(label)

    def y(value):
        return 396 - 304 * value / maximum

    shape("rect", width=900, height=490, fill="white")
    text(450, 34, title, 24, "middle")
    text(74, 72, ylabel)
    for mark in range(0, maximum + 1, step):
        shape("line", x1=76, x2=860, y1=y(mark), y2=y(mark), stroke="#d7e0e8")
        text(66, y(mark) + 5, mark, anchor="end")
    for x, rate in zip((218, 468, 718), RATES):
        text(x, 425, rate, anchor="middle")
        draw(shape, y, x, rate)
    text(468, 450, "Заданная частота таймера, срабатываний/с", anchor="middle")
    notes = (
        "Точки — три повтора; чёрная черта — среднее; ромб — заданная частота"
        if path.stem == "timer-frequency"
        else "Слева направо — повторы 1–3; синий — сообщения по базовому таймер; оранжевый — сообщения от пользователя"
    )
    text(450, 476, notes, 13, "middle")
    ElementTree(svg).write(path, encoding="utf-8", xml_declaration=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv_dir", type=Path)
    parser.add_argument("output_dir", type=Path)
    args = parser.parse_args()
    try:
        series = {
            hz: [read_run(args.csv_dir / f"test-{group}-{repeat}.csv", hz, repeat)
                 for repeat in range(1, 4)]
            for group, hz in enumerate(RATES, 1)
        }
        runs = [row for rows in series.values() for row in rows]
        if len({row["run_id"] for row in runs}) != len(runs):
            raise ValueError("run IDs must be unique")
    except (OSError, KeyError, ValueError, ArithmeticError) as error:
        parser.error(str(error))

    def draw_frequency(shape, y, x, hz):
        for offset, color, run in zip((-24, 0, 24), COLORS, series[hz]):
            shape("circle", cx=x + offset, cy=y(run["actual_hz"]), r=5, fill=color)
        average = y(mean(run["actual_hz"] for run in series[hz]))
        shape("line", x1=x-38, x2=x+38, y1=average, y2=average, stroke="#172a3a",
              stroke_width=2)
        top = y(hz)
        shape("polygon", points=f"{x},{top-6} {x+6},{top} {x},{top+6} {x-6},{top}",
              fill="white", stroke="#172a3a", stroke_width=2)

    def draw_counts(shape, y, x, hz):
        for offset, run in zip((-46, 0, 46), series[hz]):
            left = x + offset - 16
            split = y(run["timer_messages"])
            top = y(run["messages"])
            shape("rect", x=left, y=split, width=32, height=y(0)-split, fill=COLORS[0])
            shape("rect", x=left, y=top, width=32, height=split-top, fill=COLORS[1])

    args.output_dir.mkdir(parents=True, exist_ok=True)
    write_table(args.output_dir / "runs.csv", runs)
    write_table(args.output_dir / "summary.csv", [
        {
            "target_hz": hz, "repeats": len(rows),
            "mean_timer_messages": round(mean(row["timer_messages"] for row in rows), 3),
            "mean_actual_hz": round(mean(row["actual_hz"] for row in rows), 3),
            "min_actual_hz": round(min(row["actual_hz"] for row in rows), 3),
            "max_actual_hz": round(max(row["actual_hz"] for row in rows), 3),
        }
        for hz, rows in series.items()
    ])
    plot(args.output_dir / "timer-frequency.svg", "Фактическая частота учебного таймера",
         "Получено уведомлений в секунду", 120, 20, draw_frequency)
    max_count = max(run["messages"] for run in runs)
    step = 1000
    plot(args.output_dir / "message-counts.svg", "Зарегистрированные сообщения по прогонам",
         "Количество сообщений за 30 секунд", (max_count // step + 1) * step, step,
         draw_counts)


if __name__ == "__main__":
    main()
