#!/usr/bin/env python3
"""Host regressions: control math + mock HAL output/state invariants.

No attached STM32, electrical switching waveform, or real-time timing is tested.
"""
from pathlib import Path
import os
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "build" / "host"
COMMON_CASES = [
    "init", "apb_prescaler", "guard", "adc_error", "adc_timeout", "emergency",
    "primask", "bad_mode", "bad_rank", "bad_differential", "bad_clock",
    "bad_frequency", "adc_start_failure", "timer_start_failure",
]


def run(args):
    subprocess.run([str(a) for a in args], cwd=ROOT, check=True)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    cc = os.environ.get("HOST_CC", "gcc")
    flags = [cc, "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror", "-Wpedantic"]
    # App tests replace main.h with mock HAL types. Import the hardware value
    # from the real header instead of maintaining another definition in mocks.
    header = (ROOT / "Core/Inc/main.h").read_text()
    definitions = re.findall(r"^\s*#define\s+HB_DEADTIME_TICKS\s+(.+)$", header, re.MULTILINE)
    if len(definitions) != 1:
        raise RuntimeError("Expected one HB_DEADTIME_TICKS definition in Core/Inc/main.h")
    deadtime = re.sub(r"/\*.*?\*/|//.*", "", definitions[0]).strip()
    core_test = OUT / "test_control"
    run(flags + ["-ICore/Inc", "Core/Src/hb_control.c", "tests/test_control.c",
                 "-lm", "-o", core_test])
    run([core_test])
    count = 0
    for verified in (0, 1):
        target = OUT / f"test_app_{verified}"
        run(flags + [f"-DHB_HARDWARE_VERIFIED={verified}",
                     f"-DHB_DEADTIME_TICKS={deadtime}", "-Itests/fakes", "-ICore/Inc",
                     "Core/Src/hb_app.c", "Core/Src/hb_control.c", "tests/test_app.c",
                     "-o", target])
        cases = COMMON_CASES + (["run_stop"] if verified else [])
        for case in cases:
            run([target, case])
            count += 1
    print(f"PASS: control regressions and {count} mock HAL scenarios", flush=True)


if __name__ == "__main__":
    main()
