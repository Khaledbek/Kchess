# Coach Concepts Agent

## Scope

`native/ai/concepts/` owns the provider-neutral chess concept taxonomy and later concept retrieval. It is shared by position coaching and general chess questions.

## Rules

- `concept_catalog.*` is the public taxonomy API; callers use stable concept IDs instead of UI prose.
- `concept_catalog_data.*` contains canonical identifiers, categories, parent links and compact lookup aliases only.
- Do not place localized explanations or provider prompts in the catalog. Natural-language explanations belong to the later coaching/provider layer.
- General chess concepts must work without FEN/PGN context.
- Keep concept lookup separate from position motif detection. Tactical detectors may reference concept IDs later, but they remain board-evidence producers.
- Embeddings and semantic ranking are later retrieval concerns; do not couple the catalog to a model or vendor.

## Concept retrieval

- `concept_retriever.*` is the deterministic first-pass lookup over the shared catalog for free-form chess questions, including use without FEN/PGN.
- Retrieval returns stable concept IDs/categories plus confidence; it does not generate explanations or claim engine truth.
- Keep lexical retrieval lightweight and provider-independent. Embeddings may augment ranking later, but must reuse this catalog and retrieval contract rather than replace it with a second taxonomy.

## Semantic retrieval

- Update 44 augments the existing lexical `ConceptRetriever`; it does not create a second taxonomy.
- Strong lexical matches skip embedding inference to reduce latency. Weak lexical queries may use the optional `EmbeddingModel`, with cached concept vectors keyed by model id/version.
- Embedding similarity is retrieval evidence only and must never be treated as objective chess truth.
