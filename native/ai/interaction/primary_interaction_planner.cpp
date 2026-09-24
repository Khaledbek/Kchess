#include "interaction/primary_interaction_planner.h"

#include <algorithm>
#include <cctype>
#include <initializer_list>
#include <string>

#include "interaction/semantic_dictionary.h"

namespace kchess::ai::interaction {
namespace {

std::string lower_ascii(std::string_view text) {
  std::string out(text);
  std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
    return c < 128 ? static_cast<char>(std::tolower(c)) : static_cast<char>(c);
  });
  return out;
}

bool has(const std::string& text, std::string_view token) {
  return text.find(token) != std::string::npos;
}

bool has_any(const std::string& text,
             std::initializer_list<std::string_view> phrases) {
  return std::any_of(phrases.begin(), phrases.end(), [&](std::string_view phrase) {
    return has(text, phrase);
  });
}

void add(PlannerSelection& selection, std::string_view id) {
  const auto group = semantic_group_for(id);
  if (!group || selection.contains(id)) return;
  selection.tokens.push_back({std::string(id), *group});
}

bool is_start_game_request(const std::string& text) {
  return has_any(text, {
      // German, including the common typo from the reported regression.
      "lass uns spielen", "lass uns eine partie", "lass uns einer partie",
      "partie spielen", "ein spiel starten", "eine partie starten",
      "starte ein spiel", "starte eine partie", "spiel starten",
      "ich möchte spielen", "ich moechte spielen", "ich will spielen",
      "spiel gegen mich", "spiele gegen mich",
      // English.
      "let's play", "lets play", "let us play", "let us start a game",
      "start a game", "start game", "play a game", "i want to play",
      "play against me", "play me",
      // Arabic.
      "لنلعب", "ابدأ مباراة", "ابدأ لعبة", "أريد أن ألعب", "اريد ان العب",
      "العب ضدي",
  });
}

bool is_resume_game_request(const std::string& text) {
  return has_any(text, {
      "partie fortsetzen", "spiel fortsetzen", "weiter spielen",
      "partie weiterspielen", "spiel weiterspielen",
      "continue the game", "continue game", "resume the game", "resume game",
      "keep playing", "تابع المباراة", "اكمل المباراة", "أكمل المباراة",
  });
}

bool is_stop_game_request(const std::string& text) {
  return has_any(text, {
      "partie beenden", "spiel beenden", "beende die partie", "beende das spiel",
      "stop the game", "stop game", "end the game", "end game",
      "أوقف المباراة", "اوقف المباراة", "انه المباراة", "أنهِ المباراة",
  });
}


bool is_live_coaching_request(const std::string& text) {
  return has_any(text, {
      "coach me", "live coaching",
      "während der partie tipps", "waehrend der partie tipps",
      "während des spiels tipps", "waehrend des spiels tipps",
      "gib mir während der partie tipps", "gib mir waehrend der partie tipps",
      "erklär mir während der partie", "erklaer mir waehrend der partie",
      "erkläre mir während der partie", "erklaere mir waehrend der partie",
      "erklär mir während des spiels", "erklaer mir waehrend des spiels",
      "explain my mistakes while", "explain my moves while",
      "währenddessen meine fehler", "waehrenddessen meine fehler",
      "coach me while", "coach me during",
      "دربني أثناء المباراة", "دربني خلال المباراة",
  });
}

bool is_move_verdict_request(const std::string& text) {
  return has_any(text, {
      // German.
      "gut oder schlecht", "schlecht oder gut", "ist der zug gut",
      "ist dieser zug gut", "ist das zug gut", "ist der zug schlecht",
      "ist dieser zug schlecht", "war das ein blunder", "ist das ein blunder",
      "ist das wirklich ein blunder", "bewerte den zug", "bewerte diesen zug",
      "wie gut ist der zug", "wie schlecht ist der zug",
      // English.
      "good or bad", "bad or good", "is the move good", "is this move good",
      "is that move good", "is the move bad", "is this move bad",
      "was that a blunder", "is that a blunder", "is this a blunder",
      "evaluate the move", "rate the move", "how good is the move",
      "how bad is the move",
      // Arabic.
      "هل النقلة جيدة", "هل هذه النقلة جيدة", "هل النقلة سيئة",
      "هل هذه النقلة سيئة", "هل هذا خطأ فادح", "قيّم النقلة", "قيم النقلة",
  });
}

bool is_position_verdict_request(const std::string& text) {
  return has_any(text, {
      // German.
      "wer steht besser", "wer steht hier besser", "wie steht die stellung",
      "wie ist die stellung", "bewerte die stellung", "wer gewinnt hier",
      // English.
      "who is better", "who stands better", "who is better here",
      "how is the position", "evaluate the position", "who is winning here",
      // Arabic.
      "من الأفضل", "من يقف أفضل", "كيف الوضع", "قيّم الوضع", "قيم الوضع",
  });
}

