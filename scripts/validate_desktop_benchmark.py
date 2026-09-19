#!/usr/bin/env python3
"""Validate the Phase 6 desktop benchmark report shape and bounded-cache invariant."""
from pathlib import Path
import argparse

p=argparse.ArgumentParser()
p.add_argument("report",type=Path)
args=p.parse_args()
values={}
for raw in args.report.read_text(encoding="utf-8").splitlines():
    if "=" not in raw:
        continue
    key,value=raw.split("=",1)
    values[key.strip()]=value.strip()

required={
    "desktop_benchmark_version","startup_first_home_ms",
    "input_render_p50_ms","idle_cpu_percent",
    "generation_easy_p50_ms","generation_easy_max_ms",
    "generation_medium_p50_ms","generation_medium_max_ms",
    "generation_hard_p50_ms","generation_hard_max_ms",
    "save_p50_ms","rss_before_kb","rss_after_warmup_kb",
    "rss_after_repeat_kb","rss_repeat_delta_kb",
    "text_cache_entries","text_cache_capacity","renderer","scale_milli",
}
missing=sorted(required-values.keys())
assert not missing,f"missing benchmark fields: {missing}"
assert values["desktop_benchmark_version"]=="2"
for key in ("startup_first_home_ms","input_render_p50_ms","idle_cpu_percent",
            "generation_easy_p50_ms","generation_easy_max_ms",
            "generation_medium_p50_ms","generation_medium_max_ms",
            "generation_hard_p50_ms","generation_hard_max_ms",
            "save_p50_ms"):
    assert float(values[key])>=0.0,(key,values[key])
entries=int(values["text_cache_entries"])
capacity=int(values["text_cache_capacity"])
assert 0<=entries<=capacity<=128,(entries,capacity)
assert int(values["scale_milli"])>0
assert values["renderer"] in {"software","accelerated"}
print("desktop benchmark report validated; performance values remain measurements, not universal promises")
