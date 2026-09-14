#include "gemini_config.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <mutex>
#include <vector>

#include <nlohmann/json.hpp>

namespace kchess::ai {
namespace {

namespace fs = std::filesystem;
using json = nlohmann::json;
using Clock = std::chrono::system_clock;

struct UsageRecord {
  std::int64_t timestamp{0};
  std::size_t estimated_input_tokens{0};
  std::string priority{"legacy"};
};

struct UsageState {
  std::vector<UsageRecord> requests;
  int consecutive_429{0};
  std::int64_t blocked_until{0};
};

fs::path project_root() {
#ifdef KCHESS_PROJECT_ROOT
  return fs::path(KCHESS_PROJECT_ROOT);
#else
  return fs::current_path();
#endif
}

std::string trimmed(std::string value) {
  const auto first = value.find_first_not_of(" \t\r\n\xEF\xBB\xBF");
  if (first == std::string::npos) return {};
  const auto last = value.find_last_not_of(" \t\r\n");
  return value.substr(first, last - first + 1);
}

std::optional<std::string> read_text(const fs::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) return std::nullopt;
  return std::string(std::istreambuf_iterator<char>(input),
                     std::istreambuf_iterator<char>());
}

UsageState read_usage(const fs::path& path) {
  UsageState state;
  const auto text = read_text(path);
  if (!text) return state;
  try {
    const auto value = json::parse(*text);
    if (value.contains("requests") && value["requests"].is_array()) {
      for (const auto& item : value["requests"]) {
        if (item.is_number_integer()) {
          state.requests.push_back({.timestamp = item.get<std::int64_t>(),
                                    .estimated_input_tokens = 0,
                                    .priority = "legacy"});
        } else if (item.is_object()) {
          state.requests.push_back(
              {.timestamp = item.value("timestamp", std::int64_t{0}),
               .estimated_input_tokens =
                   item.value("estimatedInputTokens", std::size_t{0}),
               .priority = item.value("priority", std::string{"legacy"})});
        }
      }
    }
    if (value.contains("rateLimit") && value["rateLimit"].is_object()) {
      const auto& rate = value["rateLimit"];
      state.consecutive_429 = rate.value("consecutive429", 0);
      state.blocked_until = rate.value("blockedUntil", std::int64_t{0});
    }
  } catch (...) {
    return {};
  }
  return state;
}

void write_usage(const fs::path& path, const UsageState& state) {
  std::error_code ec;
  fs::create_directories(path.parent_path(), ec);
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output) return;
  json requests = json::array();
  for (const auto& item : state.requests) {
    requests.push_back({{"timestamp", item.timestamp},
                        {"estimatedInputTokens", item.estimated_input_tokens},
                        {"priority", item.priority}});
  }
  output << json{{"requests", std::move(requests)},
                 {"rateLimit",
                  {{"consecutive429", state.consecutive_429},
                   {"blockedUntil", state.blocked_until}}}}
                .dump();
}

std::mutex& usage_mutex() {
  static std::mutex mutex;
  return mutex;
}

std::int64_t floor_div(std::int64_t value, std::int64_t divisor) {
  auto quotient = value / divisor;
  const auto remainder = value % divisor;
  if (remainder != 0 && ((remainder < 0) != (divisor < 0))) --quotient;
  return quotient;
}

