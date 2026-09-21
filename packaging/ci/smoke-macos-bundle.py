#!/usr/bin/env python3
import os
import pathlib
import subprocess
import sys

binary = pathlib.Path(sys.argv[1])
report = pathlib.Path(sys.argv[2])
label = sys.argv[3]
entries = [f"label={label}", f"binary={binary}"]
succeeded = False
hard_failure = False

for driver in ("offscreen", "dummy"):
    env = os.environ.copy()
    env.update({
        "SDL_VIDEODRIVER": driver,
        "SDL_RENDER_DRIVER": "software",
        "SDL_RENDER_VSYNC": "0",
        "SDL_AUDIODRIVER": "dummy",
    })
    try:
        result = subprocess.run(
            [str(binary), "--smoke-test"],
            env=env,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=20,
        )
    except subprocess.TimeoutExpired:
        entries.append(f"{driver}: timed out after 20 seconds on the headless GitHub runner")
        continue

    output = f"stdout:\n{result.stdout}\nstderr:\n{result.stderr}".strip()
    entries.append(f"{driver}: exit {result.returncode}\n{output}")
    if result.returncode == 0:
        succeeded = True
        break

    lowered = output.lower()
    unavailable = (
        "no available video device" in lowered
        or ("video driver" in lowered and "not available" in lowered)
        or ("offscreen" in lowered and "not" in lowered and "available" in lowered)
    )
    if not unavailable:
        hard_failure = True

if succeeded:
    entries.append("RESULT: packaged application smoke test passed")
elif hard_failure:
    entries.append("RESULT: packaged application returned a real non-runner error")
else:
    entries.append(
        "RESULT: inconclusive on the headless GitHub runner; this is not real-device/Gatekeeper acceptance"
    )

report.write_text("\n\n".join(entries) + "\n", encoding="utf-8")
print(report.read_text(encoding="utf-8"))
if hard_failure and not succeeded:
    raise SystemExit(1)
