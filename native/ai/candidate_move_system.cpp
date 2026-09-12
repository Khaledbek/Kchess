#include "candidate_move_system.h"

#include <algorithm>
#include <utility>

#include <nlohmann/json.hpp>

namespace kchess::ai {
namespace {

// -----------------------------------------------------------------------------
// Section: Selection helpers
// -----------------------------------------------------------------------------

CandidateMove to_candidate(const CandidateLineInput& line) {
  return {
      .rank = line.rank,
      .move_uci = line.move_uci,
      .pv_uci = line.pv_uci,
      .evaluation_cp = line.evaluation_cp,
      .mate_in = line.mate_in,
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

nlohmann::json candidate_json(const CandidateMove& candidate) {
  nlohmann::json out{
      {"rank", candidate.rank},
      {"move_uci", candidate.move_uci},
      {"pv_uci", candidate.pv_uci},
  };
  if (candidate.evaluation_cp) out["evaluation_cp"] = *candidate.evaluation_cp;
  if (candidate.mate_in) out["mate_in"] = *candidate.mate_in;
  return out;
}

}  // namespace

// -----------------------------------------------------------------------------
// Section: Public candidate construction
// -----------------------------------------------------------------------------

CandidateMoveSet CandidateMoveSystem::build(
    const CandidateMoveSnapshot& snapshot,
    const std::optional<std::string>& user_move_uci) const {
  CandidateMoveSet result;
  const auto lines = ranked_lines(snapshot);
  if (lines.empty()) return result;

  result.best = to_candidate(lines.front());
  for (std::size_t i = 1; i < lines.size() && result.alternatives.size() < 3;
       ++i) {
    if (lines[i].move_uci == result.best->move_uci) continue;
    result.alternatives.push_back(to_candidate(lines[i]));
  }

  if (const auto* user_line = find_user_line(snapshot, lines, user_move_uci)) {
    result.user_move = to_candidate(*user_line);
  }

  const auto& pv = result.best->pv_uci;
  if (pv.size() >= 2 && pv.front() == result.best->move_uci) {
    result.critical_reply_uci = pv[1];
  }
  return result;
}

EvidenceItem CandidateMoveSystem::evidence(
    const CandidateMoveSet& candidates) const {
  nlohmann::json payload{{"version", 1}};
  if (candidates.best) payload["best"] = candidate_json(*candidates.best);

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

  return {
      .id = "engine.candidates.v1",
      .kind = EvidenceKind::candidate_moves,
      .payload = payload.dump(),
      .confidence = candidates.best ? 1.0 : 0.0,
  };
}

}  // namespace kchess::ai
