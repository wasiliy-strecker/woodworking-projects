#!/usr/bin/env python3
"""Copy this app's canonical English policy and URL into a new delivery folder."""

import argparse
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[1]


def privacy_url():
    return (ROOT / "store-listings/submission/privacy-url.txt").read_text(encoding="utf-8").strip()


def export_privacy(destination):
    destination = Path(destination)
    policy = ROOT / "PRIVACY.md"
    url = ROOT / "store-listings/submission/privacy-url.txt"
    # Validate inputs before creating the destination. Never replace a prior handoff.
    policy.read_text(encoding="utf-8")
    url.read_text(encoding="utf-8")
    destination.mkdir(exist_ok=False)
    shutil.copyfile(policy, destination / "PRIVACY.md")
    shutil.copyfile(url, destination / "privacy-url.txt")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()
    export_privacy(args.destination)
    print(args.destination.resolve())


if __name__ == "__main__":
    main()
