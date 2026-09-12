#include "response_validator.h"
#include "chess_validator.h"

#include <utility>

namespace kchess::ai {
namespace {

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
  if (needs_board(claim.kind) && !position_fen) return {};
  return chess.validate_claim(claim, position_fen.value_or(""), evidence);
}

}  // namespace

// Section: Reject false chess facts, not missing prose metadata.
ResponseValidationReport ResponseValidator::validate(
    const StructuredCoachContent& content,
    const std::optional<std::string>& position_fen,
    const std::vector<EvidenceItem>& evidence) const {
  ResponseValidationReport report;
  ChessValidator chess;
  for (const auto& claim : content.claims) {
    const auto result = validate_claim(chess, claim, position_fen, evidence);
    if (!result.valid) report.issues.push_back(result.error_code);
  }
  if (position_fen) {
    for (const auto& recommendation : content.recommendations) {
      const auto result = chess.validate_recommendation(
          recommendation, *position_fen, evidence);
      if (!result.valid) report.issues.push_back(result.error_code);
    }
  }
  report.valid = report.issues.empty();
  return report;
}

ResponseValidationReport ResponseValidator::sanitize_metadata(
    StructuredCoachContent& content,
    const std::optional<std::string>& position_fen,
    const std::vector<EvidenceItem>& evidence) const {
  ResponseValidationReport report;
  ChessValidator chess;

  std::vector<CoachClaim> safe_claims;
  safe_claims.reserve(content.claims.size());
  for (auto& claim : content.claims) {
    const auto result = validate_claim(chess, claim, position_fen, evidence);
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
          recommendation, *position_fen, evidence);
      if (result.valid) {
        safe_recommendations.push_back(std::move(recommendation));
      } else {
        report.issues.push_back(result.error_code);
      }
    }
    content.recommendations = std::move(safe_recommendations);
  }

  report.valid = report.issues.empty();
  return report;
}

}  // namespace kchess::ai
