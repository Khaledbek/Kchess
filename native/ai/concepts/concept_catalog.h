#pragma once

#include <string_view>
#include <vector>

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Stable concept taxonomy
// -----------------------------------------------------------------------------

enum class ConceptCategory {
  tactics,
  strategy,
  pawn_structures,
  piece_play,
  king_safety,
  openings,
  endgames,
  calculation,
  defence,
  attack,
  training,
};

struct ChessConcept {
  std::string_view id;
  ConceptCategory category{ConceptCategory::strategy};
  std::string_view parent_id;
  std::vector<std::string_view> aliases;
};

// -----------------------------------------------------------------------------
// Section: Catalog access
// -----------------------------------------------------------------------------

class ConceptCatalog {
 public:
  [[nodiscard]] static const std::vector<ChessConcept>& all();
  [[nodiscard]] static const ChessConcept* find(std::string_view id_or_alias);
  [[nodiscard]] static std::vector<const ChessConcept*> by_category(
      ConceptCategory category);
};

[[nodiscard]] std::string_view concept_category_name(ConceptCategory category);

}  // namespace kchess::ai
