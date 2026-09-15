#include "portable_small_models.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

namespace kchess::ai {
namespace {

using json = nlohmann::json;
using FeatureWeights = std::unordered_map<std::string, double>;
using FeatureVector = std::unordered_map<std::string, double>;

constexpr std::string_view kIntentSchema = "kchess.tiny_intent.linear.v1";
constexpr std::string_view kContextSchema = "kchess.tiny_context.linear.v1";
constexpr std::string_view kEmbeddingSchema = "kchess.embedding.feature_projection.v1";

// -----------------------------------------------------------------------------
// Section: Portable text features
// -----------------------------------------------------------------------------

void flush_token(std::string& token, std::vector<std::string>& tokens) {
  if (token.empty()) return;
  tokens.push_back(std::move(token));
  token.clear();
}

std::vector<std::string> tokenize(std::string_view text) {
  std::vector<std::string> tokens;
  std::string token;
  token.reserve(24);
  for (const unsigned char byte : text) {
    if (byte >= 128) {
      token.push_back(static_cast<char>(byte));
      continue;
    }
    if ((byte >= 'a' && byte <= 'z') || (byte >= '0' && byte <= '9') ||
        byte == '_' || byte == '-') {
      token.push_back(static_cast<char>(byte));
      continue;
    }
    if (byte >= 'A' && byte <= 'Z') {
      token.push_back(static_cast<char>(byte - 'A' + 'a'));
      continue;
    }
    flush_token(token, tokens);
  }
  flush_token(token, tokens);
  return tokens;
}

FeatureVector text_features(std::string_view text) {
  const auto tokens = tokenize(text);
  FeatureVector features;
  features.reserve(tokens.size() * 2 + 4);
  for (std::size_t i = 0; i < tokens.size(); ++i) {
    features[tokens[i]] += 1.0;
    if (i > 0) {
      features[tokens[i - 1] + "::" + tokens[i]] += 1.0;
    }
  }
  return features;
}

double linear_score(const double bias, const FeatureWeights& weights,
                    const FeatureVector& features) {
  double score = bias;
  for (const auto& [feature, value] : features) {
    const auto found = weights.find(feature);
    if (found != weights.end()) score += found->second * value;
  }
  return score;
}

FeatureWeights read_weights(const json& value) {
  FeatureWeights result;
  if (!value.is_object()) return result;
  for (auto it = value.begin(); it != value.end(); ++it) {
    if (!it.value().is_number()) continue;
    result.emplace(it.key(), it.value().get<double>());
  }
  return result;
}

std::optional<json> read_json_file(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) return std::nullopt;
  try {
    json value;
    stream >> value;
    if (!value.is_object()) return std::nullopt;
    return value;
  } catch (...) {
    return std::nullopt;
  }
}

// -----------------------------------------------------------------------------
// Section: Stable enum mapping
// -----------------------------------------------------------------------------

std::optional<CoachIntent> intent_from_name(const std::string_view value) {
  if (value == "position") return CoachIntent::position;
  if (value == "move_explanation") return CoachIntent::move_explanation;
  if (value == "plan") return CoachIntent::plan;
  if (value == "tactic") return CoachIntent::tactic;
  if (value == "opening") return CoachIntent::opening;
  if (value == "endgame") return CoachIntent::endgame;
  if (value == "chess_concept") return CoachIntent::chess_concept;
  if (value == "chess_rules") return CoachIntent::chess_rules;
  if (value == "chess_history") return CoachIntent::chess_history;
  if (value == "player_development") return CoachIntent::player_development;
  if (value == "game_review") return CoachIntent::game_review;
  if (value == "training") return CoachIntent::training;
  if (value == "off_topic") return CoachIntent::off_topic;
  return std::nullopt;
}

std::string_view intent_name(const CoachIntent value) {
  switch (value) {
    case CoachIntent::position: return "position";
    case CoachIntent::move_explanation: return "move_explanation";
    case CoachIntent::plan: return "plan";
    case CoachIntent::tactic: return "tactic";
    case CoachIntent::opening: return "opening";
    case CoachIntent::endgame: return "endgame";
    case CoachIntent::chess_concept: return "chess_concept";
    case CoachIntent::chess_rules: return "chess_rules";
    case CoachIntent::chess_history: return "chess_history";
    case CoachIntent::player_development: return "player_development";
    case CoachIntent::game_review: return "game_review";
    case CoachIntent::training: return "training";
    case CoachIntent::follow_up: return "follow_up";
    case CoachIntent::off_topic: return "off_topic";
    case CoachIntent::unknown:
    default: return "unknown";
  }
}

std::string_view mode_name(const CoachMode value) {
  switch (value) {
    case CoachMode::explain: return "explain";
    case CoachMode::hint: return "hint";
    case CoachMode::answer: return "answer";
    case CoachMode::compare: return "compare";
    case CoachMode::quiz: return "quiz";
    case CoachMode::plan: return "plan";
    case CoachMode::review: return "review";
    case CoachMode::teach: return "teach";
    default: return "answer";
  }
}

std::string_view depth_name(const ResponseDepth value) {
  switch (value) {
    case ResponseDepth::concise: return "concise";
    case ResponseDepth::detailed: return "detailed";
    case ResponseDepth::standard:
    default: return "standard";
  }
}

std::optional<EvidenceKind> evidence_kind_from_name(const std::string_view value) {
  if (value == "chess_concepts") return EvidenceKind::chess_concepts;
  if (value == "theory") return EvidenceKind::theory;
  if (value == "opening") return EvidenceKind::opening;
  if (value == "user_profile") return EvidenceKind::user_profile;
  if (value == "position_features") return EvidenceKind::position_features;
  if (value == "position_weaknesses") return EvidenceKind::position_weaknesses;
  if (value == "weakness_exploitation") return EvidenceKind::weakness_exploitation;
  if (value == "tactical_motifs") return EvidenceKind::tactical_motifs;
  if (value == "strategic_plans") return EvidenceKind::strategic_plans;
  return std::nullopt;
}

// -----------------------------------------------------------------------------
// Section: Tiny intent linear model
// -----------------------------------------------------------------------------

struct IntentLabel {
  CoachIntent intent{CoachIntent::unknown};
  double bias{0.0};
  FeatureWeights weights;
};

class PortableIntentModel final : public TinyIntentModel {
 public:
  PortableIntentModel(std::string id, std::string version,
                      std::vector<IntentLabel> labels)
      : id_(std::move(id)), version_(std::move(version)), labels_(std::move(labels)) {}

