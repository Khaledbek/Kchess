#!/usr/bin/env python3
"""Stream PGN games into the versioned KChess opening-book format."""

from __future__ import annotations

import argparse
import hashlib
import io
import os
import sqlite3
import struct
import sys
import tempfile
import time
from contextlib import contextmanager
from dataclasses import dataclass
from pathlib import Path
from typing import BinaryIO, Iterator, TextIO

import chess
import chess.pgn


FORMAT_VERSION = 1
BUILDER_VERSION = "kcb-builder-2"
MAGIC = b"KCB1"
HEADER = struct.Struct("<4sHHHHQIIq16s16s64s16s12s")
ENTRY = struct.Struct("<QHHIIII")
SPEED_ORDER = {
    "ultrabullet": 0,
    "bullet": 1,
    "blitz": 2,
    "rapid": 3,
    "classical": 4,
    "correspondence": 5,
}
MASK_64 = (1 << 64) - 1
STOCKFISH_PIECES = (1, 2, 3, 4, 5, 6, 9, 10, 11, 12, 13, 14)
SUPPORTED_INPUT_SUFFIXES = (".pgn", ".pgn.zst")


@dataclass(frozen=True)
class BookEntry:
    position_key: int
    move: int
    games: int
    white_wins: int
    draws: int
    black_wins: int


@dataclass(frozen=True)
class BookMetadata:
    entry_count: int
    max_ply: int
    min_games: int
    build_timestamp: int
    source: str
    license_name: str
    source_dump: str
    builder_version: str


@dataclass
class ParsedOpeningGame:
    accepted: bool
    invalid: bool
    rows: list[tuple[int, int, int, int, int, int]]


class StockfishPrng:
    """The deterministic xorshift64* PRNG used by Stockfish 18."""

    def __init__(self, seed: int) -> None:
        if seed == 0:
            raise ValueError("seed must be non-zero")
        self.state = seed & MASK_64

    def rand64(self) -> int:
        value = self.state
        value ^= value >> 12
        value ^= (value << 25) & MASK_64
        value ^= value >> 27
        self.state = value & MASK_64
        return (self.state * 2685821657736338717) & MASK_64


def _zobrist_values() -> tuple[dict[int, list[int]], list[int], list[int], int]:
    rng = StockfishPrng(1070372)
    pieces: dict[int, list[int]] = {}
    for piece in STOCKFISH_PIECES:
        pieces[piece] = [rng.rand64() for _ in range(64)]
    for square in range(chess.A8, chess.H8 + 1):
        pieces[1][square] = 0
    for square in range(chess.A1, chess.H1 + 1):
        pieces[9][square] = 0
    en_passant = [rng.rand64() for _ in range(8)]
    castling = [rng.rand64() for _ in range(16)]
    side = rng.rand64()
    rng.rand64()  # Zobrist::noPawns, intentionally not part of Position::key().
    return pieces, en_passant, castling, side


ZOBRIST_PIECES, ZOBRIST_EP, ZOBRIST_CASTLING, ZOBRIST_SIDE = _zobrist_values()


def _stockfish_ep_file(board: chess.Board) -> int | None:
    square = board.ep_square
    if square is None:
        return None
    expected_rank = 5 if board.turn == chess.WHITE else 2
    if chess.square_rank(square) != expected_rank:
        return None
    pawns = board.attackers(board.turn, square) & board.pawns
    if not pawns:
        return None
    enemy_pawn_square = square - 8 if board.turn == chess.WHITE else square + 8
    behind_square = square + 8 if board.turn == chess.WHITE else square - 8
    if not (0 <= enemy_pawn_square < 64 and 0 <= behind_square < 64):
        return None
    enemy = board.piece_at(enemy_pawn_square)
    if enemy is None or enemy.piece_type != chess.PAWN or enemy.color == board.turn:
        return None
    if board.piece_at(square) is not None or board.piece_at(behind_square) is not None:
        return None
    return chess.square_file(square)


