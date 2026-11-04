#!/usr/bin/env python3
"""Compare interleaved, same-runner desktop benchmark samples by median."""
import argparse
import json
import math
import statistics
from pathlib import Path


METADATA = ("desktop_benchmark_version", "renderer", "scale_milli")


def read_report(path):
    fields = {}
    for raw in path.read_text(encoding="utf-8").splitlines():
        if "=" not in raw:
            continue
        key, value = (part.strip() for part in raw.split("=", 1))
        if key in fields:
            raise ValueError(f"{path}: duplicate benchmark field: {key}")
        fields[key] = value
    return fields


def compare_series(baseline, candidate):
    if len(baseline) != 3 or len(candidate) != 3:
        raise ValueError("Exactly three samples per version are required")
    samples = baseline + candidate
    keys = samples[0].keys()
    if any(item.keys() != keys for item in samples[1:]):
        raise ValueError("Benchmark schemas differ between samples")
    if any(key not in keys for key in METADATA):
        raise ValueError("Missing environment identity fields")
    for key in METADATA:
        if len({item[key] for item in samples}) != 1:
            raise ValueError(f"Benchmark environment changed: {key}")
    if samples[0]["desktop_benchmark_version"] != "3":
        raise ValueError("Unsupported benchmark report version")
    if samples[0]["renderer"] != "software":
        raise ValueError("Expected an identical software renderer")
    metrics = {}
    for key in sorted(keys):
        if not (key.endswith("_ms") or key.endswith("_kb") or
                key == "idle_cpu_percent"):
            continue
        before = [float(item[key]) for item in baseline]
        after = [float(item[key]) for item in candidate]
        if any(not math.isfinite(x) for x in before + after):
            raise ValueError(f"Non-finite metric: {key}")
        if any(x < 0 for x in before + after):
            if key.endswith("_kb") and all(x < 0 for x in before + after):
                continue  # RSS is unsupported on all six runs.
            raise ValueError(f"Inconsistent/negative metric: {key}")
        baseline_median = statistics.median(before)
        candidate_median = statistics.median(after)
        metrics[key] = {
            "baseline_samples": before,
            "candidate_samples": after,
            "baseline_median": baseline_median,
            "candidate_median": candidate_median,
            "baseline_range": [min(before), max(before)],
            "candidate_range": [min(after), max(after)],
            "median_delta": round(candidate_median - baseline_median, 3),
            "median_percent": round((candidate_median / baseline_median - 1) * 100, 2)
            if baseline_median > 0 else None,
            "ranges_overlap": min(after) <= max(before) and
                              min(before) <= max(after),
        }
    if not metrics:
        raise ValueError("No numeric desktop metrics to compare")
    return {
        "comparison": "Sudokura v1.3.8 versus v1.3.9",
        "renderer": samples[0]["renderer"],
        "scale_milli": int(samples[0]["scale_milli"]),
        "benchmark_version": samples[0]["desktop_benchmark_version"],
        "samples_per_version": 3,
        "execution_order": ["v138-1", "v139-1", "v139-2", "v138-2",
                            "v138-3", "v139-3"],
        "method": "interleaved 3+3, warmup once per version, median of process samples",
        "interpretation": "descriptive; range separation alone does not establish a user-visible regression",
        "metrics": metrics,
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--baseline", nargs=3, type=Path, required=True)
    parser.add_argument("--candidate", nargs=3, type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    report = compare_series([read_report(x) for x in args.baseline],
                            [read_report(x) for x in args.candidate])
    args.output.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n",
                           encoding="utf-8")
    print(f"Comparable metrics: {len(report['metrics'])}")
    for key, item in sorted(report["metrics"].items()):
        delta = item["median_percent"]
        pct = "n/a" if delta is None else f"{delta:+.2f}%"
        print(f"{key}: {item['baseline_median']:.3f} -> "
              f"{item['candidate_median']:.3f} ({pct}); "
              f"overlap={item['ranges_overlap']}")
    print("Medians are measurements, not automatic release acceptance")


if __name__ == "__main__":
    main()
