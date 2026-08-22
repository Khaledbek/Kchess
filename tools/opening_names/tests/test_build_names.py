from __future__ import annotations

import importlib.util
import sys
import tempfile
import unittest
from pathlib import Path

import chess


MODULE_PATH = Path(__file__).parents[1] / "build_names.py"
SPEC = importlib.util.spec_from_file_location("build_names", MODULE_PATH)
assert SPEC and SPEC.loader
names = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = names
SPEC.loader.exec_module(names)


ROWS = (
    ("A00", "Anderssen's Opening", "1. a3"),
    ("C20", "King's Pawn Game", "1. e4 e5"),
    ("C60", "Ruy Lopez", "1. e4 e5 2. Nf3 Nc6 3. Bb5"),
    ("C65", "Ruy Lopez: Berlin Defense", "1. e4 e5 2. Nf3 Nc6 3. Bb5 Nf6"),
    ("D06", "Queen's Gambit", "1. d4 d5 2. c4"),
    # Two named lines that transpose to the same final position (collision).
    ("A04", "Transpose One", "1. Nf3 Nf6 2. g3 g6"),
    ("A04", "Transpose Two", "1. g3 g6 2. Nf3 Nf6"),
)


def key_after(*ucis: str) -> int:
    board = chess.Board()
    for uci in ucis:
        board.push_uci(uci)
    return names.stockfish_position_key(board)


def write_tsv(directory: Path, rows=ROWS) -> Path:
    path = directory / "fixture.tsv"
    lines = ["eco\tname\tpgn"] + ["\t".join(row) for row in rows]
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return path


def build_fixture(directory: Path, output_name: str = "fixture.kco", timestamp: int = 1_800_000_000):
    tsv = write_tsv(directory)
    output = directory / output_name
    stats = names.build(names.argparse.Namespace(
        input=[tsv], input_dir=None, output=output,
        source="lichess", license="CC0-1.0", timestamp=timestamp,
    ))
    return output, stats


class OpeningNamesBuilderTests(unittest.TestCase):
    def test_roundtrip_and_named_line_lookup(self):
        with tempfile.TemporaryDirectory() as temporary:
            output, stats = build_fixture(Path(temporary))
            metadata, entries = names.read_index(output)
            # Seven rows, one transposition pair merged -> six unique entries.
            self.assertEqual(7, stats["named_lines"])
            self.assertEqual(6, stats["entries"])
            self.assertEqual(1, stats["collisions"])
            self.assertEqual(6, metadata.entry_count)
            self.assertEqual("lichess", metadata.source)
            self.assertEqual("CC0-1.0", metadata.license_name)

            by_key = {entry.position_key: entry for entry in entries}
            berlin = by_key[key_after("e2e4", "e7e5", "g1f3", "b8c6", "f1b5", "g8f6")]
            self.assertEqual(("C65", "Ruy Lopez: Berlin Defense", 6), (berlin.eco, berlin.name, berlin.ply))

    def test_prefixes_are_distinct_entries_for_deepest_match(self):
        # King's Pawn Game, Ruy Lopez and its Berlin Defense are separate keys,
        # so the runtime walk can keep the deepest ply a game reaches.
        with tempfile.TemporaryDirectory() as temporary:
            output, _ = build_fixture(Path(temporary))
            _, entries = names.read_index(output)
            keys = {entry.position_key for entry in entries}
            self.assertIn(key_after("e2e4", "e7e5"), keys)
            self.assertIn(key_after("e2e4", "e7e5", "g1f3", "b8c6", "f1b5"), keys)
            self.assertIn(key_after("e2e4", "e7e5", "g1f3", "b8c6", "f1b5", "g8f6"), keys)

    def test_transposition_collision_is_merged_deterministically(self):
        with tempfile.TemporaryDirectory() as temporary:
            output, _ = build_fixture(Path(temporary))
            _, entries = names.read_index(output)
            collision_key = key_after("g1f3", "g8f6", "g2g3", "g7g6")
            self.assertEqual(collision_key, key_after("g2g3", "g7g6", "g1f3", "g8f6"))
            matched = [entry for entry in entries if entry.position_key == collision_key]
            self.assertEqual(1, len(matched))
            # Equal ply and ECO -> smaller name wins.
            self.assertEqual("Transpose One", matched[0].name)

    def test_strictly_sorted_unique_keys(self):
        with tempfile.TemporaryDirectory() as temporary:
            output, _ = build_fixture(Path(temporary))
            _, entries = names.read_index(output)
            keys = [entry.position_key for entry in entries]
            self.assertEqual(keys, sorted(keys))
            self.assertEqual(len(keys), len(set(keys)))

    def test_deterministic_output_bytes(self):
        with tempfile.TemporaryDirectory() as first, tempfile.TemporaryDirectory() as second:
            a, _ = build_fixture(Path(first))
            b, _ = build_fixture(Path(second))
            self.assertEqual(a.read_bytes(), b.read_bytes())

    def test_reader_rejects_magic_version_and_truncation(self):
        with tempfile.TemporaryDirectory() as temporary:
            output, _ = build_fixture(Path(temporary))
            original = output.read_bytes()
            for label, mutation in (
                ("magic", b"BAD!" + original[4:]),
                ("version", original[:4] + b"\x02\x00" + original[6:]),
                ("truncated", original[:-1]),
            ):
                damaged = Path(temporary) / f"{label}.kco"
                damaged.write_bytes(mutation)
                with self.assertRaises(ValueError):
                    names.read_index(damaged)


if __name__ == "__main__":
    unittest.main()