def stockfish_position_key(board: chess.Board) -> int:
    """Return Stockfish 18's clock-independent Position Zobrist key."""
    key = 0
    for square, piece in board.piece_map().items():
        stockfish_piece = piece.piece_type + (0 if piece.color else 8)
        key ^= ZOBRIST_PIECES[stockfish_piece][square]
    ep_file = _stockfish_ep_file(board)
    if ep_file is not None:
        key ^= ZOBRIST_EP[ep_file]
    if board.turn == chess.BLACK:
        key ^= ZOBRIST_SIDE
    rights = _castling_index(board)
    return (key ^ ZOBRIST_CASTLING[rights]) & MASK_64


def _castling_index(board: chess.Board) -> int:
    rights = 0
    if board.has_kingside_castling_rights(chess.WHITE):
        rights |= 1
    if board.has_queenside_castling_rights(chess.WHITE):
        rights |= 2
    if board.has_kingside_castling_rights(chess.BLACK):
        rights |= 4
    if board.has_queenside_castling_rights(chess.BLACK):
        rights |= 8
    return rights


def _piece_square_key(board: chess.Board, square: chess.Square) -> int:
    piece = board.piece_at(square)
    if piece is None:
        return 0
    stockfish_piece = piece.piece_type + (0 if piece.color else 8)
    return ZOBRIST_PIECES[stockfish_piece][square]


def _move_changed_squares(board: chess.Board, move: chess.Move) -> tuple[chess.Square, ...]:
    if not move:
        return ()
    changed = {move.from_square, move.to_square}
    if board.is_en_passant(move):
        changed.add(move.to_square - 8 if board.turn == chess.WHITE else move.to_square + 8)
    if board.is_castling(move):
        rank = 0 if board.turn == chess.WHITE else 7
        if board.is_kingside_castling(move):
            changed.update((chess.square(7, rank), chess.square(5, rank)))
        else:
            changed.update((chess.square(0, rank), chess.square(3, rank)))
    return tuple(sorted(changed))


def encode_move(move: chess.Move) -> int:
    promotion = {
        None: 0,
        chess.KNIGHT: 1,
        chess.BISHOP: 2,
        chess.ROOK: 3,
        chess.QUEEN: 4,
    }.get(move.promotion)
    if promotion is None:
        raise ValueError("unsupported promotion piece")
    return move.from_square | (move.to_square << 6) | (promotion << 12)


def decode_move(value: int) -> chess.Move:
    if value & 0x8000:
        raise ValueError("reserved move bit is set")
    promotion_code = (value >> 12) & 7
    promotions = {0: None, 1: chess.KNIGHT, 2: chess.BISHOP, 3: chess.ROOK, 4: chess.QUEEN}
    if promotion_code not in promotions:
        raise ValueError("invalid promotion code")
    return chess.Move(value & 63, (value >> 6) & 63, promotion=promotions[promotion_code])


def _fixed_text(value: str, size: int, field: str) -> bytes:
    encoded = value.encode("utf-8")
    if len(encoded) >= size:
        raise ValueError(f"{field} must be shorter than {size} UTF-8 bytes")
    return encoded + bytes(size - len(encoded))


def _read_fixed_text(value: bytes) -> str:
    try:
        return value.split(b"\0", 1)[0].decode("utf-8")
    except UnicodeDecodeError as error:
        raise ValueError("invalid UTF-8 metadata") from error


def write_book(
    output_path: Path,
    entries: Iterator[BookEntry],
    metadata: BookMetadata,
) -> int:
    output_path.parent.mkdir(parents=True, exist_ok=True)
    temporary_path = output_path.with_suffix(output_path.suffix + ".tmp")
    header = HEADER.pack(
        MAGIC,
        FORMAT_VERSION,
        HEADER.size,
        ENTRY.size,
        0,
        metadata.entry_count,
        metadata.max_ply,
        metadata.min_games,
        metadata.build_timestamp,
        _fixed_text(metadata.source, 16, "source"),
        _fixed_text(metadata.license_name, 16, "license"),
        _fixed_text(metadata.source_dump, 64, "source dump"),
        _fixed_text(metadata.builder_version, 16, "builder version"),
        bytes(12),
    )
    count = 0
    previous: tuple[int, int] | None = None
    with temporary_path.open("wb") as output:
        output.write(header)
        for entry in entries:
            identity = (entry.position_key, entry.move)
            if previous is not None and identity <= previous:
                raise ValueError("book entries must be strictly sorted")
            if entry.games != entry.white_wins + entry.draws + entry.black_wins:
                raise ValueError("result counts do not add up to games")
            decode_move(entry.move)
            output.write(
                ENTRY.pack(
                    entry.position_key,
                    entry.move,
                    0,
                    entry.games,
                    entry.white_wins,
                    entry.draws,
                    entry.black_wins,
                )
            )
            previous = identity
            count += 1
        output.flush()
        os.fsync(output.fileno())
    if count != metadata.entry_count:
        temporary_path.unlink(missing_ok=True)
        raise ValueError("entry count differs from header")
    temporary_path.replace(output_path)
    return output_path.stat().st_size


