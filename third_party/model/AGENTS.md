# Local AI Model Assets

## Scope

`third_party/model/` contains optional small-model assets used by native KChess AI helpers. The main Coach language model uses Gemini API and is not stored here.

## Layout

- `coach/` — main local chess-coach language model assets.
- `router/` — optional tiny routing/context-planning model assets.
- `embeddings/` — optional semantic-retrieval model assets.
- `tokenizer/` — tokenizer/config assets when required by a selected runtime.
- `runtime/` — pinned local inference executable/runtime assets; see its own `AGENTS.md`.

## Rules

- Runtime integration belongs in C++, never Flutter or required Python runtime code.
- Do not assume one model format or vendor in shared KChess contracts.
- Do not place API keys, credentials or user data here.
- Avoid committing large model binaries unless the project explicitly chooses to vendor them; prefer later download/cache tooling for large artifacts.
- Existing `nlohmann`, `sqlite`, `stockfish` and `stockfish19` trees must remain untouched by AI model work.

## Versioning

- `model_manifest.example.json` defines the provider-neutral manifest shape for coach/router/context-planner/embedding artifacts.
- Runtime adapters must validate their selected artifact/version; the shared coach contracts must not assume a concrete file extension or quantization format.
- Router/context/embedding models are optional accelerators. Missing assets must fall back to deterministic native behavior.

## Gemini Coach cleanup

- Do not store a Coach GGUF under `third_party/model/coach/`.
- Do not keep a llama.cpp Coach runtime under `third_party/model/runtime/`.
- `router/`, `embeddings/`, and `tokenizer/` may remain for optional small-model work.
