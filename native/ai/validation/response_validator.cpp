#include "response_validator.h"
#include "chess_validator.h"

#include <algorithm>
#include <cctype>
#include <set>
#include <string_view>
#include <utility>

#include <nlohmann/json.hpp>

namespace kchess::ai {
namespace {

using json = nlohmann::json;

// -----------------------------------------------------------------------------
// Section: Provider-visible authority and profile context contract
// -----------------------------------------------------------------------------

struct SuppliedGrounding {
  std::set<std::string> evidence_ids;
  std::set<std::string> fact_ids;
  json profile;
  bool profile_supplied{false};
  bool malformed_profile{false};
  bool profile_context_complete{false};
};

SuppliedGrounding supplied_grounding(const std::vector<EvidenceItem>& supplied) {
  SuppliedGrounding result;
  for (const auto& item : supplied) {
    result.evidence_ids.insert(item.id);
    if (item.kind == EvidenceKind::candidate_moves) {
      const auto candidate_payload = json::parse(item.payload, nullptr, false);
      if (candidate_payload.is_object() && candidate_payload.contains("facts") &&
          candidate_payload["facts"].is_array()) {
        for (const auto& fact : candidate_payload["facts"]) {
          if (!fact.is_object()) continue;
          const auto id = fact.value("fact_id", "");
          if (!id.empty()) result.fact_ids.insert(id);
        }
      }
    }
    if (item.kind != EvidenceKind::user_profile) continue;
    if (result.profile_supplied || item.id != "profile.context.v3") {
      result.malformed_profile = true;
      continue;
    }
    result.profile_supplied = true;
    result.profile = json::parse(item.payload, nullptr, false);
    if (!result.profile.is_object() ||
        result.profile.value("schema", "") != "profile.context.v3" ||
        !result.profile.contains("chunks") ||
        !result.profile["chunks"].is_array()) {
      result.malformed_profile = true;
      continue;
    }
    std::set<std::string> node_ids;
    for (const auto& chunk : result.profile["chunks"]) {
      if (!chunk.is_object() || !chunk.contains("nodeId") ||
          !chunk["nodeId"].is_string() ||
          chunk["nodeId"].get_ref<const std::string&>().empty() ||
          !node_ids.insert(chunk["nodeId"].get<std::string>()).second ||
          !chunk.contains("data") || !chunk["data"].is_object()) {
        result.malformed_profile = true;
      }
    }
    result.profile_context_complete = !result.malformed_profile &&
        result.profile.value("scopeComplete", false) &&
        result.profile.value("contextStatus", "insufficient_evidence") == "available" &&
        !result.profile["chunks"].empty();
  }
  return result;
}

bool references_profile(const std::vector<std::string>& ids) {
  return std::find(ids.begin(), ids.end(), "profile.context.v3") != ids.end() ||
      std::any_of(ids.begin(), ids.end(), [](const auto& id) {
        return id.starts_with("profile.");
      });
}

bool references_supplied(const std::vector<std::string>& ids,
                         const SuppliedGrounding& grounding) {
  return std::all_of(ids.begin(), ids.end(), [&](const auto& id) {
    return grounding.evidence_ids.contains(id);
  });
}

struct ProfileScalar {
  std::string value;
  std::string epistemic_status;
};

std::optional<ProfileScalar> profile_scalar(
    const std::string& subject, const SuppliedGrounding& grounding,
    std::string* error_code) {
  if (!grounding.profile_supplied || grounding.malformed_profile) {
    if (error_code) *error_code = "profile_claim_evidence_not_supplied";
    return std::nullopt;
  }
  const auto separator = subject.find('#');
  if (separator == std::string::npos || separator == 0) {
    if (error_code) *error_code = "profile_claim_subject_invalid";
    return std::nullopt;
  }
  const auto node_id = subject.substr(0, separator);
  const auto pointer = subject.substr(separator + 1);
  if (!pointer.starts_with("/data/")) {
    if (error_code) *error_code = "profile_claim_subject_invalid";
    return std::nullopt;
  }
  for (const auto& chunk : grounding.profile["chunks"]) {
    if (chunk.value("nodeId", "") != node_id) continue;
    try {
      const auto& value = chunk.at(json::json_pointer(pointer));
      if (value.is_null() || value.is_structured()) {
        if (error_code) *error_code = "profile_claim_value_not_scalar";
        return std::nullopt;
      }
      const auto expected = value.is_string() ? value.get<std::string>() : value.dump();
      const auto epistemic = chunk.value("epistemicStatus", "");
      if (epistemic.empty()) {
        if (error_code) *error_code = "profile_claim_epistemic_status_missing";
        return std::nullopt;
      }
      return ProfileScalar{.value = expected, .epistemic_status = epistemic};
    } catch (const json::exception&) {
      if (error_code) *error_code = "profile_claim_field_not_supplied";
      return std::nullopt;
    }
  }
  if (error_code) *error_code = "profile_claim_chunk_not_supplied";
  return std::nullopt;
}

ChessValidationResult validate_profile_fact(
    const CoachClaim& claim, const SuppliedGrounding& grounding) {
  if (!references_profile(claim.evidence_ids) ||
      !references_supplied(claim.evidence_ids, grounding)) {
    return {false, "profile_claim_evidence_not_supplied"};
  }
  std::string error;
  const auto scalar = profile_scalar(claim.subject, grounding, &error);
  if (!scalar.has_value()) return {false, std::move(error)};
  if (claim.epistemic_status != scalar->epistemic_status) {
    return {false, "profile_claim_epistemic_status_mismatch"};
  }
  if (claim.value != scalar->value) {
    return {false, "profile_claim_value_mismatch"};
  }
  if (!claim.support_subjects.empty()) {
    return {false, "profile_fact_support_subjects_not_allowed"};
  }
  return {};
}

ChessValidationResult validate_profile_inference(
    const CoachClaim& claim, const SuppliedGrounding& grounding) {
  if (!grounding.profile_context_complete ||
      !references_profile(claim.evidence_ids) ||
      !references_supplied(claim.evidence_ids, grounding)) {
    return {false, "profile_inference_evidence_insufficient"};
  }
  if (!claim.value.empty()) {
    return {false, "profile_inference_value_must_be_empty"};
  }
  if (claim.support_subjects.size() < 2 || claim.support_subjects.size() > 8) {
    return {false, "profile_inference_support_count_invalid"};
  }
  if (claim.epistemic_status != "inference" && claim.epistemic_status != "hypothesis") {
    return {false, "profile_inference_epistemic_status_invalid"};
  }
  std::set<std::string> unique;
  bool hypothesis_source = false;
  for (const auto& subject : claim.support_subjects) {
    if (!unique.insert(subject).second) {
      return {false, "profile_inference_support_duplicate"};
    }
    std::string error;
    const auto scalar = profile_scalar(subject, grounding, &error);
    if (!scalar.has_value()) return {false, std::move(error)};
    if (scalar->epistemic_status == "hypothesis") hypothesis_source = true;
  }
  if (claim.epistemic_status == "hypothesis" && !hypothesis_source) {
    return {false, "profile_inference_hypothesis_source_required"};
  }
  return {};
}

std::vector<std::string> profile_contract_issues(
    const StructuredCoachContent& content, const SuppliedGrounding& grounding,
    const bool profile_requested) {
  std::vector<std::string> issues;
  if (grounding.malformed_profile) issues.push_back("profile_context_invalid");

  const auto& status = content.profile_status;
  const bool valid_status = status == "not_used" || status == "grounded" ||
      status == "insufficient_evidence";
  if (!valid_status && (profile_requested || grounding.profile_supplied || !status.empty())) {
    issues.push_back("profile_answer_status_required");
  }

  if (profile_requested) {
    if (!grounding.profile_supplied || !grounding.profile_context_complete) {
      if (status != "insufficient_evidence") {
        issues.push_back("profile_insufficient_evidence_status_required");
      }
    } else if (status != "grounded" && status != "insufficient_evidence") {
      issues.push_back("profile_answer_status_required");
    }
  } else if (status == "grounded" || status == "insufficient_evidence") {
    issues.push_back("profile_status_without_profile_request");
  }

  std::size_t personal_claims = 0;
  for (const auto& claim : content.claims) {
    if (claim.kind == CoachClaimKind::profile_fact) {
      ++personal_claims;
      const auto result = validate_profile_fact(claim, grounding);
      if (!result.valid) issues.push_back(result.error_code);
    } else if (claim.kind == CoachClaimKind::profile_inference) {
      ++personal_claims;
      const auto result = validate_profile_inference(claim, grounding);
      if (!result.valid) issues.push_back(result.error_code);
    } else if (references_profile(claim.evidence_ids)) {
      issues.push_back("profile_claim_kind_required");
    }
  }

  if (status == "grounded" && personal_claims == 0) {
    issues.push_back("profile_claims_required");
  }
  if (personal_claims > 0 && status != "grounded") {
    issues.push_back("profile_answer_status_mismatch");
  }
  if (status == "grounded" && !grounding.profile_context_complete) {
    issues.push_back("profile_grounded_with_incomplete_context");
  }

  const auto check_profile_refs = [&](const std::vector<std::string>& ids) {
    for (const auto& id : ids) {
      if (id.starts_with("profile.") && !grounding.evidence_ids.contains(id)) {
        issues.push_back("profile_evidence_reference_not_supplied");
      }
    }
  };
  check_profile_refs(content.evidence_ids);
  for (const auto& concept_item : content.concepts) {
    check_profile_refs(concept_item.evidence_ids);
  }
  for (const auto& recommendation : content.recommendations) {
    check_profile_refs(recommendation.evidence_ids);
  }
  return issues;
}

// -----------------------------------------------------------------------------
// Section: Independent board fact validation
// -----------------------------------------------------------------------------

bool needs_board(const CoachClaimKind kind) {
  return kind == CoachClaimKind::legal_move ||
      kind == CoachClaimKind::gives_check ||
      kind == CoachClaimKind::gives_mate ||
      kind == CoachClaimKind::material_cp ||
      kind == CoachClaimKind::piece_on_square;
}

ChessValidationResult validate_claim(
    const ChessValidator& chess, const CoachClaim& claim,
    const std::optional<std::string>& position_fen,
    const std::vector<EvidenceItem>& evidence) {
  if (needs_board(claim.kind) && !position_fen)
    return {false, "claim_position_missing"};
  return chess.validate_claim(claim, position_fen.value_or(""), evidence);
}

ChessValidationResult validate_contrast_fact(
    const CoachClaim& claim, const std::vector<EvidenceItem>& evidence) {
  if (std::find(claim.evidence_ids.begin(), claim.evidence_ids.end(),
                "move.contrast.v1") == claim.evidence_ids.end())
    return {false, "contrast_evidence_not_cited"};
  const bool allowed_path = claim.subject == "/playedMoveUci" ||
      claim.subject == "/attemptStatus" ||
      claim.subject == "/originalQuestionCandidateUci" ||
      claim.subject.starts_with("/before/") ||
      claim.subject.starts_with("/played/") ||
      claim.subject.starts_with("/candidate/") ||
      claim.subject.starts_with("/playedMinusBefore/") ||
      claim.subject.starts_with("/playedMinusCandidate/") ||
      claim.subject.starts_with("/verifiedAnalysis/");
  if (!allowed_path) return {false, "contrast_subject_invalid"};
  for (const auto& item : evidence) {
    if (item.id != "move.contrast.v1" ||
        item.kind != EvidenceKind::move_contrast) continue;
    const auto payload = json::parse(item.payload, nullptr, false);
    if (!payload.is_object() || payload.value("schema", "") != "move.contrast.v1")
      return {false, "contrast_evidence_invalid"};
    try {
      const auto& value = payload.at(json::json_pointer(claim.subject));
      if (value.is_structured() || value.is_null())
        return {false, "contrast_value_not_scalar"};
      const auto expected = value.is_string()
          ? value.get<std::string>() : value.dump();
      return expected == claim.value
          ? ChessValidationResult{}
          : ChessValidationResult{false, "contrast_value_mismatch"};
    } catch (const json::exception&) {
      return {false, "contrast_subject_not_supplied"};
    }
  }
  return {false, "contrast_evidence_not_supplied"};
}

bool answer_quote_present(const CoachClaim& claim,
                          const StructuredCoachContent& content) {
  return claim.answer_quote.size() >= 8 &&
      claim.answer_quote.size() <= 240 &&
      std::any_of(claim.answer_quote.begin(), claim.answer_quote.end(),
                  [](unsigned char byte) { return !std::isspace(byte); }) &&
      (content.answer.find(claim.answer_quote) != std::string::npos ||
       content.follow_up_question.find(claim.answer_quote) !=
           std::string::npos);
}

// Only coordinate-move mentions are mechanically recognizable in free prose.
// Keep this gate narrow: SAN, ordinary chess words and personal deductions
// cannot be validated by a lexical scan and remain subject to structured
// claims/provider-visible evidence instead.
bool uci_token(std::string_view token) {
  if (token.size() != 4 && token.size() != 5) return false;
  if (token[0] < 'a' || token[0] > 'h' || token[2] < 'a' || token[2] > 'h' ||
      token[1] < '1' || token[1] > '8' || token[3] < '1' || token[3] > '8') return false;
  return token.size() == 4 || token[4] == 'q' || token[4] == 'r' ||
      token[4] == 'b' || token[4] == 'n';
}

std::set<std::string> supplied_coordinate_moves(
    const std::vector<EvidenceItem>& evidence) {
  std::set<std::string> moves;
  for (const auto& item : evidence) {
    if (item.kind == EvidenceKind::move_contrast &&
        item.id == "move.contrast.v1") {
      const auto payload = json::parse(item.payload, nullptr, false);
      if (payload.is_object() &&
          payload.value("schema", "") == "move.contrast.v1") {
        for (const auto* key : {"playedMoveUci", "originalQuestionCandidateUci"}) {
          const auto value = payload.value(key, json{});
          if (value.is_string()) moves.insert(value.get<std::string>());
        }
        const auto verified = payload.value("verifiedAnalysis", json{});
        if (verified.is_object()) {
          const auto reply = verified.value("criticalReplyUci", json{});
          if (reply.is_string()) moves.insert(reply.get<std::string>());
        }
      }
      continue;
    }
    if (item.kind != EvidenceKind::candidate_moves) continue;
    const auto payload = json::parse(item.payload, nullptr, false);
    if (!payload.is_object()) continue;
    const auto add_candidate = [&](const json& candidate) {
      if (!candidate.is_object()) return;
      if (const auto move = candidate.value("move_uci", json{}); move.is_string())
        moves.insert(move.get<std::string>());
      if (const auto pv = candidate.value("pv_uci", json{}); pv.is_array()) {
        for (const auto& move : pv)
          if (move.is_string()) moves.insert(move.get<std::string>());
      }
    };
    if (payload.contains("best")) add_candidate(payload["best"]);
    if (payload.contains("focus")) add_candidate(payload["focus"]);
    if (payload.contains("user_move")) add_candidate(payload["user_move"]);
    if (payload.contains("alternatives") && payload["alternatives"].is_array())
      for (const auto& candidate : payload["alternatives"])
        add_candidate(candidate);
    if (const auto reply = payload.value("critical_reply_uci", json{});
        reply.is_string()) moves.insert(reply.get<std::string>());
  }
  return moves;
}

bool prose_has_ungrounded_coordinate_move(
    std::string_view prose, const std::set<std::string>& supported) {
  std::size_t start = 0;
  while (start < prose.size()) {
    while (start < prose.size() &&
           !std::isalnum(static_cast<unsigned char>(prose[start]))) ++start;
    const auto end = start;
    while (start < prose.size() &&
           std::isalnum(static_cast<unsigned char>(prose[start]))) ++start;
    const auto word = prose.substr(end, start - end);
    if (uci_token(word) && !supported.contains(std::string(word))) return true;
  }
  return false;
}

std::string assembled_text(const std::vector<CoachAnswerSegment>& segments) {
  std::string result;
  for (const auto& segment : segments) {
    if (!result.empty()) result += ' ';
    result += segment.text;
  }
  return result;
}

std::string assembled_answer(const StructuredCoachContent& content) {
  std::string result = content.verdict_summary;
  const auto explanation = assembled_text(content.answer_segments);
  if (!explanation.empty()) {
    if (!result.empty()) result += ' ';
    result += explanation;
  }
  return result;
}

void validate_verdict_lock(
    const StructuredCoachContent& content,
    const std::optional<ChessVerdictContract>* expected_verdict,
    const std::optional<ChessVerdictReview>* expected_review,
    std::vector<std::string>& issues) {
  const bool expected_active = expected_verdict != nullptr &&
      expected_verdict->has_value() && (*expected_verdict)->authoritative;
  const std::string expected_position = expected_active
      ? std::string(position_verdict_name((*expected_verdict)->position_verdict))
      : std::string{"unknown"};
  const std::string expected_move = expected_active
      ? std::string(move_verdict_name((*expected_verdict)->move_verdict))
      : std::string{"unknown"};
  const std::string expected_review_outcome =
      expected_review != nullptr && expected_review->has_value() &&
          (*expected_review)->active
      ? std::string(verdict_review_outcome_name((*expected_review)->outcome))
      : std::string{"none"};

  if (content.verdict_lock.active != expected_active ||
      content.verdict_lock.position_verdict != expected_position ||
      content.verdict_lock.move_verdict != expected_move ||
      content.verdict_lock.review_outcome != expected_review_outcome) {
    issues.push_back("native_verdict_lock_mismatch");
  }
  if (expected_active) {
    if (content.verdict_summary.empty() || content.verdict_summary.size() > 240)
      issues.push_back("native_verdict_summary_required");
  } else if (!content.verdict_summary.empty()) {
    issues.push_back("native_verdict_summary_without_verdict");
  }
}

void validate_segments(const StructuredCoachContent& content,
                       const SuppliedGrounding& grounding,
                       std::vector<std::string>& issues) {
  if ((content.answer_segments.empty() && content.verdict_summary.empty()) ||
      assembled_answer(content) != content.answer ||
      assembled_text(content.follow_up_segments) != content.follow_up_question) {
    issues.push_back("answer_segments_missing_or_mismatched");
    return;
  }
  std::vector<bool> linked(content.claims.size(), false);
  const auto inspect = [&](const std::vector<CoachAnswerSegment>& segments) {
    for (const auto& segment : segments) {
      for (const auto& fact_id : segment.fact_ids) {
        if (!grounding.fact_ids.contains(fact_id))
          issues.push_back("segment_fact_reference_not_supplied");
      }

      if (segment.kind == CoachSegmentKind::factual) {
        const bool has_native_facts = !segment.fact_ids.empty();
        const bool has_typed_claim = segment.claim_indices.size() == 1 &&
            segment.claim_indices.front() < content.claims.size();
        if (!has_native_facts && !has_typed_claim) {
          issues.push_back("grounded_segment_reference_required");
          continue;
        }
        if (segment.claim_indices.size() > 1) {
          issues.push_back("grounded_segment_claim_limit");
          continue;
        }
        if (has_typed_claim) {
          const auto index = segment.claim_indices.front();
          const auto& claim = content.claims[index];
          if (claim.kind == CoachClaimKind::general ||
              claim.answer_quote != segment.text) {
            issues.push_back("grounded_segment_claim_mismatch");
            continue;
          }
          linked[index] = true;
        }
      } else if (!segment.claim_indices.empty() || !segment.fact_ids.empty()) {
        issues.push_back("ungrounded_segment_has_grounding_metadata");
      }

      if (segment.kind == CoachSegmentKind::dialogue &&
          segment.text.size() > 100) {
        issues.push_back("dialogue_segment_too_long");
      }
    }
  };
  inspect(content.answer_segments);
  inspect(content.follow_up_segments);
  for (std::size_t i = 0; i < content.claims.size(); ++i)
    if (content.claims[i].kind != CoachClaimKind::general && !linked[i])
      issues.push_back("claim_not_linked_to_segment");
}

}  // namespace

// -----------------------------------------------------------------------------
// Section: Validate supplied references, personal facts and chess facts
// -----------------------------------------------------------------------------

ResponseValidationReport ResponseValidator::validate(
    const StructuredCoachContent& content,
    const std::optional<std::string>& position_fen,
    const std::vector<EvidenceItem>& evidence,
    const std::vector<EvidenceItem>* supplied_evidence,
    const bool profile_requested,
    const bool require_grounded_segments,
    const std::optional<ChessVerdictContract>* expected_verdict,
    const std::optional<ChessVerdictReview>* expected_review) const {
  ResponseValidationReport report;
  const auto grounding = supplied_grounding(
      supplied_evidence ? *supplied_evidence : evidence);
  const auto& visible_evidence = supplied_evidence ? *supplied_evidence : evidence;
  report.issues = profile_contract_issues(content, grounding, profile_requested);
  validate_verdict_lock(content, expected_verdict, expected_review, report.issues);
  if (require_grounded_segments)
    validate_segments(content, grounding, report.issues);

  const auto check_refs = [&](const std::vector<std::string>& ids) {
    if (!references_supplied(ids, grounding)) {
      report.issues.push_back("evidence_reference_not_supplied");
    }
  };
  check_refs(content.evidence_ids);
  for (const auto& concept_item : content.concepts) {
    check_refs(concept_item.evidence_ids);
  }
  for (const auto& recommendation : content.recommendations) check_refs(recommendation.evidence_ids);

  ChessValidator chess;
  auto supported_moves = supplied_coordinate_moves(visible_evidence);
  for (const auto& claim : content.claims) {
    check_refs(claim.evidence_ids);
    if (require_grounded_segments && claim.kind != CoachClaimKind::general &&
        !answer_quote_present(claim, content))
      report.issues.push_back("claim_answer_quote_missing");
    if (claim.kind == CoachClaimKind::profile_fact ||
        claim.kind == CoachClaimKind::profile_inference) {
      continue;
    }
    const auto result = claim.kind == CoachClaimKind::position_contrast_fact
        ? validate_contrast_fact(claim, visible_evidence)
        : validate_claim(chess, claim, position_fen, visible_evidence);
    if (!result.valid) report.issues.push_back(result.error_code);
    else if (claim.kind == CoachClaimKind::legal_move ||
             claim.kind == CoachClaimKind::gives_check ||
             claim.kind == CoachClaimKind::gives_mate)
      supported_moves.insert(claim.subject);
  }
  if (position_fen) {
    if (prose_has_ungrounded_coordinate_move(content.answer, supported_moves) ||
        prose_has_ungrounded_coordinate_move(
            content.follow_up_question, supported_moves)) {
      report.issues.push_back("prose_coordinate_move_not_grounded");
    }
    for (const auto& recommendation : content.recommendations) {
      const auto result = chess.validate_recommendation(
          recommendation, *position_fen, visible_evidence);
      if (!result.valid) report.issues.push_back(result.error_code);
    }
  }
  report.valid = report.issues.empty();
  return report;
}

ResponseValidationReport ResponseValidator::sanitize_metadata(
    StructuredCoachContent& content,
    const std::optional<std::string>& position_fen,
    const std::vector<EvidenceItem>& evidence,
    const std::vector<EvidenceItem>* supplied_evidence,
    const bool profile_requested) const {
  ResponseValidationReport report;
  const auto grounding = supplied_grounding(
      supplied_evidence ? *supplied_evidence : evidence);
  const auto& visible_evidence = supplied_evidence ? *supplied_evidence : evidence;
  report.issues = profile_contract_issues(content, grounding, profile_requested);
  if (!report.issues.empty()) {
    report.valid = false;
    return report;
  }
  ChessValidator chess;

  // Optional non-profile metadata may be stripped after the single repair pass.
  // Personal grounding failures above remain fatal because prose cannot repair
  // an unsupported assertion merely by dropping its claim metadata.
  const auto sanitize_refs = [&](std::vector<std::string>& ids) {
    std::erase_if(ids, [&](const auto& id) {
      if (grounding.evidence_ids.contains(id)) return false;
      report.issues.push_back("evidence_reference_not_supplied");
      return true;
    });
  };
  sanitize_refs(content.evidence_ids);
  for (auto& concept_item : content.concepts) {
    sanitize_refs(concept_item.evidence_ids);
  }
  for (auto& recommendation : content.recommendations) sanitize_refs(recommendation.evidence_ids);

  std::vector<CoachClaim> safe_claims;
  safe_claims.reserve(content.claims.size());
  for (auto& claim : content.claims) {
    if (claim.kind == CoachClaimKind::profile_fact ||
        claim.kind == CoachClaimKind::profile_inference) {
      safe_claims.push_back(std::move(claim));
      continue;
    }
    sanitize_refs(claim.evidence_ids);
    const auto result = validate_claim(chess, claim, position_fen, visible_evidence);
    if (result.valid) {
      safe_claims.push_back(std::move(claim));
    } else {
      report.issues.push_back(result.error_code);
    }
  }
  content.claims = std::move(safe_claims);

  if (position_fen) {
    std::vector<CoachRecommendation> safe_recommendations;
    safe_recommendations.reserve(content.recommendations.size());
    for (auto& recommendation : content.recommendations) {
      const auto result = chess.validate_recommendation(
          recommendation, *position_fen, visible_evidence);
      if (result.valid) {
        safe_recommendations.push_back(std::move(recommendation));
      } else {
        report.issues.push_back(result.error_code);
      }
    }
    content.recommendations = std::move(safe_recommendations);
  }

  report.valid = validate(content, position_fen, evidence, supplied_evidence,
                          profile_requested).valid;
  return report;
}

}  // namespace kchess::ai
