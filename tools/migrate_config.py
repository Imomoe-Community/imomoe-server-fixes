#!/usr/bin/env python3
"""Migrate the legacy FunRounds hostage toggle into an explicit ServerFixes CFG."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import sys
import tempfile
import tomllib


LEGACY_KEY = "hostage_rescue_compat"
NEW_KEY = "sf_hostage_compat"


def migrated_value(source: Path) -> bool:
    try:
        with source.open("rb") as handle:
            config = tomllib.load(handle)
    except (OSError, tomllib.TOMLDecodeError) as exc:
        raise ValueError(f"cannot read legacy TOML: {exc}") from exc

    value = config.get(LEGACY_KEY, False)
    if type(value) is not bool:
        raise ValueError(f"{LEGACY_KEY} must be a TOML boolean when present")
    return value


def write_target(destination: Path, value: bool, overwrite: bool) -> None:
    if not destination.parent.is_dir():
        raise ValueError(f"output directory does not exist: {destination.parent}")

    content = f"// Migrated from FunRounds {LEGACY_KEY}; edit this value in ServerFixes.\n{NEW_KEY} {1 if value else 0}\n"
    if not overwrite:
        try:
            with destination.open("x", encoding="utf-8", newline="\n") as handle:
                handle.write(content)
                handle.flush()
                os.fsync(handle.fileno())
        except FileExistsError as exc:
            raise ValueError(f"output already exists (pass --overwrite to replace it): {destination}") from exc
        return

    temp_name: str | None = None
    try:
        with tempfile.NamedTemporaryFile(
            mode="w", encoding="utf-8", newline="\n", dir=destination.parent,
            prefix=f".{destination.name}.", suffix=".tmp", delete=False,
        ) as handle:
            temp_name = handle.name
            handle.write(content)
            handle.flush()
            os.fsync(handle.fileno())
        os.replace(temp_name, destination)
    except OSError:
        if temp_name is not None:
            try:
                os.unlink(temp_name)
            except OSError:
                pass
        raise


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, help="legacy FunRounds TOML config")
    parser.add_argument("--output", required=True, type=Path, help="explicit destination CFG path")
    parser.add_argument("--overwrite", action="store_true", help="replace the explicit destination file")
    args = parser.parse_args(argv)

    try:
        source = args.source.resolve(strict=True)
        destination = args.output.resolve(strict=False)
        if source == destination:
            raise ValueError("source and output must be different files")
        value = migrated_value(source)
        write_target(destination, value, args.overwrite)
    except (OSError, ValueError) as exc:
        print(f"migration failed: {exc}", file=sys.stderr)
        return 2

    print(f"wrote {destination}: {NEW_KEY} {1 if value else 0}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
