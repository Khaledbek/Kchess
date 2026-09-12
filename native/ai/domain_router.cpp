#include "domain_router.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <string>
#include <string_view>

#include "conversation/coach_session.h"
#include "domain_router_terms.h"
#include "models/small_models.h"

namespace kchess::ai {
namespace {

// -----------------------------------------------------------------------------
// Section: Text matching
// -----------------------------------------------------------------------------

std::string normalize(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return text;
}

template <std::size_t N>
int matches(std::string_view text, const std::string_view (&terms)[N]) {
  int count = 0;
  for (const std::string_view term : terms) {
    count += text.find(term) != std::string_view::npos ? 1 : 0;
  }
  return count;
}

struct IntentScore {
  CoachIntent intent;
  int score;
};

// -----------------------------------------------------------------------------
// Section: Intent scoring
// -----------------------------------------------------------------------------

IntentScore best_intent(std::string_view text, const CoachRequest& request) {
  std::array scores{
      IntentScore{CoachIntent::move_explanation,
                  matches(text, router_terms::move_explanation)},
      IntentScore{CoachIntent::plan, matches(text, router_terms::plan)},
      IntentScore{CoachIntent::tactic, matches(text, router_terms::tactic)},
      IntentScore{CoachIntent::opening, matches(text, router_terms::opening)},
      IntentScore{CoachIntent::endgame, matches(text, router_terms::endgame)},
      IntentScore{CoachIntent::chess_rules, matches(text, router_terms::rules)},
      IntentScore{CoachIntent::chess_history,
                  matches(text, router_terms::history) +
                      matches(text, router_terms::player_names)},
      IntentScore{CoachIntent::player_development,
                  matches(text, router_terms::development)},
      IntentScore{CoachIntent::game_review, matches(text, router_terms::game_review)},
      IntentScore{CoachIntent::training, matches(text, router_terms::training)},
      IntentScore{CoachIntent::chess_concept, matches(text, router_terms::concept_terms)},
  };

  const auto best = std::max_element(
      scores.begin(), scores.end(),
      [](const IntentScore& a, const IntentScore& b) { return a.score < b.score; });
  if (best != scores.end() && best->score > 0) return *best;
  if (request.position_fen || request.game_pgn) return {CoachIntent::position, 1};
  return {CoachIntent::chess_concept, 0};
}

double score_confidence(int score, bool contextual) {
  if (score >= 2) return 0.94;
  if (score == 1) return contextual ? 0.89 : 0.84;
  return contextual ? 0.72 : 0.0;
}

DomainRoute refine_with_tiny_model(DomainRoute route, const CoachRequest& request,
                                   const TinyIntentModel* model) {
  if (model == nullptr || !model->available() || request.user_text.empty()) {
    return route;
  }
  const bool ambiguous = route.intent == CoachIntent::unknown ||
                         route.intent == CoachIntent::off_topic ||
                         route.confidence < 0.80;
  if (!ambiguous) return route;

  const auto prediction = model->predict(
      request.user_text, request.position_fen.has_value() || request.game_pgn.has_value());
  if (!prediction.has_value() || prediction->confidence < 0.82) return route;

  if (!prediction->chess_domain || prediction->intent == CoachIntent::off_topic) {
    if (route.confidence < 0.65 && prediction->confidence >= 0.90) {
      return {CoachIntent::off_topic, prediction->confidence, false, false,
              CoachIntent::unknown};
    }
    return route;
  }
  if (prediction->intent == CoachIntent::unknown ||
      prediction->intent == CoachIntent::follow_up) {
    return route;
  }
  if (route.intent == CoachIntent::off_topic && prediction->confidence < 0.94) {
    return route;
  }

  route.intent = prediction->intent;
  route.chess_domain = true;
  route.confidence = std::max(route.confidence, prediction->confidence);
  return route;
}

}  // namespace

// -----------------------------------------------------------------------------
// Section: Public routing
// -----------------------------------------------------------------------------

DomainRoute ChessDomainRouter::route(const CoachRequest& request,
                                     const CoachSessionState* session,
                                     const TinyIntentModel* model) const {
  const std::string text = normalize(request.user_text);
  const bool has_session_context = session && !session->empty();
  const bool has_context = request.position_fen || request.game_pgn ||
                           has_session_context;
  const int follow_matches = matches(text, router_terms::follow_up);
  const int chess_matches = matches(text, router_terms::chess) +
                            matches(text, router_terms::player_names);
  const IntentScore scored = best_intent(text, request);
  const bool chess_domain = chess_matches > 0 || scored.score > 0 ||
                            request.position_fen.has_value() || request.game_pgn.has_value();

  DomainRoute route;
  if (text.empty()) {
    route = request.position_fen || request.game_pgn
                ? DomainRoute{CoachIntent::position, 0.72, true, false,
                              CoachIntent::unknown}
                : DomainRoute{CoachIntent::unknown, 0.0, false, false,
                              CoachIntent::unknown};
    return route;
  }

  if (follow_matches > 0 && has_context && scored.score == 0) {
    const CoachIntent context_intent =
        has_session_context ? session->current_topic : CoachIntent::unknown;
    route = {CoachIntent::follow_up, 0.90, true, true, context_intent};
    return route;
  }

  if (!chess_domain) {
    route = {CoachIntent::off_topic, 0.86, false, false, CoachIntent::unknown};
    return refine_with_tiny_model(route, request, model);
  }

  const CoachIntent intent = scored.score > 0 ? scored.intent : CoachIntent::chess_concept;
  const bool follow_up = follow_matches > 0 && has_session_context;
  const CoachIntent context_intent =
      follow_up ? session->current_topic : CoachIntent::unknown;
  route = {intent, score_confidence(scored.score, has_context || chess_matches > 0),
           true, follow_up, context_intent};
  return refine_with_tiny_model(route, request, model);
}

}  // namespace kchess::ai