def read_book(path: Path) -> tuple[BookMetadata, list[BookEntry]]:
    size = path.stat().st_size
    with path.open("rb") as source:
        raw_header = source.read(HEADER.size)
        if len(raw_header) != HEADER.size:
            raise ValueError("truncated KCB header")
        (
            magic,
            version,
            header_size,
            entry_size,
            flags,
            entry_count,
            max_ply,
            min_games,
            timestamp,
            source_name,
            license_name,
            source_dump,
            builder_version,
            reserved,
        ) = HEADER.unpack(raw_header)
        if magic != MAGIC:
            raise ValueError("invalid KCB magic")
        if version != FORMAT_VERSION:
            raise ValueError("unsupported KCB version")
        if header_size != HEADER.size or entry_size != ENTRY.size or flags != 0 or any(reserved):
            raise ValueError("unsupported KCB layout")
        expected_size = HEADER.size + entry_count * ENTRY.size
        if expected_size != size:
            raise ValueError("truncated or trailing KCB data")
        entries: list[BookEntry] = []
        previous: tuple[int, int] | None = None
        for _ in range(entry_count):
            values = ENTRY.unpack(source.read(ENTRY.size))
            key, move, entry_reserved, games, white_wins, draws, black_wins = values
            identity = (key, move)
            if entry_reserved != 0 or (previous is not None and identity <= previous):
                raise ValueError("invalid KCB entry ordering or reserved data")
            if games != white_wins + draws + black_wins:
                raise ValueError("invalid KCB result counts")
            decode_move(move)
            entries.append(BookEntry(key, move, games, white_wins, draws, black_wins))
            previous = identity
    return (
        BookMetadata(
            entry_count,
            max_ply,
            min_games,
            timestamp,
            _read_fixed_text(source_name),
            _read_fixed_text(license_name),
            _read_fixed_text(source_dump),
            _read_fixed_text(builder_version),
        ),
        entries,
    )


@contextmanager
def open_pgn(path: Path) -> Iterator[TextIO]:
    raw: BinaryIO | None = None
    stream: BinaryIO | None = None
    text: TextIO | None = None
    try:
        raw = path.open("rb")
        if path.name.lower().endswith(".zst"):
            try:
                import zstandard
            except ImportError as error:
                raise RuntimeError(".zst input requires zstandard from requirements.txt") from error
            stream = zstandard.ZstdDecompressor().stream_reader(raw)
        else:
            stream = raw
        text = io.TextIOWrapper(stream, encoding="utf-8", errors="replace", newline=None)
        yield text
    finally:
        if text is not None:
            text.close()
        elif stream is not None and stream is not raw:
            stream.close()
        elif raw is not None:
            raw.close()


def _is_supported_input(path: Path) -> bool:
    name = path.name.lower()
    return any(name.endswith(suffix) for suffix in SUPPORTED_INPUT_SUFFIXES)


