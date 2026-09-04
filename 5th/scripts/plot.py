#!/usr/bin/env python3
"""Plot time and speedup from a sieve benchmark CSV."""
from __future__ import annotations

import argparse
import csv
import statistics
from collections import defaultdict
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()

    import matplotlib.pyplot as plt

    with args.csv.open(newline="", encoding="utf-8") as stream:
        rows = list(csv.DictReader(stream))
    if not rows:
        raise SystemExit("empty CSV")

    values: dict[tuple[str, int], list[float]] = defaultdict(list)
    for row in rows:
        values[(row["version"], int(row["processes"]))].append(float(row["time_seconds"]))
    versions = list(dict.fromkeys(row["version"] for row in rows))
    processes = sorted({int(row["processes"]) for row in rows})

    fig, axes = plt.subplots(1, 2, figsize=(12, 4.8), constrained_layout=True)
    for version in versions:
        available = [np for np in processes if (version, np) in values]
        times = [statistics.median(values[(version, np)]) for np in available]
        axes[0].plot(available, times, marker="o", label=version)
        baseline = times[0]
        axes[1].plot(available, [baseline / time for time in times], marker="o", label=version)

    axes[0].set_title("Sieve runtime")
    axes[0].set_xlabel("MPI processes")
    axes[0].set_ylabel("seconds (median)")
    axes[0].set_xticks(processes)
    axes[0].grid(True, alpha=0.3)
    axes[0].legend(fontsize=8)

    axes[1].set_title("Speedup relative to p=1")
    axes[1].set_xlabel("MPI processes")
    axes[1].set_ylabel("speedup")
    axes[1].set_xticks(processes)
    axes[1].plot(processes, processes, "k--", alpha=0.45, label="ideal")
    axes[1].grid(True, alpha=0.3)
    axes[1].legend(fontsize=8)

    output = args.output or args.csv.with_suffix(".png")
    fig.savefig(output, dpi=160)
    print(output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
