#include "verified_fact_renderer.h"

#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>
#include <string_view>
#include <utility>

#include <nlohmann/json.hpp>

namespace kchess::ai {
namespace {
using json = nlohmann::json;

struct RenderReference {
  std::string token;
  std::string rendered;
  std::string fact_id;
  std::string candidate_id;
};

std::string candidate_token(std::string_view candidate_id) {
  return "<<move:" + std::string(candidate_id) + ">>";
}

std::string fact_token(std::string_view fact_id) {
  return "<<fact:" + std::string(fact_id) + ">>";
}

std::string join_uci_line(const json& pv) {
  if (!pv.is_array()) return {};
  std::string out;
  for (const auto& move : pv) {
    if (!move.is_string()) continue;
    if (!out.empty()) out += ' ';
    out += move.get<std::string>();
  }
  return out;
}

void append_reference(std::vector<RenderReference>& refs,
                      RenderReference reference) {
  if (reference.token.empty() || reference.rendered.empty()) return;
  const auto duplicate = std::find_if(
      refs.begin(), refs.end(), [&](const RenderReference& existing) {
        return existing.token == reference.token;
      });
  if (duplicate == refs.end()) refs.push_back(std::move(reference));
}

std::vector<RenderReference> native_references(
    const std::vector<EvidenceItem>& evidence) {
  std::vector<RenderReference> refs;
  for (const auto& item : evidence) {
    if (item.kind != EvidenceKind::candidate_moves) continue;
    const auto payload = json::parse(item.payload, nullptr, false);
    if (!payload.is_object()) continue;

    const auto append_candidate = [&](const json& candidate) {
      if (!candidate.is_object()) return;
      const auto id = candidate.value("candidate_id", "");
      const auto move = candidate.value("move_uci", "");
      if (!id.empty() && !move.empty()) {
        append_reference(refs, RenderReference{
            .token = candidate_token(id),
            .rendered = move,
            .candidate_id = id,
        });
      }
    };
    if (payload.contains("best")) append_candidate(payload["best"]);
    if (payload.contains("focus")) append_candidate(payload["focus"]);
    if (payload.contains("alternatives") && payload["alternatives"].is_array()) {
      for (const auto& candidate : payload["alternatives"]) append_candidate(candidate);
    }
    if (payload.contains("user_move")) append_candidate(payload["user_move"]);

    if (!payload.contains("facts") || !payload["facts"].is_array()) continue;
    for (const auto& fact : payload["facts"]) {
      if (!fact.is_object()) continue;
      const auto id = fact.value("fact_id", "");
      const auto candidate_id = fact.value("candidate_id", "");
      if (id.empty()) continue;
      if (fact.contains("move_uci") && fact["move_uci"].is_string()) {
        append_reference(refs, RenderReference{
            .token = fact_token(id),
            .rendered = fact["move_uci"].get<std::string>(),
            .fact_id = id,
            .candidate_id = candidate_id,
        });
      } else if (fact.contains("pv_uci")) {
        const auto line = join_uci_line(fact["pv_uci"]);
        if (!line.empty()) {
          append_reference(refs, RenderReference{
              .token = fact_token(id),
              .rendered = line,
              .fact_id = id,
              .candidate_id = candidate_id,
          });
        }
      }
    }
  }
  return refs;
}

std::map<std::string, std::string> fact_candidate_map(
    const std::vector<EvidenceItem>& evidence) {
  std::map<std::string, std::string> result;
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
      if (!id.empty()) result[id] = fact.value("candidate_id", "");
    }
  }
  return result;
}

bool segment_authorizes_candidate(
    const CoachAnswerSegment& segment,
    const StructuredCoachContent& content,
    std::string_view candidate_id,
    const std::map<std::string, std::string>& fact_candidates) {
  if (candidate_id.empty()) return false;
  for (const auto& fact_id : segment.fact_ids) {
    const auto found = fact_candidates.find(fact_id);
    if (found != fact_candidates.end() && found->second == candidate_id) return true;
  }
  for (const auto claim_index : segment.claim_indices) {
    if (claim_index < content.claims.size() &&
        content.claims[claim_index].candidate_id == candidate_id) {
      return true;
    }
  }
  return false;
}