def resolve_inputs(args: argparse.Namespace) -> list[Path]:
    """Resolve CLI inputs once, reject path duplicates and keep directory order stable."""
    explicit = getattr(args, "input", None)
    if explicit is None:
        inputs: list[Path] = []
    elif isinstance(explicit, (str, Path)):
        inputs = [Path(explicit)]
    else:
        inputs = [Path(path) for path in explicit]
    input_directory = getattr(args, "input_dir", None)
    if input_directory is not None:
        directory = Path(input_directory).expanduser().resolve(strict=True)
        if not directory.is_dir():
            raise ValueError(f"input directory is not a directory: {directory}")
        inputs.extend(
            sorted(
                (path for path in directory.iterdir() if path.is_file() and _is_supported_input(path)),
                key=lambda path: (path.name.casefold(), path.name),
            )
        )
    if not inputs:
        raise ValueError("at least one --input or --input-dir is required")
    resolved: list[Path] = []
    seen: set[str] = set()
    for input_path in inputs:
        path = input_path.expanduser().resolve(strict=True)
        if not path.is_file() or not _is_supported_input(path):
            raise ValueError(f"unsupported opening-book input: {path}")
        identity = os.path.normcase(str(path))
        if identity in seen:
            raise ValueError(f"duplicate input path: {path}")
        seen.add(identity)
        resolved.append(path)
    return resolved


def _source_dump_label(args: argparse.Namespace, inputs: list[Path]) -> str:
    if args.source_dump:
        return args.source_dump
    if len(inputs) == 1:
        return inputs[0].name
    names = "\n".join(path.name for path in inputs)
    digest = hashlib.sha256(names.encode("utf-8")).hexdigest()[:12]
    return f"multi-{len(inputs)}-{digest}"


def _peak_memory_bytes() -> int:
    """Return the process peak RSS where the host exposes it, otherwise zero."""
    try:
        if sys.platform == "win32":
            import ctypes
            from ctypes import wintypes

            class ProcessMemoryCounters(ctypes.Structure):
                _fields_ = [
                    ("cb", wintypes.DWORD),
                    ("PageFaultCount", wintypes.DWORD),
                    ("PeakWorkingSetSize", ctypes.c_size_t),
                    ("WorkingSetSize", ctypes.c_size_t),
                    ("QuotaPeakPagedPoolUsage", ctypes.c_size_t),
                    ("QuotaPagedPoolUsage", ctypes.c_size_t),
                    ("QuotaPeakNonPagedPoolUsage", ctypes.c_size_t),
                    ("QuotaNonPagedPoolUsage", ctypes.c_size_t),
                    ("PagefileUsage", ctypes.c_size_t),
                    ("PeakPagefileUsage", ctypes.c_size_t),
                ]

            counters = ProcessMemoryCounters()
            counters.cb = ctypes.sizeof(counters)
            kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
            psapi = ctypes.WinDLL("psapi", use_last_error=True)
            kernel32.GetCurrentProcess.restype = wintypes.HANDLE
            psapi.GetProcessMemoryInfo.argtypes = (
                wintypes.HANDLE,
                ctypes.POINTER(ProcessMemoryCounters),
                wintypes.DWORD,
            )
            psapi.GetProcessMemoryInfo.restype = wintypes.BOOL
            if psapi.GetProcessMemoryInfo(
                kernel32.GetCurrentProcess(), ctypes.byref(counters), counters.cb
            ):
                return int(counters.PeakWorkingSetSize)
            return 0
        import resource

        peak = int(resource.getrusage(resource.RUSAGE_SELF).ru_maxrss)
        return peak if sys.platform == "darwin" else peak * 1024
    except (AttributeError, ImportError, OSError, ValueError):
        return 0


def _game_speed(headers: chess.pgn.Headers) -> str | None:
    event = headers.get("Event", "").lower()
    for speed in SPEED_ORDER:
        if speed in event.replace(" ", ""):
            return speed
    return None


def _accepted_headers(headers: chess.pgn.Headers, args: argparse.Namespace) -> bool:
    if headers.get("Result") not in ("1-0", "0-1", "1/2-1/2"):
        return False
    if headers.get("Variant", "Standard").lower() not in ("standard", "chess"):
        return False
    ratings = []
    for name in ("WhiteElo", "BlackElo"):
        try:
            ratings.append(int(headers.get(name, "")))
        except ValueError:
            pass
    if args.min_rating is not None and (len(ratings) != 2 or min(ratings) < args.min_rating):
        return False
    speed = _game_speed(headers)
    if args.min_speed is not None and (speed is None or SPEED_ORDER[speed] < SPEED_ORDER[args.min_speed]):
        return False
    if args.include_speeds and speed not in args.include_speeds:
        return False
    if args.exclude_speeds and speed in args.exclude_speeds:
        return False
    return True


