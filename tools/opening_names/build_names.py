#!/usr/bin/env python3
"""Build the KChess opening-*name* index (KCO1) from Lichess chess-openings TSVs.

Input is the CC0 `lichess-org/chess-openings` catalog: tab-separated `eco`,
`name`, `pgn` rows, one per named opening/variation. Each row's movetext is
replayed to its final position, keyed with the exact Stockfish-18 key used by
the statistical opening book, and written as a compact sorted binary the C++
runtime memory-loads to label stored games with an opening name.

The Stockfish key function is imported from `../opening_book/build_book.py` so
KCO position keys are byte-identical to the shipped `KCB` book and to the native
reader; there is intentionally a single key implementation.
"""

from __future__ import annotations

import argparse
import importlib.util
import os
import struct
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Iterator

import chess


FORMAT_VERSION = 1
BUILDER_VERSION = "kco-builder-1"
MAGIC = b"KCO1"
HEADER = struct.Struct("<4sHHHHQQIq16s16s16s8s")
ENTRY = struct.Struct("<QI4sHH")
SUPPORTED_INPUT_SUFFIXES = (".tsv",)


def _load_book_module():
    """Import build_book.py so KCO reuses its cross-checked Stockfish key."""
    module_path = Path(__file__).resolve().parents[1] / "opening_book" / "build_book.py"
    spec = importlib.util.spec_from_file_location("kchess_build_book", module_path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot import opening-book key module at {module_path}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


_BOOK = _load_book_module()
stockfish_position_key = _BOOK.stockfish_position_key
_fixed_text = _BOOK._fixed_text
_read_fixed_text = _BOOK._read_fixed_text


@dataclass(frozen=True)
class NameEntry:
    position_key: int
    eco: str
    name: str
    ply: int


@dataclass(frozen=True)
class NameMetadata:
    entry_count: int
    string_table_size: int
    max_ply: int
    build_timestamp: int
    source: str
    license_name: str
    builder_version: str


def parse_line_plies(movetext: str) -> tuple[int, chess.Board]:
    """Replay SAN movetext (with move numbers) and return (ply_count, board)."""
    board = chess.Board()
    plies = 0
    for token in movetext.split():
        # Skip move-number tokens such as "1." or a black-continuation "1...".
        stripped = token.rstrip(".")
        if stripped.isdigit():
            continue
        if not token:
            continue
        board.push_san(token)
        plies += 1
    if plies == 0:
        raise ValueError("named opening line has no moves")
    return plies, board


def read_openings(paths: Iterable[Path]) -> Iterator[NameEntry]:
    for path in paths:
        # Universal-newline mode so both LF and CRLF catalog files parse cleanly.
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
                if not eco or not name or not movetext.strip():
                    raise ValueError(f"{path.name}:{line_number}: empty eco/name/pgn")
                if len(eco.encode("utf-8")) > 4:
                    raise ValueError(f"{path.name}:{line_number}: ECO '{eco}' exceeds 4 bytes")
                try:
                    ply, board = parse_line_plies(movetext)
                except ValueError as error:
                    raise ValueError(f"{path.name}:{line_number}: {error}") from error
                yield NameEntry(stockfish_position_key(board), eco, name, ply)


def deduplicate(entries: Iterable[NameEntry]) -> list[NameEntry]:
    """Keep one entry per position key: deeper line wins, then smaller eco/name."""
    best: dict[int, NameEntry] = {}
    for entry in entries:
        current = best.get(entry.position_key)
        if current is None or entry.ply > current.ply or (
            entry.ply == current.ply and (entry.eco, entry.name) < (current.eco, current.name)
        ):
            best[entry.position_key] = entry
    return sorted(best.values(), key=lambda item: item.position_key)


def build_string_table(names: Iterable[str]) -> tuple[bytes, dict[str, int]]:
    blob = bytearray()
    offsets: dict[str, int] = {}
    for name in names:
        if name in offsets:
            continue
        offsets[name] = len(blob)
        blob.extend(name.encode("utf-8"))
        blob.append(0)
    return bytes(blob), offsets


def write_index(output_path: Path, entries: list[NameEntry], *, timestamp: int,
                source: str, license_name: str) -> int:
    output_path.parent.mkdir(parents=True, exist_ok=True)
    table, offsets = build_string_table(entry.name for entry in entries)
    max_ply = max((entry.ply for entry in entries), default=0)
    header = HEADER.pack(
        MAGIC, FORMAT_VERSION, HEADER.size, ENTRY.size, 0,
        len(entries), len(table), max_ply, timestamp,
        _fixed_text(source, 16, "source"),
        _fixed_text(license_name, 16, "license"),
        _fixed_text(BUILDER_VERSION, 16, "builder version"),
        bytes(8),
    )
    temporary_path = output_path.with_suffix(output_path.suffix + ".tmp")
    previous_key: int | None = None
    with temporary_path.open("wb") as output:
        output.write(header)
        for entry in entries:
            if previous_key is not None and entry.position_key <= previous_key:
                raise ValueError("index entries must be strictly sorted and unique")
            eco_bytes = entry.eco.encode("utf-8")
            if len(eco_bytes) >= ENTRY.size and len(eco_bytes) > 4:
                raise ValueError(f"ECO '{entry.eco}' does not fit four bytes")
            output.write(ENTRY.pack(
                entry.position_key, offsets[entry.name],
                eco_bytes + bytes(4 - len(eco_bytes)), entry.ply, 0,
            ))
            previous_key = entry.position_key
        output.write(table)
        output.flush()
        os.fsync(output.fileno())
    temporary_path.replace(output_path)
    return output_path.stat().st_size


def read_index(path: Path) -> tuple[NameMetadata, list[NameEntry]]:
    size = path.stat().st_size
    with path.open("rb") as source:
        raw_header = source.read(HEADER.size)
        if len(raw_header) != HEADER.size:
            raise ValueError("truncated KCO header")
        (magic, version, header_size, entry_size, flags, entry_count, table_size,
         max_ply, timestamp, source_name, license_name, builder, reserved) = HEADER.unpack(raw_header)
        if magic != MAGIC:
            raise ValueError("invalid KCO magic")
        if version != FORMAT_VERSION:
            raise ValueError("unsupported KCO version")
        if header_size != HEADER.size or entry_size != ENTRY.size or flags != 0 or any(reserved):
            raise ValueError("unsupported KCO layout")
        expected = HEADER.size + entry_count * ENTRY.size + table_size
        if expected != size:
            raise ValueError("truncated or trailing KCO data")
        raw_entries = source.read(entry_count * ENTRY.size)
        table = source.read(table_size)

    def name_at(offset: int) -> str:
        if offset >= len(table):
            raise ValueError("name offset outside string table")
        end = table.find(b"\0", offset)
        if end < 0:
            raise ValueError("name is not NUL terminated")
        return table[offset:end].decode("utf-8")

    entries: list[NameEntry] = []
    previous_key: int | None = None
    for index in range(entry_count):
        key, name_offset, eco_raw, ply, entry_reserved = ENTRY.unpack_from(raw_entries, index * ENTRY.size)
        if entry_reserved != 0:
            raise ValueError("KCO entry reserved bytes are nonzero")
        if previous_key is not None and key <= previous_key:
            raise ValueError("KCO entries are not strictly sorted")
        eco = eco_raw.split(b"\0", 1)[0].decode("utf-8")
        entries.append(NameEntry(key, eco, name_at(name_offset), ply))
        previous_key = key
    metadata = NameMetadata(
        entry_count, table_size, max_ply, timestamp,
        _read_fixed_text(source_name), _read_fixed_text(license_name), _read_fixed_text(builder),
    )
    return metadata, entries


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


def build(args: argparse.Namespace) -> dict[str, int]:
    inputs = resolve_inputs(args)
    raw_entries = list(read_openings(inputs))
    entries = deduplicate(raw_entries)
    timestamp = args.timestamp if args.timestamp is not None else int(time.time())
    output_size = write_index(
        Path(args.output), entries,
        timestamp=timestamp, source=args.source, license_name=args.license,
    )
    return {
        "input_files": len(inputs),
        "named_lines": len(raw_entries),
        "entries": len(entries),
        "collisions": len(raw_entries) - len(entries),
        "max_ply": max((entry.ply for entry in entries), default=0),
        "output_size": output_size,
    }


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(description=__doc__)
    result.add_argument("--input", action="append", type=Path, help="Repeat for every .tsv")
    result.add_argument("--input-dir", type=Path, help="Add every .tsv in this directory")
    result.add_argument("--output", required=True, type=Path)
    result.add_argument("--source", default="lichess")
    result.add_argument("--license", default="CC0-1.0")
    result.add_argument("--timestamp", type=int, help="Deterministic Unix build timestamp")
    return result


def main(argv: list[str] | None = None) -> int:
    args = parser().parse_args(argv)
    try:
        stats = build(args)
    except (OSError, ValueError) as error:
        raise SystemExit(str(error)) from error
    print(f"Input files: {stats['input_files']:,}")
    print(f"Named lines read: {stats['named_lines']:,}")
    print(f"Unique position entries: {stats['entries']:,}")
    print(f"Transposition collisions merged: {stats['collisions']:,}")
    print(f"Maximum named-line ply: {stats['max_ply']:,}")
    print(f"File size: {stats['output_size']:,} bytes")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
