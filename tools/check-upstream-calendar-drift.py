#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Shannon Smith

"""Report relevant Cinnamon calendar changes after the reviewed baseline.

This is deliberately a monitoring tool, not a source dependency. Calendar Plus
never downloads or incorporates Cinnamon code during its ordinary build. A
scheduled CI job compares upstream history and fails only when files in the
reviewed calendar integration surface changed, forcing a human compatibility
review before the baseline is advanced.
"""

from __future__ import annotations

import json
import os
import sys
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
BASELINE = ROOT / "tools/upstream-calendar-baseline.json"


def api_json(url: str) -> object:
    headers = {
        "Accept": "application/vnd.github+json",
        "User-Agent": "calendar-plus-upstream-drift",
        "X-GitHub-Api-Version": "2022-11-28",
    }
    token = os.environ.get("GITHUB_TOKEN", "").strip()
    if token:
        headers["Authorization"] = f"Bearer {token}"
    request = urllib.request.Request(url, headers=headers)
    try:
        with urllib.request.urlopen(request, timeout=30) as response:
            return json.load(response)
    except (urllib.error.HTTPError, urllib.error.URLError) as error:
        raise RuntimeError(f"GitHub API request failed: {error}") from error


def main() -> None:
    config = json.loads(BASELINE.read_text(encoding="utf-8"))
    repository = config["repository"]
    reference = config["ref"]
    baseline = config["commit"]
    watched = tuple(config["paths"])

    branch_url = (
        f"https://api.github.com/repos/{repository}/branches/"
        f"{urllib.parse.quote(reference, safe='')}"
    )
    branch = api_json(branch_url)
    assert isinstance(branch, dict)
    latest = branch["commit"]["sha"]
    if latest == baseline:
        print(f"Cinnamon calendar baseline is current at {baseline}.")
        return

    compare_url = (
        f"https://api.github.com/repos/{repository}/compare/"
        f"{baseline}...{latest}?per_page=100"
    )
    comparison = api_json(compare_url)
    assert isinstance(comparison, dict)
    files = comparison.get("files", [])
    changed = sorted(
        entry["filename"]
        for entry in files
        if any(
            entry["filename"] == path or entry["filename"].startswith(path)
            for path in watched
        )
    )

    if not changed:
        print(
            "Cinnamon advanced from "
            f"{baseline} to {latest}, with no watched calendar changes."
        )
        return

    print(
        "Relevant upstream Cinnamon calendar drift detected. "
        "Review these files before advancing the baseline:",
        file=sys.stderr,
    )
    for path in changed:
        print(f"  {path}", file=sys.stderr)
    print(f"Reviewed baseline: {baseline}", file=sys.stderr)
    print(f"Current upstream:  {latest}", file=sys.stderr)
    raise SystemExit(1)


if __name__ == "__main__":
    main()
