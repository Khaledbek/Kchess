#!/usr/bin/env python3
"""Summarize human-reviewed Coach dialogues; never calls the app or a model."""

import argparse
import hashlib
import json
from collections import Counter
from pathlib import Path


DIMENSIONS = (
    "factual_support",
    "scope_and_uncertainty",
    "feedback_continuity",
    "conversational_usefulness",
)
VERDICTS = {"pass", "partial", "fail"}
EDIT_CATEGORIES = {
    "none", "evidence_gap", "teaching_point", "reveal_timing",
    "repetition", "tone_only",
}


def rows(path: Path):
    with path.open(encoding="utf-8") as source:
        for line_number, line in enumerate(source, 1):
            if not line.strip():
                continue
            value = json.loads(line)
            if not isinstance(value, dict):
                raise ValueError(f"{path}:{line_number}: expected JSON object")
            yield line_number, value


def partition(scenario: dict) -> str:
    if scenario.get("source") != "real_local":
        return "synthetic"
    group_id = scenario.get("groupId")
    if not isinstance(group_id, str) or not group_id:
        raise ValueError("real_local scenario requires private groupId")
    # Use one group for every case from the same player, including all games.
    digest = hashlib.sha256(group_id.encode("utf-8")).digest()
    return "heldout" if digest[0] % 5 == 0 else "development"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reviews", type=Path, help="Local human-review JSONL")
    parser.add_argument(
        "--scenarios",
        type=Path,
        action="append",
        default=[],
        help="Additional private real-case JSONL; synthetic catalogue is always included",
    )
    args = parser.parse_args()

    scenarios = {}
    for path in [Path(__file__).with_name("coach_dialogue_scenarios.jsonl"),
                 *args.scenarios]:
        for line_number, item in rows(path):
            scenario_id = item.get("id")
            if not isinstance(scenario_id, str) or not scenario_id or scenario_id in scenarios:
                raise ValueError(f"{path}:{line_number}: missing or duplicate id")
            partition(item)
            scenarios[scenario_id] = item
    counts = {dimension: Counter() for dimension in DIMENSIONS}
    reviewed = set()
    problems = []
    provider_calls = Counter()
    partition_counts = Counter()
    edit_categories = Counter()
    heldout_factual_failures = 0
    heldout_scope_failures = 0
    eligible_tone_edits = 0
    for line_number, review in rows(args.reviews):
        scenario_id = review.get("scenarioId")
        if scenario_id not in scenarios or scenario_id in reviewed:
            raise ValueError(
                f"{args.reviews}:{line_number}: unknown or duplicate scenarioId"
            )
        reviewed.add(scenario_id)
        case_partition = partition(scenarios[scenario_id])
        partition_counts[case_partition] += 1
        verdicts = review.get("verdicts", {})
        if not isinstance(verdicts, dict):
            raise ValueError(f"{args.reviews}:{line_number}: verdicts must be object")
        for dimension in DIMENSIONS:
            verdict = verdicts.get(dimension)
            if verdict not in VERDICTS:
                raise ValueError(
                    f"{args.reviews}:{line_number}: invalid {dimension} verdict"
                )
            counts[dimension][verdict] += 1
        if (verdicts["factual_support"] != "pass" or
                verdicts["scope_and_uncertainty"] != "pass"):
            problems.append(scenario_id)
        if case_partition == "heldout":
            heldout_factual_failures += verdicts["factual_support"] != "pass"
            heldout_scope_failures += verdicts["scope_and_uncertainty"] != "pass"
        if case_partition != "synthetic":
            category = review.get("editCategory")
            if category not in EDIT_CATEGORIES:
                raise ValueError(
                    f"{args.reviews}:{line_number}: invalid editCategory"
                )
            edit_categories[category] += 1
            if (category == "tone_only" and
                    all(verdicts[key] == "pass" for key in DIMENSIONS[:3]) and
                    isinstance(review.get("reviewerEditedAnswer"), str) and
                    review["reviewerEditedAnswer"].strip()):
                eligible_tone_edits += 1
        trace = review.get("trace", {})
        if isinstance(trace, dict) and isinstance(trace.get("providerCalls"), int):
            provider_calls[trace["providerCalls"]] += 1

    if partition_counts["heldout"] < 30:
        tuning_readiness = "insufficient_real_heldout_cases"
    elif heldout_factual_failures or heldout_scope_failures:
        tuning_readiness = "fix_grounding_before_tuning"
    elif eligible_tone_edits < 10:
        tuning_readiness = "insufficient_verified_tone_edits"
    else:
        tuning_readiness = "eligible_for_controlled_style_comparison"

    output = {
        "schema": "coach.dialogue.review_summary.v1",
        "reviewed": len(reviewed),
        "scenarioCount": len(scenarios),
        "missingScenarioIds": sorted(set(scenarios) - reviewed),
        "dimensions": {
            dimension: {verdict: counts[dimension][verdict]
                        for verdict in ("pass", "partial", "fail")}
            for dimension in DIMENSIONS
        },
        "factualOrScopeProblems": problems,
        "providerCallsDistribution": dict(sorted(provider_calls.items())),
        "partitionCounts": dict(sorted(partition_counts.items())),
        "editCategories": dict(sorted(edit_categories.items())),
        "eligibleToneEdits": eligible_tone_edits,
        "tuningReadiness": tuning_readiness,
    }
    print(json.dumps(output, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
