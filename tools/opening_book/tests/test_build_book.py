from __future__ import annotations

import argparse
import io
import importlib.util
import sys
import tempfile
import unittest
from pathlib import Path

import chess


MODULE_PATH = Path(__file__).parents[1] / "build_book.py"
SPEC = importlib.util.spec_from_file_location("build_book", MODULE_PATH)
assert SPEC and SPEC.loader
book = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = book
SPEC.loader.exec_module(book)


class OpeningBookBuilderTests(unittest.TestCase):
    def _args(
        self,
        output: Path,
        *,
        inputs: list[Path] | None = None,
        input_dir: Path | None = None,
        max_ply: int = 6,
        min_games: int = 2,
    ):
        return argparse.Namespace(
            input=inputs or [Path(__file__).with_name("fixture.pgn")],
            input_dir=input_dir,
            output=output,
            max_ply=max_ply,
            min_games=min_games,
            min_rating=None,
            max_rating=None,
            min_speed=None,
            include_speeds=None,
            exclude_speeds=None,
            max_games=None,
            temp_directory=None,
            sqlite_cache_mb=128,
            source="lichess",
            license="CC0-1.0",
            source_dump="phase3-test-fixture.pgn",
            timestamp=1_777_777_777,
            progress_games=0,
        )

    def test_builder_filters_counts_transpositions_and_multiple_moves(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "fixture.kcb"
            stats = book.build(self._args(output))
            metadata, entries = book.read_book(output)
            self.assertEqual(6, stats["games_processed"])
            self.assertEqual(6, metadata.max_ply)
            self.assertEqual(2, metadata.min_games)
            start = chess.Board()
            start_entries = [e for e in entries if e.position_key == book.stockfish_position_key(start)]
            self.assertEqual({"e2e4"}, {book.decode_move(e.move).uci() for e in start_entries})
            e4 = next(e for e in start_entries if book.decode_move(e.move).uci() == "e2e4")
            self.assertEqual((4, 1, 2, 1), (e4.games, e4.white_wins, e4.draws, e4.black_wins))

            transposed_a = chess.Board()
            for uci in ("g1f3", "d7d5", "g2g3", "g8f6"):
                transposed_a.push_uci(uci)
            transposed_b = chess.Board()
            for uci in ("g2g3", "g8f6", "g1f3", "d7d5"):
                transposed_b.push_uci(uci)
            self.assertEqual(book.stockfish_position_key(transposed_a), book.stockfish_position_key(transposed_b))

    def test_max_ply_and_min_games(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "short.kcb"
            stats = book.build(self._args(output, max_ply=1, min_games=1))
            metadata, entries = book.read_book(output)
            self.assertEqual(1, metadata.max_ply)
            self.assertEqual(3, len(entries))
            self.assertEqual(6, stats["positions_seen"])

    def test_key_includes_side_castling_and_relevant_en_passant(self):
        start = chess.Board()
        black = chess.Board(start.fen())
        black.turn = chess.BLACK
        no_castle = chess.Board(start.fen())
        no_castle.castling_rights = 0
        self.assertNotEqual(book.stockfish_position_key(start), book.stockfish_position_key(black))
        self.assertNotEqual(book.stockfish_position_key(start), book.stockfish_position_key(no_castle))
        ep = chess.Board("4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1")
        no_ep = chess.Board("4k3/8/8/3pP3/8/8/8/4K3 w - - 0 1")
        self.assertNotEqual(book.stockfish_position_key(ep), book.stockfish_position_key(no_ep))

    def test_incremental_visitor_keys_match_full_keys(self):
        samples = (
            '[Result "1-0"]\n\n1. e4 e5 2. Nf3 Nc6 3. Bb5 a6 4. Ba4 Nf6 5. O-O Be7 1-0',
            '[SetUp "1"]\n[FEN "4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1"]\n'
            '[Result "1-0"]\n\n1. exd6 Kd7 1-0',
            '[SetUp "1"]\n[FEN "4k3/P7/8/8/8/8/8/4K3 w - - 0 1"]\n'
            '[Result "1-0"]\n\n1. a8=Q+ 1-0',
        )
        for pgn in samples:
            args = self._args(Path("unused.kcb"), max_ply=20, min_games=1)
            parsed = book.chess.pgn.read_game(
                io.StringIO(pgn), Visitor=lambda: book.OpeningGameVisitor(args)
            )
            reference = book.chess.pgn.read_game(io.StringIO(pgn))
            self.assertIsNotNone(parsed)
            self.assertIsNotNone(reference)
            board = reference.board()
            expected = []
            for move in reference.mainline_moves():
                expected.append(book.stockfish_position_key(board) - (1 << 63))
                board.push(move)
            self.assertEqual(expected, [row[0] for row in parsed.rows])

    def test_reader_rejects_magic_version_and_truncation(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "fixture.kcb"
            book.build(self._args(output))
            original = output.read_bytes()
            for name, mutation in (
                ("magic", b"BAD!" + original[4:]),
                ("version", original[:4] + b"\x02\x00" + original[6:]),
                ("truncated", original[:-1]),
            ):
                damaged = Path(temporary) / f"{name}.kcb"
                damaged.write_bytes(mutation)
                with self.assertRaises(ValueError):
                    book.read_book(damaged)

    def test_multiple_inputs_aggregate_before_min_games(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            first = root / "first.pgn"
            second = root / "second.pgn"
            first.write_text(
                '[Result "1-0"]\n\n1. e4 e5 1-0\n', encoding="utf-8"
            )
            second.write_text(
                '[Result "1/2-1/2"]\n\n1. e4 c5 1/2-1/2\n\n'
                '[Result "0-1"]\n\n1. e4 e6 0-1\n',
                encoding="utf-8",
            )
            output = root / "aggregate.kcb"
            stats = book.build(
                self._args(
                    output,
                    inputs=[first, second],
                    max_ply=1,
                    min_games=3,
                )
            )
            metadata, entries = book.read_book(output)
            self.assertEqual(2, stats["files_processed"])
            self.assertEqual(3, stats["games_processed"])
            self.assertEqual(1, metadata.entry_count)
            self.assertEqual(1, stats["unique_moves"])
            self.assertEqual(0, stats["discarded_moves"])
            self.assertEqual(
                (3, 1, 1, 1),
                (
                    entries[0].games,
                    entries[0].white_wins,
                    entries[0].draws,
                    entries[0].black_wins,
                ),
            )

    def test_duplicate_input_is_rejected(self):
        fixture = Path(__file__).with_name("fixture.pgn")
        with tempfile.TemporaryDirectory() as temporary:
            args = self._args(
                Path(temporary) / "duplicate.kcb",
                inputs=[fixture, fixture],
            )
            with self.assertRaisesRegex(ValueError, "duplicate input"):
                book.build(args)

    def test_input_dir_is_sorted_and_output_is_deterministic(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            inputs = root / "inputs"
            inputs.mkdir()
            alpha = inputs / "a.pgn"
            zulu = inputs / "z.pgn"
            alpha.write_text('[Result "1-0"]\n\n1. d4 d5 1-0\n', encoding="utf-8")
            zulu.write_text('[Result "0-1"]\n\n1. e4 e5 0-1\n', encoding="utf-8")
            (inputs / "ignored.txt").write_text("not PGN", encoding="utf-8")

            directory_output = root / "directory.kcb"
            directory_args = self._args(
                directory_output,
                inputs=[],
                input_dir=inputs,
                max_ply=2,
                min_games=1,
            )
            directory_args.input = []
            explicit_output = root / "explicit.kcb"
            explicit_args = self._args(
                explicit_output,
                inputs=[zulu, alpha],
                max_ply=2,
                min_games=1,
            )
            book.build(directory_args)
            book.build(explicit_args)
            self.assertEqual(directory_output.read_bytes(), explicit_output.read_bytes())
            self.assertEqual([alpha.resolve(), zulu.resolve()], book.resolve_inputs(directory_args))


if __name__ == "__main__":
    unittest.main()