def _result_counts(result: str) -> tuple[int, int, int]:
    if result == "1-0":
        return 1, 0, 0
    if result == "0-1":
        return 0, 0, 1
    return 0, 1, 0


class OpeningGameVisitor(chess.pgn.BaseVisitor[ParsedOpeningGame]):
    """Collect only opening plies and skip game-tree allocation and all variations."""

    def __init__(self, args: argparse.Namespace) -> None:
        self.args = args
        self.headers = chess.pgn.Headers({})
        self.accepted = False
        self.invalid = False
        self.ply = 0
        self.rows: list[tuple[int, int, int, int, int, int]] = []
        self.result_counts = (0, 0, 0)
        self.position_key: int | None = None
        self.pending_key_update: tuple[int, int, int, tuple[chess.Square, ...]] | None = None

    def begin_headers(self) -> chess.pgn.Headers:
        return self.headers

    def visit_header(self, tagname: str, tagvalue: str) -> None:
        self.headers[tagname] = tagvalue

    def end_headers(self) -> chess.pgn.SkipType | None:
        self.accepted = _accepted_headers(self.headers, self.args)
        if not self.accepted:
            return chess.pgn.SKIP
        self.result_counts = _result_counts(self.headers["Result"])
        return None

    def begin_parse_san(self, board: chess.Board, san: str) -> chess.pgn.SkipType | None:
        del board, san
        return chess.pgn.SKIP if self.ply >= self.args.max_ply else None

    def visit_move(self, board: chess.Board, move: chess.Move) -> None:
        if self.position_key is None:
            self.position_key = stockfish_position_key(board)
        self.rows.append((
            self.position_key - (1 << 63),
            encode_move(move),
            1,
            self.result_counts[0],
            self.result_counts[1],
            self.result_counts[2],
        ))
        changed = _move_changed_squares(board, move)
        old_piece_keys = 0
        for square in changed:
            old_piece_keys ^= _piece_square_key(board, square)
        old_ep_file = _stockfish_ep_file(board)
        self.pending_key_update = (
            old_piece_keys,
            -1 if old_ep_file is None else old_ep_file,
            _castling_index(board),
            changed,
        )
        self.ply += 1

    def visit_board(self, board: chess.Board) -> None:
        if self.position_key is None:
            self.position_key = stockfish_position_key(board)
            return
        if self.pending_key_update is None:
            return
        old_piece_keys, old_ep_file, old_castling, changed = self.pending_key_update
        key = self.position_key ^ old_piece_keys ^ ZOBRIST_CASTLING[old_castling] ^ ZOBRIST_SIDE
        if old_ep_file >= 0:
            key ^= ZOBRIST_EP[old_ep_file]
        for square in changed:
            key ^= _piece_square_key(board, square)
        new_ep_file = _stockfish_ep_file(board)
        if new_ep_file is not None:
            key ^= ZOBRIST_EP[new_ep_file]
        key ^= ZOBRIST_CASTLING[_castling_index(board)]
        self.position_key = key & MASK_64
        self.pending_key_update = None

    def begin_variation(self) -> chess.pgn.SkipType:
        return chess.pgn.SKIP

    def handle_error(self, error: Exception) -> None:
        del error
        self.invalid = True

    def result(self) -> ParsedOpeningGame:
        return ParsedOpeningGame(self.accepted, self.invalid, self.rows)


