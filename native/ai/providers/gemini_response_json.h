#pragma once

#include <stdexcept>
#include <string_view>
#include <utility>
#include <nlohmann/json.hpp>
#include "../dto/structured_coach_response.h"

namespace kchess::ai::gemini_json {
using json = nlohmann::json;

// Section: Provider wire contract (native validation remains authoritative)
inline constexpr std::string_view claim_names[] = {
    "general", "legal_move", "gives_check", "gives_mate", "material_cp",
    "piece_on_square", "tactical_motif", "engine_evaluation", "opening_fact"};

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
  return object({
      {"answer", text}, {"follow_up_question", text}, {"evidence_ids", refs},
      {"claims", array(object({
          {"kind", {{"type", "string"}, {"enum", kinds}}},
          {"text", text}, {"subject", text}, {"value", text}, {"evidence_ids", refs}},
          {"kind", "text", "subject", "value", "evidence_ids"}))},
      {"recommendations", array(object({
          {"move_uci", text}, {"text", text}, {"evidence_ids", refs}},
          {"move_uci", "text", "evidence_ids"}))}},
      {"answer"});
}

inline StructuredCoachContent parse(json value) {
  if (!value.contains("follow_up_question")) value["follow_up_question"] = "";
  for (const auto* field : {"evidence_ids", "claims", "recommendations"})
    if (!value.contains(field)) value[field] = json::array();
  StructuredCoachContent result;
  result.answer = value.at("answer").get<std::string>();
  result.follow_up_question = value.at("follow_up_question").get<std::string>();
  result.evidence_ids = value.at("evidence_ids").get<std::vector<std::string>>();
  if (!value.at("claims").is_array() || !value.at("recommendations").is_array() ||
      value.at("claims").size() > 16 || value.at("recommendations").size() > 3 ||
      result.answer.size() > 12000 || result.follow_up_question.size() > 800) {
    throw std::invalid_argument("coach_output_bounds");
  }
  for (const auto& entry : value.at("claims")) {
    CoachClaim claim;
    const auto kind = entry.at("kind").get<std::string>();
    bool found = false;
    for (std::size_t i = 0; i < 9; ++i) {
      if (claim_names[i] == kind) {
        claim.kind = static_cast<CoachClaimKind>(i);
        found = true;
        break;
      }
    }
    if (!found) throw std::invalid_argument("coach_claim_kind_unknown");
    claim.text = entry.at("text").get<std::string>();
    claim.subject = entry.at("subject").get<std::string>();
    claim.value = entry.at("value").get<std::string>();
    claim.evidence_ids = entry.at("evidence_ids").get<std::vector<std::string>>();
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
              {"evidence_ids", content.evidence_ids}, {"claims", json::array()},
              {"recommendations", json::array()}};
  for (const auto& claim : content.claims) {
    result["claims"].push_back({{"kind", std::string(claim_names[static_cast<int>(claim.kind)])},
        {"text", claim.text}, {"subject", claim.subject}, {"value", claim.value},
        {"evidence_ids", claim.evidence_ids}});
  }
  for (const auto& recommendation : content.recommendations) {
    result["recommendations"].push_back({{"move_uci", recommendation.move_uci},
        {"text", recommendation.text}, {"evidence_ids", recommendation.evidence_ids}});
  }
  return result;
}
}  // namespace kchess::ai::gemini_json
