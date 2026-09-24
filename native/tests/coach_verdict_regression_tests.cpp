#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "conversation/coach_session.h"
#include "dto/chess_verdict.h"
#include "dto/structured_coach_response.h"
#include "interaction/primary_interaction_planner.h"
#include "validation/response_validator.h"
#include "verdict/chess_verdict_review.h"

namespace {

using namespace kchess::ai;
using namespace kchess::ai::interaction;

int assertions = 0;
int failures = 0;

void expect(bool condition, const std::string& message) {
  ++assertions;
  if (!condition) {
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
  }
}

bool has_issue(const ResponseValidationReport& report, std::string_view issue) {
  return std::find(report.issues.begin(), report.issues.end(), issue) !=
      report.issues.end();
}

ChessVerdictContract authoritative_blunder() {
  ChessVerdictContract verdict;
  verdict.authoritative = true;
  verdict.position_scope = "after_evaluated_move";
  verdict.position_verdict = PositionVerdict::white_clearly_better;
  verdict.move_verdict = MoveVerdict::blunder;
  verdict.evaluated_move_uci = "d4c3";
  verdict.white_evaluation_cp = 420;
  verdict.move_loss_cp = 310;
  verdict.basis = "regression_fixture";
  return verdict;
}

StructuredCoachContent matching_content(
    const ChessVerdictContract& verdict,
    VerdictReviewOutcome review_outcome = VerdictReviewOutcome::none) {
  StructuredCoachContent content;
  content.verdict_summary =
      "Der Zug ist ein Blunder; Weiß steht danach klar besser.";
  content.answer = content.verdict_summary;
  content.verdict_lock.active = true;
  content.verdict_lock.position_verdict =
      std::string(position_verdict_name(verdict.position_verdict));
  content.verdict_lock.move_verdict =
      std::string(move_verdict_name(verdict.move_verdict));
  content.verdict_lock.review_outcome =
      std::string(verdict_review_outcome_name(review_outcome));
  return content;
}

void explicit_verdict_requests_are_multilingual_and_deterministic() {
  PrimaryInteractionPlanner planner;
  ConversationState state;
  state.position_key = "fixture-position";

  for (const auto& text : {
           std::string{"Also ist der Zug jetzt gut oder schlecht?"},
           std::string{"Is the move good or bad now?"},
           std::string{"هل هذه النقلة جيدة أم سيئة؟"},
       }) {
    const auto result = planner.classify(text, state);
    expect(result.request_kind == InteractionRequestKind::analysis,
           text + " must remain an analysis request");
    expect(result.contains("evaluate_move"),
           text + " must require native move evaluation");
  }

  for (const auto& text : {
           std::string{"Wer steht hier besser?"},
           std::string{"Who stands better here?"},
           std::string{"من الأفضل في هذا الوضع؟"},
       }) {
    const auto result = planner.classify(text, state);
    expect(result.request_kind == InteractionRequestKind::analysis,
           text + " must remain an analysis request");
    expect(result.contains("evaluate_position"),
           text + " must require native position evaluation");
  }
}

void objections_reopen_the_last_authoritative_verdict() {
  CoachSessionState session;
  session.last_chess_verdict = authoritative_blunder();
  session.last_verdict_fen =
      "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

  for (const auto& text : {
           std::string{"Nein, der Zug ist doch gut."},
           std::string{"Aber nach dxc3 ist das Material doch ausgeglichen."},
           std::string{"Also gibst du mir jetzt recht?"},
           std::string{"But I win a pawn after that, are you sure?"},
           std::string{"So do you agree with me now?"},
           std::string{"لكن أعتقد أن النقلة جيدة"},
           std::string{"هل توافقني الآن؟"},
       }) {
    CoachRequest request;
    request.user_text = text;
    const auto challenge = resolve_verdict_challenge(request, &session);
    expect(challenge.has_value(), text + " must trigger native verdict recheck");
    if (challenge) {
      expect(challenge->challenged_move_uci == std::optional<std::string>{"d4c3"},
             text + " must recheck the previously judged move");
      expect(challenge->analysis_fen == session.last_verdict_fen,
             text + " must reuse the original verdict position");
    }
  }

  CoachRequest new_question;
  new_question.user_text = "Was ist jetzt der beste Zug?";
  expect(!resolve_verdict_challenge(new_question, &session).has_value(),
         "a new extreme-move question must not be mistaken for an objection");
}

void review_changes_only_when_native_verdict_changes() {
  const auto previous = authoritative_blunder();
  ChessVerdictChallenge challenge;
  challenge.previous_verdict = previous;
  challenge.challenged_move_uci = previous.evaluated_move_uci;
  challenge.analysis_fen =
      "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

  const auto unchanged = review_chess_verdict(challenge, previous);
  expect(unchanged && unchanged->outcome == VerdictReviewOutcome::unchanged,
         "same native verdict must remain unchanged after user objection");

  auto corrected = previous;
  corrected.move_verdict = MoveVerdict::playable;
  corrected.position_verdict = PositionVerdict::equal;
  const auto changed = review_chess_verdict(challenge, corrected);
  expect(changed && changed->outcome == VerdictReviewOutcome::changed,
         "only changed native evidence may change the verdict");

  auto wrong_move = corrected;
  wrong_move.evaluated_move_uci = "e2e4";
  const auto inconclusive = review_chess_verdict(challenge, wrong_move);
  expect(inconclusive &&
             inconclusive->outcome == VerdictReviewOutcome::inconclusive,
         "a recheck of another move must not overwrite the challenged verdict");
}

void provider_cannot_soften_or_replace_native_verdict() {
  const auto verdict = authoritative_blunder();
  const std::optional<ChessVerdictContract> expected{verdict};
  ResponseValidator validator;

  auto valid = matching_content(verdict);
  const auto valid_report = validator.validate(
      valid, std::nullopt, {}, nullptr, false, false, &expected, nullptr);
  expect(valid_report.valid, "matching provider verdict echo must validate");

  auto softened = valid;
  softened.verdict_lock.move_verdict = "playable";
  const auto softened_report = validator.validate(
      softened, std::nullopt, {}, nullptr, false, false, &expected, nullptr);
  expect(!softened_report.valid &&
             has_issue(softened_report, "native_verdict_lock_mismatch"),
         "provider may not soften a native blunder verdict to playable");

  auto missing_summary = valid;
  missing_summary.verdict_summary.clear();
  missing_summary.answer.clear();
  const auto summary_report = validator.validate(
      missing_summary, std::nullopt, {}, nullptr, false, false, &expected,
      nullptr);
  expect(!summary_report.valid &&
             has_issue(summary_report, "native_verdict_summary_required"),
         "authoritative verdict must have a decisive visible summary");

  StructuredCoachContent invented;
  invented.verdict_summary = "Der Zug ist gut.";
  invented.answer = invented.verdict_summary;
  invented.verdict_lock.active = false;
  invented.verdict_lock.position_verdict = "unknown";
  invented.verdict_lock.move_verdict = "unknown";
  invented.verdict_lock.review_outcome = "none";
  const auto invented_report = validator.validate(
      invented, std::nullopt, {}, nullptr, false, false, nullptr, nullptr);
  expect(!invented_report.valid &&
             has_issue(invented_report, "native_verdict_summary_without_verdict"),
         "provider may not invent a verdict summary without native authority");
}

void provider_review_echo_is_fail_closed() {
  const auto verdict = authoritative_blunder();
  const std::optional<ChessVerdictContract> expected{verdict};

  ChessVerdictReview review;
  review.active = true;
  review.previous_verdict = verdict;
  review.outcome = VerdictReviewOutcome::unchanged;
  review.challenged_move_uci = verdict.evaluated_move_uci;
  review.trigger = "user_objection";
  const std::optional<ChessVerdictReview> expected_review{review};

  ResponseValidator validator;
  auto content = matching_content(verdict, VerdictReviewOutcome::unchanged);
  const auto valid = validator.validate(
      content, std::nullopt, {}, nullptr, false, false, &expected,
      &expected_review);
  expect(valid.valid, "matching unchanged review echo must validate");

  content.verdict_lock.review_outcome = "changed";
  const auto invalid = validator.validate(
      content, std::nullopt, {}, nullptr, false, false, &expected,
      &expected_review);
  expect(!invalid.valid && has_issue(invalid, "native_verdict_lock_mismatch"),
         "provider cannot claim the user changed the verdict when native recheck did not");
}

}  // namespace

int main() {
  explicit_verdict_requests_are_multilingual_and_deterministic();
  objections_reopen_the_last_authoritative_verdict();
  review_changes_only_when_native_verdict_changes();
  provider_cannot_soften_or_replace_native_verdict();
  provider_review_echo_is_fail_closed();

  if (failures != 0) {
    std::cerr << failures << " of " << assertions
              << " coach verdict regression assertions failed\n";
    return EXIT_FAILURE;
  }
  std::cout << "coach verdict regression tests passed (" << assertions
            << " assertions)\n";
  return EXIT_SUCCESS;
}