std::int64_t days_from_civil(int year, unsigned month, unsigned day) {
  year -= month <= 2;
  const int era = (year >= 0 ? year : year - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(year - era * 400);
  const int adjusted_month = static_cast<int>(month) + (month > 2 ? -3 : 9);
  const unsigned doy =
      static_cast<unsigned>((153 * adjusted_month + 2) / 5) + day - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return static_cast<std::int64_t>(era) * 146097 +
         static_cast<std::int64_t>(doe) - 719468;
}

struct CivilDate {
  int year{1970};
  unsigned month{1};
  unsigned day{1};
};

CivilDate civil_from_days(std::int64_t z) {
  z += 719468;
  const std::int64_t era = (z >= 0 ? z : z - 146096) / 146097;
  const unsigned doe = static_cast<unsigned>(z - era * 146097);
  const unsigned yoe =
      (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  int year = static_cast<int>(yoe) + static_cast<int>(era) * 400;
  const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const unsigned mp = (5 * doy + 2) / 153;
  const unsigned day = doy - (153 * mp + 2) / 5 + 1;
  const int month_value = static_cast<int>(mp) + (mp < 10 ? 3 : -9);
  const unsigned month = static_cast<unsigned>(month_value);
  year += month <= 2;
  return {.year = year, .month = month, .day = day};
}

unsigned weekday_from_days(std::int64_t days) {
  const auto value = (days + 4) % 7;  // 1970-01-01 = Thursday, Sunday = 0.
  return static_cast<unsigned>(value < 0 ? value + 7 : value);
}

unsigned nth_sunday(int year, unsigned month, unsigned nth) {
  const auto first_days = days_from_civil(year, month, 1);
  const unsigned first_weekday = weekday_from_days(first_days);
  return 1 + ((7 - first_weekday) % 7) + 7 * (nth - 1);
}

std::int64_t utc_seconds_for_civil(int year, unsigned month, unsigned day,
                                   unsigned hour) {
  return days_from_civil(year, month, day) * 86400 +
         static_cast<std::int64_t>(hour) * 3600;
}

int pacific_utc_offset_seconds(std::int64_t utc_seconds) {
  const auto utc_date = civil_from_days(floor_div(utc_seconds, 86400));
  const auto dst_start = utc_seconds_for_civil(
      utc_date.year, 3, nth_sunday(utc_date.year, 3, 2), 10);
  const auto dst_end = utc_seconds_for_civil(
      utc_date.year, 11, nth_sunday(utc_date.year, 11, 1), 9);
  return utc_seconds >= dst_start && utc_seconds < dst_end ? -7 * 3600
                                                           : -8 * 3600;
}

std::int64_t pacific_day_start_utc(std::int64_t now) {
  const int now_offset = pacific_utc_offset_seconds(now);
  const auto local_day = floor_div(now + now_offset, 86400);
  auto midnight_utc = local_day * 86400 - now_offset;
  const int midnight_offset = pacific_utc_offset_seconds(midnight_utc);
  if (midnight_offset != now_offset) {
    midnight_utc = local_day * 86400 - midnight_offset;
  }
  return midnight_utc;
}

const char* priority_name(GeminiRequestPriority priority) {
  switch (priority) {
    case GeminiRequestPriority::automatic: return "automatic";
    case GeminiRequestPriority::repair: return "repair";
    case GeminiRequestPriority::manual: return "manual";
  }
  return "manual";
}

std::size_t bounded_size(const json& value, const char* key,
                         std::size_t fallback, std::size_t minimum,
                         std::size_t maximum) {
  const auto raw = value.value(key, fallback);
  return std::min(maximum, std::max(minimum, raw));
}

}  // namespace

std::optional<GeminiConfig> load_gemini_config(std::string* error_code) {
  const auto root = project_root();
  const auto config_text = read_text(root / "config" / "coach_provider.json");
  if (!config_text) {
    if (error_code) *error_code = "gemini_config_missing";
    return std::nullopt;
  }

  GeminiConfig config;
  try {
    const auto value = json::parse(*config_text);
    if (value.value("provider", std::string{}) != "gemini") {
      if (error_code) *error_code = "gemini_provider_disabled";
      return std::nullopt;
    }
    config.model = value.value("model", config.model);
    config.timeout_ms = value.value("timeoutMs", config.timeout_ms);
    config.max_output_tokens =
        value.value("maxOutputTokens", config.max_output_tokens);
    config.free_tier_only = value.value("freeTierOnly", true);

    config.rpm_hard_limit = bounded_size(
        value, "rpmHardLimit", config.rpm_hard_limit, 1, 14);
    config.rpm_soft_limit = bounded_size(
        value, "rpmSoftLimit", config.rpm_soft_limit, 1,
        config.rpm_hard_limit);
    config.tpm_hard_limit = bounded_size(
        value, "tpmHardLimit", config.tpm_hard_limit, 1, 240'000);
    config.tpm_soft_limit = bounded_size(
        value, "tpmSoftLimit", config.tpm_soft_limit, 1,
        config.tpm_hard_limit);
    config.rpd_hard_limit = bounded_size(
        value, "rpdHardLimit", config.rpd_hard_limit, 1, 495);
    config.rpd_soft_limit = bounded_size(
        value, "rpdSoftLimit", config.rpd_soft_limit, 1,
        config.rpd_hard_limit);
    config.backoff_base_seconds = std::min(
        30, std::max(1, value.value("backoffBaseSeconds",
                                    config.backoff_base_seconds)));
    config.backoff_max_seconds = std::min(
        300, std::max(config.backoff_base_seconds,
                      value.value("backoffMaxSeconds",
                                  config.backoff_max_seconds)));
    config.max_output_tokens =
        std::min(700, std::max(64, config.max_output_tokens));
    config.timeout_ms = std::min(60'000, std::max(5'000, config.timeout_ms));
  } catch (...) {
    if (error_code) *error_code = "gemini_config_invalid";
    return std::nullopt;
  }

  const auto key_text = read_text(root / "secrets" / "gemini_api_key.txt");
  config.api_key = key_text ? trimmed(*key_text) : std::string{};
  if (config.api_key.empty() ||
      config.api_key == "PASTE_YOUR_GEMINI_AUTH_KEY_HERE") {
    if (error_code) *error_code = "gemini_api_key_missing";
    return std::nullopt;
  }
  if (!config.free_tier_only || config.model != "gemini-3.5-flash-lite") {
    if (error_code) *error_code = "gemini_free_tier_guard_config";
    return std::nullopt;
  }
  return config;
}

bool reserve_gemini_request(const GeminiConfig& config,
                            GeminiRequestPriority priority,
                            std::size_t estimated_input_tokens,
                            std::string* error_code) {
  std::lock_guard lock(usage_mutex());
  const auto usage_path = project_root() / "secrets" / "gemini_usage.json";
  const auto now = std::chrono::duration_cast<std::chrono::seconds>(
                       Clock::now().time_since_epoch())
                       .count();
  auto state = read_usage(usage_path);

  const auto day_start = pacific_day_start_utc(now);
  state.requests.erase(
      std::remove_if(state.requests.begin(), state.requests.end(),
                     [day_start](const UsageRecord& item) {
                       return item.timestamp < day_start;
                     }),
      state.requests.end());

  if (state.blocked_until > now) {
    write_usage(usage_path, state);
    if (error_code) *error_code = "gemini_local_backoff_guard";
    return false;
  }

  const auto minute_start = now - 60;
  std::size_t rpm = 0;
  std::size_t tpm = 0;
  for (const auto& item : state.requests) {
    if (item.timestamp > minute_start) {
      ++rpm;
      tpm += item.estimated_input_tokens;
    }
  }
  const auto rpd = state.requests.size();
  const bool automatic = priority == GeminiRequestPriority::automatic;

  if (rpd >= config.rpd_hard_limit) {
    write_usage(usage_path, state);
    if (error_code) *error_code = "gemini_local_rpd_hard_guard";
    return false;
  }
  if (rpm >= config.rpm_hard_limit) {
    write_usage(usage_path, state);
    if (error_code) *error_code = "gemini_local_rpm_hard_guard";
    return false;
  }
  if (tpm + estimated_input_tokens > config.tpm_hard_limit) {
    write_usage(usage_path, state);
    if (error_code) *error_code = "gemini_local_tpm_hard_guard";
    return false;
  }

  if (automatic && rpd >= config.rpd_soft_limit) {
    write_usage(usage_path, state);
    if (error_code) *error_code = "gemini_automatic_rpd_soft_guard";
    return false;
  }
  if (automatic && rpm >= config.rpm_soft_limit) {
    write_usage(usage_path, state);
    if (error_code) *error_code = "gemini_automatic_rpm_soft_guard";
    return false;
  }
  if (automatic && tpm + estimated_input_tokens > config.tpm_soft_limit) {
    write_usage(usage_path, state);
    if (error_code) *error_code = "gemini_automatic_tpm_soft_guard";
    return false;
  }

  state.requests.push_back({.timestamp = now,
                            .estimated_input_tokens = estimated_input_tokens,
                            .priority = priority_name(priority)});
  write_usage(usage_path, state);
  return true;
}

void record_gemini_rate_limit(const GeminiConfig& config,
                              int retry_after_seconds) {
  std::lock_guard lock(usage_mutex());
  const auto usage_path = project_root() / "secrets" / "gemini_usage.json";
  const auto now = std::chrono::duration_cast<std::chrono::seconds>(
                       Clock::now().time_since_epoch())
                       .count();
  auto state = read_usage(usage_path);
  state.consecutive_429 = std::min(8, state.consecutive_429 + 1);
  int exponential = config.backoff_base_seconds;
  for (int i = 1; i < state.consecutive_429; ++i) {
    exponential = std::min(config.backoff_max_seconds, exponential * 2);
  }
  const int delay = std::min(
      config.backoff_max_seconds,
      std::max(exponential, std::max(0, retry_after_seconds)));
  state.blocked_until = std::max(state.blocked_until, now + delay);
  write_usage(usage_path, state);
}

void record_gemini_success() {
  std::lock_guard lock(usage_mutex());
  const auto usage_path = project_root() / "secrets" / "gemini_usage.json";
  auto state = read_usage(usage_path);
  if (state.consecutive_429 == 0 && state.blocked_until == 0) return;
  state.consecutive_429 = 0;
  state.blocked_until = 0;
  write_usage(usage_path, state);
}

}  // namespace kchess::ai
