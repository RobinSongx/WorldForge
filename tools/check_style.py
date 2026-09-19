"""Check all project-owned C++ against the repository's Google style policy."""

import argparse
import importlib.metadata
from pathlib import Path
import re
import subprocess
import sys
import sysconfig

import cpplint


ROOT = Path(__file__).resolve().parents[1]
VERSIONS = {"clang-format": "21.1.8", "cpplint": "2.0.2"}
SOURCE_ROOTS = ("apps", "src", "tests")


def header_guard(filename):
    """Keep PROJECT_PATH_H_ guards independent of the checkout folder name."""
    relative = Path(filename).resolve().relative_to(ROOT).as_posix()
    return "WORLDFORGE_" + re.sub(r"[^a-zA-Z0-9]", "_", relative).upper() + "_"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fix", action="store_true", help="Apply clang-format first")
    args = parser.parse_args()

    for package, expected in VERSIONS.items():
        if importlib.metadata.version(package) != expected:
            parser.error(f"Install pinned versions: pip install -r tools/style-requirements.txt")

    files = sorted(
        path
        for directory in SOURCE_ROOTS
        for path in (ROOT / directory).rglob("*")
        if path.is_file() and path.suffix in {".cc", ".h", ".cpp", ".hpp", ".cxx"}
    )
    if not files:
        parser.error("No project C++ files found")
    for path in files:
        if not re.fullmatch(r"[a-z][a-z0-9_]*\.(cc|h)", path.name):
            parser.error(f"Use lowercase snake_case .cc/.h filenames: {path}")

    executable = Path(sysconfig.get_path("scripts")) / (
        "clang-format.exe" if sys.platform == "win32" else "clang-format"
    )
    if args.fix:
        subprocess.run([str(executable), "-i", *map(str, files)], check=True)
    subprocess.run(
        [str(executable), "--dry-run", "--Werror", *map(str, files)], check=True
    )

    # cpplint 2.0.2 normally derives the guard prefix from the checkout path.
    # Only replace that calculation; keep its guard checks and all rules enabled.
    cpplint.GetHeaderGuardCPPVariable = header_guard
    sys.argv = ["cpplint", *map(str, files)]
    cpplint.main()


if __name__ == "__main__":
    main()
