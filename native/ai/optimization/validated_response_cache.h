#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "../dto/structured_coach_response.h"
#include "../providers/llm_provider.h"

namespace kchess::ai {

// -----------------------------------------------------------------------------
// Section: Exact validated provider-response cache
// -----------------------------------------------------------------------------

struct ValidatedResponseCacheStats {
  std::uint64_t requests{0};
  std::uint64_t hits{0};
  std::uint64_t stores{0};
  std::uint64_t evictions{0};
  std::size_t entries{0};
  std::size_t capacity{0};
  std::uint64_t key_builds{0};
  std::uint64_t key_bytes{0};
};

struct PreparedValidatedResponseCacheKey {
  std::string value;
};

class ValidatedResponseCache {
 public:
  explicit ValidatedResponseCache(std::size_t capacity = 96);

  [[nodiscard]] PreparedValidatedResponseCacheKey prepare_key(
      std::string_view provider_cache_identity, const LLMProviderRequest& request,
      const std::vector<EvidenceItem>& validation_evidence) const;
  [[nodiscard]] std::optional<StructuredCoachContent> lookup(
      const PreparedValidatedResponseCacheKey& key) const;
  void store(const PreparedValidatedResponseCacheKey& key,
             const StructuredCoachContent& content) const;
  [[nodiscard]] ValidatedResponseCacheStats stats() const;

 private:
  struct Entry {
    StructuredCoachContent content;
  };

  [[nodiscard]] static std::string key_for(
      std::string_view provider_cache_identity, const LLMProviderRequest& request,
      const std::vector<EvidenceItem>& validation_evidence);

  std::size_t capacity_{96};
  mutable std::mutex mutex_;
  mutable std::unordered_map<std::string, Entry> entries_;
  mutable std::deque<std::string> order_;
  mutable std::uint64_t requests_{0};
  mutable std::uint64_t hits_{0};
  mutable std::uint64_t stores_{0};
  mutable std::uint64_t evictions_{0};
  mutable std::uint64_t key_builds_{0};
  mutable std::uint64_t key_bytes_{0};
};

}  // namespace kchess::ai