  [[nodiscard]] std::string_view id() const noexcept override { return id_; }
  [[nodiscard]] std::string_view version() const noexcept override { return version_; }
  [[nodiscard]] bool available() const noexcept override { return labels_.size() >= 2; }

  [[nodiscard]] std::optional<IntentModelPrediction> predict(
      const std::string_view text, const bool has_position) const override {
    if (!available() || text.empty()) return std::nullopt;
    auto features = text_features(text);
    if (has_position) features["__has_position__"] = 1.0;

    std::vector<double> scores;
    scores.reserve(labels_.size());
    double maximum = -std::numeric_limits<double>::infinity();
    for (const auto& label : labels_) {
      const double score = linear_score(label.bias, label.weights, features);
      scores.push_back(score);
      maximum = std::max(maximum, score);
    }

    double denominator = 0.0;
    std::size_t best_index = 0;
    double best_value = -1.0;
    for (std::size_t i = 0; i < scores.size(); ++i) {
      const double value = std::exp(std::clamp(scores[i] - maximum, -40.0, 40.0));
      denominator += value;
      if (value > best_value) {
        best_value = value;
        best_index = i;
      }
    }
    if (!(denominator > 0.0)) return std::nullopt;
    const double confidence = best_value / denominator;
    const auto intent = labels_[best_index].intent;
    return IntentModelPrediction{
        .intent = intent,
        .chess_domain = intent != CoachIntent::off_topic,
        .confidence = confidence,
    };
  }

