#!/usr/bin/env python3
"""Run the four sieve versions, record CSV data, and create a summary."""
from __future__ import annotations

import argparse
import csv
import datetime as dt
import os
import platform
import shlex
import statistics
import subprocess
import sys
from pathlib import Path

PROGRAMS = [
    ("book", "sieve_book"),
    ("odd_broadcast", "sieve_odd"),
    ("odd_local_primes", "sieve_local"),
    ("odd_local_blocked", "sieve_blocked"),
]


def read_field(output: str, name: str) -> str:
    for line in output.splitlines():
        key, _, value = line.partition(" ")
        if key == name:
            return value.strip()
    raise RuntimeError(f"missing {name!r} in program output:\n{output}")


def command_line(executable: str, n: int, np: int, block_kib: int) -> list[str]:
    launcher = os.environ.get("MPIEXEC", "mpirun")
    flags = shlex.split(os.environ.get("MPIEXEC_FLAGS", "--oversubscribe"))
    if os.geteuid() == 0 and "--allow-run-as-root" not in flags:
        flags.append("--allow-run-as-root")
    args = [launcher, *flags, "-np", str(np), f"./{executable}", str(n)]
    if executable == "sieve_blocked":
        args.append(str(block_kib))
    return args


def run_once(executable: str, n: int, np: int, block_kib: int) -> dict[str, str]:
    command = command_line(executable, n, np, block_kib)
    completed = subprocess.run(command, text=True, capture_output=True)
    if completed.returncode != 0:
        message = completed.stderr.strip() or completed.stdout.strip()
        raise RuntimeError(f"command failed ({completed.returncode}): {' '.join(command)}\n{message}")
    output = completed.stdout
    return {
        "primes": read_field(output, "PRIMES"),
        "time_seconds": read_field(output, "TIME_SECONDS"),
    }


def median_rows(rows: list[dict[str, str]]) -> dict[tuple[str, int], float]:
    values: dict[tuple[str, int], list[float]] = {}
    for row in rows:
        values.setdefault((row["version"], int(row["processes"])), []).append(float(row["time_seconds"]))
    return {key: statistics.median(items) for key, items in values.items()}


def write_summary(path: Path, rows: list[dict[str, str]]) -> None:
    medians = median_rows(rows)
    versions = [version for version, _ in PROGRAMS]
    processes = sorted({int(row["processes"]) for row in rows})
    lines = [
        "# 第五章性能实验记录",
        "",
        f"- 生成时间：`{rows[0]['timestamp']}`",
        f"- 主机：`{rows[0]['host']}`",
        f"- 输入规模：`n = {rows[0]['n']}`",
        f"- 每个配置重复：`{rows[0]['repeats']}` 次；表格使用中位数",
        f"- 原始数据：`{Path(rows[0]['csv']).name}`",
        "",
        "## 中位数运行时间",
        "",
        "| 版本 | MPI 进程数 | 时间（秒） | 相对该版本 p=1 加速比 |",
        "|---|---:|---:|---:|",
    ]
    for version in versions:
        baseline = medians.get((version, 1))
        for np in processes:
            value = medians.get((version, np))
            if value is None:
                continue
            speedup = baseline / value if baseline else float("nan")
            lines.append(f"| `{version}` | {np} | {value:.9f} | {speedup:.2f}x |")
    lines += [
        "",
        "## 解释",
        "",
        "- `TIME_SECONDS` 是各 rank 局部计时的最大值，表示一次并行运行的完成时间。",
        "- 本实验主要用于比较版本演进和通信/局部性影响，不代表所有机器上的绝对性能。",
        "- 运行前应保证机器负载稳定，并记录 MPI 进程数、输入规模和块大小。",
    ]
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--n", type=int, default=10_000_000)
    parser.add_argument("--processes", default="1 2 4", help="例如: '1 2 4 8'")
    parser.add_argument("--repeats", type=int, default=3)
    parser.add_argument("--block-kib", type=int, default=32)
    parser.add_argument("--outdir", type=Path, default=Path("results"))
    parser.add_argument("--no-plot", action="store_true")
    args = parser.parse_args()

    if args.n < 2 or args.repeats < 1 or args.block_kib < 1:
        parser.error("n >= 2, repeats >= 1, block-kib >= 1 are required")
    processes = [int(item) for item in args.processes.split()]
    if not processes or any(item < 1 for item in processes):
        parser.error("processes must be positive integers")

    args.outdir.mkdir(parents=True, exist_ok=True)
    timestamp = dt.datetime.now().astimezone().isoformat(timespec="seconds")
    stem = f"benchmark_{dt.datetime.now().strftime('%Y%m%d_%H%M%S')}_n{args.n}"
    csv_path = args.outdir / f"{stem}.csv"
    summary_path = args.outdir / f"{stem}.md"
    plot_path = args.outdir / f"{stem}.png"

    rows: list[dict[str, str]] = []
    for version, executable in PROGRAMS:
        for np in processes:
            print(f"{version:18s} np={np} repeats={args.repeats}", file=sys.stderr)
            for repeat in range(1, args.repeats + 1):
                result = run_once(executable, args.n, np, args.block_kib)
                rows.append({
                    "timestamp": timestamp,
                    "host": platform.node(),
                    "n": str(args.n),
                    "version": version,
                    "executable": executable,
                    "processes": str(np),
                    "repeat": str(repeat),
                    "repeats": str(args.repeats),
                    "block_kib": str(args.block_kib if executable == "sieve_blocked" else ""),
                    "primes": result["primes"],
                    "time_seconds": result["time_seconds"],
                    "csv": str(csv_path),
                })

    with csv_path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    write_summary(summary_path, rows)

    if not args.no_plot:
        plot_script = Path(__file__).with_name("plot.py")
        subprocess.run([sys.executable, str(plot_script), str(csv_path), "--output", str(plot_path)], check=True)

    print(f"CSV:     {csv_path}")
    print(f"Summary: {summary_path}")
    if not args.no_plot:
        print(f"Plot:    {plot_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
