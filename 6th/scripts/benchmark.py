#!/usr/bin/env python3
"""Run repeatable Chapter 6 MPI benchmarks and save CSV/Markdown output."""
from __future__ import annotations

import argparse
import csv
import datetime as dt
import pathlib
import shlex
import subprocess


ROOT = pathlib.Path(__file__).resolve().parents[1]


def run_once(program: str, n: int, processes: int, seed: int) -> tuple[float, int]:
    command = [
        "mpirun", "--oversubscribe", "-np", str(processes),
        str(ROOT / program), str(n), str(seed),
    ]
    result = subprocess.run(command, cwd=ROOT, text=True, capture_output=True, check=True)
    values: dict[str, str] = {}
    for line in result.stdout.splitlines():
        fields = line.split(maxsplit=1)
        if len(fields) == 2:
            values[fields[0]] = fields[1]
    if values.get("CORRECT") != "1":
        raise RuntimeError(f"incorrect result from {shlex.join(command)}:\n{result.stdout}\n{result.stderr}")
    return float(values["TIME"]), int(values["DIST_SUM"])


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--n", type=int, default=512)
    parser.add_argument("--processes", nargs="+", default=["1", "2", "4"])
    parser.add_argument("--repeats", type=int, default=3)
    parser.add_argument("--seed", type=int, default=7)
    args = parser.parse_args()
    try:
        process_counts = [int(item) for value in args.processes for item in value.split()]
    except ValueError as exc:
        parser.error(f"invalid process count: {exc}")
    if any(p <= 0 for p in process_counts):
        parser.error("process counts must be positive")
    if args.n <= 0 or args.repeats <= 0:
        parser.error("--n and --repeats must be positive")

    stamp = dt.datetime.now().strftime("%Y%m%d_%H%M%S")
    csv_path = ROOT / "results" / f"benchmark_{stamp}_n{args.n}.csv"
    md_path = csv_path.with_suffix(".md")
    rows: list[dict[str, object]] = []
    for program in ("floyd_p2p", "floyd_bcast"):
        for process_count in process_counts:
            times = []
            checksum = None
            for repeat in range(1, args.repeats + 1):
                elapsed, checksum = run_once(program, args.n, process_count, args.seed)
                times.append(elapsed)
                rows.append({"program": program, "n": args.n, "processes": process_count,
                             "repeat": repeat, "time_seconds": elapsed,
                             "dist_sum": checksum})
            print(f"{program:12s} np={process_count:2d} "
                  f"min={min(times):.6f}s median={sorted(times)[len(times)//2]:.6f}s")

    csv_path.parent.mkdir(exist_ok=True)
    with csv_path.open("w", newline="") as file:
        writer = csv.DictWriter(file, fieldnames=rows[0].keys())
        writer.writeheader()
        writer.writerows(rows)

    grouped: dict[tuple[str, int], list[float]] = {}
    for row in rows:
        grouped.setdefault((str(row["program"]), int(row["processes"])), []).append(float(row["time_seconds"]))
    baseline = {program: min(values) for (program, p), values in grouped.items() if p == min(process_counts)}
    lines = [
        f"# 第六章 Floyd-Warshall 性能记录（N={args.n}）", "",
        f"- 生成时间：{dt.datetime.now().astimezone().isoformat(timespec='seconds')}",
        f"- 命令：`make bench N={args.n} P=\"{' '.join(map(str, process_counts))}\" R={args.repeats}`", "",
        "| program | MPI processes | min time (s) | median time (s) | speedup vs. min np |", "|---|---:|---:|---:|---:|",
    ]
    for (program, processes), values in sorted(grouped.items()):
        values.sort()
        minimum = min(values)
        median = values[len(values) // 2]
        speedup = baseline[program] / minimum
        lines.append(f"| {program} | {processes} | {minimum:.6f} | {median:.6f} | {speedup:.3f} |")
    lines += ["", "原始逐次测量数据见同名 CSV；不同机器、MPI 实现和 CPU 绑定策略下的绝对时间不可直接比较。"]
    md_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"saved {csv_path}")
    print(f"saved {md_path}")


if __name__ == "__main__":
    main()
