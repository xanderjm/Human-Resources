#!/usr/bin/env python3
"""Compatibility entry point; tools/project.py owns all orchestration."""
from project import main

if __name__ == "__main__":
    raise SystemExit(main())
