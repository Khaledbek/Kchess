#include "query_classifier.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <string>
#include <string_view>

namespace kchess::ai {
namespace {

std::string lower_ascii(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
    return c < 128 ? static_cast<char>(std::tolower(c)) : static_cast<char>(c);
  });
  return text;
}

template <std::size_t N>
bool contains_any(std::string_view text,
                  const std::array<std::string_view, N>& terms) {
  return std::any_of(terms.begin(), terms.end(), [&](std::string_view term) {
    return text.find(term) != std::string_view::npos;
  });
}

bool contains_word(std::string_view text, std::string_view word) {
  const auto word_byte = [](unsigned char c) {
    return c >= 128 || std::isalnum(c) != 0 || c == '_';
  };
  std::size_t position = 0;
  while ((position = text.find(word, position)) != std::string_view::npos) {
    const auto end = position + word.size();
    if ((position == 0 || !word_byte(static_cast<unsigned char>(text[position - 1]))) &&
        (end == text.size() || !word_byte(static_cast<unsigned char>(text[end])))) {
      return true;
    }
    position = end;
  }
  return false;
}

template <std::size_t N>
void add_topic_matches(std::vector<std::string>& out, std::string_view text,
                       const std::array<std::pair<std::string_view, std::string_view>, N>& terms) {
  for (const auto& [needle, id] : terms) {
    if (text.find(needle) == std::string_view::npos) continue;
    if (std::find(out.begin(), out.end(), id) == out.end()) out.emplace_back(id);
  }
}

bool position_intent(CoachIntent intent) {
  return intent == CoachIntent::position || intent == CoachIntent::move_explanation ||
      intent == CoachIntent::plan || intent == CoachIntent::tactic ||
      intent == CoachIntent::game_review;
}

bool general_intent(CoachIntent intent) {
  return intent == CoachIntent::opening || intent == CoachIntent::endgame ||
      intent == CoachIntent::chess_concept || intent == CoachIntent::chess_rules ||
      intent == CoachIntent::chess_history;
}

bool scope_empty(const ProfileQueryScope& scope) {
  return scope.topics.empty() && scope.time_controls.empty() && scope.phases.empty() &&
      scope.player_colors.empty() && !scope.needs_endgame_material_type &&
      !scope.wants_proof && !scope.compare_scopes;
}

