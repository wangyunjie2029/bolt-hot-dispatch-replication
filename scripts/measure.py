#!/usr/bin/env python3
"""Run a command N times and report timing statistics.

Usage:
    measure.py --reps 10 --warmup 2 --json out.json -- <cmd> [args...]

The wall-clock time of every repetition is measured with time.perf_counter().
The first --warmup runs are discarded.  The report contains min / median /
mean / stdev plus the raw samples, so that the experiment driver can compute
speedups and confidence intervals afterwards.
"""
import argparse
import json
import os
import statistics
import subprocess
import sys
import time


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--reps", type=int, default=10)
    ap.add_argument("--warmup", type=int, default=2)
    ap.add_argument("--json", default=None)
    ap.add_argument("--cwd", default=None)
    ap.add_argument("command", nargs=argparse.REMAINDER)
    args = ap.parse_args()

    cmd = args.command
    if cmd and cmd[0] == "--":
        cmd = cmd[1:]
    if not cmd:
        ap.error("no command given")

    samples = []
    for i in range(args.warmup + args.reps):
        t0 = time.perf_counter()
        proc = subprocess.run(cmd, cwd=args.cwd,
                              stdout=subprocess.DEVNULL,
                              stderr=subprocess.DEVNULL)
        dt = time.perf_counter() - t0
        if proc.returncode != 0:
            print(f"command failed with {proc.returncode}: {' '.join(cmd)}",
                  file=sys.stderr)
            return 1
        if i >= args.warmup:
            samples.append(dt)

    result = {
        "command": cmd,
        "reps": len(samples),
        "min": min(samples),
        "median": statistics.median(samples),
        "mean": statistics.mean(samples),
        "stdev": statistics.stdev(samples) if len(samples) > 1 else 0.0,
        "samples": samples,
    }
    print(f"median={result['median']:.4f}s min={result['min']:.4f}s "
          f"mean={result['mean']:.4f}s stdev={result['stdev']:.4f}s "
          f"(n={result['reps']})")
    if args.json:
        with open(args.json, "w", encoding="utf-8") as f:
            json.dump(result, f, indent=2)
    return 0


if __name__ == "__main__":
    sys.exit(main())
