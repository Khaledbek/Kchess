# Small Model Runtime Instructions

## Scope

`native/ai/models/` owns provider-neutral contracts for optional tiny intent, context-planning and embedding models plus stable AI component versioning.

## Rules

- Small models augment deterministic C++ baselines; they must never become required for normal coach operation.
- Invoke tiny models only when their result can materially reduce ambiguity, tokens or latency.
- Do not put model-format/vendor branches into shared contracts. Runtime adapters may implement these interfaces elsewhere.
- Embedding models support semantic retrieval only; they do not become a source of objective chess truth.
- Model assets stay under `third_party/model/`; large binaries are not committed by default.
- Keep version identifiers stable and bump them only when the corresponding contract/behavior changes incompatibly.

`CoachPrompt` v3 introduced request-scoped Gemini instructions. The current v4 also requires quote-linked typed claims and explains native move-contrast evidence. The validated-response cache includes this version so old prose cannot be reused under the newer contract.

## Update 116 - shared Knowledge Graph embedding contract

- The existing `EmbeddingModel` interface is also the provider-neutral text-embedding inference contract for the general Knowledge Graph. Do not create a parallel embedding interface in `src/knowledge/`.
- Knowledge Graph vector persistence, chunk freshness and semantic candidate retrieval remain owned by `src/knowledge/`; this folder owns only the optional inference-model contract/runtime adapters.

## Update 162 - portable linear router/context adapter

- `portable_small_models.*` is the native adapter for optional small linear Intent and Context Planner assets. Stable schemas are `kchess.tiny_intent.linear.v1` and `kchess.tiny_context.linear.v1`; model identity/version come from the asset and are diagnostics/cache metadata, not chess truth.
- The default runtime root is `<app-data>/models`; `KCHESS_SMALL_MODEL_DIR` may override it for explicit developer/evaluation runs. Never expose the resolved filesystem path through user-visible/provider context.
- Loading and inference fail closed. Invalid/missing assets return null model pointers so deterministic routing/planning remains authoritative. Do not hand-author model weights in C++ to simulate a trained model.

## Update 163 - optional portable embedding projection

- `portable_small_models.*` may also load `embedding_projection.json` with schema `kchess.embedding.feature_projection.v1` into the existing shared `EmbeddingModel` contract. This extends the Update-162 model root; it does not create a second embedding interface or retriever.
- The portable adapter is a learned sparse token/bigram projection supplied by an external/offline asset. KChess must not synthesize or hand-author projection weights at runtime. Missing, malformed, dimension-invalid or zero-coverage assets fail closed to the existing lexical/deterministic retrieval baseline.
- The same loaded `EmbeddingModel` instance is passed through the existing `SmallModelSuite` to Concept Retrieval and Knowledge Graph retrieval. Semantic similarity only reorders/filter-ranks already eligible evidence and is never objective chess truth.

## Update 170 - Windows environment lookup hygiene

- Windows optional-model environment discovery uses `_dupenv_s` and frees the returned buffer. Do not suppress MSVC secure-CRT warnings globally just to retain `getenv`; non-Windows builds may continue to use standard `getenv`.
- `KCHESS_SMALL_MODEL_DIR` remains an optional developer/evaluation override only. This warning cleanup does not change model-root precedence or runtime behavior.

## Update 175 - prompt cache version

`CoachPrompt` v5 identifies the Gemini segmented-answer contract. Keep its version in the exact validated-response-cache identity; old prompt/schema output must not be reused as current grounded dialogue. Small model adapters remain optional inference aids, not style-tuning truth.