ProfileQueryScope classify_profile_scope(std::string_view text) {
  ProfileQueryScope scope;
  constexpr std::array topic_terms{
      std::pair{std::string_view{"weakness"}, std::string_view{"weaknesses"}},
      std::pair{std::string_view{"schwäch"}, std::string_view{"weaknesses"}},
      std::pair{std::string_view{"schlecht"}, std::string_view{"weaknesses"}},
      std::pair{std::string_view{"problem"}, std::string_view{"weaknesses"}},
      std::pair{std::string_view{"poor"}, std::string_view{"weaknesses"}},
      std::pair{std::string_view{"struggle"}, std::string_view{"weaknesses"}},
      std::pair{std::string_view{"strength"}, std::string_view{"strengths"}},
      std::pair{std::string_view{"stärk"}, std::string_view{"strengths"}},
      std::pair{std::string_view{"spielstil"}, std::string_view{"play_style"}},
      std::pair{std::string_view{"play style"}, std::string_view{"play_style"}},
      std::pair{std::string_view{"style"}, std::string_view{"play_style"}},
      std::pair{std::string_view{"improv"}, std::string_view{"improvement"}},
      std::pair{std::string_view{"verbesser"}, std::string_view{"improvement"}},
      std::pair{std::string_view{"fortschritt"}, std::string_view{"improvement"}},
      std::pair{std::string_view{"progress"}, std::string_view{"improvement"}},
      std::pair{std::string_view{"train"}, std::string_view{"training"}},
      std::pair{std::string_view{"üb"}, std::string_view{"training"}},
      std::pair{std::string_view{"opening"}, std::string_view{"opening"}},
      std::pair{std::string_view{"eröffnung"}, std::string_view{"opening"}},
      std::pair{std::string_view{"openning"}, std::string_view{"opening"}},
      std::pair{std::string_view{"endgame"}, std::string_view{"endgame"}},
      std::pair{std::string_view{"endspiel"}, std::string_view{"endgame"}},
      std::pair{std::string_view{"tactic"}, std::string_view{"tactics"}},
      std::pair{std::string_view{"takt"}, std::string_view{"tactics"}},
      std::pair{std::string_view{"calculat"}, std::string_view{"calculation"}},
      std::pair{std::string_view{"berechn"}, std::string_view{"calculation"}},
      std::pair{std::string_view{"position"}, std::string_view{"positional"}},
      std::pair{std::string_view{"defen"}, std::string_view{"defense"}},
      std::pair{std::string_view{"verteidig"}, std::string_view{"defense"}},
      std::pair{std::string_view{"king safety"}, std::string_view{"king_safety"}},
      std::pair{std::string_view{"königssicherheit"}, std::string_view{"king_safety"}},
      std::pair{std::string_view{"time management"}, std::string_view{"time_management"}},
      std::pair{std::string_view{"zeitmanagement"}, std::string_view{"time_management"}},
      std::pair{std::string_view{"zeitnot"}, std::string_view{"time_management"}},
      std::pair{std::string_view{"elo"}, std::string_view{"rating"}},
      std::pair{std::string_view{"rating"}, std::string_view{"rating"}},
      std::pair{std::string_view{"wertungszahl"}, std::string_view{"rating"}},
      std::pair{std::string_view{"statistik"}, std::string_view{"statistics"}},
      std::pair{std::string_view{"statistics"}, std::string_view{"statistics"}},
      std::pair{std::string_view{"win rate"}, std::string_view{"statistics"}},
      std::pair{std::string_view{"siegquote"}, std::string_view{"statistics"}},
      std::pair{std::string_view{"resultate"}, std::string_view{"statistics"}},
  };
  add_topic_matches(scope.topics, text, topic_terms);

  constexpr std::array time_terms{
      std::pair{std::string_view{"bullet"}, std::string_view{"bullet"}},
      std::pair{std::string_view{"blitz"}, std::string_view{"blitz"}},
      std::pair{std::string_view{"rapid"}, std::string_view{"rapid"}},
      std::pair{std::string_view{"classical"}, std::string_view{"classical"}},
      std::pair{std::string_view{"klassisch"}, std::string_view{"classical"}},
  };
  add_topic_matches(scope.time_controls, text, time_terms);

  constexpr std::array phase_terms{
      std::pair{std::string_view{"opening"}, std::string_view{"opening"}},
      std::pair{std::string_view{"eröffnung"}, std::string_view{"opening"}},
      std::pair{std::string_view{"middlegame"}, std::string_view{"middlegame"}},
      std::pair{std::string_view{"mittelspiel"}, std::string_view{"middlegame"}},
      std::pair{std::string_view{"endgame"}, std::string_view{"endgame"}},
      std::pair{std::string_view{"endspiel"}, std::string_view{"endgame"}},
  };
  add_topic_matches(scope.phases, text, phase_terms);

  constexpr std::array material_endgame_terms{
      std::string_view{"turmendspiel"}, std::string_view{"bauernendspiel"},
      std::string_view{"damenendspiel"}, std::string_view{"läuferendspiel"},
      std::string_view{"springerendspiel"}, std::string_view{"rook endgame"},
      std::string_view{"pawn endgame"}, std::string_view{"queen endgame"},
      std::string_view{"bishop endgame"}, std::string_view{"knight endgame"},
      std::string_view{"welche endspiel"}, std::string_view{"welchen endspiel"},
      std::string_view{"welchem endspiel"},
      std::string_view{"welche endgame"}, std::string_view{"welchen endgame"},
      std::string_view{"welchem endgame"},
      std::string_view{"which endgame"}, std::string_view{"what endgame"},
      std::string_view{"what type of endgame"},
  };
  scope.needs_endgame_material_type = contains_any(text, material_endgame_terms);

  constexpr std::array white_context{
      std::string_view{"mit weiß"}, std::string_view{"mit weiss"},
      std::string_view{"als weiß"}, std::string_view{"als weiss"},
      std::string_view{"weißen steinen"}, std::string_view{"weissen steinen"},
      std::string_view{"meine weiß"}, std::string_view{"meinen weiß"},
      std::string_view{"meine weiss"}, std::string_view{"meinen weiss"},
      std::string_view{"with white"}, std::string_view{"as white"},
      std::string_view{"playing white"}, std::string_view{"white pieces"},
      std::string_view{"white games"}, std::string_view{"my white"},
  };
  if (contains_any(text, white_context) &&
      (contains_word(text, "weiß") || contains_word(text, "weiss") ||
       contains_word(text, "weißen") || contains_word(text, "weissen") ||
       contains_word(text, "white"))) {
    scope.player_colors.push_back("white");
  }
  constexpr std::array black_context{
      std::string_view{"mit schwarz"}, std::string_view{"als schwarz"},
      std::string_view{"schwarzen steinen"},
      std::string_view{"meine schwarz"}, std::string_view{"meinen schwarz"},
      std::string_view{"with black"}, std::string_view{"as black"},
      std::string_view{"playing black"}, std::string_view{"black pieces"},
      std::string_view{"black games"}, std::string_view{"my black"},
  };
  if (contains_any(text, black_context) &&
      (contains_word(text, "schwarz") || contains_word(text, "schwarzen") ||
       contains_word(text, "black"))) {
    scope.player_colors.push_back("black");
  }

  constexpr std::array proof_terms{
      std::string_view{"why do you think"}, std::string_view{"how do you know"},
      std::string_view{"show examples"}, std::string_view{"prove"},
      std::string_view{"warum glaubst"}, std::string_view{"woher weißt"},
      std::string_view{"zeig beispiele"}, std::string_view{"beleg"},
  };
  scope.wants_proof = contains_any(text, proof_terms);

  constexpr std::array compare_terms{
      std::string_view{"compare"}, std::string_view{"versus"}, std::string_view{" vs "},
      std::string_view{"compared"}, std::string_view{"vergleich"},
      std::string_view{"unterschied"}, std::string_view{"besser als"},
      std::string_view{"schlechter als"},
  };
  scope.compare_scopes = contains_any(text, compare_terms) ||
      scope.time_controls.size() > 1 || scope.player_colors.size() > 1;
  return scope;
}

}  // namespace

