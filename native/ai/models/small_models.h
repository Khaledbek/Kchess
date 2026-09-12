#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "../coach_types.h"
#include "../dto/evidence.h"

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Tiny model contracts
// -----------------------------------------------------------------------------

struct IntentModelPrediction {
  CoachIntent intent{CoachIntent::unknown};
  bool chess_domain{false};
  double confidence{0.0};
};

struct ContextPlannerInput {
  std::string_view user_text;
  CoachIntent intent{CoachIntent::unknown};
  CoachMode mode{CoachMode::answer};
  ResponseDepth requested_depth{ResponseDepth::standard};
  bool has_position{false};
  bool has_game{false};
  bool has_session{false};
};

struct ContextPlannerPrediction {
  std::vector<EvidenceKind> extra_evidence;
  std::size_t concept_limit{0};
  double confidence{0.0};
};

class TinyIntentModel {
 public:
  virtual ~TinyIntentModel() = default;
  [[nodiscard]] virtual std::string_view id() const noexcept = 0;
  [[nodiscard]] virtual std::string_view version() const noexcept = 0;
  [[nodiscard]] virtual bool available() const noexcept = 0;
  [[nodiscard]] virtual std::optional<IntentModelPrediction> predict(
      std::string_view text, bool has_position) const = 0;
};

class TinyContextPlannerModel {
 public:
  virtual ~TinyContextPlannerModel() = default;
  [[nodiscard]] virtual std::string_view id() const noexcept = 0;
  [[nodiscard]] virtual std::string_view version() const noexcept = 0;
  [[nodiscard]] virtual bool available() const noexcept = 0;
  [[nodiscard]] virtual std::optional<ContextPlannerPrediction> predict(
      const ContextPlannerInput& input) const = 0;
};

class EmbeddingModel {
 public:
  virtual ~EmbeddingModel() = default;
  [[nodiscard]] virtual std::string_view id() const noexcept = 0;
  [[nodiscard]] virtual std::string_view version() const noexcept = 0;
  [[nodiscard]] virtual bool available() const noexcept = 0;
  [[nodiscard]] virtual std::size_t dimensions() const noexcept = 0;
  [[nodiscard]] virtual std::optional<std::vector<float>> embed(
      std::string_view text) const = 0;
};

struct SmallModelSuite {
  std::shared_ptr<const TinyIntentModel> intent;
  std::shared_ptr<const TinyContextPlannerModel> context_planner;
  std::shared_ptr<const EmbeddingModel> embeddings;
};

[[nodiscard]] double cosine_similarity(const std::vector<float>& a,
                                       const std::vector<float>& b) noexcept;

}  // namespace kchess::ai