 private:
  std::string id_;
  std::string version_;
  std::vector<IntentLabel> labels_;
};

std::shared_ptr<const TinyIntentModel> load_intent_model(
    const std::filesystem::path& path, SmallModelComponentStatus& status) {
  status.file_present = std::filesystem::exists(path);
  if (!status.file_present) return {};
  const auto root = read_json_file(path);
  if (!root || root->value("schema", "") != kIntentSchema ||
      !root->contains("labels") || !(*root)["labels"].is_array()) {
    status.error_code = "invalid_schema";
    return {};
  }

  const auto id = root->value("id", "");
  const auto version = root->value("version", "");
  if (id.empty() || version.empty()) {
    status.error_code = "identity_missing";
    return {};
  }

  std::vector<IntentLabel> labels;
  for (const auto& item : (*root)["labels"]) {
    if (!item.is_object()) continue;
    const auto intent = intent_from_name(item.value("intent", ""));
    if (!intent.has_value()) continue;
    labels.push_back(IntentLabel{
        .intent = *intent,
        .bias = item.value("bias", 0.0),
        .weights = read_weights(item.value("weights", json::object())),
    });
  }
  if (labels.size() < 2) {
    status.error_code = "labels_insufficient";
    return {};
  }

  auto model = std::make_shared<PortableIntentModel>(id, version, std::move(labels));
  status.available = model->available();
  status.id = id;
  status.version = version;
  return model;
}

// -----------------------------------------------------------------------------
// Section: Tiny context-planner linear model
// -----------------------------------------------------------------------------

struct ContextAction {
  enum class Kind { concept_limit, evidence };
  Kind kind{Kind::evidence};
  std::size_t concept_limit{0};
  EvidenceKind evidence{EvidenceKind::chess_concepts};
  double bias{0.0};
  double threshold{0.85};
  FeatureWeights weights;
};

double sigmoid(const double value) {
  if (value >= 0.0) {
    const double z = std::exp(-std::min(value, 40.0));
    return 1.0 / (1.0 + z);
  }
  const double z = std::exp(std::max(value, -40.0));
  return z / (1.0 + z);
}

class PortableContextPlannerModel final : public TinyContextPlannerModel {
 public:
  PortableContextPlannerModel(std::string id, std::string version,
                              std::vector<ContextAction> actions)
      : id_(std::move(id)), version_(std::move(version)), actions_(std::move(actions)) {}

  [[nodiscard]] std::string_view id() const noexcept override { return id_; }
  [[nodiscard]] std::string_view version() const noexcept override { return version_; }
  [[nodiscard]] bool available() const noexcept override { return !actions_.empty(); }

  [[nodiscard]] std::optional<ContextPlannerPrediction> predict(
      const ContextPlannerInput& input) const override {
    if (!available()) return std::nullopt;
    auto features = text_features(input.user_text);
    features["__intent_" + std::string(intent_name(input.intent)) + "__"] = 1.0;
    features["__mode_" + std::string(mode_name(input.mode)) + "__"] = 1.0;
    features["__depth_" + std::string(depth_name(input.requested_depth)) + "__"] = 1.0;
    if (input.has_position) features["__has_position__"] = 1.0;
    if (input.has_game) features["__has_game__"] = 1.0;
    if (input.has_session) features["__has_session__"] = 1.0;

    ContextPlannerPrediction prediction;
    double minimum_confidence = 1.0;
    double best_concept_confidence = -1.0;
    for (const auto& action : actions_) {
      const double confidence = sigmoid(
          linear_score(action.bias, action.weights, features));
      if (confidence < action.threshold) continue;
      if (action.kind == ContextAction::Kind::concept_limit) {
        if (confidence > best_concept_confidence) {
          prediction.concept_limit = action.concept_limit;
          best_concept_confidence = confidence;
        }
      } else if (std::find(prediction.extra_evidence.begin(),
                           prediction.extra_evidence.end(), action.evidence) ==
                 prediction.extra_evidence.end()) {
        prediction.extra_evidence.push_back(action.evidence);
      }
      minimum_confidence = std::min(minimum_confidence, confidence);
    }

    if (prediction.concept_limit == 0 && prediction.extra_evidence.empty()) {
      return std::nullopt;
    }
    prediction.confidence = minimum_confidence;
    return prediction;
  }