CoachQueryClassification CoachQueryClassifier::classify(
    const CoachRequest& request, const DomainRoute& route) const {
  CoachQueryClassification out;
  out.intent = route.context_intent != CoachIntent::unknown && route.follow_up
      ? route.context_intent
      : route.intent;
  if (!route.chess_domain || out.intent == CoachIntent::off_topic ||
      out.intent == CoachIntent::unknown) {
    return out;
  }

  const bool has_position = request.position_fen.has_value() || request.game_pgn.has_value();
  const std::string text = lower_ascii(request.user_text);
  out.profile_scope = classify_profile_scope(text);

  constexpr std::array personal_terms{
      std::string_view{"my weakness"}, std::string_view{"my weaknesses"},
      std::string_view{"my strength"}, std::string_view{"my strengths"},
      std::string_view{"my style"}, std::string_view{"my play style"},
      std::string_view{"my chess"}, std::string_view{"my games"},
      std::string_view{"my profile"}, std::string_view{"my blitz"},
      std::string_view{"my elo"}, std::string_view{"my rating"},
      std::string_view{"my openings"}, std::string_view{"which openings do i"},
      std::string_view{"which opening do i"},
      std::string_view{"what opening do i"},
      std::string_view{"my rapid"}, std::string_view{"my bullet"},
      std::string_view{"my progress"}, std::string_view{"why do i lose"},
      std::string_view{"why do i keep"}, std::string_view{"am i improving"},
      std::string_view{"meine schw"}, std::string_view{"meine stärk"},
      std::string_view{"meine staerk"},
      std::string_view{"mein spielstil"}, std::string_view{"meinen spielstil"},
      std::string_view{"wie spiele ich"}, std::string_view{"was für ein spieler"},
      std::string_view{"habe ich mich verbessert"}, std::string_view{"mein profil"},
      std::string_view{"mein elo"}, std::string_view{"meine elo"},
      std::string_view{"welche elo habe ich"}, std::string_view{"wie hoch ist mein elo"},
      std::string_view{"meine wertungszahl"}, std::string_view{"mein rating"},
      std::string_view{"meine eröffnung"}, std::string_view{"welche eröffnung spiele ich"},
      std::string_view{"welche openings spiele ich"},
      std::string_view{"welche opening spiele ich"},
      std::string_view{"welche openning spiele ich"},
      std::string_view{"meine partien"}, std::string_view{"mein blitz"},
      std::string_view{"mein rapid"}, std::string_view{"mein fortschritt"},
      std::string_view{"how can i improve"}, std::string_view{"how do i improve"},
      std::string_view{"wie kann ich mich verbessern"}, std::string_view{"wie werde ich besser"},
      std::string_view{"warum verliere ich"}, std::string_view{"mein zeitmanagement"},
  };
  constexpr std::array profile_subjects{
      std::string_view{"eröffnung"}, std::string_view{"opening"},
      std::string_view{"endspiel"}, std::string_view{"endgame"},
      std::string_view{"mittelspiel"}, std::string_view{"middlegame"},
      std::string_view{"partien"}, std::string_view{"games"},
      std::string_view{"statistik"}, std::string_view{"statistics"},
      std::string_view{"schach"}, std::string_view{"chess"},
      std::string_view{"spielstil"}, std::string_view{"style"},
      std::string_view{"schwäch"}, std::string_view{"weakness"},
      std::string_view{"elo"}, std::string_view{"rating"},
      std::string_view{"weiß"}, std::string_view{"white"},
      std::string_view{"schwarz"}, std::string_view{"black"},
      std::string_view{"siegquote"}, std::string_view{"win rate"},
      std::string_view{"resultate"}, std::string_view{"results"},
      std::string_view{"repertoire"},
  };
  constexpr std::array personal_ownership{
      std::string_view{"mein "}, std::string_view{"meine "},
      std::string_view{"meinen "}, std::string_view{"my "},
  };
  constexpr std::array personal_action{
      std::string_view{"spiele ich"}, std::string_view{"habe ich"},
      std::string_view{"verliere ich"}, std::string_view{"gewinne ich"},
      std::string_view{"do i play"}, std::string_view{"have i played"},
      std::string_view{"i struggle"}, std::string_view{"i lose"},
  };
  const bool explicit_personal = contains_any(text, personal_terms) ||
      (contains_any(text, profile_subjects) &&
       (contains_any(text, personal_ownership) ||
        contains_any(text, personal_action)));
  const bool inherited_personal = route.follow_up && route.inherited_needs_profile;
  if (route.follow_up && inherited_personal && scope_empty(out.profile_scope)) {
    out.profile_scope = route.inherited_profile_scope;
  } else if (route.follow_up && inherited_personal) {
    if (out.profile_scope.topics.empty()) {
      out.profile_scope.topics = route.inherited_profile_scope.topics;
    }
    if (out.profile_scope.time_controls.empty()) {
      out.profile_scope.time_controls = route.inherited_profile_scope.time_controls;
    }
    if (out.profile_scope.phases.empty()) {
      out.profile_scope.phases = route.inherited_profile_scope.phases;
    }
    if (out.profile_scope.player_colors.empty()) {
      out.profile_scope.player_colors = route.inherited_profile_scope.player_colors;
    }
    out.profile_scope.needs_endgame_material_type =
        out.profile_scope.needs_endgame_material_type ||
        route.inherited_profile_scope.needs_endgame_material_type;
    out.profile_scope.wants_proof = out.profile_scope.wants_proof ||
        route.inherited_profile_scope.wants_proof;
  }

  if (has_position && (position_intent(out.intent) || request.hint_move_uci.has_value() ||
                       request.mode == CoachMode::hint || request.mode == CoachMode::compare ||
                       request.mode == CoachMode::quiz)) {
    out.family = QueryFamily::position;
    out.needs_position = true;
    const bool profile_already_consumed_for_personal_training =
        request.personal_training_position_selected;
    if (profile_already_consumed_for_personal_training) {
      // The profile was already consumed by CoachService to select one concrete
      // own-game exercise. Do not carry inherited personal scope into the
      // provider turn or trigger a second Knowledge/profile retrieval.
      out.profile_scope = {};
    }
    out.needs_profile = !profile_already_consumed_for_personal_training &&
        (explicit_personal || inherited_personal ||
         out.intent == CoachIntent::game_review);
    out.needs_concepts = out.intent == CoachIntent::plan || out.intent == CoachIntent::tactic ||
        out.intent == CoachIntent::training;
    out.may_need_engine = true;
    out.confidence = out.needs_profile ? 0.98 : 0.96;
    return out;
  }

  // Training is personal only when the user asks what *they* should train or a
  // prior personal scope is being continued. Generic "how to train endgames"
  // remains general chess and must not expose profile data.
  const bool personal_training = out.intent == CoachIntent::training &&
      (explicit_personal || inherited_personal);
  if (explicit_personal || inherited_personal || personal_training) {
    out.family = QueryFamily::personal_chess;
    out.needs_profile = true;
    out.needs_concepts = out.intent == CoachIntent::training;
    out.confidence = explicit_personal || inherited_personal ? 0.98 : 0.93;
    return out;
  }

  if (general_intent(out.intent) || out.intent == CoachIntent::training ||
      out.intent == CoachIntent::player_development) {
    out.family = QueryFamily::general_chess;
    out.needs_concepts = true;
    out.confidence = out.intent == CoachIntent::training ? 0.90 : 0.95;
    return out;
  }

  if (has_position) {
    out.family = QueryFamily::position;
    out.needs_position = true;
    out.may_need_engine = true;
    out.confidence = 0.85;
    return out;
  }

  out.family = route.inherited_query_family != QueryFamily::unknown
      ? route.inherited_query_family
      : QueryFamily::general_chess;
  out.needs_profile = out.family == QueryFamily::personal_chess;
  out.needs_concepts = out.family == QueryFamily::general_chess;
  out.confidence = route.follow_up ? 0.88 : 0.75;
  return out;
}

}  // namespace kchess::ai
