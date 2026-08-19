# KCB 1 binary format

KCB (`Kchess Book`) is a little-endian, fixed-record opening-book format. The
runtime never parses PGN and never needs Python. All byte counts below are part
of format version 1.

## Position and move identities

`positionKey` is the deterministic 64-bit Zobrist key from Stockfish 18
(`Position::key()` with the FEN halfmove/fullmove clocks canonicalized to
`0 1`). It includes every piece, side to move, castling rights and a Stockfish-
valid en-passant file. An en-passant target is ignored when the side to move has
no pawn that could pseudo-legally capture it. Clocks are intentionally excluded,
so transpositions have the same identity. The builder reproduces the official
Stockfish PRNG seed and key layout and the native reader computes the key with
the vendored Stockfish core. Cross-language fixtures guard this contract.

Moves are unsigned 16-bit values: bits `0..5` from-square (`a1=0`), bits
`6..11` to-square and bits `12..14` promotion (`0` none, `1` knight, `2`
bishop, `3` rook, `4` queen). Bit 15 is reserved and must be zero. Castling is
stored with regular UCI king coordinates, for example `e1g1`.

## Header (160 bytes)

| Offset | Type | Meaning |
|---:|---|---|
| 0 | char[4] | magic `KCB1` |
| 4 | uint16 | format version, `1` |
| 6 | uint16 | header size, `160` |
| 8 | uint16 | entry size, `28` |
| 10 | uint16 | flags, `0` |
| 12 | uint64 | entry count |
| 20 | uint32 | configured maximum ply |
| 24 | uint32 | configured minimum games |
| 28 | int64 | Unix build timestamp (UTC) |
| 36 | char[16] | NUL-terminated source (`lichess`) |
| 52 | char[16] | NUL-terminated license (`CC0-1.0`) |
| 68 | char[64] | NUL-terminated dump/fixture name |
| 132 | char[16] | NUL-terminated builder version |
| 148 | byte[12] | reserved, all zero |

UTF-8 metadata must contain a NUL terminator inside its field. A future format
version can widen fields or add indexes without silently changing KCB 1.

## Entry (28 bytes)

| Offset | Type | Meaning |
|---:|---|---|
| 0 | uint64 | position key |
| 8 | uint16 | encoded move |
| 10 | uint16 | reserved, zero |
| 12 | uint32 | games |
| 16 | uint32 | white wins |
| 20 | uint32 | draws |
| 24 | uint32 | black wins |

Entries are strictly sorted by `(positionKey, move)`. `games` must equal the
sum of the three result counters. The reader rejects wrong magic/version/layout,
nonzero reserved bytes, invalid moves, unsorted/duplicate entries, inconsistent
counters, truncation and trailing bytes before exposing a lookup.

## Multi-input aggregation and determinism

KCB1 does not encode a different record shape for multiple source dumps. The
builder aggregates `games`, `whiteWins`, `draws` and `blackWins` for each
`(positionKey, move)` across all inputs in one temporary database, applies
`minGames` only after that aggregation, and writes the same sorted entry
layout. The existing native reader therefore needs no format change.

For identical source data and filters, the entry area from byte 160 onward is
deterministic regardless of input order. A current build timestamp and source
label intentionally make the header differ. Supplying the same `--timestamp`
and `--source-dump` makes the complete KCB file byte-for-byte reproducible.
