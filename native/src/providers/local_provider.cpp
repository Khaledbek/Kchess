#include "providers/game_provider.h"

namespace kchess {

ProviderResult<ProviderProfile> LocalPgnFenProvider::fetch_profile(
    const std::string& name,
    const CacheValidators&,
    const std::shared_ptr<CancelToken>&) {
  return {.value = ProviderProfile{
              .provider = ProviderType::local,
              .username = name,
              .display_name = name,
              .fallback_asset = "profile_unknown.png",
          }};
}

ProviderResult<ProviderStats> LocalPgnFenProvider::fetch_stats(
    const std::string&, const CacheValidators&, const std::shared_ptr<CancelToken>&) {
  return {.value = ProviderStats{.provider = ProviderType::local}};
}

ProviderResult<std::vector<ProviderGame>> LocalPgnFenProvider::fetch_games(
    const std::string&, const ProviderGameQuery&, const CacheValidators&,
    const std::shared_ptr<CancelToken>&) {
  return {.value = std::vector<ProviderGame>{}};
}

ProviderResult<std::vector<std::string>> LocalPgnFenProvider::fetch_available_months(
    const std::string&, const CacheValidators&, const std::shared_ptr<CancelToken>&) {
  return {.value = std::vector<std::string>{}};
}

}  // namespace kchess
