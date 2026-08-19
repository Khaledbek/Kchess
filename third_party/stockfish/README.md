# Stockfish 18 source

This directory contains the vendored Stockfish 18 source at commit
`cb3d4ee9b47d0c5aae855b12379378ea1439675c`. License, NNUE checksums and the
reproducible build procedure are recorded in `docs/LICENSE_COMPLIANCE.md`.

KChess carries one Windows compatibility patch in `source/src/nnue/network.cpp`:
NNUE file paths are opened through `std::filesystem::u8path`, and a failed
Windows NNUE load throws a controlled error instead of terminating the host
Flutter process. No search, evaluation or playing logic is changed.
