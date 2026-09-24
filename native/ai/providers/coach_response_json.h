#pragma once

#include <iterator>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <nlohmann/json.hpp>
#include "../dto/structured_coach_response.h"

namespace kchess::ai::coach_response_json {
using json = nlohmann::json;

// Section: Provider wire contract (native validation remains authoritative)
inline constexpr std::string_view claim_names[] = {
    "general", "legal_move", "gives_check", "gives_mate", "material_cp",
    "piece_on_square", "tactical_motif", "engine_evaluation", "opening_fact",
    "profile_fact", "profile_inference", "position_contrast_fact"};
inline constexpr std::string_view segment_names[] = {
    "factual", "general", "uncertainty", "dialogue"};

inline std::string join_segments(const std::vector<CoachAnswerSegment>& segments) {
  std::string out;
  for (const auto& segment : segments) {
    if (!out.empty()) out += ' ';
    out += segment.text;
  }
  return out;
}

inline std::vector<CoachAnswerSegment> parse_segments(const json& values) {
  if (!values.is_array() || values.size() > 16)
    throw std::invalid_argument("coach_segment_bounds");
  std::vector<CoachAnswerSegment> segments;
  for (const auto& entry : values) {
    CoachAnswerSegment segment;
    segment.text = entry.at("text").get<std::string>();
    const auto kind = entry.at("kind").get<std::string>();
    bool found = false;
    for (std::size_t i = 0; i < std::size(segment_names); ++i) {
      if (segment_names[i] != kind) continue;
      segment.kind = static_cast<CoachSegmentKind>(i);
      found = true;
      break;
    }
    if (!found || segment.text.empty() || segment.text.size() > 800)
      throw std::invalid_argument("coach_segment_invalid");
    segment.claim_indices =
        entry.at("claim_indices").get<std::vector<std::size_t>>();
    if (segment.claim_indices.size() > 16)
      throw std::invalid_argument("coach_segment_claim_limit");
    segments.push_back(std::move(segment));
  }
  return segments;
}

inline json schema() {
  const json text{{"type", "string"}};
  const json refs{{"type", "array"}, {"items", text}};
  const auto object = [](json properties, json required) {
    return json{{"type", "object"}, {"properties", properties}, {"required", required}};
  };
  const auto array = [](json item) {
    return json{{"type", "array"}, {"items", item}};
  };
  json kinds = json::array();
  for (auto name : claim_names) kinds.push_back(std::string(name));
  json segment_kinds = json::array();
  for (auto name : segment_names) segment_kinds.push_back(std::string(name));
  const json segment = object({
      {"text", text},
      {"kind", {{"type", "string"}, {"enum", segment_kinds}}},
      {"claim_indices", array(json{{"type", "integer"}})}},
      {"text", "kind", "claim_indices"});
  return object({
      {"answer_segments", array(segment)},
      {"follow_up_segments", array(segment)}, {"evidence_ids", refs},
      {"profile_status", {{"type", "string"},
          {"enum", {"not_used", "grounded", "insufficient_evidence"}}}},
      {"claims", array(object({
          {"kind", {{"type", "string"}, {"enum", kinds}}},
          {"text", text}, {"answer_quote", text}, {"subject", text}, {"value", text}, {"evidence_ids", refs},
          {"epistemic_status", text}, {"support_subjects", refs}},
          {"kind", "text", "answer_quote", "subject", "value", "evidence_ids"}))},
      {"recommendations", array(object({
          {"move_uci", text}, {"text", text}, {"evidence_ids", refs}},
          {"move_uci", "text", "evidence_ids"}))}},
      {"answer_segments", "follow_up_segments", "profile_status"});
}

inline StructuredCoachContent parse(json value) {
  for (const auto* field : {"follow_up_segments", "evidence_ids", "claims",
                            "recommendations"})
    if (!value.contains(field)) value[field] = json::array();
  StructuredCoachContent result;
  result.answer_segments = parse_segments(value.at("answer_segments"));
  result.follow_up_segments = parse_segments(value.at("follow_up_segments"));
  result.answer = join_segments(result.answer_segments);
  result.follow_up_question = join_segments(result.follow_up_segments);
  result.evidence_ids = value.at("evidence_ids").get<std::vector<std::string>>();
  result.profile_status = value.value("profile_status", "");
  if (!value.at("claims").is_array() || !value.at("recommendations").is_array() ||
      value.at("claims").size() > 16 || value.at("recommendations").size() > 3 ||
      result.answer.size() > 12000 || result.follow_up_question.size() > 800) {
    throw std::invalid_argument("coach_output_bounds");
  }
  for (const auto& entry : value.at("claims")) {
    CoachClaim claim;
    const auto kind = entry.at("kind").get<std::string>();
    bool found = false;
    for (std::size_t i = 0; i < std::size(claim_names); ++i) {
      if (claim_names[i] == kind) {
        claim.kind = static_cast<CoachClaimKind>(i);
        found = true;
        break;
      }
    }
    if (!found) throw std::invalid_argument("coach_claim_kind_unknown");
    claim.text = entry.at("text").get<std::string>();
    claim.answer_quote = entry.value("answer_quote", "");
    claim.subject = entry.at("subject").get<std::string>();
    claim.value = entry.at("value").get<std::string>();
    claim.evidence_ids = entry.at("evidence_ids").get<std::vector<std::string>>();
    claim.epistemic_status = entry.value("epistemic_status", "");
    if (entry.contains("support_subjects") && entry["support_subjects"].is_array()) {
      claim.support_subjects = entry["support_subjects"].get<std::vector<std::string>>();
    }
    result.claims.push_back(std::move(claim));
  }
  for (const auto& entry : value.at("recommendations")) {
    CoachRecommendation recommendation;
    recommendation.move_uci = entry.at("move_uci").get<std::string>();
    recommendation.text = entry.at("text").get<std::string>();
    recommendation.evidence_ids = entry.at("evidence_ids").get<std::vector<std::string>>();
    result.recommendations.push_back(std::move(recommendation));
  }
  return result;
}

inline json serialize(const StructuredCoachContent& content) {
  json result{{"answer", content.answer}, {"follow_up_question", content.follow_up_question},
              {"answer_segments", json::array()},
              {"follow_up_segments", json::array()},
              {"evidence_ids", content.evidence_ids}, {"claims", json::array()},
              {"recommendations", json::array()}, {"profile_status", content.profile_status}};
  const auto append_segments = [&](const char* key,
                                   const std::vector<CoachAnswerSegment>& segments) {
    for (const auto& segment : segments)
      result[key].push_back({{"text", segment.text},
          {"kind", std::string(segment_names[static_cast<int>(segment.kind)])},
          {"claim_indices", segment.claim_indices}});
  };
  append_segments("answer_segments", content.answer_segments);
  append_segments("follow_up_segments", content.follow_up_segments);
  for (const auto& claim : content.claims) {
    result["claims"].push_back({{"kind", std::string(claim_names[static_cast<int>(claim.kind)])},
        {"text", claim.text}, {"answer_quote", claim.answer_quote},
        {"subject", claim.subject}, {"value", claim.value},
        {"evidence_ids", claim.evidence_ids}, {"epistemic_status", claim.epistemic_status},
        {"support_subjects", claim.support_subjects}});
  }
  for (const auto& recommendation : content.recommendations) {
    result["recommendations"].push_back({{"move_uci", recommendation.move_uci},
        {"text", recommendation.text}, {"evidence_ids", recommendation.evidence_ids}});
  }
  return result;
}
}  // namespace kchess::ai::coach_response_json
