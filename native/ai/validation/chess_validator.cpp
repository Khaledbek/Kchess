#include "chess_validator.h"

#include <charconv>
#include <cmath>
#include <exception>
#include <string_view>
#include <system_error>

#include <nlohmann/json.hpp>

#include "chess/move.h"
#include "chess/position_view.h"
#include "../position/position_features.h"

namespace kchess::ai {
namespace {

// -----------------------------------------------------------------------------
// Section: Evidence helpers
// -----------------------------------------------------------------------------

bool engine_contradicts(const CoachClaim& claim, const std::vector<EvidenceItem>& evidence) {
  for (const auto& item : evidence) {
    if (item.kind != EvidenceKind::candidate_moves) continue;
    try {
      const auto payload = nlohmann::json::parse(item.payload);
      auto candidates = payload.value("alternatives", nlohmann::json::array());
      if (payload.contains("best")) candidates.push_back(payload["best"]);
      if (payload.contains("user_move")) candidates.push_back(payload["user_move"]);
      for (const auto& candidate : candidates) {
        if (candidate.value("move_uci", "") == claim.subject &&
            candidate.contains("evaluation_cp") && candidate["evaluation_cp"].is_number_integer()) {
          return std::to_string(candidate["evaluation_cp"].get<int>()) != claim.value;
        }
      }
    } catch (...) { continue; }
  }
  return false; // No comparable engine value: unknown, not contradicted.
}
// -----------------------------------------------------------------------------
// Section: Board helpers
// -----------------------------------------------------------------------------

bool parse_int(std::string_view text, int& out) {
  const auto* begin = text.data();
  const auto* end = begin + text.size();
  const auto [ptr, error] = std::from_chars(begin, end, out);
  return error == std::errc{} && ptr == end;
}

int square_index(std::string_view square) {
  if (square.size() != 2 || square[0] < 'a' || square[0] > 'h' ||
      square[1] < '1' || square[1] > '8') {
    return -1;
  }
  const int file = square[0] - 'a';
  const int rank = square[1] - '1';
  return (7 - rank) * 8 + file;
}

bool piece_matches(const std::string& fen,
                   std::string_view square,
                   std::string_view expected) {
  const int index = square_index(square);
  if (index < 0) return false;
  const auto view = nlohmann::json::parse(kchess::position_view_json(fen));
  const auto pieces = view.at("pieces");
  if (index >= static_cast<int>(pieces.size())) return false;
  const auto actual = pieces.at(index).get<std::string>();
  return expected == "empty" ? actual.empty() : actual == expected;
}

ChessValidationResult validate_move_fact(const CoachClaim& claim,
                                         const std::string& fen) {
  if (claim.subject.empty()) return {false, "claim_move_missing"};
  try {
    const auto applied = kchess::apply_legal_uci_move(fen, claim.subject);
    if (claim.kind == CoachClaimKind::legal_move) return {};
    const bool check = applied.san.find('+') != std::string::npos ||
                       applied.san.find('#') != std::string::npos;
    if (claim.kind == CoachClaimKind::gives_check) {
      return check ? ChessValidationResult{}
                   : ChessValidationResult{false, "claim_check_false"};
    }
    const auto outcome = kchess::position_outcome(applied.fen_after);
    return outcome.checkmate ? ChessValidationResult{}
                             : ChessValidationResult{false, "claim_mate_false"};
  } catch (...) {
    return {false, "claim_move_illegal"};
  }
}

ChessValidationResult validate_material(const CoachClaim& claim,
                                         const std::string& fen) {
  int expected = 0;
  if (!parse_int(claim.value, expected)) {
    return {false, "claim_material_value_invalid"};
  }
  const auto features = PositionFeatureExtractor{}.extract(fen);
  int actual = 0;
  if (claim.subject == "white") actual = features.white.material_cp;
  else if (claim.subject == "black") actual = features.black.material_cp;
  else if (claim.subject == "balance") {
    actual = features.white.material_cp - features.black.material_cp;
  } else {
    return {false, "claim_material_subject_invalid"};
  }
  return actual == expected ? ChessValidationResult{}
                            : ChessValidationResult{false, "claim_material_false"};
}

}  // namespace

// -----------------------------------------------------------------------------
// Section: Public validation
// -----------------------------------------------------------------------------

ChessValidationResult ChessValidator::validate_claim(
    const CoachClaim& claim,
    const std::string& fen,
    const std::vector<EvidenceItem>& evidence) const {
  try {
    switch (claim.kind) {
      case CoachClaimKind::general:
        return {};
      case CoachClaimKind::legal_move:
      case CoachClaimKind::gives_check:
      case CoachClaimKind::gives_mate:
        return validate_move_fact(claim, fen);
      case CoachClaimKind::material_cp:
        return validate_material(claim, fen);
      case CoachClaimKind::piece_on_square:
        return piece_matches(fen, claim.subject, claim.value)
                   ? ChessValidationResult{}
                   : ChessValidationResult{false, "claim_piece_location_false"};
      case CoachClaimKind::tactical_motif:
        return {}; // Missing heuristic evidence is not a refutation.
      case CoachClaimKind::engine_evaluation:
        return !engine_contradicts(claim, evidence)
                   ? ChessValidationResult{}
                   : ChessValidationResult{false, "claim_engine_contradicted"};
      case CoachClaimKind::opening_fact: return {};
    }
  } catch (...) {
    return {false, "claim_validation_error"};
  }
  return {false, "claim_kind_unknown"};
}

ChessValidationResult ChessValidator::validate_recommendation(
    const CoachRecommendation& recommendation,
    const std::string& fen,
    const std::vector<EvidenceItem>& evidence) const {
  if (recommendation.move_uci.empty()) return {};
  try {
    (void)kchess::apply_legal_uci_move(fen, recommendation.move_uci);
  } catch (...) {
    return {false, "recommendation_move_illegal"};
  }

  return {};
}

}  // namespace kchess::ai
