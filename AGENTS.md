# KChess project instructions

The complete, original project instructions are preserved in
[`agent/AGENTS.md`](agent/AGENTS.md) and are binding for this repository.

The runtime split is mandatory: Flutter/Dart owns presentation and view state;
C++20 owns domain logic, persistence, providers, parsing, settings and analysis;
Python is development tooling only. Android and Windows are the supported
targets. No backend server is part of the architecture.
