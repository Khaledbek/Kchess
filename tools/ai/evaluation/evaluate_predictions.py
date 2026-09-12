#!/usr/bin/env python3
"""Evaluate labelled JSONL predictions with accuracy and per-label recall."""

import argparse
import json
from collections import Counter
from pathlib import Path


def load(path: Path, value_key: str) -> dict[str, str]:
    rows = {}
    for line_no, line in enumerate(path.open(encoding="utf-8"), 1):
        if not line.strip():
            continue
        item = json.loads(line)
        key = str(item.get("id", ""))
        value = item.get(value_key)
        if not key or not isinstance(value, str):
            raise SystemExit(f"{path}:{line_no}: expected id and {value_key}")
        rows[key] = value
    return rows


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("truth", type=Path)
    parser.add_argument("predictions", type=Path)
    args = parser.parse_args()
    truth = load(args.truth, "label")
    pred = load(args.predictions, "prediction")
    total = correct = 0
    labels = Counter()
    hits = Counter()
    for key, label in truth.items():
        labels[label] += 1
        total += 1
        if pred.get(key) == label:
            correct += 1
            hits[label] += 1
    result = {
        "samples": total,
        "accuracy": correct / total if total else 0.0,
        "missing_predictions": sum(1 for key in truth if key not in pred),
        "recall_by_label": {label: hits[label] / count for label, count in sorted(labels.items())},
    }
    print(json.dumps(result, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
