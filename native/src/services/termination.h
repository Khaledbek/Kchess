// -----------------------------------------------------------------------------
// Section: Native game termination categories
// -----------------------------------------------------------------------------

#pragma once

#include <algorithm>
#include <cctype>
#include <string>

namespace kchess {

// Value of a PGN header tag (e.g. [Termination "..."]), lowercased, or empty.
inline std::string pgn_tag_value(const std::string& pgn,
                                 const std::string& tag) {
  const std::string key = "[" + tag + " \"";
  const auto start = pgn.find(key);
  if (start == std::string::npos) return {};
  const auto from = start + key.size();
  const auto end = pgn.find('"', from);
  if (end == std::string::npos) return {};
  std::string value = pgn.substr(from, end - from);
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char character) {
                   return static_cast<char>(std::tolower(character));
                 });
  return value;
}

// Classify how a game ended into a coarse bucket the UI groups on:
// "checkmate" | "resignation" | "timeout" | "draw" | "other". Prefers the PGN
// Termination tag (present for provider games); falls back to the result string
// and a checkmate marker for imports without the tag.
inline std::string termination_bucket(const std::string& pgn,
                                      const std::string& result) {
  const std::string tag = pgn_tag_value(pgn, "Termination");

  // Chess.com writes "<username> won on time" / "<username> won by checkmate",
  // so the leading token is a player name and must not be searched for
  // keywords: an opponent called "chessmaster50" otherwise reads as a
  // fifty-move draw, and "resigner99" as a resignation. Usernames cannot
  // contain spaces, so " won " only ever marks the verb. Lichess values
  // ("Time forfeit", "Normal") carry no name and are left intact.
  std::string reason = tag;
  if (reason.find(" won ") != std::string::npos) {
    const auto space = reason.find(' ');
    if (space != std::string::npos) reason = reason.substr(space + 1);
  }

  const auto has = [&](const char* needle) {
    return reason.find(needle) != std::string::npos;
  };
  if (!reason.empty()) {
    // Draw phrases first, each specific enough that stray digits or words left
    // in the reason cannot trigger them (never a bare "50").
    if (has("stalemate") || has("drawn") || has("agree") || has("repetition") ||
        has("insufficient") || has("50-move") || has("50 move") ||
        has("fifty-move") || has("fifty move")) {
      return "draw";
    }
    if (has("checkmate")) return "checkmate";
    if (has("resign")) return "resignation";
    if (has("time")) return "timeout";
    if (has("abandon")) return "other";
  }
  if (result == "1/2-1/2" || result == "\xc2\xbd-\xc2\xbd") return "draw";
  if (pgn.find('#') != std::string::npos) return "checkmate";
  return "other";
}

}  // namespace kchess
