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
