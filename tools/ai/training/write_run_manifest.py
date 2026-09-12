#!/usr/bin/env python3
"""Write reproducible metadata for an externally executed training run."""

import argparse
import hashlib
import json
from pathlib import Path
from datetime import datetime, timezone


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--role", required=True, choices=["router", "context_planner", "embeddings", "coach"])
    parser.add_argument("--version", required=True)
    parser.add_argument("--dataset", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--base-model", required=True)
    parser.add_argument("--notes", default="")
    args = parser.parse_args()
    payload = {
        "schema": "kchess.ai.training_run.v1",
        "role": args.role,
        "version": args.version,
        "base_model": args.base_model,
        "dataset": str(args.dataset),
        "dataset_sha256": sha256(args.dataset),
        "created_utc": datetime.now(timezone.utc).isoformat(),
        "notes": args.notes,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