std::string replace_all(std::string text,
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

std::string render_segment_text(
    std::string text,
    const CoachAnswerSegment& segment,
    const StructuredCoachContent& content,
    const std::vector<RenderReference>& refs,
    const std::map<std::string, std::string>& fact_candidates) {
  for (const auto& ref : refs) {
    if (text.find(ref.token) == std::string::npos) continue;
    if (!ref.fact_id.empty()) {
      if (std::find(segment.fact_ids.begin(), segment.fact_ids.end(), ref.fact_id) ==
          segment.fact_ids.end()) {
        throw std::invalid_argument("verified_fact_token_not_grounded");
      }
    } else if (!segment_authorizes_candidate(
                   segment, content, ref.candidate_id, fact_candidates)) {
      throw std::invalid_argument("verified_candidate_token_not_grounded");
    }
    text = replace_all(std::move(text), ref.token, ref.rendered);
  }
  if (text.find("<<") != std::string::npos ||
      text.find(">>") != std::string::npos) {
    throw std::invalid_argument("verified_fact_token_unknown");
  }
  return text;
}

std::string render_recommendation_text(
    std::string text,
    const CoachRecommendation& recommendation,
    const std::vector<RenderReference>& refs) {
  for (const auto& ref : refs) {
    if (text.find(ref.token) == std::string::npos) continue;
    if (!ref.candidate_id.empty() &&
        ref.candidate_id != recommendation.candidate_id) {
      throw std::invalid_argument("verified_recommendation_token_mismatch");
    }
    text = replace_all(std::move(text), ref.token, ref.rendered);
  }
  if (text.find("<<") != std::string::npos ||
      text.find(">>") != std::string::npos) {
    throw std::invalid_argument("verified_recommendation_token_unknown");
  }
  return text;
}

std::string join_segments(const std::vector<CoachAnswerSegment>& segments) {
  std::string out;
  for (const auto& segment : segments) {
    if (!out.empty()) out += ' ';
    out += segment.text;
  }
  return out;
}

}  // namespace

StructuredCoachContent VerifiedFactRenderer::render(
    const StructuredCoachContent& content,
    const std::vector<EvidenceItem>& evidence) const {
  auto result = content;
  const auto refs = native_references(evidence);
  const auto fact_candidates = fact_candidate_map(evidence);

  const auto render_segments = [&](std::vector<CoachAnswerSegment>& segments) {
    for (auto& segment : segments) {
      segment.text = render_segment_text(
          std::move(segment.text), segment, result, refs, fact_candidates);
    }
  };
  render_segments(result.answer_segments);
  render_segments(result.follow_up_segments);

  for (auto& recommendation : result.recommendations) {
    recommendation.text = render_recommendation_text(
        std::move(recommendation.text), recommendation, refs);
  }

  // Claims mirror their owning segment text. Rebuild those strings after the
  // final native substitution so diagnostics/UI never expose opaque tokens.
  for (auto& segment : result.answer_segments) {
    for (const auto index : segment.claim_indices) {
      if (index >= result.claims.size()) continue;
      result.claims[index].text = segment.text;
      result.claims[index].answer_quote = segment.text;
    }
  }
  for (auto& segment : result.follow_up_segments) {
    for (const auto index : segment.claim_indices) {
      if (index >= result.claims.size()) continue;
      result.claims[index].text = segment.text;
      result.claims[index].answer_quote = segment.text;
    }
  }

  result.answer = result.verdict_summary;
  const auto explanation = join_segments(result.answer_segments);
  if (!explanation.empty()) {
    if (!result.answer.empty()) result.answer += ' ';
    result.answer += explanation;
  }
  result.follow_up_question = join_segments(result.follow_up_segments);
  return result;
}

}  // namespace kchess::ai
