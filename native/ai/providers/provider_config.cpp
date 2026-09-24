#include "provider_config.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <mutex>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

namespace kchess::ai {
namespace {

namespace fs = std::filesystem;
using json = nlohmann::json;
using Clock = std::chrono::system_clock;

// -----------------------------------------------------------------------------
// Section: Built-in provider presets
// -----------------------------------------------------------------------------

struct ProviderPreset {
  std::string_view id;
  ProviderApi api;
  std::string_view endpoint;
  std::string_view model;
  std::string_view max_tokens_field;
  // Non-empty only for Gemini: the single model the free-tier guard accepts.
  std::string_view free_tier_model;
  int max_output_tokens;
  int max_output_tokens_ceiling;
  int timeout_ms_ceiling;
  std::size_t rpm_ceiling;
  std::size_t tpm_ceiling;
  std::size_t rpd_ceiling;
};

// Gemini ceilings mirror the Google AI Studio free tier. The other vendors have
// no free tier; their limits are local spend guards with wider ceilings.
constexpr ProviderPreset kProviderPresets[] = {
    {.id = "gemini",
     .api = ProviderApi::gemini_interactions,
     .endpoint = "https://generativelanguage.googleapis.com/v1beta/interactions",
     .model = "gemini-3.5-flash-lite",
     .max_tokens_field = "",
     .free_tier_model = "gemini-3.5-flash-lite",
     .max_output_tokens = 700,
     .max_output_tokens_ceiling = 700,
     .timeout_ms_ceiling = 60'000,
     .rpm_ceiling = 14,
     .tpm_ceiling = 240'000,
     .rpd_ceiling = 495},
    {.id = "claude",
     .api = ProviderApi::anthropic_messages,
     .endpoint = "https://api.anthropic.com/v1/messages",
     .model = "claude-opus-5",
     .max_tokens_field = "",
     .free_tier_model = "",
     .max_output_tokens = 4'096,
     .max_output_tokens_ceiling = 16'000,
     .timeout_ms_ceiling = 120'000,
     .rpm_ceiling = 120,
     .tpm_ceiling = 2'000'000,
     .rpd_ceiling = 10'000},
    {.id = "deepseek",
     .api = ProviderApi::openai_chat_completions,
     .endpoint = "https://api.deepseek.com/chat/completions",
     .model = "deepseek-chat",
     .max_tokens_field = "max_tokens",
     .free_tier_model = "",
     .max_output_tokens = 1'500,
     .max_output_tokens_ceiling = 8'000,
     .timeout_ms_ceiling = 120'000,
     .rpm_ceiling = 120,
     .tpm_ceiling = 2'000'000,
     .rpd_ceiling = 10'000},
    {.id = "openai",
     .api = ProviderApi::openai_chat_completions,
     .endpoint = "https://api.openai.com/v1/chat/completions",
     .model = "gpt-5-mini",
     .max_tokens_field = "max_completion_tokens",
     .free_tier_model = "",
     .max_output_tokens = 4'096,
     .max_output_tokens_ceiling = 16'000,
     .timeout_ms_ceiling = 120'000,
     .rpm_ceiling = 120,
     .tpm_ceiling = 2'000'000,
     .rpd_ceiling = 10'000},
};

// Limits for a provider id without a preset. Its settings must name the wire
// format ("api"), an https endpoint and a model.
constexpr ProviderPreset kCustomProviderLimits{
    .id = "",
    .api = ProviderApi::openai_chat_completions,
    .endpoint = "",
    .model = "",
    .max_tokens_field = "max_tokens",
    .free_tier_model = "",
    .max_output_tokens = 1'500,
    .max_output_tokens_ceiling = 16'000,
    .timeout_ms_ceiling = 120'000,
    .rpm_ceiling = 120,
    .tpm_ceiling = 2'000'000,
    .rpd_ceiling = 10'000};

const ProviderPreset* find_preset(std::string_view id) {
  for (const auto& preset : kProviderPresets) {
    if (preset.id == id) return &preset;
  }
  return nullptr;
}

std::optional<ProviderApi> parse_api(std::string_view name) {
  if (name == "openai_chat_completions") {
    return ProviderApi::openai_chat_completions;
  }
  if (name == "anthropic_messages") return ProviderApi::anthropic_messages;
  if (name == "gemini_interactions") return ProviderApi::gemini_interactions;
  return std::nullopt;
}

// The id becomes part of secrets/<id>_api_key.txt, so it must stay a plain
// file-name fragment.
bool valid_provider_id(std::string_view id) {
  if (id.empty() || id.size() > 32) return false;
  return std::all_of(id.begin(), id.end(), [](unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' ||
           c == '-';
  });
}

// -----------------------------------------------------------------------------
// Section: Local files
// -----------------------------------------------------------------------------

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

fs::path usage_path(const ProviderConfig& config) {
  return project_root() / "secrets" / (config.id + "_usage.json");
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

// -----------------------------------------------------------------------------
// Section: Pacific quota day
// -----------------------------------------------------------------------------

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


const char* priority_name(ProviderRequestPriority priority) {
  switch (priority) {
    case ProviderRequestPriority::automatic: return "automatic";
    case ProviderRequestPriority::repair: return "repair";
    case ProviderRequestPriority::manual: return "manual";
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

// -----------------------------------------------------------------------------
// Section: Configuration loading
// -----------------------------------------------------------------------------

std::optional<ProviderConfig> load_coach_provider_config(std::string* error_code) {
  const auto fail = [error_code](std::string code) {
    if (error_code) *error_code = std::move(code);
    return std::optional<ProviderConfig>{};
  };
  const auto root = project_root();
  const auto config_text = read_text(root / "config" / "coach_provider.json");
  if (!config_text) return fail("coach_provider_config_missing");

  ProviderConfig config;
  try {
    const auto document = json::parse(*config_text);
    config.id = document.value("provider", std::string{});
    if (!valid_provider_id(config.id)) return fail("coach_provider_invalid");

    // Current format: one settings object per provider under "providers".
    // Legacy format: the selected provider's settings live at the top level.
    json settings = json::object();
    if (document.contains("providers") && document["providers"].is_object()) {
      const auto& providers = document["providers"];
      if (providers.contains(config.id) && providers[config.id].is_object()) {
        settings = providers[config.id];
      }
    } else {
      settings = document;
    }

    ProviderPreset preset = kCustomProviderLimits;
    if (const auto* known = find_preset(config.id)) {
      preset = *known;
    } else {
      const auto api = parse_api(settings.value("api", std::string{}));
      if (!api) return fail("coach_provider_unknown");
      preset.api = *api;
    }

    config.api = preset.api;
    config.endpoint = settings.value("endpoint", std::string(preset.endpoint));
    config.model = settings.value("model", std::string(preset.model));
    config.effort = settings.value("effort", std::string{});
    config.max_tokens_field = settings.value(
        "maxTokensField", std::string(preset.max_tokens_field));
    config.refusal_fallback = settings.value("refusalFallback", false);
    config.free_tier_only =
        settings.value("freeTierOnly", !preset.free_tier_model.empty());
    config.timeout_ms = std::min(
        preset.timeout_ms_ceiling,
        std::max(5'000, settings.value("timeoutMs", config.timeout_ms)));
    config.max_output_tokens = std::min(
        preset.max_output_tokens_ceiling,
        std::max(64, settings.value("maxOutputTokens",
                                    preset.max_output_tokens)));

    config.rpm_hard_limit = bounded_size(
        settings, "rpmHardLimit", config.rpm_hard_limit, 1, preset.rpm_ceiling);
    config.rpm_soft_limit = bounded_size(
        settings, "rpmSoftLimit", config.rpm_soft_limit, 1,
        config.rpm_hard_limit);
    config.tpm_hard_limit = bounded_size(
        settings, "tpmHardLimit", config.tpm_hard_limit, 1, preset.tpm_ceiling);
    config.tpm_soft_limit = bounded_size(
        settings, "tpmSoftLimit", config.tpm_soft_limit, 1,
        config.tpm_hard_limit);
    config.rpd_hard_limit = bounded_size(
        settings, "rpdHardLimit", config.rpd_hard_limit, 1, preset.rpd_ceiling);
    config.rpd_soft_limit = bounded_size(
        settings, "rpdSoftLimit", config.rpd_soft_limit, 1,
        config.rpd_hard_limit);
    config.backoff_base_seconds = std::min(
        30, std::max(1, settings.value("backoffBaseSeconds",
                                       config.backoff_base_seconds)));
    config.backoff_max_seconds = std::min(
        300, std::max(config.backoff_base_seconds,
                      settings.value("backoffMaxSeconds",
                                     config.backoff_max_seconds)));
  } catch (...) {
    return fail("coach_provider_config_invalid");
  }
  if (!config.endpoint.starts_with("https://") || config.model.empty() ||
      (config.api == ProviderApi::openai_chat_completions &&
       config.max_tokens_field != "max_tokens" &&
       config.max_tokens_field != "max_completion_tokens")) {
    return fail("coach_provider_config_invalid");
  }

  const auto key_text =
      read_text(root / "secrets" / (config.id + "_api_key.txt"));
  config.api_key = key_text ? trimmed(*key_text) : std::string{};
  if (config.api_key.empty() || config.api_key.starts_with("PASTE_YOUR_")) {
    return fail(config.id + "_api_key_missing");
  }
  if (const auto* known = find_preset(config.id);
      known && !known->free_tier_model.empty() &&
      (!config.free_tier_only || config.model != known->free_tier_model)) {
    return fail(config.id + "_free_tier_guard_config");
  }
  return config;
}

// -----------------------------------------------------------------------------
// Section: Local quota guard
// -----------------------------------------------------------------------------

// RPD follows the Pacific quota day of Google AI Studio. For paid providers it
// is a local daily spend budget on the same boundary.
bool reserve_provider_request(const ProviderConfig& config,
                              ProviderRequestPriority priority,
                              std::size_t estimated_input_tokens,
                              std::string* error_code) {
  std::lock_guard lock(usage_mutex());
  const auto path = usage_path(config);
  const auto now = std::chrono::duration_cast<std::chrono::seconds>(
                       Clock::now().time_since_epoch())
                       .count();
  auto state = read_usage(path);
  const auto reject = [&](const char* guard) {
    write_usage(path, state);
    if (error_code) *error_code = config.id + guard;
    return false;
  };

  const auto day_start = pacific_day_start_utc(now);
  state.requests.erase(
      std::remove_if(state.requests.begin(), state.requests.end(),
                     [day_start](const UsageRecord& item) {
                       return item.timestamp < day_start;
                     }),
      state.requests.end());

  if (state.blocked_until > now) return reject("_local_backoff_guard");

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
  const bool automatic = priority == ProviderRequestPriority::automatic;

  if (rpd >= config.rpd_hard_limit) return reject("_local_rpd_hard_guard");
  if (rpm >= config.rpm_hard_limit) return reject("_local_rpm_hard_guard");
  if (tpm + estimated_input_tokens > config.tpm_hard_limit) {
    return reject("_local_tpm_hard_guard");
  }

  if (automatic && rpd >= config.rpd_soft_limit) {
    return reject("_automatic_rpd_soft_guard");
  }
  if (automatic && rpm >= config.rpm_soft_limit) {
    return reject("_automatic_rpm_soft_guard");
  }
  if (automatic && tpm + estimated_input_tokens > config.tpm_soft_limit) {
    return reject("_automatic_tpm_soft_guard");
  }

  state.requests.push_back({.timestamp = now,
                            .estimated_input_tokens = estimated_input_tokens,
                            .priority = priority_name(priority)});
  write_usage(path, state);
  return true;
}

void record_provider_rate_limit(const ProviderConfig& config,
                                int retry_after_seconds) {
  std::lock_guard lock(usage_mutex());
  const auto path = usage_path(config);
  const auto now = std::chrono::duration_cast<std::chrono::seconds>(
                       Clock::now().time_since_epoch())
                       .count();
  auto state = read_usage(path);
  state.consecutive_429 = std::min(8, state.consecutive_429 + 1);
  int exponential = config.backoff_base_seconds;
  for (int i = 1; i < state.consecutive_429; ++i) {
    exponential = std::min(config.backoff_max_seconds, exponential * 2);
  }
  const int delay = std::min(
      config.backoff_max_seconds,
      std::max(exponential, std::max(0, retry_after_seconds)));
  state.blocked_until = std::max(state.blocked_until, now + delay);
  write_usage(path, state);
}

void record_provider_success(const ProviderConfig& config) {
  std::lock_guard lock(usage_mutex());
  const auto path = usage_path(config);
  auto state = read_usage(path);
  if (state.consecutive_429 == 0 && state.blocked_until == 0) return;
  state.consecutive_429 = 0;
  state.blocked_until = 0;
  write_usage(path, state);
}

}  // namespace kchess::ai
