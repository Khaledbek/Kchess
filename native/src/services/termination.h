#pragma once

#include <algorithm>
#include <cctype>
#include <string>

namespace kchess {

// Value of a PGN header tag (e.g. [Termination "..."]), lowercased, or empty.
inline std::string pgn_tag_value(const std::string& pgn, const std::string& tag) {
  const std::string key = "[" + tag + " \"";
  const auto start = pgn.find(key);
  if (start == std::string::npos) return {};
  const auto from = start + key.size();
  const auto end = pgn.find('"', from);
  if (end == std::string::npos) return {};
  std::string value = pgn.substr(from, end - from);
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
    return static_cast<char>(std::tolower(character));
  });
  return value;
}

// Classify how a game ended into a coarse bucket the UI groups on:
// "checkmate" | "resignation" | "timeout" | "draw" | "other". Prefers the PGN
// Termination tag (present for provider games); falls back to the result string
// and a checkmate marker for imports without the tag.
inline std::string termination_bucket(
    const std::string& pgn, const std::string& result) {
  const std::string tag = pgn_tag_value(pgn, "Termination");
  const auto has = [&](const char* needle) {
    return tag.find(needle) != std::string::npos;
  };
  if (!tag.empty()) {
    if (has("checkmate")) return "checkmate";
    if (has("stalemate")) return "draw";
    if (has("resign")) return "resignation";
    if (has("drawn") || has("agree") || has("repetition") || has("insufficient")
        || has("50") || has("fifty")) {
      return "draw";
    }
    if (has("time")) return "timeout";
    if (has("abandon")) return "other";
  }
  if (result == "1/2-1/2" || result == "\xc2\xbd-\xc2\xbd") return "draw";
  if (pgn.find('#') != std::string::npos) return "checkmate";
  return "other";
}

}  // namespace kchess
