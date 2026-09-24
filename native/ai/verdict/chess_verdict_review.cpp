#include "chess_verdict_review.h"

#include <algorithm>
#include <cctype>
#include <initializer_list>
#include <string>
#include <string_view>

#include "../conversation/coach_session.h"

namespace kchess::ai {
namespace {

std::string normalize(std::string_view text) {
  std::string out;
  out.reserve(text.size() + 2);
  out.push_back(' ');
  for (const unsigned char c : text) {
    if (c < 128) {
      out.push_back(std::isalnum(c)
                        ? static_cast<char>(std::tolower(c))
                        : ' ');
    } else {
      out.push_back(static_cast<char>(c));
    }
  }
  out.push_back(' ');
  return out;
}

bool contains_any(std::string_view text,
                  std::initializer_list<std::string_view> phrases) {
  return std::any_of(phrases.begin(), phrases.end(), [&](const auto phrase) {
    return text.find(phrase) != std::string_view::npos;
  });
}

bool is_new_extreme_question(std::string_view text) {
  return contains_any(text, {
      " best move ", " bester zug ", " beste zug ", " besten zug ",
      " worst move ", " schlechtester zug ", " schlechteste zug ",
      " schlechtesten zug ", " fastest loss ", " schnellster verlust "});
}

bool looks_like_objection(std::string_view original) {
  const auto text = normalize(original);
  if (is_new_extreme_question(text)) return false;
  if (contains_any(text, {
          " aber ", " doch ", " nein ", " dafür ", " dafuer ",
          " stimmt nicht ", " bist du sicher ", " wirklich ",
          " ich glaube ", " ich denke ", " ausgeglichen ",
          " kompensation ", " kompensiert ", " gut oder schlecht ",
          " also ist der zug ", " but ", " however ", " actually ",
          " i disagree ", " are you sure ", " that is not ",
          " what about ", " compensation ", " compensated ",
          " good or bad ", " so is the move ", " no ",
          " gibst du mir recht ", " gibst du mir jetzt recht ",
          " stimmst du mir zu ", " agree with me ",
          " do you agree with me ", " so do you agree "})) {
    return true;
  }
  return original.find("لكن") != std::string_view::npos ||
         original.find("هل أنت متأكد") != std::string_view::npos ||
         original.find("لا أوافق") != std::string_view::npos ||
         original.find("تعويض") != std::string_view::npos ||
         original.find("جيد أم سيئ") != std::string_view::npos ||
         original.find("هل توافقني") != std::string_view::npos ||
         original.find("ألا توافقني") != std::string_view::npos;
}

}  // namespace

std::optional<ChessVerdictChallenge> resolve_verdict_challenge(
    const CoachRequest& request, const CoachSessionState* session) {
  if (request.automatic_turn || request.user_text.empty() || session == nullptr ||
      !session->last_chess_verdict ||
      !session->last_chess_verdict->authoritative ||
      !looks_like_objection(request.user_text)) {
    return std::nullopt;
  }

  ChessVerdictChallenge challenge;
  challenge.previous_verdict = *session->last_chess_verdict;
  challenge.trigger = "user_objection";
  challenge.challenged_move_uci = challenge.previous_verdict.evaluated_move_uci;

  if (session->last_move_attribution) {
    const auto& move = *session->last_move_attribution;
    if (!challenge.challenged_move_uci && move.played_move_uci) {
      challenge.challenged_move_uci = move.played_move_uci;
    }
    if (move.previous_fen && move.played_move_uci &&
        (!challenge.challenged_move_uci ||
         *challenge.challenged_move_uci == *move.played_move_uci)) {
      challenge.analysis_fen = move.previous_fen;
    }
  }
  if (!challenge.analysis_fen && session->last_verdict_fen) {
    challenge.analysis_fen = session->last_verdict_fen;
  }
  if (!challenge.analysis_fen && request.position_fen) {
    challenge.analysis_fen = request.position_fen;
  }

  return challenge.analysis_fen ? std::optional<ChessVerdictChallenge>{challenge}
                                : std::nullopt;
}

std::optional<ChessVerdictReview> review_chess_verdict(
    const std::optional<ChessVerdictChallenge>& challenge,
    const std::optional<ChessVerdictContract>& current_verdict) {
  if (!challenge) return std::nullopt;

  ChessVerdictReview review;
  review.active = true;
  review.previous_verdict = challenge->previous_verdict;
  review.challenged_move_uci = challenge->challenged_move_uci;
  review.trigger = challenge->trigger;

  if (!current_verdict || !current_verdict->authoritative) {
    review.outcome = VerdictReviewOutcome::inconclusive;
    return review;
  }

  if (challenge->challenged_move_uci.has_value() &&
      (!current_verdict->evaluated_move_uci.has_value() ||
       *current_verdict->evaluated_move_uci !=
           *challenge->challenged_move_uci)) {
    review.outcome = VerdictReviewOutcome::inconclusive;
    return review;
  }

  bool comparable = false;
  bool changed = false;
  if (challenge->previous_verdict.move_verdict != MoveVerdict::unknown &&
      current_verdict->move_verdict != MoveVerdict::unknown) {
    comparable = true;
    changed = changed || challenge->previous_verdict.move_verdict !=
                           current_verdict->move_verdict;
  }
  if (challenge->previous_verdict.position_scope ==
          current_verdict->position_scope &&
      challenge->previous_verdict.position_verdict != PositionVerdict::unknown &&
      current_verdict->position_verdict != PositionVerdict::unknown) {
    comparable = true;
    changed = changed || challenge->previous_verdict.position_verdict !=
                           current_verdict->position_verdict;
  }

  review.outcome = !comparable
      ? VerdictReviewOutcome::inconclusive
      : (changed ? VerdictReviewOutcome::changed
                 : VerdictReviewOutcome::unchanged);
  return review;
}

}  // namespace kchess::ai
