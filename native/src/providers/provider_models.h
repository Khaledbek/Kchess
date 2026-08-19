#pragma once

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "http/http_client.h"

namespace kchess {

enum class ProviderType : std::int32_t { chess_com = 0, lichess = 1, local = 2 };
enum class TimeControlType { daily, rapid, blitz, bullet, classical, correspondence, unknown };
enum class ProviderOutcome { win, loss, draw, unknown };

struct CacheValidators {
  std::string etag;
  std::string last_modified;
};

struct ResponseCacheInfo {
  std::string etag;
  std::string last_modified;
  std::string cache_control;
  std::int64_t fetched_at{0};
  std::int64_t expires_at{0};
};

struct ProviderPerformance {
  std::string key;
  std::optional<int> current_rating;
  std::optional<int> best_rating;
  std::optional<int> lowest_rating;
  std::optional<int> rating_progress;
  std::optional<int> games;
  std::optional<int> wins;
  std::optional<int> losses;
  std::optional<int> draws;
};

struct ProviderStats {
  ProviderType provider{ProviderType::local};
  std::vector<ProviderPerformance> performances;
};

struct ProviderProfile {
  ProviderType provider{ProviderType::local};
  std::string username;
  std::string display_name;
  std::optional<std::string> title;
  std::optional<std::string> avatar_url;
  std::string fallback_asset;
  std::optional<std::string> flair;
  std::optional<std::int64_t> joined;
  std::optional<std::int64_t> last_online;
  std::optional<std::string> country;
  std::optional<std::string> location;
  std::optional<std::string> public_url;
  std::optional<std::string> provider_specific_id;
  std::optional<int> followers;
  std::optional<int> fide;
  std::optional<int> games;
  std::optional<int> wins;
  std::optional<int> losses;
  std::optional<int> draws;
  std::optional<std::int64_t> play_time_seconds;
  std::optional<std::string> status;
  bool disabled{false};
  bool tos_violation{false};
  std::vector<ProviderPerformance> rating_summary;
};

struct ProviderGame {
  ProviderType provider{ProviderType::local};
  std::string provider_game_id;
  std::string url;
  std::string pgn;
  std::string white_username;
  std::string black_username;
  std::optional<int> white_rating;
  std::optional<int> black_rating;
  std::string result;
  ProviderOutcome profile_outcome{ProviderOutcome::unknown};
  std::int64_t ended_at{0};
  std::string time_control;
  TimeControlType time_control_type{TimeControlType::unknown};
  std::string rules;
  std::optional<double> provider_accuracy_white;
  std::optional<double> provider_accuracy_black;
  std::optional<std::string> eco;
  std::optional<std::string> tournament;
  std::optional<std::string> match;
  bool completed{true};
};

template <typename T>
struct ProviderResult {
  std::optional<T> value;
  ResponseCacheInfo cache;
  bool not_modified{false};
};

class ProviderException final : public std::runtime_error {
 public:
  ProviderException(HttpError kind, std::string message)
      : std::runtime_error(std::move(message)), kind_(kind) {}
  HttpError kind() const noexcept { return kind_; }

 private:
  HttpError kind_;
};

std::string provider_type_name(ProviderType type);
std::string time_control_name(TimeControlType type);
std::string provider_outcome_name(ProviderOutcome outcome);

}  // namespace kchess
