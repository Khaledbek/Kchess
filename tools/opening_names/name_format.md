# KCO 1 binary format

KCO (`KChess Openings`) is a little-endian, fixed-record opening-*name* index.
It maps a canonical position to the named opening whose line ends at that
position, so the runtime can label each stored game with an ECO code and an
opening/variation name (for example `D06 Queen's Gambit Declined: Marshall
Defense`). It is unrelated to the statistical `KCB` opening *book*
(`book_format.md`), which stores per-move popularity and results. The two files
share only the `positionKey` identity so a single native key implementation
serves both.

The runtime never parses PGN to build the index and never needs Python.

## Position identity

`positionKey` is the same 64-bit Stockfish 18 `Position::key()` used by `KCB`
(see `../opening_book/book_format.md`): every piece, side to move, castling
rights and a Stockfish-valid en-passant file, with the FEN halfmove/fullmove
clocks canonicalized to `0 1`. Clocks are excluded so transpositions share one
identity. The builder imports the exact key function from
`../opening_book/build_book.py` so KCO keys are byte-identical to KCB keys, and
the native reader computes them with the vendored Stockfish core.

Each entry stores the key of the **final** position of one named line. Because
the source catalog contains an entry for every named prefix (`Ruy Lopez` and
`Ruy Lopez: Berlin Defense` are separate rows), a game is classified by walking
its plies, keying each resulting position and keeping the **deepest ply** that
matches any entry. That yields the most specific named opening the game entered,
and matches by position so move-order transpositions are handled.

## Header (96 bytes)

| Offset | Type | Meaning |
|---:|---|---|
| 0 | char[4] | magic `KCO1` |
| 4 | uint16 | format version, `1` |
| 6 | uint16 | header size, `96` |
| 8 | uint16 | entry size, `20` |
| 10 | uint16 | flags, `0` |
| 12 | uint64 | entry count |
| 20 | uint64 | string-table size in bytes |
| 28 | uint32 | maximum named-line ply in this index |
| 32 | int64 | Unix build timestamp (UTC) |
| 40 | char[16] | NUL-terminated source (`lichess`) |
| 56 | char[16] | NUL-terminated license (`CC0-1.0`) |
| 72 | char[16] | NUL-terminated builder version |
| 88 | byte[8] | reserved, all zero |

UTF-8 metadata must contain a NUL terminator inside its field.

## Entry (20 bytes)

| Offset | Type | Meaning |
|---:|---|---|
| 0 | uint64 | position key |
| 8 | uint32 | name offset (byte offset into the string table) |
| 12 | char[4] | ECO code, NUL-padded (for example `C60`) |
| 16 | uint16 | ply (half-move count of the named line) |
| 18 | uint16 | reserved, zero |

Entries are strictly sorted by `positionKey` and every key is unique, so the
native reader can `lower_bound` a key. The `name offset` points at a
NUL-terminated UTF-8 string inside the string table.

## String table

The string table follows the entry array (`header + entryCount * 20`). It is a
blob of NUL-terminated UTF-8 opening names; identical names are stored once and
shared by offset. Its length equals the header `string-table size` and it is the
final bytes of the file (no trailing padding).

## Determinism and collisions

Two different named lines can transpose to the same final position. The builder
resolves such a `positionKey` collision deterministically: keep the deeper line
(greater ply), breaking further ties by smaller ECO then smaller name. For
identical sources the whole file is byte-for-byte reproducible when `--timestamp`
and `--source-version` are supplied.

## Validation

The native reader rejects a wrong magic/version/layout, nonzero reserved bytes,
a size that disagrees with `entryCount`/`stringTableSize`, unsorted or duplicate
keys, a name offset outside the string table, and a name that is not
NUL-terminated, before exposing any lookup.