 private:
  std::string id_;
  std::string version_;
  std::vector<ContextAction> actions_;
};

std::shared_ptr<const TinyContextPlannerModel> load_context_model(
    const std::filesystem::path& path, SmallModelComponentStatus& status) {
  status.file_present = std::filesystem::exists(path);
  if (!status.file_present) return {};
  const auto root = read_json_file(path);
  if (!root || root->value("schema", "") != kContextSchema ||
      !root->contains("actions") || !(*root)["actions"].is_array()) {
    status.error_code = "invalid_schema";
    return {};
  }

  const auto id = root->value("id", "");
  const auto version = root->value("version", "");
  if (id.empty() || version.empty()) {
    status.error_code = "identity_missing";
    return {};
  }
  const double default_threshold = std::clamp(root->value("threshold", 0.85), 0.5, 0.99);

  std::vector<ContextAction> actions;
  for (const auto& item : (*root)["actions"]) {
    if (!item.is_object()) continue;
    ContextAction action;
    const auto kind = item.value("action", "");
    if (kind == "concept_limit") {
      const auto limit = item.value("value", 0);
      if (limit < 1 || limit > 5) continue;
      action.kind = ContextAction::Kind::concept_limit;
      action.concept_limit = static_cast<std::size_t>(limit);
    } else if (kind == "evidence") {
      const auto evidence = evidence_kind_from_name(item.value("value", ""));
      if (!evidence.has_value()) continue;
      action.kind = ContextAction::Kind::evidence;
      action.evidence = *evidence;
    } else {
      continue;
    }
    action.bias = item.value("bias", 0.0);
    action.threshold = std::clamp(item.value("threshold", default_threshold), 0.5, 0.99);
    action.weights = read_weights(item.value("weights", json::object()));
    actions.push_back(std::move(action));
  }
  if (actions.empty()) {
    status.error_code = "actions_missing";
    return {};
  }

  auto model = std::make_shared<PortableContextPlannerModel>(
      id, version, std::move(actions));
  status.available = model->available();
  status.id = id;
  status.version = version;
  return model;
}

// -----------------------------------------------------------------------------
// Section: Portable embedding projection model
// -----------------------------------------------------------------------------

using EmbeddingVector = std::vector<float>;

bool finite_vector(const EmbeddingVector& values) {
  return std::all_of(values.begin(), values.end(), [](const float value) {
    return std::isfinite(value);
  });
}

class PortableEmbeddingModel final : public EmbeddingModel {
 public:
  PortableEmbeddingModel(std::string id, std::string version,
                         std::size_t dimensions,
                         std::unordered_map<std::string, EmbeddingVector> projections)
      : id_(std::move(id)),
        version_(std::move(version)),
        dimensions_(dimensions),
        projections_(std::move(projections)) {}

  [[nodiscard]] std::string_view id() const noexcept override { return id_; }
  [[nodiscard]] std::string_view version() const noexcept override { return version_; }
  [[nodiscard]] bool available() const noexcept override {
    return dimensions_ > 0 && !projections_.empty();
  }
  [[nodiscard]] std::size_t dimensions() const noexcept override { return dimensions_; }

  [[nodiscard]] std::optional<std::vector<float>> embed(
      const std::string_view text) const override {
    if (!available() || text.empty()) return std::nullopt;
    const auto features = text_features(text);
    EmbeddingVector result(dimensions_, 0.0F);
    double matched_weight = 0.0;
    for (const auto& [feature, value] : features) {
      const auto found = projections_.find(feature);
      if (found == projections_.end()) continue;
      matched_weight += value;
      for (std::size_t i = 0; i < dimensions_; ++i) {
        result[i] += static_cast<float>(value) * found->second[i];
      }
    }
    if (!(matched_weight > 0.0)) return std::nullopt;

    double norm_squared = 0.0;
    for (const float value : result) {
      norm_squared += static_cast<double>(value) * static_cast<double>(value);
    }
    if (!(norm_squared > 1.0e-12)) return std::nullopt;
    const float inverse_norm = static_cast<float>(1.0 / std::sqrt(norm_squared));
    for (float& value : result) value *= inverse_norm;
    return result;
  }

