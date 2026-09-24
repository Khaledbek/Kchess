#!/usr/bin/env python3
"""Run an explicitly supplied quantizer and write KChess artifact metadata."""

import argparse
import hashlib
import json
import subprocess
from pathlib import Path


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--quantizer", type=Path, required=True)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--qtype", required=True)
    parser.add_argument("--role", required=True, choices=["coach", "router", "context_planner", "embeddings"])
    parser.add_argument("--version", required=True)
    args = parser.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run([str(args.quantizer), str(args.input), str(args.output), args.qtype], check=True)
    metadata = {
        "schema": "kchess.ai.model_artifact.v1",
        "role": args.role,
        "version": args.version,
        "quantization": args.qtype,
        "file": args.output.name,
        "sha256": sha256(args.output),
        "bytes": args.output.stat().st_size,
    }
    args.output.with_suffix(args.output.suffix + ".kchess.json").write_text(
        json.dumps(metadata, indent=2, sort_keys=True) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
