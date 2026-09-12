#pragma once

#include <vector>

#include "concept_catalog.h"

namespace kchess::ai::concepts_internal {

[[nodiscard]] const std::vector<ChessConcept>& catalog_data();

}  // namespace kchess::ai::concepts_internal
