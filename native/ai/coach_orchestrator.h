#pragma once

#include <memory>

#include "conversation/coach_session.h"
#include "evidence_retriever.h"
#include "models/small_models.h"
#include "dto/coach_request.h"
#include "dto/coach_response.h"
#include "providers/llm_provider.h"

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Coach pipeline entry point
// -----------------------------------------------------------------------------

class CoachOrchestrator {
 public:
  explicit CoachOrchestrator(
      EvidenceSources evidence_sources = {},
      std::shared_ptr<const LLMProvider> provider = {},
      SmallModelSuite small_models = {});

  [[nodiscard]] CoachResponse handle(const CoachRequest& request) const;

 private:
  EvidenceRetriever evidence_retriever_;
  std::shared_ptr<const LLMProvider> provider_;
  SmallModelSuite small_models_;
  mutable CoachSessionMemory sessions_;
};

}  // namespace kchess::ai
