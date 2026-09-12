#!/usr/bin/env python3
"""Validate and deterministically split small-model JSONL datasets."""

import argparse
import hashlib
import json
from pathlib import Path


def split_bucket(key: str) -> int:
    return int(hashlib.sha256(key.encode("utf-8")).hexdigest()[:8], 16) % 1000


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("output_dir", type=Path)
    parser.add_argument("--validation-per-mille", type=int, default=100)
    args = parser.parse_args()
    if not 0 <= args.validation_per_mille <= 500:
        raise SystemExit("validation split must be between 0 and 500 per mille")

    args.output_dir.mkdir(parents=True, exist_ok=True)
    train = (args.output_dir / "train.jsonl").open("w", encoding="utf-8")
    valid = (args.output_dir / "validation.jsonl").open("w", encoding="utf-8")
    counts = {"train": 0, "validation": 0}
    try:
        for line_no, line in enumerate(args.input.open(encoding="utf-8"), 1):
            if not line.strip():
                continue
            item = json.loads(line)
            if not isinstance(item.get("text"), str) or not item["text"].strip():
                raise SystemExit(f"line {line_no}: missing text")
            if not isinstance(item.get("label"), str) or not item["label"].strip():
                raise SystemExit(f"line {line_no}: missing label")
            key = str(item.get("id") or f"{item['label']}\n{item['text']}")
            is_valid = split_bucket(key) < args.validation_per_mille
            target = valid if is_valid else train
            target.write(json.dumps(item, ensure_ascii=False, sort_keys=True) + "\n")
            counts["validation" if is_valid else "train"] += 1
    finally:
        train.close()
        valid.close()
    print(json.dumps(counts, sort_keys=True))


if __name__ == "__main__":
    main()
