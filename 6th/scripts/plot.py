#!/usr/bin/env python3
"""Plot Chapter 6 CSV benchmark results."""
from __future__ import annotations

import argparse
import csv
import pathlib
from collections import defaultdict


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("csv", type=pathlib.Path)
    parser.add_argument("--output", type=pathlib.Path)
    args = parser.parse_args()

    import matplotlib.pyplot as plt

    groups: dict[str, dict[int, list[float]]] = defaultdict(lambda: defaultdict(list))
    with args.csv.open(newline="") as file:
        for row in csv.DictReader(file):
            groups[row["program"]][int(row["processes"])].append(float(row["time_seconds"]))

    fig, ax = plt.subplots(figsize=(8, 5))
    for program, values in sorted(groups.items()):
        x = sorted(values)
        y = [min(values[p]) for p in x]
        ax.plot(x, y, marker="o", label=program)
    ax.set_xlabel("MPI processes")
    ax.set_ylabel("minimum elapsed time (s)")
    ax.set_title("Floyd-Warshall MPI scaling")
    ax.grid(True, alpha=0.3)
    ax.legend()
    fig.tight_layout()
    output = args.output or args.csv.with_suffix(".png")
    fig.savefig(output, dpi=160)
    print(f"saved {output}")


if __name__ == "__main__":
    main()
