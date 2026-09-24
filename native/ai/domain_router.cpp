#include "domain_router.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <string>
#include <string_view>

#include "conversation/coach_session.h"
#include "domain_router_terms.h"

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
    std::size_t pos = 0;
    while ((pos = text.find(term, pos)) != std::string_view::npos) {
      const auto word_byte = [](unsigned char c) {
        return c >= 128 || std::isalnum(c) != 0 || c == '_';
      };
      const auto end = pos + term.size();
      if ((pos == 0 || !word_byte(static_cast<unsigned char>(text[pos - 1]))) &&
          (term.size() >= 5 || end == text.size() ||
           !word_byte(static_cast<unsigned char>(text[end])))) {
        ++count;
        break;
      }
      pos = end;
    }
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

IntentScore best_intent(std::string_view text) {
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
  return {CoachIntent::chess_concept, 0};
}

double score_confidence(int score, bool contextual) {
  if (score >= 2) return 0.94;
  if (score == 1) return contextual ? 0.89 : 0.84;
  return contextual ? 0.72 : 0.0;
}


}  // namespace

// -----------------------------------------------------------------------------
// Section: Public routing
// -----------------------------------------------------------------------------

DomainRoute ChessDomainRouter::route(const CoachRequest& request,
                                     const CoachSessionState* session) const {
  const std::string text = normalize(request.user_text);
  const bool has_session_context = session && !session->empty();
  const bool has_context = request.position_fen || request.game_pgn ||
                           has_session_context;
  const int follow_matches = matches(text, router_terms::follow_up);
  const int chess_matches = matches(text, router_terms::chess) +
                            matches(text, router_terms::player_names);
  const IntentScore scored = best_intent(text);
  constexpr std::string_view personal_ownership[]{
      "mein", "meine", "meinen", "meinem", "meiner", "my"};
  const bool personal_library_question =
      matches(text, personal_ownership) > 0 &&
      matches(text, router_terms::profile_data) > 0;
  const bool chess_domain = chess_matches > 0 || scored.score > 0 ||
                            personal_library_question ||
                            request.position_fen.has_value() || request.game_pgn.has_value();

  DomainRoute route;
  if (text.empty()) {
    route = request.position_fen || request.game_pgn
                ? DomainRoute{CoachIntent::position, 0.72, true, false, false,
                              CoachIntent::unknown}
                : DomainRoute{CoachIntent::unknown, 0.0, false, false, false,
                              CoachIntent::unknown};
    return route;
  }

  constexpr std::string_view scope_follow_up[]{
      "what about", "and in", "and for", "und im", "und bei", "woher",
      "show examples", "zeig beispiele"};
  const bool refines_scope = has_session_context && matches(text, scope_follow_up) > 0;
  if (follow_matches > 0 && has_context && (scored.score == 0 || refines_scope)) {
    const CoachIntent context_intent =
        has_session_context ? session->current_topic : CoachIntent::unknown;
    route = {CoachIntent::follow_up, 0.90, true, true, false, context_intent};
    if (has_session_context) {
      route.inherited_query_family = session->query_family;
      route.inherited_profile_scope = session->profile_scope;
      route.inherited_needs_profile = session->needs_profile;
    }
    return route;
  }

  if (!chess_domain) {
    route = {CoachIntent::off_topic, 0.86, false, false, false, CoachIntent::unknown};
    return route;
  }

  const CoachIntent intent = scored.score > 0 ? scored.intent :
      (request.position_fen || request.game_pgn) ? CoachIntent::position :
      personal_library_question ? CoachIntent::player_development :
      CoachIntent::chess_concept;
  const bool follow_up = follow_matches > 0 && has_session_context;
  // Update 210: a concrete current question keeps its own intent, but any
  // established session may still contribute compact conversational context.
  // context_intent is continuity metadata only; QueryPlanner inherits it as the
  // effective intent exclusively for an elliptical CoachIntent::follow_up.
  // This lets questions such as "can you name one?" or a rephrased request on
  // the same board see the immediately preceding goal/answer without turning
  // that prior prose into current-board evidence.
  const CoachIntent context_intent =
      has_session_context ? session->current_topic : CoachIntent::unknown;
  route = {intent, score_confidence(scored.score, has_context || chess_matches > 0),
           true, follow_up, scored.score > 0, context_intent};
  if (route.intent != CoachIntent::follow_up &&
      route.intent != CoachIntent::unknown &&
      route.intent != CoachIntent::off_topic && !request.user_text.empty()) {
    route.explicit_current_intent = true;
  }
  return route;
}

}  // namespace kchess::ai