def build(args: argparse.Namespace) -> dict[str, int | float]:
    started = time.monotonic()
    input_games = accepted_games = positions_seen = invalid_games = 0
    files_processed = 0
    peak_memory = _peak_memory_bytes()
    inputs = resolve_inputs(args)
    temporary_directory = Path(args.temp_directory) if args.temp_directory else None
    with tempfile.TemporaryDirectory(prefix="kchess-book-", dir=temporary_directory) as temporary:
        database_path = Path(temporary) / "counts.sqlite3"
        connection = sqlite3.connect(database_path)
        connection.execute("PRAGMA journal_mode=OFF")
        connection.execute("PRAGMA synchronous=OFF")
        connection.execute("PRAGMA temp_store=FILE")
        connection.execute(f"PRAGMA cache_size=-{args.sqlite_cache_mb * 1024}")
        connection.execute("PRAGMA locking_mode=EXCLUSIVE")
        connection.execute(
            "CREATE TABLE counts(position_key INTEGER NOT NULL, move INTEGER NOT NULL, "
            "games INTEGER NOT NULL, white_wins INTEGER NOT NULL, draws INTEGER NOT NULL, "
            "black_wins INTEGER NOT NULL, PRIMARY KEY(position_key, move)) WITHOUT ROWID"
        )
        upsert = (
            "INSERT INTO counts VALUES(?,?,?,?,?,?) ON CONFLICT(position_key,move) DO UPDATE SET "
            "games=games+1,white_wins=white_wins+excluded.white_wins,"
            "draws=draws+excluded.draws,black_wins=black_wins+excluded.black_wins"
        )
        pending_rows: list[tuple[int, int, int, int, int, int]] = []

        def flush_rows() -> None:
            if pending_rows:
                connection.executemany(upsert, pending_rows)
                pending_rows.clear()

        for file_index, input_path in enumerate(inputs, start=1):
            if args.max_games is not None and input_games >= args.max_games:
                break
            files_processed += 1
            file_games_before = input_games
            print(
                f"Processing dump {file_index} / {len(inputs)}: {input_path.name}",
                file=sys.stderr,
                flush=True,
            )
            with open_pgn(input_path) as pgn:
                while args.max_games is None or input_games < args.max_games:
                    try:
                        game = chess.pgn.read_game(
                            pgn, Visitor=lambda: OpeningGameVisitor(args)
                        )
                    except (ValueError, UnicodeError):
                        invalid_games += 1
                        continue
                    if game is None:
                        break
                    input_games += 1
                    if game.invalid or not game.accepted:
                        if game.invalid:
                            invalid_games += 1
                        continue
                    pending_rows.extend(game.rows)
                    if len(pending_rows) >= 25_000:
                        flush_rows()
                    positions_seen += len(game.rows)
                    accepted_games += 1
                    if args.progress_games and input_games % args.progress_games == 0:
                        peak_memory = max(peak_memory, _peak_memory_bytes())
                        elapsed = time.monotonic() - started
                        print(
                            f"Games: {input_games:,} | Positions: {positions_seen:,} | "
                            f"Elapsed: {elapsed:.1f} s",
                            file=sys.stderr,
                            flush=True,
                        )
            flush_rows()
            connection.commit()
            peak_memory = max(peak_memory, _peak_memory_bytes())
            print(
                f"Finished {input_path.name}: {input_games - file_games_before:,} games read",
                file=sys.stderr,
                flush=True,
            )
        flush_rows()
        connection.commit()
        unique_moves = connection.execute("SELECT count(*) FROM counts").fetchone()[0]
        unique_positions = connection.execute(
            "SELECT count(DISTINCT position_key) FROM counts"
        ).fetchone()[0]
        entry_count = connection.execute(
            "SELECT count(*) FROM counts WHERE games>=?", (args.min_games,)
        ).fetchone()[0]
        metadata = BookMetadata(
            entry_count=entry_count,
            max_ply=args.max_ply,
            min_games=args.min_games,
            build_timestamp=args.timestamp if args.timestamp is not None else int(time.time()),
            source=args.source,
            license_name=args.license,
            source_dump=_source_dump_label(args, inputs),
            builder_version=BUILDER_VERSION,
        )

        def accepted_entries() -> Iterator[BookEntry]:
            cursor = connection.execute(
                "SELECT position_key,move,games,white_wins,draws,black_wins FROM counts "
                "WHERE games>=? ORDER BY position_key,move",
                (args.min_games,),
            )
            for key, move, games, white_wins, draws, black_wins in cursor:
                yield BookEntry(key + (1 << 63), move, games, white_wins, draws, black_wins)

        output_size = write_book(Path(args.output), accepted_entries(), metadata)
        accepted_positions = connection.execute(
            "SELECT count(DISTINCT position_key) FROM counts WHERE games>=?", (args.min_games,)
        ).fetchone()[0]
        connection.close()
    return {
        "input_files": len(inputs),
        "files_processed": files_processed,
        "games_processed": input_games,
        "games_accepted": accepted_games,
        "invalid_games": invalid_games,
        "positions_seen": positions_seen,
        "unique_positions": unique_positions,
        "unique_moves": unique_moves,
        "accepted_positions": accepted_positions,
        "book_moves": entry_count,
        "discarded_moves": unique_moves - entry_count,
        "output_size": output_size,
        "peak_memory_bytes": max(peak_memory, _peak_memory_bytes()),
        "build_seconds": time.monotonic() - started,
    }


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(description=__doc__)
    result.add_argument("--input", action="append", type=Path, help="Repeat for every PGN/PGN.ZST")
    result.add_argument("--input-dir", type=Path, help="Add supported files from this directory")
    result.add_argument("--output", required=True, type=Path)
    result.add_argument("--max-ply", type=int, default=20)
    result.add_argument("--min-games", type=int, default=100)
    result.add_argument("--min-rating", type=int)
    result.add_argument("--max-rating", type=int, help="Reserved for a later builder version")
    result.add_argument("--min-speed", choices=tuple(SPEED_ORDER))
    result.add_argument("--include-speeds", nargs="+", choices=tuple(SPEED_ORDER))
    result.add_argument("--exclude-speeds", nargs="+", choices=tuple(SPEED_ORDER))
    result.add_argument("--max-games", type=int)
    result.add_argument("--temp-directory", type=Path)
    result.add_argument(
        "--sqlite-cache-mb", type=int, default=128,
        help="Bounded SQLite page cache for the temporary aggregate",
    )
    result.add_argument("--source", default="lichess")
    result.add_argument("--license", default="CC0-1.0")
    result.add_argument("--source-dump")
    result.add_argument("--timestamp", type=int, help="Deterministic Unix build timestamp")
    result.add_argument(
        "--progress-games", type=int, default=100_000,
        help="Print progress after this many input games; 0 disables periodic progress",
    )
    return result


