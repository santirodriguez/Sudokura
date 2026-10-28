#!/usr/bin/env python3
"""Compare same-runner desktop benchmark reports without inventing limits."""
import argparse
import json
import math
from pathlib import Path


def read_report(path):
    data = {}
    for raw in path.read_text(encoding="utf-8").splitlines():
        if "=" not in raw:
            continue
        key, value = (part.strip() for part in raw.split("=", 1))
        if key in data:
            raise ValueError(f"{path}: duplicate field {key}")
        data[key] = value
    return data


def compare(baseline, candidate):
    if baseline.keys() != candidate.keys():
        raise ValueError("Benchmark schemas differ; comparable metrics required")
    for key in ("desktop_benchmark_version", "renderer", "scale_milli"):
        if baseline[key] != candidate[key]:
            raise ValueError(f"Benchmark environment mismatch: {key}: "
                             f"{baseline[key]} != {candidate[key]}")
    if baseline["desktop_benchmark_version"] != "3":
        raise ValueError("Unsupported benchmark report version")
    if baseline["renderer"] != "software":
        raise ValueError("Same-runner comparison requires software renderer")
    metrics = {}
    for key in sorted(baseline):
        if not (key.endswith("_ms") or key.endswith("_kb") or
                key == "idle_cpu_percent"):
            continue
        before = float(baseline[key])
        after = float(candidate[key])
        if not (math.isfinite(before) and math.isfinite(after)):
            raise ValueError(f"Non-finite benchmark value: {key}")
        # Negative RSS may indicate that /proc/self is unavailable.
        if key.endswith("_kb") and (before < 0 or after < 0):
            continue
        metrics[key] = {
            "v1.3.8": before,
            "v1.3.9": after,
            "delta": round(after - before, 3),
            "percent": round((after / before - 1) * 100, 2)
            if before > 0 else None,
        }
    if not metrics:
        raise ValueError("No benchmark measurements to compare")
    return {
        "comparison": "Sudokura v1.3.8 versus v1.3.9",
        "benchmark_version": baseline["desktop_benchmark_version"],
        "renderer": baseline["renderer"],
        "scale_milli": int(baseline["scale_milli"]),
        "method": "one sequential measurement per version on same CI runner",
        "metrics": metrics,
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("baseline", type=Path)
    parser.add_argument("candidate", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    report = compare(read_report(args.baseline), read_report(args.candidate))
    args.output.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n",
                           encoding="utf-8")
    print(f"Comparable measurements: {len(report['metrics'])}")
    for key in sorted(report["metrics"]):
        value = report["metrics"][key]
        change = value["percent"]
        label = "n/a" if change is None else f"{change:+.2f}%"
        print(f"{key}: {value['v1.3.8']} -> {value['v1.3.9']} ({label})")
    print("Comparison is observational, not an automatic performance PASS")


if __name__ == "__main__":
    main()
