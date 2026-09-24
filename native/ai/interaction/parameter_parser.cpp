#include "interaction/parameter_parser.h"

#include <algorithm>
#include <cctype>
#include <regex>
#include <string>

namespace kchess::ai::interaction {
namespace {

std::string ascii_lower(std::string_view input) {
  std::string out(input);
  std::transform(out.begin(), out.end(), out.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return out;
}

bool contains_any(const std::string& text,
                  std::initializer_list<std::string_view> needles) {
  for (const auto needle : needles) {
    if (text.find(needle) != std::string::npos) {
      return true;
    }
  }
  return false;
}

std::optional<int> parse_elo(std::string_view original, const std::string& lower) {
  // Accept "1300 elo", "elo 1300", "rating 1300" and "1300 rating".
  // Keep the range broad enough for imported/custom profiles while rejecting
  // accidental move numbers and years.
  static const std::regex kNumberBefore(
      R"((?:^|[^0-9])([1-9][0-9]{2,3})\s*(?:elo|rating)(?:[^a-z0-9]|$))",
      std::regex::icase);
  static const std::regex kLabelBefore(
      R"((?:elo|rating)\s*[:=\-]?\s*([1-9][0-9]{2,3})(?:[^0-9]|$))",
      std::regex::icase);

  const std::string text(original);
  std::smatch match;
  auto parse_match = [](const std::smatch& m) -> std::optional<int> {
    if (m.size() < 2) return std::nullopt;
    try {
      const int value = std::stoi(m[1].str());
      if (value >= 100 && value <= 4000) return value;
    } catch (...) {
    }
    return std::nullopt;
  };

  if (std::regex_search(text, match, kNumberBefore)) {
    if (auto value = parse_match(match)) return value;
  }
  if (std::regex_search(text, match, kLabelBefore)) {
    if (auto value = parse_match(match)) return value;
  }

  // German users often write "1300er" for a rating class. Only accept this
  // shorthand when nearby wording clearly indicates a player/rating context.
  if (contains_any(lower, {"spieler", "player", "rating", "elo",
                           "gegen mich wie ein", "spiele wie ein"})) {
    static const std::regex kGermanClass(R"((?:^|[^0-9])([1-9][0-9]{2,3})er(?:[^a-z0-9]|$))",
                                         std::regex::icase);
    if (std::regex_search(text, match, kGermanClass)) {
      if (auto value = parse_match(match)) return value;
    }
  }
  return std::nullopt;
}

std::optional<int> parse_move_number(const std::string& lower) {
  static const std::regex kMoveNumber(
      R"((?:move|zug)\s*(?:number|nr\.?|#)?\s*([1-9][0-9]{0,2}))",
      std::regex::icase);
  std::smatch match;
  if (!std::regex_search(lower, match, kMoveNumber) || match.size() < 2) {
    return std::nullopt;
  }
  try {
    const int value = std::stoi(match[1].str());
    if (value >= 1 && value <= 300) return value;
  } catch (...) {
  }
  return std::nullopt;
}

}  // namespace

bool ParsedInteractionParameters::empty() const noexcept {
  return !elo && !move_number && !color && !move_reference && !game_mode &&
         !time_control;
}

ParsedInteractionParameters parse_interaction_parameters(std::string_view user_text) {
  ParsedInteractionParameters out;
  const std::string lower = ascii_lower(user_text);

  out.elo = parse_elo(user_text, lower);
  out.move_number = parse_move_number(lower);

  // Explicit color only. Avoid "side to move" inference here; that belongs to
  // the deterministic resolver where board context is available.
  if (contains_any(lower, {"white", "weiss", "weiß", "الابيض", "الأبيض"})) {
    out.color = RequestedColor::white;
  } else if (contains_any(lower, {"black", "schwarz", "الاسود", "الأسود"})) {
    out.color = RequestedColor::black;
  }

  if (contains_any(lower, {"last move", "previous move", "letzter zug", "letzten zug",
                           "vorheriger zug", "vorherigen zug", "النقلة السابقة"})) {
    out.move_reference = MoveReference::last_move;
  } else if (contains_any(lower, {"selected move", "markierter zug", "ausgewählter zug",
                                  "ausgewaehlter zug"})) {
    out.move_reference = MoveReference::selected_move;
  } else if (contains_any(lower, {"current move", "aktueller zug", "aktuellen zug"})) {
    out.move_reference = MoveReference::current_move;
  }

  if (contains_any(lower, {"bullet"})) {
    out.time_control = RequestedTimeControl::bullet;
  } else if (contains_any(lower, {"blitz"})) {
    out.time_control = RequestedTimeControl::blitz;
  } else if (contains_any(lower, {"rapid", "schnellschach"})) {
    out.time_control = RequestedTimeControl::rapid;
  } else if (contains_any(lower, {"classical", "klassisch", "langschach"})) {
    out.time_control = RequestedTimeControl::classical;
  }

  if (contains_any(lower, {"play against", "play me", "let's play", "lets play",
                           "lass uns spielen", "spiel gegen mich", "spiele gegen mich",
                           "العب ضدي", "لنلعب"})) {
    out.game_mode = RequestedGameMode::play;
  } else if (contains_any(lower, {"training", "trainieren", "training mode"})) {
    out.game_mode = RequestedGameMode::training;
  } else if (contains_any(lower, {"analyse", "analysis", "analysiere", "analyze"})) {
    out.game_mode = RequestedGameMode::analysis;
  }

  return out;
}

std::string_view to_string(RequestedColor value) noexcept {
  switch (value) {
    case RequestedColor::white:
      return "white";
    case RequestedColor::black:
      return "black";
  }
  return "";
}

std::string_view to_string(MoveReference value) noexcept {
  switch (value) {
    case MoveReference::last_move:
      return "last_move";
    case MoveReference::selected_move:
      return "selected_move";
    case MoveReference::current_move:
      return "current_move";
  }
  return "";
}

std::string_view to_string(RequestedGameMode value) noexcept {
  switch (value) {
    case RequestedGameMode::play:
      return "play";
    case RequestedGameMode::analysis:
      return "analysis";
    case RequestedGameMode::training:
      return "training";
  }
  return "";
}

std::string_view to_string(RequestedTimeControl value) noexcept {
  switch (value) {
    case RequestedTimeControl::bullet:
      return "bullet";
    case RequestedTimeControl::blitz:
      return "blitz";
    case RequestedTimeControl::rapid:
      return "rapid";
    case RequestedTimeControl::classical:
      return "classical";
  }
  return "";
}

}  // namespace kchess::ai::interaction
