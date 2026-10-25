#!/usr/bin/env python3
"""Validate the renderer identity written by a bounded desktop smoke run."""

from __future__ import annotations

import argparse
from pathlib import Path
import re


REPORT_PATTERN = re.compile(
    r"foundation-smoke backend=(?P<backend>\S+) "
    r"device=(?P<device>.+) driver=(?P<driver>.+)"
)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--report", required=True, type=Path)
    parser.add_argument("--expected-backend", required=True)
    args = parser.parse_args()

    if not args.report.is_file():
        raise RuntimeError(f"smoke report is missing: {args.report}")
    if args.report.stat().st_size > 4096:
        raise RuntimeError("smoke report exceeds the bounded 4096-byte contract")
    text = args.report.read_text(encoding="utf-8")
    lines = text.splitlines()
    if len(lines) != 1:
        raise RuntimeError("smoke report must contain exactly one line")
    match = REPORT_PATTERN.fullmatch(lines[0])
    if match is None:
        raise RuntimeError("smoke report does not match the renderer evidence contract")
    backend = match.group("backend")
    device = match.group("device").strip()
    driver = match.group("driver").strip()
    if backend != args.expected_backend:
        raise RuntimeError(
            f"unexpected renderer backend: expected {args.expected_backend!r}, found {backend!r}"
        )
    if device in {"", "unknown", "unavailable"}:
        raise RuntimeError(f"renderer device identity is not usable: {device!r}")
    if not driver:
        raise RuntimeError("renderer driver identity is empty")
    print(f"renderer smoke report passed: backend={backend} device={device} driver={driver}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, UnicodeError) as error:
        print(f"renderer smoke report failed: {error}")
        raise SystemExit(1)