def main(argv: list[str] | None = None) -> int:
    args = parser().parse_args(argv)
    if args.max_ply < 1 or args.max_ply > 255:
        raise SystemExit("--max-ply must be between 1 and 255")
    if args.min_games < 1:
        raise SystemExit("--min-games must be positive")
    if args.max_games is not None and args.max_games < 1:
        raise SystemExit("--max-games must be positive")
    if args.progress_games < 0:
        raise SystemExit("--progress-games must not be negative")
    if args.sqlite_cache_mb < 1 or args.sqlite_cache_mb > 2048:
        raise SystemExit("--sqlite-cache-mb must be between 1 and 2048")
    if args.max_rating is not None:
        print("warning: --max-rating is reserved and currently not applied", file=sys.stderr)
    try:
        stats = build(args)
    except (OSError, ValueError) as error:
        raise SystemExit(str(error)) from error
    print(f"Input files: {stats['input_files']:,}")
    print(f"Files processed: {stats['files_processed']:,}")
    print(f"Games processed: {stats['games_processed']:,}")
    print(f"Games accepted: {stats['games_accepted']:,}")
    print(f"Invalid games: {stats['invalid_games']:,}")
    print(f"Positions seen: {stats['positions_seen']:,}")
    print(f"Unique positions: {stats['unique_positions']:,}")
    print(f"Unique position/move pairs: {stats['unique_moves']:,}")
    print(f"Accepted positions: {stats['accepted_positions']:,}")
    print(f"Book moves: {stats['book_moves']:,}")
    print(f"Moves discarded by min_games: {stats['discarded_moves']:,}")
    print(f"File size: {stats['output_size']:,} bytes")
    if stats["peak_memory_bytes"]:
        print(f"Peak process memory: {stats['peak_memory_bytes'] / (1024 * 1024):,.1f} MiB")
    else:
        print("Peak process memory: unavailable on this platform")
    print(f"Build time: {stats['build_seconds']:.2f} s")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
