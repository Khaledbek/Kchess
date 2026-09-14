# Knowledge Embedding Evaluation Contract

This directory is the development-only hand-off for Knowledge Graph embedding and reranker evaluation.

## Runtime boundary

Python never owns the shipped vector index or semantic retrieval pipeline. Product runtime uses `native/src/knowledge/vector_index.*` plus `text_semantic_retrieval.*` and reuses the provider-neutral `native/ai/models/EmbeddingModel` contract. Offline tools may evaluate candidate models and export reproducible ONNX artifacts/manifests; a selected model still requires a native runtime adapter. No Python process is required by the app.

## Evaluation record

Use one JSON object per line with these stable fields when preparing embedding records:

- `embedding_id`: deterministic candidate/vector identity.
- `owner_kind`: `chunk` or `node`.
- `owner_id`: owning Knowledge Graph chunk/node ID.
- `player_id`: player scope, or empty for globally reusable knowledge.
- `vector_space`: `text_semantic` or `chess_position`.
- `model_id` and `model_version`: exact candidate model identity.
- `dimensions`: emitted vector dimension.
- `source_version`: source/chunk version used to produce the vector.
- `text`: benchmark input only; never import this field into vector metadata storage.
- `relevance`: optional offline labels for retrieval/reranker evaluation.

Do not commit private player text, PGNs, engine lines, model binaries or generated embeddings. Synthetic/test fixtures are preferred for checked-in evaluation data.

## Update 116 text-model benchmark shape

`benchmark_local_text_encoder.py` and `benchmark_local_reranker.py` consume synthetic/development JSONL rows shaped as:

```json
{"query":"...","candidates":[{"id":"chunk-a","text":"..."}],"relevant_ids":["chunk-a"]}
```

Both tools require an explicitly supplied **local** model directory and use `local_files_only=True`; they never choose or download a vendor model. Record at least retrieval quality (Recall/MRR; encoder also nDCG), latency, model ID/version and emitted dimensions before accepting a candidate.

`export_text_encoder_onnx.py` defines the ONNX target for a selected local encoder: `input_ids` + `attention_mask` -> one L2-normalized mean-pooled `embedding`. It emits a sidecar `kchess.text_embedding.onnx.v1` manifest. Model/tokenizer binaries are packaging assets and are not committed here.

A reranker is optional. Native semantic retrieval must continue to work from vector order when no reranker is installed or when a reranker omits/returns invalid candidate scores.
