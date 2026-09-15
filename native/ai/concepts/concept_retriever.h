#pragma once

#include <cstddef>
#include <string_view>
#include <vector>

#include "../dto/evidence.h"
#include "concept_catalog.h"

namespace kchess::ai {

class EmbeddingModel;

// -----------------------------------------------------------------------------
// Section: Retrieval result
// -----------------------------------------------------------------------------

struct ConceptMatch {
  const ChessConcept* entry{nullptr};
  std::string_view matched_term;
  double confidence{0.0};
  bool semantic{false};
};

// -----------------------------------------------------------------------------
// Section: Hybrid concept retrieval
// -----------------------------------------------------------------------------

class ConceptRetriever {
 public:
  [[nodiscard]] std::vector<ConceptMatch> retrieve(
      std::string_view query, std::size_t limit = 5,
      const EmbeddingModel* embeddings = nullptr) const;
  [[nodiscard]] EvidenceItem evidence(
      std::string_view query, std::size_t limit = 5,
      const EmbeddingModel* embeddings = nullptr) const;
};

}  // namespace kchess::ai
