#include "candidate_move_system.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>

#include <nlohmann/json.hpp>

namespace kchess::ai {
namespace {

// -----------------------------------------------------------------------------
// Section: Selection helpers
// -----------------------------------------------------------------------------

CandidateMove to_candidate(const CandidateLineInput& line, std::string candidate_id) {
  return {
      .candidate_id = std::move(candidate_id),
      .rank = line.rank,
      .move_uci = line.move_uci,
      .pv_uci = line.pv_uci,
      .evaluation_cp = line.evaluation_cp,
      .mate_in = line.mate_in,
      .expected_score = line.expected_score,
  };
}

std::vector<CandidateLineInput> ranked_lines(
    const CandidateMoveSnapshot& snapshot) {
  auto lines = snapshot.root_lines;
  std::erase_if(lines, [](const auto& line) { return line.move_uci.empty(); });
  std::stable_sort(lines.begin(), lines.end(), [](const auto& a, const auto& b) {
    if (a.rank <= 0) return false;
    if (b.rank <= 0) return true;
    return a.rank < b.rank;
  });
  return lines;
}

const CandidateLineInput* find_user_line(
    const CandidateMoveSnapshot& snapshot,
    const std::vector<CandidateLineInput>& lines,
    const std::optional<std::string>& user_move_uci) {
  if (!user_move_uci.has_value() || user_move_uci->empty()) {
    return snapshot.user_move_line ? &*snapshot.user_move_line : nullptr;
  }
  const auto found = std::find_if(lines.begin(), lines.end(), [&](const auto& line) {
    return line.move_uci == *user_move_uci;
  });
  if (found != lines.end()) return &*found;
  if (snapshot.user_move_line &&
      snapshot.user_move_line->move_uci == *user_move_uci) {
    return &*snapshot.user_move_line;
  }
  return nullptr;
}

// -----------------------------------------------------------------------------
// Section: Evidence serialization
// -----------------------------------------------------------------------------

const char* fact_kind_name(const ChessFactKind kind) {
  switch (kind) {
    case ChessFactKind::candidate_move: return "candidate_move";
    case ChessFactKind::candidate_rank: return "candidate_rank";
    case ChessFactKind::evaluation_cp: return "evaluation_cp";
    case ChessFactKind::mate_distance: return "mate_distance";
    case ChessFactKind::expected_score: return "expected_score";
    case ChessFactKind::principal_variation: return "principal_variation";
    case ChessFactKind::critical_reply: return "critical_reply";
    case ChessFactKind::analysis_focus: return "analysis_focus";
  }
  return "candidate_move";
}

std::string fact_id(const CandidateMove& candidate, const std::string_view suffix) {
  return "fact." + candidate.candidate_id + "." + std::string(suffix);
}

void append_candidate_facts(std::vector<ChessFact>& facts,
                            const CandidateMove& candidate) {
  facts.push_back({
      .fact_id = fact_id(candidate, "move"),
      .kind = ChessFactKind::candidate_move,
      .candidate_id = candidate.candidate_id,
      .move_uci = candidate.move_uci,
  });
  facts.push_back({
      .fact_id = fact_id(candidate, "rank"),
      .kind = ChessFactKind::candidate_rank,
      .candidate_id = candidate.candidate_id,
      .integer_value = candidate.rank,
  });
  if (candidate.evaluation_cp) {
    facts.push_back({
        .fact_id = fact_id(candidate, "evaluation_cp"),
        .kind = ChessFactKind::evaluation_cp,
        .candidate_id = candidate.candidate_id,
        .integer_value = *candidate.evaluation_cp,
    });
  }
  if (candidate.mate_in) {
    facts.push_back({
        .fact_id = fact_id(candidate, "mate_in"),
        .kind = ChessFactKind::mate_distance,
        .candidate_id = candidate.candidate_id,
        .integer_value = *candidate.mate_in,
    });
  }
  if (candidate.expected_score) {
    facts.push_back({
        .fact_id = fact_id(candidate, "expected_score"),
        .kind = ChessFactKind::expected_score,
        .candidate_id = candidate.candidate_id,
        .decimal_value = *candidate.expected_score,
    });
  }
  if (!candidate.pv_uci.empty()) {
    facts.push_back({
        .fact_id = fact_id(candidate, "pv"),
        .kind = ChessFactKind::principal_variation,
        .candidate_id = candidate.candidate_id,
        .pv_uci = candidate.pv_uci,
    });
  }
}

nlohmann::json chess_fact_json(const ChessFact& fact) {
  nlohmann::json out{
      {"fact_id", fact.fact_id},
      {"kind", fact_kind_name(fact.kind)},
  };
  if (!fact.candidate_id.empty()) out["candidate_id"] = fact.candidate_id;
  if (!fact.move_uci.empty()) out["move_uci"] = fact.move_uci;
  if (!fact.pv_uci.empty()) out["pv_uci"] = fact.pv_uci;
  if (fact.integer_value) out["integer_value"] = *fact.integer_value;
  if (fact.decimal_value) out["decimal_value"] = *fact.decimal_value;
  if (!fact.text_value.empty()) out["text_value"] = fact.text_value;
  return out;
}

nlohmann::json candidate_json(const CandidateMove& candidate) {
  nlohmann::json out{
      {"candidate_id", candidate.candidate_id},
      {"rank", candidate.rank},
      {"move_uci", candidate.move_uci},
      {"pv_uci", candidate.pv_uci},
  };
  if (candidate.evaluation_cp) out["evaluation_cp"] = *candidate.evaluation_cp;
  if (candidate.mate_in) out["mate_in"] = *candidate.mate_in;
  if (candidate.expected_score) out["expected_score"] = *candidate.expected_score;
  return out;
}

}  // namespace

// -----------------------------------------------------------------------------
// Section: Public candidate construction
// -----------------------------------------------------------------------------

CandidateMoveSet CandidateMoveSystem::build(
    const CandidateMoveSnapshot& snapshot,
    const std::optional<std::string>& user_move_uci,
    const PositionAnalysisMode analysis_mode) const {
  CandidateMoveSet result;
  const auto lines = ranked_lines(snapshot);
  if (lines.empty()) return result;

  const bool inverted_root_analysis =
      analysis_mode == PositionAnalysisMode::worst_move ||
      analysis_mode == PositionAnalysisMode::fastest_loss;
  if (inverted_root_analysis) {
    result.focus_kind = analysis_mode == PositionAnalysisMode::fastest_loss
        ? "fastest_loss"
        : "worst_move";
    result.focus = to_candidate(
        lines.front(), analysis_mode == PositionAnalysisMode::fastest_loss
            ? "candidate.fastest_loss"
            : "candidate.worst");
    for (std::size_t i = 1; i < lines.size() && result.alternatives.size() < 2;
         ++i) {
      if (lines[i].move_uci == result.focus->move_uci) continue;
      const auto ordinal = result.alternatives.size() + 1;
      result.alternatives.push_back(to_candidate(
          lines[i], "candidate.extreme.alt." + std::to_string(ordinal)));
    }
    if (result.focus->pv_uci.size() >= 2 &&
        result.focus->pv_uci.front() == result.focus->move_uci) {
      result.critical_reply_uci = result.focus->pv_uci[1];
    }
    return result;
  }

  result.best = to_candidate(lines.front(), "candidate.best");
  for (std::size_t i = 1; i < lines.size() && result.alternatives.size() < 3;
       ++i) {
    if (lines[i].move_uci == result.best->move_uci ||
        std::any_of(result.alternatives.begin(), result.alternatives.end(),
                    [&](const CandidateMove& candidate) {
                      return candidate.move_uci == lines[i].move_uci;
                    })) {
      continue;
    }
    const auto ordinal = result.alternatives.size() + 1;
    result.alternatives.push_back(
        to_candidate(lines[i], "candidate.alt." + std::to_string(ordinal)));
  }

  if (const auto* user_line = find_user_line(snapshot, lines, user_move_uci)) {
    std::string candidate_id = "candidate.user";
    if (result.best && result.best->move_uci == user_line->move_uci) {
      candidate_id = result.best->candidate_id;
    } else {
      const auto match = std::find_if(
          result.alternatives.begin(), result.alternatives.end(),
          [&](const CandidateMove& candidate) {
            return candidate.move_uci == user_line->move_uci;
          });
      if (match != result.alternatives.end()) candidate_id = match->candidate_id;
    }
    result.user_move = to_candidate(*user_line, std::move(candidate_id));
  }

  const auto& pv = result.best->pv_uci;
  if (pv.size() >= 2 && pv.front() == result.best->move_uci) {
    result.critical_reply_uci = pv[1];
  }
  return result;
}

std::vector<ChessFact> CandidateMoveSystem::facts(
    const CandidateMoveSet& candidates) const {
  std::vector<ChessFact> result;
  if (candidates.best) append_candidate_facts(result, *candidates.best);
  if (candidates.focus) append_candidate_facts(result, *candidates.focus);
  for (const auto& candidate : candidates.alternatives) {
    append_candidate_facts(result, candidate);
  }
  if (candidates.user_move) {
    const bool already_present = std::any_of(
        result.begin(), result.end(), [&](const ChessFact& fact) {
          return fact.kind == ChessFactKind::candidate_move &&
                 fact.candidate_id == candidates.user_move->candidate_id;
        });
    if (!already_present) append_candidate_facts(result, *candidates.user_move);
  }

  const CandidateMove* primary = candidates.focus ? &*candidates.focus
                                                   : (candidates.best ? &*candidates.best : nullptr);
  if (primary != nullptr && candidates.critical_reply_uci) {
    result.push_back({
        .fact_id = fact_id(*primary, "critical_reply"),
        .kind = ChessFactKind::critical_reply,
        .candidate_id = primary->candidate_id,
        .move_uci = *candidates.critical_reply_uci,
    });
  }
  if (candidates.focus && !candidates.focus_kind.empty()) {
    result.push_back({
        .fact_id = fact_id(*candidates.focus, "analysis_focus"),
        .kind = ChessFactKind::analysis_focus,
        .candidate_id = candidates.focus->candidate_id,
        .text_value = candidates.focus_kind,
    });
  }
  return result;
}

EvidenceItem CandidateMoveSystem::evidence(
    const CandidateMoveSet& candidates) const {
  nlohmann::json payload{{"version", 1}, {"fact_schema", "coach.chess_facts.v1"}};
  if (candidates.best) payload["best"] = candidate_json(*candidates.best);
  if (candidates.focus) payload["focus"] = candidate_json(*candidates.focus);
  if (!candidates.focus_kind.empty()) payload["focus_kind"] = candidates.focus_kind;

  payload["alternatives"] = nlohmann::json::array();
  for (const auto& candidate : candidates.alternatives) {
    payload["alternatives"].push_back(candidate_json(candidate));
  }
  if (candidates.user_move) {
    payload["user_move"] = candidate_json(*candidates.user_move);
  }
  if (candidates.critical_reply_uci) {
    payload["critical_reply_uci"] = *candidates.critical_reply_uci;
  }

  payload["facts"] = nlohmann::json::array();
  for (const auto& fact : facts(candidates)) {
    payload["facts"].push_back(chess_fact_json(fact));
  }

  return {
      .id = "engine.candidates.v1",
      .kind = EvidenceKind::candidate_moves,
      .payload = payload.dump(),
      .confidence = (candidates.best || candidates.focus) ? 1.0 : 0.0,
  };
}

}  // namespace kchess::ai
