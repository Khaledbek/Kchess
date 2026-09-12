#include "concept_retriever.h"

#include <algorithm>
#include <cctype>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include <nlohmann/json.hpp>

#include "../models/small_models.h"

namespace kchess::ai {
namespace {

// -----------------------------------------------------------------------------
// Section: Lexical baseline
// -----------------------------------------------------------------------------

std::string normalized_phrase(std::string_view text) {
  std::string out;
  out.reserve(text.size());
  bool pending_space = false;
  for (const unsigned char ch : text) {
    if (ch >= 128 || std::isalnum(ch)) {
      if (pending_space && !out.empty()) out.push_back(' ');
      out.push_back(ch >= 128 ? static_cast<char>(ch)
                              : static_cast<char>(std::tolower(ch)));
      pending_space = false;
    } else {
      pending_space = !out.empty();
    }
  }
  return out;
}

std::unordered_set<std::string> tokens(std::string_view text) {
  std::unordered_set<std::string> out;
  const std::string normalized = normalized_phrase(text);
  std::size_t begin = 0;
  while (begin < normalized.size()) {
    const auto end = normalized.find(' ', begin);
    out.insert(normalized.substr(begin, end - begin));
    if (end == std::string::npos) break;
    begin = end + 1;
  }
  return out;
}

bool contains_phrase(std::string_view query, std::string_view term) {
  const std::string q = " " + normalized_phrase(query) + " ";
  const std::string t = " " + normalized_phrase(term) + " ";
  return t.size() > 2 && q.find(t) != std::string::npos;
}

double term_score(std::string_view query, std::string_view term) {
  const std::string q = normalized_phrase(query);
  const std::string t = normalized_phrase(term);
  if (q.empty() || t.empty()) return 0.0;
  if (q == t) return 1.0;
  if (contains_phrase(q, t)) return 0.96;

  const auto query_tokens = tokens(q);
  const auto term_tokens = tokens(t);
  if (term_tokens.empty()) return 0.0;
  std::size_t hits = 0;
  for (const auto& token : term_tokens) hits += query_tokens.contains(token) ? 1 : 0;
  if (hits == term_tokens.size()) return term_tokens.size() > 1 ? 0.90 : 0.84;
  if (hits >= 2 && hits * 3 >= term_tokens.size() * 2) return 0.72;
  return 0.0;
}

ConceptMatch lexical_match(const ChessConcept& concept_entry, std::string_view query) {
  ConceptMatch result{.entry = &concept_entry};
  const auto consider = [&](std::string_view term) {
    const double score = term_score(query, term);
    if (score > result.confidence) {
      result.matched_term = term;
      result.confidence = score;
    }
  };
  consider(concept_entry.id);
  for (const auto alias : concept_entry.aliases) consider(alias);
  return result;
}

// -----------------------------------------------------------------------------
// Section: Optional semantic augmentation
// -----------------------------------------------------------------------------

std::string concept_text(const ChessConcept& concept_entry) {
  std::string text(concept_entry.id);
  for (const auto alias : concept_entry.aliases) {
    text.push_back(' ');
    text.append(alias);
  }
  return text;
}

std::optional<std::vector<float>> cached_concept_embedding(
    const ChessConcept& concept_entry, const EmbeddingModel& model) {
  static std::mutex mutex;
  static std::unordered_map<std::string, std::vector<float>> cache;
  const std::string key = std::string(model.id()) + ":" +
                          std::string(model.version()) + ":" +
                          std::string(concept_entry.id);
  {
    std::lock_guard lock(mutex);
    if (const auto it = cache.find(key); it != cache.end()) return it->second;
  }
  auto vector = model.embed(concept_text(concept_entry));
  if (!vector.has_value() || vector->empty()) return std::nullopt;
  {
    std::lock_guard lock(mutex);
    cache.emplace(key, *vector);
  }
  return vector;
}

void add_semantic_scores(std::vector<ConceptMatch>& matches,
                         std::string_view query,
                         const EmbeddingModel* model) {
  if (model == nullptr || !model->available()) return;
  const double lexical_best = matches.empty() ? 0.0 : matches.front().confidence;
  if (lexical_best >= 0.90) return;  // Exact/strong lexical hits avoid model latency.

  const auto query_vector = model->embed(query);
  if (!query_vector.has_value() || query_vector->empty()) return;
  for (auto& match : matches) {
    const auto concept_vector = cached_concept_embedding(*match.entry, *model);
    if (!concept_vector.has_value()) continue;
    const double cosine = cosine_similarity(*query_vector, *concept_vector);
    const double semantic = std::clamp((cosine + 1.0) * 0.5, 0.0, 1.0);
    const double weighted = semantic * 0.92;
    if (semantic >= 0.60 && weighted > match.confidence) {
      match.confidence = weighted;
      match.matched_term = {};
      match.semantic = true;
    }
  }
}

}  // namespace

// -----------------------------------------------------------------------------
// Section: Public retrieval
// -----------------------------------------------------------------------------

std::vector<ConceptMatch> ConceptRetriever::retrieve(
    std::string_view query, std::size_t limit,
    const EmbeddingModel* embeddings) const {
  std::vector<ConceptMatch> matches;
  if (limit == 0) return matches;
  matches.reserve(ConceptCatalog::all().size());
  for (const auto& concept_entry : ConceptCatalog::all()) {
    matches.push_back(lexical_match(concept_entry, query));
  }
  std::sort(matches.begin(), matches.end(), [](const ConceptMatch& a,
                                               const ConceptMatch& b) {
    if (a.confidence != b.confidence) return a.confidence > b.confidence;
    return a.entry->id < b.entry->id;
  });
  add_semantic_scores(matches, query, embeddings);
  std::sort(matches.begin(), matches.end(), [](const ConceptMatch& a,
                                               const ConceptMatch& b) {
    if (a.confidence != b.confidence) return a.confidence > b.confidence;
    return a.entry->id < b.entry->id;
  });
  matches.erase(std::remove_if(matches.begin(), matches.end(),
                               [](const ConceptMatch& match) {
                                 return match.confidence <= 0.0;
                               }),
                matches.end());
  if (matches.size() > limit) matches.resize(limit);
  return matches;
}

EvidenceItem ConceptRetriever::evidence(std::string_view query,
                                        std::size_t limit,
                                        const EmbeddingModel* embeddings) const {
  const auto matches = retrieve(query, limit, embeddings);
  nlohmann::json items = nlohmann::json::array();
  for (const auto& match : matches) {
    items.push_back({
        {"concept_id", std::string(match.entry->id)},
        {"category", std::string(concept_category_name(match.entry->category))},
        {"parent_id", std::string(match.entry->parent_id)},
        {"matched_term", std::string(match.matched_term)},
        {"match_type", match.semantic ? "semantic" : "lexical"},
        {"confidence", match.confidence},
    });
  }
  return {
      .id = "concepts.lookup.v2",
      .kind = EvidenceKind::chess_concepts,
      .payload = nlohmann::json{{"version", 2}, {"matches", items}}.dump(),
      .confidence = matches.empty() ? 0.0 : matches.front().confidence,
  };
}

}  // namespace kchess::ai
