#!/usr/bin/env python3
"""Regression checks for repeatable desktop benchmark comparison input."""
import unittest

from scripts.compare_desktop_benchmarks import compare_series


def report(startup, renderer="software"):
    return {
        "desktop_benchmark_version": "3",
        "renderer": renderer,
        "scale_milli": "1000",
        "startup_first_home_ms": str(startup),
        "idle_cpu_percent": "0.5",
        "rss_before_kb": "27000",
    }


class BenchmarkSeriesTests(unittest.TestCase):
    def test_medians_and_separated_ranges(self):
        result = compare_series(
            [report(5), report(3), report(4)],
            [report(8), report(9), report(7)],
        )
        metric = result["metrics"]["startup_first_home_ms"]
        self.assertEqual(metric["baseline_median"], 4)
        self.assertEqual(metric["candidate_median"], 8)
        self.assertEqual(metric["median_percent"], 100)
        self.assertFalse(metric["ranges_overlap"])

    def test_overlapping_ranges(self):
        metric = compare_series(
            [report(5), report(7), report(9)],
            [report(6), report(7), report(8)],
        )["metrics"]["startup_first_home_ms"]
        self.assertTrue(metric["ranges_overlap"])

    def test_reject_noncomparable_samples(self):
        for baseline, candidate in (
            ([report(1)], [report(1)] * 3),
            ([report(1)] * 3,
             [report(1), report(2, renderer="accelerated"), report(3)]),
            ([report(1)] * 3,
             [report(1), dict(report(1), extra_field="bad"), report(2)]),
            ([report(1)] * 3, [report(1), report(float("nan")), report(1)]),
        ):
            with self.subTest(baseline=baseline, candidate=candidate):
                with self.assertRaises(ValueError):
                    compare_series(baseline, candidate)


if __name__ == "__main__":
    unittest.main()
