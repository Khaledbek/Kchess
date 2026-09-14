#include "entity_extractor.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <regex>
#include <string>
#include <string_view>

namespace kchess::knowledge {
namespace {

std::string lower_ascii(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
    return c < 128 ? static_cast<char>(std::tolower(c)) : static_cast<char>(c);
  });
  return text;
}

std::string trim_copy(std::string_view value) {
  std::size_t begin = 0;
  while (begin < value.size() &&
         std::isspace(static_cast<unsigned char>(value[begin])) != 0) {
    ++begin;
  }
  std::size_t end = value.size();
  while (end > begin &&
         std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
    --end;
  }
  return std::string(value.substr(begin, end - begin));
}

bool contains(std::string_view text, std::string_view needle) {
  return text.find(needle) != std::string_view::npos;
}

bool word_byte(unsigned char c) {
  return c >= 128 || std::isalnum(c) != 0 || c == '_';
}

bool contains_word(std::string_view text, std::string_view word) {
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

void append_unique(std::vector<KnowledgeQueryEntity>& out,
                   KnowledgeQueryEntity entity) {
  const auto duplicate = std::find_if(
      out.begin(), out.end(), [&](const KnowledgeQueryEntity& existing) {
        return existing.kind == entity.kind &&
            existing.canonical_value == entity.canonical_value;
      });
  if (duplicate == out.end()) out.push_back(std::move(entity));
}

template <std::size_t N>
void add_aliases(
    std::vector<KnowledgeQueryEntity>& out, std::string_view text,
    KnowledgeQueryEntityKind kind,
    const std::array<std::pair<std::string_view, std::string_view>, N>& aliases,
    double confidence = 0.98) {
  for (const auto& [needle, canonical] : aliases) {
    if (!contains(text, needle)) continue;
    append_unique(out, KnowledgeQueryEntity{
                           kind, std::string(canonical), std::string(needle), confidence});
  }
}

bool opening_scope(const ai::QueryPlan& plan) {
  if (plan.needs_opening || plan.intent == ai::CoachIntent::opening) return true;
  return std::find(plan.profile_scope.topics.begin(), plan.profile_scope.topics.end(),
                   "opening") != plan.profile_scope.topics.end() ||
      std::find(plan.profile_scope.phases.begin(), plan.profile_scope.phases.end(),
                "opening") != plan.profile_scope.phases.end();
}

void add_plan_scope_entities(std::vector<KnowledgeQueryEntity>& out,
                             const ai::QueryPlan& plan) {
  for (const auto& value : plan.profile_scope.time_controls) {
    append_unique(out, {KnowledgeQueryEntityKind::kTimeControl, value, value, 1.0});
  }
  for (const auto& value : plan.profile_scope.phases) {
    append_unique(out, {KnowledgeQueryEntityKind::kGamePhase, value, value, 1.0});
  }
  for (const auto& value : plan.profile_scope.player_colors) {
    append_unique(out, {KnowledgeQueryEntityKind::kPlayerColor, value, value, 1.0});
  }
  if (plan.profile_scope.wants_proof) {
    append_unique(out, {KnowledgeQueryEntityKind::kEvidenceRequest, "proof", "proof", 1.0});
  }
}

void add_eco_codes(std::vector<KnowledgeQueryEntity>& out,
                   std::string_view original_text) {
  static const std::regex eco_regex(R"((?:^|[^A-Za-z0-9])([A-Ea-e][0-9]{2})(?=$|[^A-Za-z0-9]))");
  const std::string text(original_text);
  for (std::sregex_iterator it(text.begin(), text.end(), eco_regex), end;
       it != end; ++it) {
    std::string eco = (*it)[1].str();
    eco[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(eco[0])));
    append_unique(out, {KnowledgeQueryEntityKind::kEcoCode, eco, (*it)[1].str(), 1.0});
  }
}

