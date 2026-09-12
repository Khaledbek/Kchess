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
