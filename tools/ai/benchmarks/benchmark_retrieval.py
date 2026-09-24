#!/usr/bin/env python3
"""Benchmark top-k recall for precomputed embedding vectors in JSONL."""

import argparse
import json
import math
import time
from pathlib import Path


def cosine(a: list[float], b: list[float]) -> float:
    if len(a) != len(b) or not a:
        return -1.0
    dot = sum(x * y for x, y in zip(a, b))
    na = math.sqrt(sum(x * x for x in a))
    nb = math.sqrt(sum(y * y for y in b))
    return dot / (na * nb) if na and nb else -1.0


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("--top-k", type=int, default=5)
    args = parser.parse_args()
    rows = [json.loads(line) for line in args.input.open(encoding="utf-8") if line.strip()]
    start = time.perf_counter()
    hits = 0
    for row in rows:
        ranked = sorted(row["candidates"], key=lambda c: cosine(row["query_vector"], c["vector"]), reverse=True)
        expected = set(row.get("relevant_ids", []))
        hits += bool(expected.intersection(c["id"] for c in ranked[: args.top_k]))
    elapsed = time.perf_counter() - start
    print(json.dumps({
        "queries": len(rows),
        "top_k": args.top_k,
        "recall_at_k": hits / len(rows) if rows else 0.0,
        "milliseconds_per_query": elapsed * 1000 / len(rows) if rows else 0.0,
    }, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