void add_opening_name_hint(std::vector<KnowledgeQueryEntity>& out,
                           std::string_view original_text,
                           std::string_view lower_text,
                           const ai::QueryPlan& plan) {
  if (!opening_scope(plan)) return;

  // Explicit quotes are the safest open-ended named-entity hint. The graph's
  // lexical retrieval still resolves the phrase to the authoritative Opening.
  static const std::regex quoted(R"([\"']([^\"']{3,80})[\"'])");
  const std::string original(original_text);
  for (std::sregex_iterator it(original.begin(), original.end(), quoted), end;
       it != end; ++it) {
    const auto phrase = trim_copy((*it)[1].str());
    if (!phrase.empty()) {
      append_unique(out, {KnowledgeQueryEntityKind::kOpeningName,
                          lower_ascii(phrase), phrase, 0.96});
    }
  }

  // Keep a compact alias set only for highly conventional names that users
  // commonly write without the word "opening". It is a query-normalization
  // aid, not a catalog; unknown names remain discoverable through lexical/vector
  // retrieval in Update 119.
  constexpr std::array common_openings{
      std::pair{std::string_view{"ruy lopez"}, std::string_view{"ruy lopez"}},
      std::pair{std::string_view{"spanish game"}, std::string_view{"ruy lopez"}},
      std::pair{std::string_view{"spanische partie"}, std::string_view{"ruy lopez"}},
      std::pair{std::string_view{"sicilian"}, std::string_view{"sicilian defense"}},
      std::pair{std::string_view{"sizilian"}, std::string_view{"sicilian defense"}},
      std::pair{std::string_view{"caro-kann"}, std::string_view{"caro-kann"}},
      std::pair{std::string_view{"queen's gambit"}, std::string_view{"queen's gambit"}},
      std::pair{std::string_view{"queens gambit"}, std::string_view{"queen's gambit"}},
      std::pair{std::string_view{"damengambit"}, std::string_view{"queen's gambit"}},
      std::pair{std::string_view{"king's indian"}, std::string_view{"king's indian"}},
      std::pair{std::string_view{"kings indian"}, std::string_view{"king's indian"}},
      std::pair{std::string_view{"königsindisch"}, std::string_view{"king's indian"}},
      std::pair{std::string_view{"london system"}, std::string_view{"london system"}},
      std::pair{std::string_view{"italian game"}, std::string_view{"italian game"}},
      std::pair{std::string_view{"italienische partie"}, std::string_view{"italian game"}},
  };
  add_aliases(out, lower_text, KnowledgeQueryEntityKind::kOpeningName,
              common_openings, 0.99);
}

}  // namespace

// -----------------------------------------------------------------------------
// Section: Public extraction
// -----------------------------------------------------------------------------

