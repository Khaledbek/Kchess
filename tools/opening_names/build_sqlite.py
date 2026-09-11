#!/usr/bin/env python3
"""Build the KChess openings SQLite database from Lichess chess-openings TSVs."""

import argparse
import os
import sqlite3
from pathlib import Path

SUPPORTED_INPUT_SUFFIXES = (".tsv",)

def create_schema(cursor):
    cursor.execute("""
        CREATE TABLE openings (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            eco TEXT NOT NULL,
            name TEXT NOT NULL,
            family TEXT NOT NULL,
            variation TEXT NOT NULL,
            pgn TEXT NOT NULL
        )
    """)
    cursor.execute("CREATE INDEX idx_openings_eco ON openings(eco)")
    cursor.execute("CREATE INDEX idx_openings_family ON openings(family)")

def process_file(path: Path, cursor):
    with path.open("r", encoding="utf-8") as handle:
        header = handle.readline().rstrip("\n").split("\t")
        if header[:3] != ["eco", "name", "pgn"]:
            raise ValueError(f"{path.name}: expected 'eco\\tname\\tpgn' header")
        for line_number, raw in enumerate(handle, start=2):
            line = raw.rstrip("\n")
            if not line:
                continue
            fields = line.split("\t")
            if len(fields) < 3:
                raise ValueError(f"{path.name}:{line_number}: fewer than three columns")
            eco, name, movetext = fields[0], fields[1], fields[2]
            
            # Extract family and variation
            parts = name.split(":", 1)
            family = parts[0].strip()
            variation = parts[1].strip() if len(parts) > 1 else ""

            cursor.execute(
                "INSERT INTO openings (eco, name, family, variation, pgn) VALUES (?, ?, ?, ?, ?)",
                (eco, name, family, variation, movetext)
            )

def resolve_inputs(args: argparse.Namespace) -> list[Path]:
    inputs: list[Path] = [Path(path) for path in (args.input or [])]
    if args.input_dir is not None:
        directory = Path(args.input_dir).expanduser().resolve(strict=True)
        inputs.extend(sorted(
            (p for p in directory.iterdir() if p.is_file() and p.suffix.lower() in SUPPORTED_INPUT_SUFFIXES),
            key=lambda p: (p.name.casefold(), p.name),
        ))
    if not inputs:
        raise ValueError("at least one --input or --input-dir is required")
    resolved: list[Path] = []
    seen: set[str] = set()
    for candidate in inputs:
        path = candidate.expanduser().resolve(strict=True)
        if path.suffix.lower() not in SUPPORTED_INPUT_SUFFIXES:
            raise ValueError(f"unsupported opening-name input: {path}")
        identity = os.path.normcase(str(path))
        if identity in seen:
            raise ValueError(f"duplicate input path: {path}")
        seen.add(identity)
        resolved.append(path)
    return resolved

def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", action="append", type=Path, help="Repeat for every .tsv")
    parser.add_argument("--input-dir", type=Path, help="Add every .tsv in this directory")
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args(argv)

    inputs = resolve_inputs(args)
    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    if output_path.exists():
        output_path.unlink()

    conn = sqlite3.connect(output_path)
    try:
        cursor = conn.cursor()
        create_schema(cursor)
        for path in inputs:
            process_file(path, cursor)
        conn.commit()
        
        # Count rows
        cursor.execute("SELECT COUNT(*) FROM openings")
        count = cursor.fetchone()[0]
        print(f"Generated {output_path} with {count} openings.")
    finally:
        conn.close()
    
    return 0

if __name__ == "__main__":
    import sys
    sys.exit(main())