 private:
  std::string id_;
  std::string version_;
  std::size_t dimensions_{0};
  std::unordered_map<std::string, EmbeddingVector> projections_;
};

std::shared_ptr<const EmbeddingModel> load_embedding_model(
    const std::filesystem::path& path, SmallModelComponentStatus& status) {
  status.file_present = std::filesystem::exists(path);
  if (!status.file_present) return {};
  const auto root = read_json_file(path);
  if (!root || root->value("schema", "") != kEmbeddingSchema ||
      !root->contains("projections") || !(*root)["projections"].is_object()) {
    status.error_code = "invalid_schema";
    return {};
  }

  const auto id = root->value("id", "");
  const auto version = root->value("version", "");
  const auto raw_dimensions = root->value("dimensions", 0);
  if (id.empty() || version.empty()) {
    status.error_code = "identity_missing";
    return {};
  }
  if (raw_dimensions < 8 || raw_dimensions > 512) {
    status.error_code = "dimensions_invalid";
    return {};
  }
  const auto dimensions = static_cast<std::size_t>(raw_dimensions);

  std::unordered_map<std::string, EmbeddingVector> projections;
  projections.reserve((*root)["projections"].size());
  for (auto it = (*root)["projections"].begin();
       it != (*root)["projections"].end(); ++it) {
    if (!it.value().is_array() || it.value().size() != dimensions) continue;
    EmbeddingVector values;
    values.reserve(dimensions);
    bool valid = true;
    for (const auto& item : it.value()) {
      if (!item.is_number()) {
        valid = false;
        break;
      }
      values.push_back(item.get<float>());
    }
    if (!valid || !finite_vector(values)) continue;
    projections.emplace(it.key(), std::move(values));
  }
  if (projections.empty()) {
    status.error_code = "projections_missing";
    return {};
  }

  auto model = std::make_shared<PortableEmbeddingModel>(
      id, version, dimensions, std::move(projections));
  status.available = model->available();
  status.id = id;
  status.version = version;
  return model;
}

// -----------------------------------------------------------------------------
// Section: Runtime discovery
// -----------------------------------------------------------------------------

std::optional<std::string> environment_value(const char* name) {
#ifdef _WIN32
  char* value = nullptr;
  std::size_t value_size = 0;
  if (_dupenv_s(&value, &value_size, name) != 0 || value == nullptr) {
    return std::nullopt;
  }
  std::string copy(value);
  std::free(value);
  if (copy.empty()) return std::nullopt;
  return copy;
#else
  if (const char* value = std::getenv(name);
      value != nullptr && *value != '\0') {
    return std::string(value);
  }
  return std::nullopt;
#endif
}

std::pair<std::filesystem::path, std::string> model_root(
    std::filesystem::path default_root) {
  if (auto override_root = environment_value("KCHESS_SMALL_MODEL_DIR")) {
    return {std::filesystem::path(*override_root), "environment"};
  }
  if (!default_root.empty()) return {std::move(default_root), "app_data"};
  return {{}, "none"};
}

}  // namespace

LoadedSmallModelSuite load_optional_small_model_suite(
    std::filesystem::path default_root) noexcept {
  LoadedSmallModelSuite loaded;
  try {
    auto [root, source] = model_root(std::move(default_root));
    loaded.status.source = std::move(source);
    if (root.empty()) return loaded;

    loaded.suite.intent = load_intent_model(
        root / "intent_linear.json", loaded.status.intent);
    loaded.suite.context_planner = load_context_model(
        root / "context_planner_linear.json", loaded.status.context_planner);
    loaded.suite.embeddings = load_embedding_model(
        root / "embedding_projection.json", loaded.status.embeddings);
  } catch (...) {
    // Optional model loading must never make the Coach unavailable.
    if (loaded.status.intent.file_present && !loaded.status.intent.available &&
        loaded.status.intent.error_code.empty()) {
      loaded.status.intent.error_code = "load_failed";
    }
    if (loaded.status.context_planner.file_present &&
        !loaded.status.context_planner.available &&
        loaded.status.context_planner.error_code.empty()) {
      loaded.status.context_planner.error_code = "load_failed";
    }
    if (loaded.status.embeddings.file_present &&
        !loaded.status.embeddings.available &&
        loaded.status.embeddings.error_code.empty()) {
      loaded.status.embeddings.error_code = "load_failed";
    }
  }
  return loaded;
}

}  // namespace kchess::ai
