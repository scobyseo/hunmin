#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Shared helpers for standalone tools under tools/.

Issue #272 Phase 2: reduce duplicated ROOT/runner patterns while keeping each
tool runnable as `python tools/<tool>.py` (sibling imports). Registered
filenames and module-level anchors required by tests/CMake stay in place.

This module is additive; tools migrate here incrementally.
"""
from __future__ import annotations

import pathlib
import subprocess
from typing import Sequence

ROOT = pathlib.Path(__file__).resolve().parents[1]
DEFAULT_DATA = ROOT / "libhunmin" / "data"
DEFAULT_SIM = ROOT / "build" / "hunmin-sim"
UTF8 = "utf-8"


def sim_path(explicit: str | pathlib.Path | None = None) -> pathlib.Path:
    """Resolve hunmin-sim path: explicit arg, else in-tree build default."""
    return pathlib.Path(explicit) if explicit else DEFAULT_SIM


def run_sim(args: Sequence[str], *, sim: str | pathlib.Path | None = None,
            check: bool = True, text: bool = True, **kwargs) -> subprocess.CompletedProcess:
    """Run hunmin-sim with common defaults."""
    cmd = [str(sim_path(sim)), *[str(a) for a in args]]
    return subprocess.run(cmd, check=check, text=text, capture_output=True, **kwargs)


def write_lf(path: pathlib.Path, text: str) -> None:
    """Write UTF-8 with LF on every host (see tools/_hostcompat.py)."""
    path.write_bytes(text.replace("\r\n", "\n").encode(UTF8))
