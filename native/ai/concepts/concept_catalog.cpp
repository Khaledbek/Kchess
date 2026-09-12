#include "concept_catalog.h"

#include <algorithm>
#include <cctype>
#include <string>

#include "concept_catalog_data.h"

namespace kchess::ai {
namespace {

// -----------------------------------------------------------------------------
// Section: Canonical lookup helpers
// -----------------------------------------------------------------------------

std::string normalized(std::string_view value) {
  std::string out;
  out.reserve(value.size());
  for (const unsigned char ch : value) {
    if (std::isalnum(ch)) {
      out.push_back(static_cast<char>(std::tolower(ch)));
    }
  }
  return out;
}

bool matches(const ChessConcept& concept_entry, std::string_view query) {
  const auto key = normalized(query);
  if (key.empty()) return false;
  if (normalized(concept_entry.id) == key) return true;
  return std::any_of(concept_entry.aliases.begin(), concept_entry.aliases.end(),
                     [&](std::string_view alias) {
                       return normalized(alias) == key;
                     });
}

}  // namespace

// -----------------------------------------------------------------------------
// Section: Public catalog API
// -----------------------------------------------------------------------------

const std::vector<ChessConcept>& ConceptCatalog::all() {
  return concepts_internal::catalog_data();
}

const ChessConcept* ConceptCatalog::find(std::string_view id_or_alias) {
  const auto& concepts = all();
  const auto it = std::find_if(concepts.begin(), concepts.end(),
                               [&](const ChessConcept& concept_entry) {
                                 return matches(concept_entry, id_or_alias);
                               });
  return it == concepts.end() ? nullptr : &*it;
}

std::vector<const ChessConcept*> ConceptCatalog::by_category(
    ConceptCategory category) {
  std::vector<const ChessConcept*> result;
  for (const auto& concept_entry : all()) {
    if (concept_entry.category == category) result.push_back(&concept_entry);
  }
  return result;
}

std::string_view concept_category_name(ConceptCategory category) {
  switch (category) {
    case ConceptCategory::tactics: return "tactics";
    case ConceptCategory::strategy: return "strategy";
    case ConceptCategory::pawn_structures: return "pawn_structures";
    case ConceptCategory::piece_play: return "piece_play";
    case ConceptCategory::king_safety: return "king_safety";
    case ConceptCategory::openings: return "openings";
    case ConceptCategory::endgames: return "endgames";
    case ConceptCategory::calculation: return "calculation";
    case ConceptCategory::defence: return "defence";
    case ConceptCategory::attack: return "attack";
    case ConceptCategory::training: return "training";
  }
  return "unknown";
}

}  // namespace kchess::ai