bool has_analysis_semantics(const PlannerSelection& selection) {
  return selection.contains("find_worst") || selection.contains("find_fastest_loss") ||
      selection.contains("find_fastest_win") || selection.contains("find_best") ||
      selection.contains("find_tactic") || selection.contains("find_plan") ||
      selection.contains("find_blunder") || selection.contains("find_mistake") ||
      selection.contains("evaluate_move") || selection.contains("evaluate_position") ||
      selection.contains("compare") || selection.contains("predict");
}

}  // namespace

PlannerSelection PrimaryInteractionPlanner::classify(
    std::string_view user_text, const ConversationState& state) const {
  PlannerSelection result;
  const auto text = lower_ascii(user_text);
  const bool live_coaching_requested = is_live_coaching_request(text);

  // Direct product/session intent is resolved before board scope. A visible
  // start position must never turn "let us start a game" into a position query.
  if (is_start_game_request(text)) {
    result.request_kind = InteractionRequestKind::app_action;
    add(result, "start_game");
  } else if (is_resume_game_request(text)) {
    result.request_kind = InteractionRequestKind::conversation_control;
    add(result, "continue_game");
  } else if (is_stop_game_request(text)) {
    result.request_kind = InteractionRequestKind::conversation_control;
    add(result, "stop_game");
  }

  // Scope defaults to the active board only for semantic context. The request
  // kind above remains authoritative and later routing can suppress positional
  // evidence for direct app/session actions.
  if (state.position_key.has_value()) add(result, "current_position");

  if (has(text, "schlechtest") || has(text, "worst")) {
    add(result, "find_worst");
  } else if (has(text, "schnell") &&
             (has(text, "verlieren") || has(text, "lose"))) {
    add(result, "find_fastest_loss");
  } else if (has(text, "schnell") &&
             (has(text, "gewinnen") || has(text, "win"))) {
    add(result, "find_fastest_win");
  } else if (has(text, "beste") || has(text, "best move") ||
             has(text, "besten zug")) {
    add(result, "find_best");
  } else if (has(text, "taktik") || has(text, "tactic")) {
    add(result, "find_tactic");
  } else if (has(text, "plan")) {
    add(result, "find_plan");
  } else if (has(text, "blunder") || has(text, "patzer")) {
    add(result, "find_blunder");
  } else if (has(text, "fehler") || has(text, "mistake")) {
    if (!(result.contains("start_game") && live_coaching_requested)) {
      add(result, "find_mistake");
    }
  }

  // Explicit chess judgments must be grounded before language generation.
  // These semantics are deliberately separate from generic position questions
  // so the orchestrator can require a native verdict and fail closed when the
  // engine/classification evidence is unavailable.
  if (is_move_verdict_request(text)) add(result, "evaluate_move");
  if (is_position_verdict_request(text)) add(result, "evaluate_position");

  if (!result.contains("start_game") &&
      (has(text, "elo") || has(text, "rating") ||
       has(text, "spieler mit") || has(text, "player with"))) {
    add(result, "predict");
    add(result, "human_likelihood");
    add(result, "human_move_prediction");
    add(result, "human_model");
  }
  if (result.contains("start_game") &&
      has_any(text, {"gegen mich wie ein", "spiele wie ein", "play like a",
                     "play like an", "human style", "menschlich",
                     "مثل لاعب"})) {
    add(result, "human_style");
  }

  if ((has(text, "warum") || has(text, "why") || has(text, "erklär") ||
       has(text, "erklaer") || has(text, "explain")) &&
      !(result.contains("start_game") && live_coaching_requested)) {
    add(result, "explain");
  }
  if (has(text, "vergleich") || has(text, "compare") || has(text, "vs")) {
    add(result, "compare");
  }
  if (has(text, "variante") || has(text, "line")) add(result, "show_line");
  if (has(text, "pfeil") || has(text, "arrow")) add(result, "show_arrows");
  if (has(text, "kandidaten") || has(text, "candidates")) {
    add(result, "show_candidates");
  }
  if (has(text, "eröffnung") || has(text, "eroeffnung") ||
      has(text, "opening")) {
    add(result, "opening");
  }
  if (has(text, "trainier") || has(text, "quiz") || has(text, "training")) {
    add(result, "train");
  }
  if (live_coaching_requested) {
    add(result, "live_coaching");
    // Mixed action + coaching requests need a short language acknowledgement
    // in addition to the native session actions. The provider never selects or
    // executes those actions; it only renders the requested language portion.
    if (result.contains("start_game")) add(result, "answer");
  }

  if (result.request_kind == InteractionRequestKind::question &&
      has_analysis_semantics(result)) {
    result.request_kind = InteractionRequestKind::analysis;
  }

  // A non-empty chess request that only established scope still needs an answer.
  // Direct app/session actions deliberately do not gain an implicit prose answer.
  if (result.request_kind == InteractionRequestKind::question &&
      (result.tokens.empty() ||
       (result.tokens.size() == 1 && result.contains("current_position")))) {
    add(result, "answer");
  }
  return result;
}

}  // namespace kchess::ai::interaction
