"""Use Apple's Clang Static Analyzer when clang-tidy is unavailable on macOS."""

import json
from pathlib import Path
import shlex
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
entries = json.loads(Path(sys.argv[1]).read_text())
count = 0
for entry in entries:
    source = Path(entry["file"]).resolve()
    if not source.is_relative_to(root / "src"):
        continue
    arguments = entry.get("arguments") or shlex.split(entry["command"])
    filtered = []
    skip = False
    for argument in arguments:
        if skip:
            skip = False
        elif argument == "-o":
            skip = True
        elif argument != "-c":
            filtered.append(argument)
    filtered += ["--analyze", "-Xanalyzer", "-analyzer-output=text", "-o", "/dev/null"]
    subprocess.run(filtered, cwd=entry["directory"], check=True)
    count += 1
if not count:
    raise SystemExit("No application compilation entries found")
print(f"Apple Clang Static Analyzer: {count} application translation units checked.")
