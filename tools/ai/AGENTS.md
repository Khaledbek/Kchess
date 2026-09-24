# AI Development Tooling Instructions

## Scope

`tools/ai/` contains offline dataset preparation and Coach dialogue evaluation utilities. KChess no longer ships or trains a local semantic/evidence planner, local router, local text encoder, reranker, or other local language model.

## Rules

- Python remains development-only; the shipped KChess runtime must not require it.
- Do not add local model binaries, checkpoints, ONNX exports, KCEP artifacts, tokenizer bundles, or model-training run directories back into the repository.
- Do not recreate a learned Evidence Planner beside the deterministic C++ planning path.
- Never place secrets, private user data, real user transcripts, or private PGNs in source control.
- Dataset/evaluation fixtures committed to the repository must be synthetic and non-private.
- Do not duplicate chess truth generation that already exists in native KChess.

## Dialogue evaluation

`evaluation/coach_dialogue_scenarios.jsonl` is a synthetic, non-private scenario catalogue. `evaluation/coach_dialogue_review.md` defines the human review rubric. `evaluation/review_coach_dialogue.py` summarizes explicitly human-labelled local verdicts and does not call the app, Gemini, or another judge.

Private real-case reviews may be supplied locally to the review script, but player transcripts and private source data must never be committed. Summary counts and edit categories are development aids, not runtime evidence.

## Dataset preparation

`datasets/prepare_jsonl.py` is a generic development helper for preparing explicitly supplied non-private JSONL material. It is not a model-runtime dependency and does not establish a supported local-model training pipeline.
