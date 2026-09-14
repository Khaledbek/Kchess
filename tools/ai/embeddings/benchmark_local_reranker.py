#!/usr/bin/env python3
"""Benchmark a local sequence-classification reranker on retrieval candidates.

Input uses the same JSONL shape as benchmark_local_text_encoder.py. The model
must already exist locally; this tool never downloads model assets.
"""

from __future__ import annotations

import argparse
import json
import math
import statistics
import time
from pathlib import Path
from typing import Any


def _load_rows(path: Path) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    with path.open(encoding="utf-8") as handle:
        for line_no, line in enumerate(handle, 1):
            if not line.strip():
                continue
            row = json.loads(line)
            if not isinstance(row.get("query"), str):
                raise ValueError(f"line {line_no}: query must be a string")
            candidates = row.get("candidates")
            if not isinstance(candidates, list) or not candidates:
                raise ValueError(f"line {line_no}: candidates must be non-empty")
            rows.append(row)
    return rows


def _percentile(values: list[float], fraction: float) -> float:
    if not values:
        return 0.0
    ordered = sorted(values)
    index = min(len(ordered) - 1, max(0, math.ceil(fraction * len(ordered)) - 1))
    return ordered[index]


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("--model", type=Path, required=True)
    parser.add_argument("--model-id", required=True)
    parser.add_argument("--model-version", required=True)
    parser.add_argument("--top-k", type=int, default=8)
    parser.add_argument("--max-length", type=int, default=384)
    parser.add_argument("--batch-size", type=int, default=16)
    parser.add_argument("--device", default="cpu")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()

    if args.top_k <= 0 or args.max_length <= 0 or args.batch_size <= 0:
        parser.error("top-k, max-length and batch-size must be positive")

    try:
        import torch
        from transformers import AutoModelForSequenceClassification, AutoTokenizer
    except ImportError as exc:
        raise SystemExit(
            "This development benchmark requires torch and transformers."
        ) from exc

    rows = _load_rows(args.input)
    tokenizer = AutoTokenizer.from_pretrained(
        args.model, local_files_only=True, use_fast=True
    )
    model = AutoModelForSequenceClassification.from_pretrained(
        args.model, local_files_only=True
    )
    model.eval().to(args.device)

    recalls: list[float] = []
    reciprocal_ranks: list[float] = []
    latencies_ms: list[float] = []

    for row in rows:
        candidates = row["candidates"]
        scored: list[tuple[str, float]] = []
        started = time.perf_counter()
        for offset in range(0, len(candidates), args.batch_size):
            batch_candidates = candidates[offset : offset + args.batch_size]
            encoded = tokenizer(
                [row["query"]] * len(batch_candidates),
                [candidate["text"] for candidate in batch_candidates],
                padding=True,
                truncation=True,
                max_length=args.max_length,
                return_tensors="pt",
            )
            encoded = {key: value.to(args.device) for key, value in encoded.items()}
            with torch.inference_mode():
                logits = model(**encoded).logits
                if logits.ndim == 1:
                    scores = logits
                elif logits.shape[-1] == 1:
                    scores = logits[:, 0]
                else:
                    scores = logits[:, -1]
            for candidate, score in zip(batch_candidates, scores.cpu().tolist()):
                scored.append((candidate["id"], float(score)))
        latencies_ms.append((time.perf_counter() - started) * 1000.0)

        ranked_ids = [item[0] for item in sorted(scored, key=lambda item: (-item[1], item[0]))]
        relevant = set(row.get("relevant_ids", []))
        if not relevant:
            continue
        top = ranked_ids[: args.top_k]
        recalls.append(len(relevant.intersection(top)) / len(relevant))
        first_rank = next(
            (rank for rank, value in enumerate(ranked_ids, 1) if value in relevant),
            None,
        )
        reciprocal_ranks.append(0.0 if first_rank is None else 1.0 / first_rank)

    report = {
        "task": "knowledge_graph_text_reranking",
        "model_id": args.model_id,
        "model_version": args.model_version,
        "model_path": str(args.model),
        "queries": len(rows),
        "scored_queries": len(recalls),
        "top_k": args.top_k,
        "recall_at_k": statistics.fmean(recalls) if recalls else 0.0,
        "mrr": statistics.fmean(reciprocal_ranks) if reciprocal_ranks else 0.0,
        "mean_latency_ms_per_query": statistics.fmean(latencies_ms)
        if latencies_ms
        else 0.0,
        "p95_latency_ms_per_query": _percentile(latencies_ms, 0.95),
        "max_length": args.max_length,
        "device": args.device,
        "runtime_dependency": "development-only; product runtime remains C++",
    }
    rendered = json.dumps(report, indent=2, sort_keys=True) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(rendered, encoding="utf-8")
    else:
        print(rendered, end="")


if __name__ == "__main__":
    main()
