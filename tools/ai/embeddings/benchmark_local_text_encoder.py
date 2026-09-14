#!/usr/bin/env python3
"""Benchmark a local Hugging Face text encoder for Knowledge Graph retrieval.

The script is development-only and deliberately uses local_files_only=True. It
never downloads a model and never writes user text or generated embeddings into
the repository.

Input JSONL rows:
{
  "query": "why do I lose in closed centers?",
  "candidates": [{"id": "c1", "text": "..."}, ...],
  "relevant_ids": ["c1"]
}
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
            for candidate in candidates:
                if not isinstance(candidate.get("id"), str) or not isinstance(
                    candidate.get("text"), str
                ):
                    raise ValueError(
                        f"line {line_no}: candidate requires string id/text"
                    )
            relevant = row.get("relevant_ids", [])
            if not isinstance(relevant, list) or not all(
                isinstance(value, str) for value in relevant
            ):
                raise ValueError(f"line {line_no}: relevant_ids must be strings")
            rows.append(row)
    return rows


def _dcg(ranked_ids: list[str], relevant_ids: set[str], k: int) -> float:
    score = 0.0
    for rank, candidate_id in enumerate(ranked_ids[:k], 1):
        if candidate_id in relevant_ids:
            score += 1.0 / math.log2(rank + 1.0)
    return score


def _percentile(values: list[float], fraction: float) -> float:
    if not values:
        return 0.0
    ordered = sorted(values)
    index = min(len(ordered) - 1, max(0, math.ceil(fraction * len(ordered)) - 1))
    return ordered[index]


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("--model", type=Path, required=True,
                        help="Local Hugging Face model directory")
    parser.add_argument("--model-id", required=True)
    parser.add_argument("--model-version", required=True)
    parser.add_argument("--top-k", type=int, default=8)
    parser.add_argument("--max-length", type=int, default=256)
    parser.add_argument("--batch-size", type=int, default=32)
    parser.add_argument("--device", default="cpu")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()

    if args.top_k <= 0 or args.max_length <= 0 or args.batch_size <= 0:
        parser.error("top-k, max-length and batch-size must be positive")

    try:
        import torch
        import torch.nn.functional as functional
        from transformers import AutoModel, AutoTokenizer
    except ImportError as exc:
        raise SystemExit(
            "This development benchmark requires torch and transformers."
        ) from exc

    rows = _load_rows(args.input)
    tokenizer = AutoTokenizer.from_pretrained(
        args.model, local_files_only=True, use_fast=True
    )
    model = AutoModel.from_pretrained(args.model, local_files_only=True)
    model.eval().to(args.device)

    def encode(texts: list[str]):
        batch = tokenizer(
            texts,
            padding=True,
            truncation=True,
            max_length=args.max_length,
            return_tensors="pt",
        )
        batch = {key: value.to(args.device) for key, value in batch.items()}
        with torch.inference_mode():
            output = model(**batch)
            hidden = output.last_hidden_state
            mask = batch["attention_mask"].unsqueeze(-1).to(hidden.dtype)
            pooled = (hidden * mask).sum(dim=1) / mask.sum(dim=1).clamp(min=1.0)
            pooled = functional.normalize(pooled, p=2, dim=1)
        return pooled.cpu()

    recalls: list[float] = []
    reciprocal_ranks: list[float] = []
    ndcgs: list[float] = []
    latencies_ms: list[float] = []
    dimensions = 0

    for row in rows:
        candidates = row["candidates"]
        texts = [row["query"]] + [candidate["text"] for candidate in candidates]
        started = time.perf_counter()
        vectors = []
        for offset in range(0, len(texts), args.batch_size):
            vectors.append(encode(texts[offset : offset + args.batch_size]))
        vectors = torch.cat(vectors, dim=0)
        latencies_ms.append((time.perf_counter() - started) * 1000.0)
        dimensions = int(vectors.shape[1]) if vectors.ndim == 2 else 0

        query = vectors[0]
        candidate_vectors = vectors[1:]
        scores = torch.mv(candidate_vectors, query)
        order = torch.argsort(scores, descending=True).tolist()
        ranked_ids = [candidates[index]["id"] for index in order]
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
        ideal = sum(
            1.0 / math.log2(rank + 1.0)
            for rank in range(1, min(len(relevant), args.top_k) + 1)
        )
        ndcgs.append(0.0 if ideal == 0.0 else _dcg(ranked_ids, relevant, args.top_k) / ideal)

    report = {
        "task": "knowledge_graph_text_embedding_retrieval",
        "model_id": args.model_id,
        "model_version": args.model_version,
        "model_path": str(args.model),
        "queries": len(rows),
        "scored_queries": len(recalls),
        "dimensions": dimensions,
        "top_k": args.top_k,
        "recall_at_k": statistics.fmean(recalls) if recalls else 0.0,
        "mrr": statistics.fmean(reciprocal_ranks) if reciprocal_ranks else 0.0,
        "ndcg_at_k": statistics.fmean(ndcgs) if ndcgs else 0.0,
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
