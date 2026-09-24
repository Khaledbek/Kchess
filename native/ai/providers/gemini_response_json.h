#pragma once

#include <algorithm>
#include <iterator>
#include <optional>
#include <regex>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "../dto/evidence.h"
#include "../dto/structured_coach_response.h"

namespace kchess::ai::gemini_json {
using json = nlohmann::json;

// Section: Provider wire contract (native validation remains authoritative)
inline constexpr std::string_view claim_names[] = {
    "general", "legal_move", "gives_check", "gives_mate", "material_cp",
    "piece_on_square", "tactical_motif", "engine_evaluation", "opening_fact",
    "profile_fact", "profile_inference", "position_contrast_fact"};
inline constexpr std::string_view segment_purposes[] = {
    "explanation", "question", "uncertainty", "transition"};

// coach_response.v10 keeps provider-authored typed claims only for evidence
// domains that have not yet migrated to native chess fact IDs. Current-board
// chess truth (moves, checks, mates, evaluations, piece placement and motifs)
// must be expressed through fact_ids instead of a parallel claim language.
inline bool provider_typed_claim_allowed(const CoachClaimKind kind) {
  return kind == CoachClaimKind::opening_fact ||
      kind == CoachClaimKind::profile_fact ||
      kind == CoachClaimKind::profile_inference ||
      kind == CoachClaimKind::position_contrast_fact;
}

struct CandidateReference {
  std::string id;
  std::string move_uci;
};

struct MoveReference {
  std::string token;
  std::string move_uci;
};

inline std::string candidate_move_token(std::string_view candidate_id) {
  return "<<move:" + std::string(candidate_id) + ">>";
}

inline std::string fact_move_token(std::string_view fact_id) {
  return "<<fact:" + std::string(fact_id) + ">>";
}

inline std::string join_uci_line(const json& pv) {
  if (!pv.is_array()) return {};
  std::string out;
  for (const auto& move : pv) {
    if (!move.is_string()) continue;
    if (!out.empty()) out += ' ';
    out += move.get<std::string>();
  }
  return out;
}

inline void append_move_reference(std::vector<MoveReference>& result,
                                  std::string token,
                                  std::string move_uci) {
  if (token.empty() || move_uci.empty()) return;
  const auto duplicate = std::find_if(
      result.begin(), result.end(), [&](const MoveReference& ref) {
        return ref.token == token;
      });
  if (duplicate == result.end()) {
    result.push_back({std::move(token), std::move(move_uci)});
  }
}

inline std::vector<MoveReference> move_references(
    const std::vector<EvidenceItem>& evidence) {
  std::vector<MoveReference> result;
  for (const auto& item : evidence) {
    if (item.kind != EvidenceKind::candidate_moves) continue;
    const auto payload = json::parse(item.payload, nullptr, false);
    if (!payload.is_object()) continue;

    const auto append_candidate = [&](const json& candidate) {
      if (!candidate.is_object()) return;
      const auto id = candidate.value("candidate_id", "");
      const auto move = candidate.value("move_uci", "");
      if (!id.empty() && !move.empty()) {
        append_move_reference(result, candidate_move_token(id), move);
      }
    };
    if (payload.contains("best")) append_candidate(payload["best"]);
    if (payload.contains("focus")) append_candidate(payload["focus"]);
    if (payload.contains("alternatives") && payload["alternatives"].is_array()) {
      for (const auto& candidate : payload["alternatives"]) append_candidate(candidate);
    }
    if (payload.contains("user_move")) append_candidate(payload["user_move"]);

    if (payload.contains("facts") && payload["facts"].is_array()) {
      for (const auto& fact : payload["facts"]) {
        if (!fact.is_object()) continue;
        const auto id = fact.value("fact_id", "");
        const auto move = fact.value("move_uci", "");
        if (!id.empty() && !move.empty()) {
          append_move_reference(result, fact_move_token(id), move);
        }
        if (!id.empty() && fact.contains("pv_uci")) {
          const auto pv = join_uci_line(fact["pv_uci"]);
          if (!pv.empty()) append_move_reference(result, fact_move_token(id), pv);
        }
      }
    }
  }
  return result;
}

inline std::string replace_all(std::string text,
                               std::string_view from,
                               std::string_view to) {
  if (from.empty()) return text;
  std::size_t pos = 0;
  while ((pos = text.find(from, pos)) != std::string::npos) {
    text.replace(pos, from.size(), to);
    pos += to.size();
  }
  return text;
}

inline bool contains_raw_coordinate_move(std::string_view text) {
  static const std::regex coordinate_move(
      R"((^|[^A-Za-z0-9])([a-h][1-8][a-h][1-8][qrbn]?)(?=$|[^A-Za-z0-9]))",
      std::regex::icase);
  return std::regex_search(text.begin(), text.end(), coordinate_move);
}

inline std::string validate_move_tokens(
    std::string text, const std::vector<MoveReference>& refs) {
  // Provider prose is kept tokenized through parsing and native validation.
  // Concrete notation is resolved only by VerifiedFactRenderer after the
  // response has crossed the validation boundary.
  if (contains_raw_coordinate_move(text)) {
    throw std::invalid_argument("coach_provider_authored_move_notation");
  }
  auto remaining = text;
  for (const auto& ref : refs) {
    remaining = replace_all(std::move(remaining), ref.token, "");
  }
  if (remaining.find("<<") != std::string::npos ||
      remaining.find(">>") != std::string::npos) {
    // Reject every unknown internal marker, including legacy <<pv:...>> tokens.
    // Only native move/fact references removed above may cross this boundary.
    throw std::invalid_argument("coach_internal_reference_unknown");
  }
  return text;
}

inline std::string tokenize_native_moves(
    std::string text, const std::vector<MoveReference>& refs) {
  // Longest moves first so promotion forms are not partially replaced.
  auto ordered = refs;
  std::stable_sort(ordered.begin(), ordered.end(), [](const auto& a, const auto& b) {
    return a.move_uci.size() > b.move_uci.size();
  });
  std::set<std::string> replaced_moves;
  for (const auto& ref : ordered) {
    if (!replaced_moves.insert(ref.move_uci).second) continue;
    text = replace_all(std::move(text), ref.move_uci, ref.token);
  }
  return text;
}

inline bool move_bound_claim(const CoachClaimKind kind) {
  return kind == CoachClaimKind::legal_move ||
      kind == CoachClaimKind::gives_check ||
      kind == CoachClaimKind::gives_mate ||
      kind == CoachClaimKind::engine_evaluation;
}

inline std::vector<CandidateReference> candidate_references(
    const std::vector<EvidenceItem>& evidence) {
  std::vector<CandidateReference> result;
  const auto append = [&](const json& candidate) {
    if (!candidate.is_object()) return;
    const auto id = candidate.value("candidate_id", "");
    const auto move = candidate.value("move_uci", "");
    if (id.empty() || move.empty()) return;
    const auto duplicate = std::find_if(
        result.begin(), result.end(), [&](const CandidateReference& ref) {
          return ref.id == id;
        });
    if (duplicate == result.end()) result.push_back({id, move});
  };

  for (const auto& item : evidence) {
    if (item.kind != EvidenceKind::candidate_moves) continue;
    const auto payload = json::parse(item.payload, nullptr, false);
    if (!payload.is_object()) continue;
    if (payload.contains("best")) append(payload["best"]);
    if (payload.contains("focus")) append(payload["focus"]);
    if (payload.contains("alternatives") && payload["alternatives"].is_array()) {
      for (const auto& candidate : payload["alternatives"]) append(candidate);
    }
    if (payload.contains("user_move")) append(payload["user_move"]);
  }
  return result;
}


inline std::vector<std::string> fact_references(
    const std::vector<EvidenceItem>& evidence) {
  std::vector<std::string> result;
  for (const auto& item : evidence) {
    if (item.kind != EvidenceKind::candidate_moves) continue;
    const auto payload = json::parse(item.payload, nullptr, false);
    if (!payload.is_object() || !payload.contains("facts") ||
        !payload["facts"].is_array()) {
      continue;
    }
    for (const auto& fact : payload["facts"]) {
      if (!fact.is_object()) continue;
      const auto id = fact.value("fact_id", "");
      if (!id.empty() &&
          std::find(result.begin(), result.end(), id) == result.end()) {
        result.push_back(id);
      }
    }
  }
  return result;
}

inline CoachSegmentKind segment_kind_for_purpose(
    std::string_view purpose, bool grounded) {
  if (grounded) return CoachSegmentKind::factual;
  if (purpose == "uncertainty") return CoachSegmentKind::uncertainty;
  if (purpose == "question" || purpose == "transition")
    return CoachSegmentKind::dialogue;
  return CoachSegmentKind::general;
}

inline std::string_view purpose_for_segment_kind(CoachSegmentKind kind) {
  switch (kind) {
    case CoachSegmentKind::uncertainty: return "uncertainty";
    case CoachSegmentKind::dialogue: return "question";
    case CoachSegmentKind::factual:
    case CoachSegmentKind::general: return "explanation";
  }
  return "explanation";
}

inline std::optional<std::string> move_for_candidate(
    const std::vector<CandidateReference>& refs, std::string_view id) {
  const auto found = std::find_if(
      refs.begin(), refs.end(), [&](const CandidateReference& ref) {
        return ref.id == id;
      });
  return found == refs.end() ? std::nullopt
                             : std::optional<std::string>(found->move_uci);
}

inline std::optional<std::string> candidate_for_move(
    const std::vector<CandidateReference>& refs, std::string_view move_uci) {
  const auto found = std::find_if(
      refs.begin(), refs.end(), [&](const CandidateReference& ref) {
        return ref.move_uci == move_uci;
      });
  return found == refs.end() ? std::nullopt
                             : std::optional<std::string>(found->id);
}

inline std::string join_segments(const std::vector<CoachAnswerSegment>& segments) {
  std::string out;
  for (const auto& segment : segments) {
    if (!out.empty()) out += ' ';
    out += segment.text;
  }
  return out;
}

inline bool parse_claim_kind(const std::string& value, CoachClaimKind& kind) {
  for (std::size_t i = 0; i < std::size(claim_names); ++i) {
    if (claim_names[i] != value) continue;
    kind = static_cast<CoachClaimKind>(i);
    return true;
  }
  return false;
}

inline std::vector<CoachAnswerSegment> parse_segments(
    const json& values,
    const std::vector<CandidateReference>&,
    const std::vector<MoveReference>& move_refs,
    const std::vector<std::string>& fact_refs,
    std::vector<CoachClaim>& claims) {
  if (!values.is_array() || values.size() > 16)
    throw std::invalid_argument("coach_segment_bounds");
  std::vector<CoachAnswerSegment> segments;
  for (const auto& entry : values) {
    CoachAnswerSegment segment;
    segment.text = validate_move_tokens(
        entry.at("text").get<std::string>(), move_refs);
    const auto purpose = entry.at("purpose").get<std::string>();
    if (std::find(std::begin(segment_purposes), std::end(segment_purposes),
                  purpose) == std::end(segment_purposes) ||
        segment.text.empty() || segment.text.size() > 800) {
      throw std::invalid_argument("coach_segment_invalid");
    }

    if (entry.contains("fact_ids") && entry["fact_ids"].is_array()) {
      segment.fact_ids = entry["fact_ids"].get<std::vector<std::string>>();
      if (segment.fact_ids.size() > 16)
        throw std::invalid_argument("coach_segment_fact_limit");
      for (const auto& id : segment.fact_ids) {
        if (std::find(fact_refs.begin(), fact_refs.end(), id) == fact_refs.end())
          throw std::invalid_argument("coach_segment_fact_unknown");
      }
    }

    // v7 no longer asks the provider to classify prose as factual/general.
    // A segment becomes grounded natively when it references verified fact IDs
    // or carries typed non-board grounding metadata.
    bool has_typed_claim = false;
    if (entry.contains("claim_kind") && entry["claim_kind"].is_string()) {
      CoachClaim claim;
      if (!parse_claim_kind(entry["claim_kind"].get<std::string>(), claim.kind) ||
          !provider_typed_claim_allowed(claim.kind)) {
        throw std::invalid_argument("coach_typed_claim_not_allowed");
      }
      claim.text = segment.text;
      claim.answer_quote = segment.text;
      claim.subject = entry.value("subject", "");
      claim.value = entry.value("value", "");
      if (entry.contains("evidence_ids") && entry["evidence_ids"].is_array())
        claim.evidence_ids = entry["evidence_ids"].get<std::vector<std::string>>();
      claim.epistemic_status = entry.value("epistemic_status", "");
      if (entry.contains("support_subjects") &&
          entry["support_subjects"].is_array()) {
        claim.support_subjects =
            entry["support_subjects"].get<std::vector<std::string>>();
      }
      if (claim.evidence_ids.size() > 16 || claim.support_subjects.size() > 16)
        throw std::invalid_argument("coach_segment_claim_limit");
      segment.claim_indices.push_back(claims.size());
      claims.push_back(std::move(claim));
      has_typed_claim = true;
    }
    segment.kind = segment_kind_for_purpose(
        purpose, !segment.fact_ids.empty() || has_typed_claim);
    segments.push_back(std::move(segment));
  }
  return segments;
}

inline json schema(const bool profile_requested,
                   const std::vector<EvidenceItem>& evidence,
                   const std::size_t max_recommendations,
                   const std::optional<ChessVerdictContract>& chess_verdict,
                   const std::optional<ChessVerdictReview>& verdict_review) {
  const json text{{"type", "string"}};
  const json refs{{"type", "array"}, {"items", text}};
  const auto object = [](json properties, json required) {
    return json{{"type", "object"}, {"properties", properties}, {"required", required}};
  };
  const auto array = [](json item) {
    return json{{"type", "array"}, {"items", item}};
  };
  const auto candidate_refs = candidate_references(evidence);
  const auto facts = fact_references(evidence);
  json kinds = json::array();
  for (std::size_t i = 0; i < std::size(claim_names); ++i) {
    const auto kind = static_cast<CoachClaimKind>(i);
    if (provider_typed_claim_allowed(kind))
      kinds.push_back(std::string(claim_names[i]));
  }
  json purposes = json::array();
  for (auto name : segment_purposes) purposes.push_back(std::string(name));

  json candidate_ids = json::array();
  for (const auto& ref : candidate_refs) candidate_ids.push_back(ref.id);
  const json candidate_id = candidate_ids.empty()
      ? text
      : json{{"type", "string"}, {"enum", candidate_ids}};

  json fact_ids = json::array();
  for (const auto& id : facts) fact_ids.push_back(id);
  const json fact_refs = facts.empty()
      ? json{{"type", "array"}, {"items", text}, {"maxItems", 0}}
      : json{{"type", "array"},
             {"items", json{{"type", "string"}, {"enum", fact_ids}}},
             {"maxItems", 16}};

  // v7 separates rhetorical purpose from grounding. The provider never labels
  // a sentence factual/nonfactual; it selects native fact IDs when a current-
  // position assertion depends on verified engine truth. Typed claim metadata
  // remains available for profile/opening/contrast evidence until those paths
  // migrate to native fact rendering.
  const json segment = object({
      {"text", text},
      {"purpose", {{"type", "string"}, {"enum", purposes}}},
      {"fact_ids", fact_refs},
      {"claim_kind", {{"type", "string"}, {"enum", kinds}}},
      {"subject", text},
      {"value", text},
      {"evidence_ids", refs},
      {"epistemic_status", text},
      {"support_subjects", refs}},
      {"text", "purpose", "fact_ids"});

  const json profile_status = profile_requested
      ? json{{"type", "string"}, {"enum", {"grounded", "insufficient_evidence"}}}
      : json{{"type", "string"}, {"enum", {"not_used"}}};

  const bool verdict_lock_active =
      chess_verdict.has_value() && chess_verdict->authoritative;
  const std::string expected_position_verdict = verdict_lock_active
      ? std::string(position_verdict_name(chess_verdict->position_verdict))
      : std::string{"unknown"};
  const std::string expected_move_verdict = verdict_lock_active
      ? std::string(move_verdict_name(chess_verdict->move_verdict))
      : std::string{"unknown"};
  const std::string expected_review_outcome =
      verdict_review.has_value() && verdict_review->active
      ? std::string(verdict_review_outcome_name(verdict_review->outcome))
      : std::string{"none"};
  const json verdict_lock = object({
      {"active", {{"type", "boolean"}, {"enum", {verdict_lock_active}}}},
      {"positionVerdict", {{"type", "string"}, {"enum", {expected_position_verdict}}}},
      {"moveVerdict", {{"type", "string"}, {"enum", {expected_move_verdict}}}},
      {"reviewOutcome", {{"type", "string"}, {"enum", {expected_review_outcome}}}}},
      {"active", "positionVerdict", "moveVerdict", "reviewOutcome"});

  const json verdict_summary = verdict_lock_active
      ? json{{"type", "string"}, {"minLength", 1}, {"maxLength", 240}}
      : json{{"type", "string"}, {"enum", {""}}};

  json recommendations = array(object({
      {"candidate_id", candidate_id}, {"text", text}, {"evidence_ids", refs}},
      {"candidate_id", "text", "evidence_ids"}));
  recommendations["maxItems"] = candidate_refs.empty()
      ? 0
      : static_cast<nlohmann::json::number_unsigned_t>(max_recommendations);

  return object({
      {"verdict_summary", verdict_summary},
      {"answer_segments", array(segment)},
      {"follow_up_segments", array(segment)},
      {"evidence_ids", refs},
      {"profile_status", profile_status},
      {"verdict_lock", verdict_lock},
      {"recommendations", std::move(recommendations)}},
      {"verdict_summary", "answer_segments", "follow_up_segments",
       "profile_status", "verdict_lock"});
}

inline StructuredCoachContent parse(json value,
                                    const std::vector<EvidenceItem>& evidence) {
  for (const auto* field : {"evidence_ids", "recommendations"})
    if (!value.contains(field)) value[field] = json::array();
  const auto candidate_refs = candidate_references(evidence);
  const auto move_refs = move_references(evidence);
  const auto fact_refs = fact_references(evidence);
  StructuredCoachContent result;
  result.verdict_summary = value.at("verdict_summary").get<std::string>();
  result.answer_segments = parse_segments(
      value.at("answer_segments"), candidate_refs, move_refs, fact_refs, result.claims);
  result.follow_up_segments = parse_segments(
      value.at("follow_up_segments"), candidate_refs, move_refs, fact_refs, result.claims);
  result.answer = result.verdict_summary;
  const auto explanation = join_segments(result.answer_segments);
  if (!explanation.empty()) {
    if (!result.answer.empty()) result.answer += ' ';
    result.answer += explanation;
  }
  result.follow_up_question = join_segments(result.follow_up_segments);
  result.evidence_ids = value.at("evidence_ids").get<std::vector<std::string>>();
  result.profile_status = value.value("profile_status", "");
  if (!value.contains("verdict_lock") || !value["verdict_lock"].is_object())
    throw std::invalid_argument("coach_verdict_lock_missing");
  const auto& verdict_lock = value["verdict_lock"];
  result.verdict_lock.active = verdict_lock.at("active").get<bool>();
  result.verdict_lock.position_verdict =
      verdict_lock.at("positionVerdict").get<std::string>();
  result.verdict_lock.move_verdict =
      verdict_lock.at("moveVerdict").get<std::string>();
  result.verdict_lock.review_outcome =
      verdict_lock.at("reviewOutcome").get<std::string>();
  if (!value.at("recommendations").is_array() ||
      result.claims.size() > 16 || value.at("recommendations").size() > 3 ||
      result.evidence_ids.size() > 32 || result.answer.size() > 12000 ||
      result.follow_up_question.size() > 800) {
    throw std::invalid_argument("coach_output_bounds");
  }
  for (const auto& entry : value.at("recommendations")) {
    CoachRecommendation recommendation;
    recommendation.candidate_id = entry.at("candidate_id").get<std::string>();
    const auto move = move_for_candidate(candidate_refs, recommendation.candidate_id);
    if (!move) throw std::invalid_argument("coach_recommendation_candidate_unknown");
    recommendation.move_uci = *move;
    recommendation.text = validate_move_tokens(
        entry.at("text").get<std::string>(), move_refs);
    recommendation.evidence_ids =
        entry.at("evidence_ids").get<std::vector<std::string>>();
    result.recommendations.push_back(std::move(recommendation));
  }
  return result;
}

inline json serialize(const StructuredCoachContent& content,
                      const std::vector<EvidenceItem>& evidence) {
  const auto candidate_refs = candidate_references(evidence);
  const auto move_refs = move_references(evidence);
  json result{{"verdict_summary", content.verdict_summary},
              {"answer_segments", json::array()},
              {"follow_up_segments", json::array()},
              {"evidence_ids", content.evidence_ids},
              {"recommendations", json::array()},
              {"profile_status", content.profile_status},
              {"verdict_lock",
               {{"active", content.verdict_lock.active},
                {"positionVerdict", content.verdict_lock.position_verdict},
                {"moveVerdict", content.verdict_lock.move_verdict},
                {"reviewOutcome", content.verdict_lock.review_outcome}}}};
  const auto append_segments = [&](const char* key,
                                   const std::vector<CoachAnswerSegment>& segments) {
    for (const auto& segment : segments) {
      json entry{{"text", tokenize_native_moves(segment.text, move_refs)},
                 {"purpose", std::string(purpose_for_segment_kind(segment.kind))},
                 {"fact_ids", segment.fact_ids}};
      if (segment.claim_indices.size() == 1 &&
          segment.claim_indices.front() < content.claims.size()) {
        const auto& claim = content.claims[segment.claim_indices.front()];
        if (provider_typed_claim_allowed(claim.kind)) {
          entry["claim_kind"] =
              std::string(claim_names[static_cast<int>(claim.kind)]);
          entry["subject"] = claim.subject;
          entry["value"] = claim.value;
          entry["evidence_ids"] = claim.evidence_ids;
          entry["epistemic_status"] = claim.epistemic_status;
          entry["support_subjects"] = claim.support_subjects;
        }
      }
      result[key].push_back(std::move(entry));
    }
  };
  append_segments("answer_segments", content.answer_segments);
  append_segments("follow_up_segments", content.follow_up_segments);
  for (const auto& recommendation : content.recommendations) {
    auto id = recommendation.candidate_id.empty()
        ? candidate_for_move(candidate_refs, recommendation.move_uci)
        : std::optional<std::string>(recommendation.candidate_id);
    if (!id) continue;
    result["recommendations"].push_back({{"candidate_id", *id},
        {"text", tokenize_native_moves(recommendation.text, move_refs)},
         {"evidence_ids", recommendation.evidence_ids}});
  }
  return result;
}
}  // namespace kchess::ai::gemini_json
