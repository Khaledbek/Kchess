#!/usr/bin/env python3
"""Export a local transformer encoder to the KChess ONNX embedding target.

The exported graph accepts input_ids + attention_mask and emits one normalized
sentence embedding. Tokenization remains a separate runtime concern. This tool
is development-only and never downloads model assets.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--model", type=Path, required=True)
    parser.add_argument("--model-id", required=True)
    parser.add_argument("--model-version", required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--opset", type=int, default=17)
    parser.add_argument("--max-length", type=int, default=256)
    args = parser.parse_args()

    if args.opset <= 0 or args.max_length <= 0:
        parser.error("opset and max-length must be positive")

    try:
        import torch
        import torch.nn.functional as functional
        from transformers import AutoModel, AutoTokenizer
    except ImportError as exc:
        raise SystemExit(
            "This development exporter requires torch and transformers."
        ) from exc

    tokenizer = AutoTokenizer.from_pretrained(
        args.model, local_files_only=True, use_fast=True
    )
    encoder = AutoModel.from_pretrained(args.model, local_files_only=True)
    encoder.eval()

    class MeanPoolEncoder(torch.nn.Module):
        def __init__(self, model):
            super().__init__()
            self.model = model

        def forward(self, input_ids, attention_mask):
            hidden = self.model(
                input_ids=input_ids, attention_mask=attention_mask
            ).last_hidden_state
            mask = attention_mask.unsqueeze(-1).to(hidden.dtype)
            pooled = (hidden * mask).sum(dim=1) / mask.sum(dim=1).clamp(min=1.0)
            return functional.normalize(pooled, p=2, dim=1)

    wrapper = MeanPoolEncoder(encoder).eval()
    example = tokenizer(
        ["KChess semantic embedding export"],
        padding="max_length",
        truncation=True,
        max_length=min(args.max_length, 32),
        return_tensors="pt",
    )

    args.output.parent.mkdir(parents=True, exist_ok=True)
    torch.onnx.export(
        wrapper,
        (example["input_ids"], example["attention_mask"]),
        args.output,
        input_names=["input_ids", "attention_mask"],
        output_names=["embedding"],
        dynamic_axes={
            "input_ids": {0: "batch", 1: "sequence"},
            "attention_mask": {0: "batch", 1: "sequence"},
            "embedding": {0: "batch"},
        },
        opset_version=args.opset,
        do_constant_folding=True,
    )

    hidden_size = int(getattr(encoder.config, "hidden_size", 0))
    manifest = {
        "contract": "kchess.text_embedding.onnx.v1",
        "model_id": args.model_id,
        "model_version": args.model_version,
        "onnx_file": args.output.name,
        "dimensions": hidden_size,
        "opset": args.opset,
        "max_length": args.max_length,
        "inputs": ["input_ids", "attention_mask"],
        "output": "embedding",
        "pooling": "attention_mask_mean",
        "normalization": "l2",
        "vector_space": "text_semantic",
        "tokenizer_source": "copy compatible tokenizer assets from the local model directory at packaging time",
        "runtime_note": "Python is not required by the shipped KChess runtime.",
    }
    manifest_path = args.output.with_suffix(args.output.suffix + ".json")
    manifest_path.write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )


if __name__ == "__main__":
    main()
