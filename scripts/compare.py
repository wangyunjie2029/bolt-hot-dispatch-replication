#!/usr/bin/env python3
"""Interleaved A/B benchmark driver.

Every repetition runs all variants once, in the same order, so that slow drift
of the machine (frequency scaling, neighbours) affects all variants equally.
The reported metric is the median wall-clock time of each variant, plus the
speedup relative to the first variant.

Usage:
    compare.py --reps 15 --json out.json \
        --run base::./vm_base 200000 \
        --run hdr::./vm_hdr 200000
"""
import argparse
import json
import shlex
import random
import statistics
import subprocess
import sys
import time


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--reps", type=int, default=15)
    ap.add_argument("--warmup", type=int, default=2)
    ap.add_argument("--json", default=None)
    ap.add_argument("--cwd", default=".")
    ap.add_argument("--shuffle", action="store_true",
                    help="randomise the order of variants in every repetition")
    ap.add_argument("--run", action="append", default=[],
                    help="name::command")
    args = ap.parse_args()

    variants = []
    for spec in args.run:
        if "::" not in spec:
            ap.error(f"bad --run spec: {spec}")
        name, cmd = spec.split("::", 1)
        variants.append((name, shlex.split(cmd)))

    if not variants:
        ap.error("no --run given")

    samples = {name: [] for name, _ in variants}
    rng = random.Random(20261220)
    for i in range(args.warmup + args.reps):
        order = list(variants)
        if args.shuffle:
            rng.shuffle(order)
        for name, cmd in order:
            t0 = time.perf_counter()
            proc = subprocess.run(cmd, cwd=args.cwd,
                                  stdout=subprocess.DEVNULL,
                                  stderr=subprocess.DEVNULL)
            dt = time.perf_counter() - t0
            if proc.returncode != 0:
                print(f"{name}: command failed ({proc.returncode})",
                      file=sys.stderr)
                return 1
            if i >= args.warmup:
                samples[name].append(dt)

    base = variants[0][0]
    base_med = statistics.median(samples[base])
    out = {}
    print(f"{'variant':<16}{'median(s)':>12}{'min(s)':>10}{'stdev':>10}"
          f"{'speedup vs ' + base:>22}")
    for name, _ in variants:
        med = statistics.median(samples[name])
        print(f"{name:<16}{med:>12.4f}{min(samples[name]):>10.4f}"
              f"{statistics.stdev(samples[name]):>10.4f}"
              f"{base_med / med:>21.3f}x")
        out[name] = {
            "median": med,
            "min": min(samples[name]),
            "stdev": statistics.stdev(samples[name]),
            "speedup": base_med / med,
            "samples": samples[name],
        }

    if args.json:
        with open(args.json, "w", encoding="utf-8") as f:
            json.dump(out, f, indent=2)
    return 0


if __name__ == "__main__":
    sys.exit(main())
