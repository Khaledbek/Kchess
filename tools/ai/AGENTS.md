# AI Development Tooling Instructions

## Scope

`tools/ai/` contains offline dataset, training-run metadata, evaluation, conversion/quantization and benchmark tools for optional coach/router/context/embedding models.

## Rules

- Python remains development-only; the shipped KChess runtime must not require it.
- Prefer JSONL and explicit version metadata so runs are reproducible.
- Never place secrets, private user data or downloaded model binaries in source control.
- Training/evaluation outputs must record task, model role, version and input checksum where practical.
- Conversion tools may call an explicitly supplied external quantizer; do not hard-code a model vendor/runtime.
- Do not duplicate chess truth generation that already exists in native KChess.

## Local runtime installer

## Update 115 - Knowledge Graph embedding evaluation hand-off

- `tools/ai/embeddings/` defines the offline interchange contract for candidate embedding/reranker evaluation. Python remains development-only and must not become a runtime dependency.
- Candidate records must preserve native `embedding_id`, owner, player scope, vector-space, model/version, dimensions and source-version semantics so Update 116 benchmarks measure the same retrieval contract the C++ runtime uses.
- Never export private production PGNs, profile prose, engine lines or generated user embeddings into the repository. Prefer synthetic fixtures and reproducible manifests.

## Update 116 - local text encoder/reranker evaluation

- `embeddings/benchmark_local_text_encoder.py` and `benchmark_local_reranker.py` evaluate explicitly supplied local model directories only. They must not auto-download or silently select a vendor/model.
- `embeddings/export_text_encoder_onnx.py` defines the development export target `kchess.text_embedding.onnx.v1`: token IDs + attention mask -> one normalized sentence embedding plus a reproducibility manifest.
- Benchmark fixtures committed to the repository must be synthetic/non-private. Reports should record exact model ID/version, dimensions, retrieval quality and latency; generated user embeddings and model binaries stay outside source control.
- These tools do not imply a Python runtime dependency. Product semantic retrieval remains native C++ and a selected ONNX artifact requires a native adapter before shipping.

## Update 173 - dialogue quality catalogue

`evaluation/coach_dialogue_scenarios.jsonl` contains synthetic, non-private question/turn scenarios. `evaluation/coach_dialogue_review.md` defines a human review rubric for factual support, uncertainty, feedback and usefulness. These files are offline review material; do not treat them as run tests, live app telemetry or training data from a real user.

## Update 174 - human dialogue review summary

`evaluation/review_coach_dialogue.py` only summarizes explicitly human-labelled local JSONL verdicts for the synthetic scenario IDs. It does not call the app, Gemini or another judge and it must not ingest committed private transcripts. Keep factual/scope failures distinct from merely awkward wording when deciding whether future model tuning is justified.

## Update 175 - private real-case dialogue review

The offline review script always includes the synthetic catalogue and may additionally read explicitly supplied local private real-case indexes and human reviews. A private player/game `groupId` keeps turns on one deterministic development or held-out side. Summary counts and edit categories are decision aids, never live runtime evidence. Only human-confirmed tone-only edits with factual, scope and continuity passes are candidates for a later controlled style comparison; do not train a model or commit player transcripts through this workflow.