std::vector<KnowledgeQueryEntity> KnowledgeEntityExtractor::extract(
    std::string_view query_text, const ai::QueryPlan& plan) const {
  std::vector<KnowledgeQueryEntity> out;
  const std::string lower = lower_ascii(std::string(query_text));

  add_plan_scope_entities(out, plan);
  add_eco_codes(out, query_text);

  constexpr std::array time_controls{
      std::pair{std::string_view{"bullet"}, std::string_view{"bullet"}},
      std::pair{std::string_view{"blitz"}, std::string_view{"blitz"}},
      std::pair{std::string_view{"rapid"}, std::string_view{"rapid"}},
      std::pair{std::string_view{"classical"}, std::string_view{"classical"}},
      std::pair{std::string_view{"klassisch"}, std::string_view{"classical"}},
      std::pair{std::string_view{"daily"}, std::string_view{"daily"}},
      std::pair{std::string_view{"correspondence"}, std::string_view{"daily"}},
  };
  add_aliases(out, lower, KnowledgeQueryEntityKind::kTimeControl, time_controls);

  constexpr std::array phases{
      std::pair{std::string_view{"opening"}, std::string_view{"opening"}},
      std::pair{std::string_view{"eröffnung"}, std::string_view{"opening"}},
      std::pair{std::string_view{"middlegame"}, std::string_view{"middlegame"}},
      std::pair{std::string_view{"mittelspiel"}, std::string_view{"middlegame"}},
      std::pair{std::string_view{"endgame"}, std::string_view{"endgame"}},
      std::pair{std::string_view{"endspiel"}, std::string_view{"endgame"}},
  };
  add_aliases(out, lower, KnowledgeQueryEntityKind::kGamePhase, phases);

  constexpr std::array colors{
      std::pair{std::string_view{"with white"}, std::string_view{"white"}},
      std::pair{std::string_view{"as white"}, std::string_view{"white"}},
      std::pair{std::string_view{"mit weiß"}, std::string_view{"white"}},
      std::pair{std::string_view{"mit weiss"}, std::string_view{"white"}},
      std::pair{std::string_view{"als weiß"}, std::string_view{"white"}},
      std::pair{std::string_view{"als weiss"}, std::string_view{"white"}},
      std::pair{std::string_view{"with black"}, std::string_view{"black"}},
      std::pair{std::string_view{"as black"}, std::string_view{"black"}},
      std::pair{std::string_view{"mit schwarz"}, std::string_view{"black"}},
      std::pair{std::string_view{"als schwarz"}, std::string_view{"black"}},
  };
  add_aliases(out, lower, KnowledgeQueryEntityKind::kPlayerColor, colors);

  constexpr std::array results{
      std::pair{std::string_view{"win rate"}, std::string_view{"win"}},
      std::pair{std::string_view{"siegquote"}, std::string_view{"win"}},
      std::pair{std::string_view{"wins"}, std::string_view{"win"}},
      std::pair{std::string_view{"siege"}, std::string_view{"win"}},
      std::pair{std::string_view{"losses"}, std::string_view{"loss"}},
      std::pair{std::string_view{"niederlagen"}, std::string_view{"loss"}},
      std::pair{std::string_view{"draws"}, std::string_view{"draw"}},
      std::pair{std::string_view{"remis"}, std::string_view{"draw"}},
  };
  add_aliases(out, lower, KnowledgeQueryEntityKind::kResult, results, 0.96);

  constexpr std::array terminations{
      std::pair{std::string_view{"checkmate"}, std::string_view{"checkmate"}},
      std::pair{std::string_view{"matt"}, std::string_view{"checkmate"}},
      std::pair{std::string_view{"timeout"}, std::string_view{"timeout"}},
      std::pair{std::string_view{"time out"}, std::string_view{"timeout"}},
      std::pair{std::string_view{"zeitüberschreitung"}, std::string_view{"timeout"}},
      std::pair{std::string_view{"resignation"}, std::string_view{"resignation"}},
      std::pair{std::string_view{"resign"}, std::string_view{"resignation"}},
      std::pair{std::string_view{"aufgabe"}, std::string_view{"resignation"}},
      std::pair{std::string_view{"abbruch"}, std::string_view{"aborted"}},
      std::pair{std::string_view{"aborted"}, std::string_view{"aborted"}},
  };
  add_aliases(out, lower, KnowledgeQueryEntityKind::kTermination,
              terminations, 0.99);

  constexpr std::array temporal{
      std::pair{std::string_view{"recent"}, std::string_view{"recent"}},
      std::pair{std::string_view{"recently"}, std::string_view{"recent"}},
      std::pair{std::string_view{"aktuell"}, std::string_view{"recent"}},
      std::pair{std::string_view{"zuletzt"}, std::string_view{"recent"}},
      std::pair{std::string_view{"letzte zeit"}, std::string_view{"recent"}},
      std::pair{std::string_view{"lifetime"}, std::string_view{"lifetime"}},
      std::pair{std::string_view{"all time"}, std::string_view{"lifetime"}},
      std::pair{std::string_view{"insgesamt"}, std::string_view{"lifetime"}},
      std::pair{std::string_view{"historical"}, std::string_view{"historical"}},
      std::pair{std::string_view{"historisch"}, std::string_view{"historical"}},
      std::pair{std::string_view{"früher"}, std::string_view{"historical"}},
  };
  add_aliases(out, lower, KnowledgeQueryEntityKind::kTemporalScope, temporal,
              0.98);

  constexpr std::array metrics{
      std::pair{std::string_view{"accuracy"}, std::string_view{"accuracy"}},
      std::pair{std::string_view{"genauigkeit"}, std::string_view{"accuracy"}},
      std::pair{std::string_view{"win rate"}, std::string_view{"win_rate"}},
      std::pair{std::string_view{"siegquote"}, std::string_view{"win_rate"}},
      std::pair{std::string_view{"rating"}, std::string_view{"rating"}},
      std::pair{std::string_view{"elo"}, std::string_view{"rating"}},
      std::pair{std::string_view{"score"}, std::string_view{"score"}},
      std::pair{std::string_view{"häufig"}, std::string_view{"frequency"}},
      std::pair{std::string_view{"frequent"}, std::string_view{"frequency"}},
      std::pair{std::string_view{"how often"}, std::string_view{"frequency"}},
      std::pair{std::string_view{"wie oft"}, std::string_view{"frequency"}},
  };
  add_aliases(out, lower, KnowledgeQueryEntityKind::kStatisticMetric, metrics,
              0.96);

  constexpr std::array position_refs{
      std::pair{std::string_view{"this position"}, std::string_view{"current_position"}},
      std::pair{std::string_view{"current position"}, std::string_view{"current_position"}},
      std::pair{std::string_view{"on the board"}, std::string_view{"current_position"}},
      std::pair{std::string_view{"diese stellung"}, std::string_view{"current_position"}},
      std::pair{std::string_view{"dieser stellung"}, std::string_view{"current_position"}},
      std::pair{std::string_view{"aktuellen stellung"}, std::string_view{"current_position"}},
      std::pair{std::string_view{"auf dem brett"}, std::string_view{"current_position"}},
      std::pair{std::string_view{"this move"}, std::string_view{"current_move"}},
      std::pair{std::string_view{"dieser zug"}, std::string_view{"current_move"}},
      std::pair{std::string_view{"diesen zug"}, std::string_view{"current_move"}},
  };
  add_aliases(out, lower, KnowledgeQueryEntityKind::kPositionReference,
              position_refs, 1.0);

  if (contains_word(lower, "why") || contains_word(lower, "warum") ||
      contains(lower, "show me") || contains(lower, "zeig mir") ||
      contains(lower, "evidence") || contains(lower, "beleg")) {
    append_unique(out, {KnowledgeQueryEntityKind::kEvidenceRequest,
                        "explanation", "explanation", 0.90});
  }

  add_opening_name_hint(out, query_text, lower, plan);
  return out;
}

std::string_view to_string(KnowledgeQueryEntityKind kind) noexcept {
  switch (kind) {
    case KnowledgeQueryEntityKind::kUnknown: return "unknown";
    case KnowledgeQueryEntityKind::kEcoCode: return "eco_code";
    case KnowledgeQueryEntityKind::kOpeningName: return "opening_name";
    case KnowledgeQueryEntityKind::kTimeControl: return "time_control";
    case KnowledgeQueryEntityKind::kGamePhase: return "game_phase";
    case KnowledgeQueryEntityKind::kPlayerColor: return "player_color";
    case KnowledgeQueryEntityKind::kResult: return "result";
    case KnowledgeQueryEntityKind::kTermination: return "termination";
    case KnowledgeQueryEntityKind::kTemporalScope: return "temporal_scope";
    case KnowledgeQueryEntityKind::kStatisticMetric: return "statistic_metric";
    case KnowledgeQueryEntityKind::kPositionReference: return "position_reference";
    case KnowledgeQueryEntityKind::kEvidenceRequest: return "evidence_request";
  }
  return "unknown";
}

}  // namespace kchess::knowledge
