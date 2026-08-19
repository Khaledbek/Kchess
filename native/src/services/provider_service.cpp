#include "services/provider_service.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "chess/pgn.h"
#include "providers/provider_common.h"
#include "providers/provider_models.h"

namespace kchess {
namespace {

std::int64_t unix_time_seconds() {
  return std::chrono::duration_cast<std::chrono::seconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

std::string escape_json(const std::string& input) {
  std::ostringstream output;
  for (const unsigned char character : input) {
    switch (character) {
      case '"': output << "\\\""; break;
      case '\\': output << "\\\\"; break;
      case '\b': output << "\\b"; break;
      case '\f': output << "\\f"; break;
      case '\n': output << "\\n"; break;
      case '\r': output << "\\r"; break;
      case '\t': output << "\\t"; break;
      default:
        if (character < 0x20) {
          output << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                 << static_cast<int>(character) << std::dec;
        } else {
          output << character;
        }
    }
  }
  return output.str();
}

void validate_token(const std::string& value, const char* field) {
  if (value.empty() || value.size() > 128) {
    throw std::invalid_argument(std::string(field) + " must contain 1-128 characters");
  }
  if (std::any_of(value.begin(), value.end(), [](const unsigned char character) {
        return std::iscntrl(character) != 0;
      })) {
    throw std::invalid_argument(std::string(field) + " contains control characters");
  }
}

nlohmann::json performance_json(const ProviderPerformance& value) {
  nlohmann::json result{{"key", value.key}};
  const auto put = [&](const char* key, const auto& item) {
    if (item.has_value()) result[key] = *item;
    else result[key] = nullptr;
  };
  put("currentRating", value.current_rating);
  put("bestRating", value.best_rating);
  put("lowestRating", value.lowest_rating);
  put("ratingProgress", value.rating_progress);
  put("games", value.games);
  put("wins", value.wins);
  put("losses", value.losses);
  put("draws", value.draws);
  if (value.games.has_value() && *value.games > 0) {
    if (value.wins) result["winRate"] = 100.0 * *value.wins / *value.games;
    if (value.losses) result["lossRate"] = 100.0 * *value.losses / *value.games;
    if (value.draws) result["drawRate"] = 100.0 * *value.draws / *value.games;
  }
  return result;
}

std::string normalized_profile_json(const ProviderProfile& profile) {
  nlohmann::json result{
      {"provider", provider_type_name(profile.provider)},
      {"username", profile.username},
      {"displayName", profile.display_name},
      {"fallbackAsset", profile.fallback_asset},
      {"disabled", profile.disabled},
      {"tosViolation", profile.tos_violation},
  };
  const auto put = [&](const char* key, const auto& item) {
    if (item.has_value()) result[key] = *item;
    else result[key] = nullptr;
  };
  put("title", profile.title);
  put("avatarUrl", profile.avatar_url);
  put("flair", profile.flair);
  put("fide", profile.fide);
  put("games", profile.games);
  put("wins", profile.wins);
  put("losses", profile.losses);
  put("draws", profile.draws);
  put("playTimeSeconds", profile.play_time_seconds);
  result["ratingSummary"] = nlohmann::json::array();
  for (const auto& performance : profile.rating_summary) {
    result["ratingSummary"].push_back(performance_json(performance));
  }
  return result.dump();
}

std::string normalized_stats_json(const ProviderStats& stats) {
  nlohmann::json result = nlohmann::json::array();
  for (const auto& performance : stats.performances) {
    result.push_back(performance_json(performance));
  }
  return result.dump();
}

std::string lowercase(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char character) {
    return static_cast<char>(std::tolower(character));
  });
  return value;
}

ProviderGameQuery provider_month_query(int requested_year, int requested_month) {
  using namespace std::chrono;
  const year_month_day today{floor<days>(system_clock::now())};
  if (requested_year == 0 && requested_month == 0) {
    requested_year = static_cast<int>(today.year());
    requested_month = static_cast<unsigned>(today.month());
  }
  const year_month_day first{
      std::chrono::year{requested_year}, std::chrono::month{static_cast<unsigned>(requested_month)},
      std::chrono::day{1}};
  if (!first.ok()) throw std::invalid_argument("invalid provider month");
  const year_month next = first.year() / first.month() + months{1};
  const auto since = duration_cast<milliseconds>(sys_days{first}.time_since_epoch()).count();
  auto until = duration_cast<milliseconds>(sys_days{next / day{1}}.time_since_epoch()).count() - 1;
  const auto now = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
  if (until > now) until = now;
  return {
      .year = requested_year,
      .month = requested_month,
      .since_ms = since,
      .until_ms = until,
  };
}

std::string provider_month_name(const ProviderGameQuery& query) {
  std::ostringstream result;
  result << std::setfill('0') << std::setw(4) << query.year << '-'
         << std::setw(2) << query.month;
  return result.str();
}

}  // namespace

ProviderService::ProviderService(
    Database& database, ProfileService& profile_service, std::filesystem::path data_directory)
    : database_(database),
      profile_service_(profile_service),
      data_directory_(std::move(data_directory)),
      http_client_(make_platform_http_client()) {}

ProviderService::~ProviderService() {
  std::vector<std::shared_ptr<ProviderJob>> jobs;
  {
    std::lock_guard lock(provider_jobs_mutex_);
    for (const auto& [id, job] : provider_jobs_) {
      (void)id;
      job->cancel->cancel();
      jobs.push_back(job);
    }
  }
  for (const auto& job : jobs) {
    if (job->worker.joinable()) job->worker.join();
  }
}

void ProviderService::cancel_jobs_for_other_profiles(const std::string& profile_id) {
  std::lock_guard lock(provider_jobs_mutex_);
  for (const auto& [id, job] : provider_jobs_) {
    (void)id;
    std::lock_guard state_lock(job->state_mutex);
    if (!job->finished && !job->profile_id.empty() && job->profile_id != profile_id) {
      job->cancel->cancel();
    }
  }
}

void ProviderService::cancel_jobs_for_profile(const std::string& profile_id) {
  std::vector<std::shared_ptr<ProviderJob>> jobs;
  {
    std::lock_guard lock(provider_jobs_mutex_);
    for (auto iterator = provider_jobs_.begin(); iterator != provider_jobs_.end();) {
      const auto& job = iterator->second;
      std::lock_guard state_lock(job->state_mutex);
      if (job->profile_id == profile_id) {
        job->cancel->cancel();
        jobs.push_back(job);
        iterator = provider_jobs_.erase(iterator);
      } else {
        ++iterator;
      }
    }
  }
  for (const auto& job : jobs) {
    if (job->worker.joinable()) job->worker.join();
  }
}

std::unique_ptr<GameProvider> ProviderService::provider_for(const ProfileType type) {
  switch (type) {
    case ProfileType::chess_com:
      return std::make_unique<ChessComProvider>(*http_client_, provider_scheduler_);
    case ProfileType::lichess:
      return std::make_unique<LichessProvider>(*http_client_, provider_scheduler_);
    case ProfileType::local_pgn_fen:
      break;
  }
  throw std::invalid_argument("local profiles do not have an online provider");
}

void ProviderService::finish_provider_job(
    const std::shared_ptr<ProviderJob>& job,
    std::string state,
    std::string result,
    std::string error_kind,
    std::string error_message) noexcept {
  try {
    std::lock_guard lock(job->state_mutex);
    job->state = std::move(state);
    job->result_json = std::move(result);
    job->error_kind = std::move(error_kind);
    job->error_message = std::move(error_message);
    job->finished = true;
  } catch (...) {
    job->finished = true;
  }
}

void ProviderService::cache_provider_avatar(
    const std::string& profile_id,
    const ProviderProfile& profile,
    const std::shared_ptr<CancelToken>& cancel) noexcept {
  try {
    if (profile.provider != ProviderType::chess_com || !profile.avatar_url.has_value()) return;
    auto url = *profile.avatar_url;
    auto lower_url = lowercase(url);
    if (!lower_url.starts_with("https://")) return;
    const auto host_end = lower_url.find('/', 8);
    const auto host = lower_url.substr(8, host_end == std::string::npos
        ? std::string::npos : host_end - 8);
    if (!(host == "chess.com" || host.ends_with(".chess.com")
          || host == "chesscomfiles.com" || host.ends_with(".chesscomfiles.com"))) {
      return;
    }
    const auto avatar_directory = data_directory_ / "avatars";
    const auto destination = avatar_directory / (profile_id + ".img");
    if (std::filesystem::exists(destination)) {
      database_.set_profile_avatar_file(profile_id, destination.string());
      return;
    }
    auto request = provider_request(url, "image/*", {}, cancel);
    request.max_body_bytes = 8U * 1024U * 1024U;
    const auto response = provider_scheduler_.get(profile.provider, *http_client_, request);
    if (!response.ok() || !response.header("content-type").starts_with("image/")
        || response.body.empty()) {
      return;
    }
    std::filesystem::create_directories(avatar_directory);
    const auto temporary = avatar_directory / (profile_id + ".tmp");
    {
      std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
      output.write(response.body.data(), static_cast<std::streamsize>(response.body.size()));
      if (!output) return;
    }
    std::filesystem::rename(temporary, destination);
    database_.set_profile_avatar_file(profile_id, destination.string());
  } catch (...) {
    // Avatar failures intentionally fall back to the bundled provider asset.
  }
}

void ProviderService::sync_provider_resources(
    const std::string& profile_id,
    GameProvider& provider,
    const int year,
    const int month,
    const std::shared_ptr<ProviderJob>& job) {
  auto profile = database_.profile(profile_id);
  if (!profile.has_value() || !profile->provider_username.has_value()) {
    throw std::invalid_argument("online profile not found");
  }
  const auto ensure_active = [&] {
    if (job->cancel->is_cancelled()) {
      throw ProviderException(HttpError::cancelled, "provider request cancelled");
    }
  };
  const auto set_phase = [&](const char* phase) {
    std::lock_guard lock(job->state_mutex);
    job->state = phase;
  };
  std::optional<std::int64_t> transient_provider_joined;
  ensure_active();
  set_phase("profile");
  const auto profile_cache = database_.provider_cache(profile_id, "profile");
  if (!profile_cache || profile_cache->expires_at <= unix_time_seconds()) {
    auto remote_profile = provider.fetch_profile(
        *profile->provider_username,
        profile_cache ? profile_cache->validators : CacheValidators{}, job->cancel);
    if (remote_profile.value.has_value()) {
      // Keep provider creation time in memory only long enough to derive the
      // initial Lichess month list. Database::update_provider_profile deliberately
      // does not persist this account metadata.
      transient_provider_joined = remote_profile.value->joined;
      profile = database_.update_provider_profile(
          profile_id, *remote_profile.value, remote_profile.cache,
          normalized_profile_json(*remote_profile.value));
      cache_provider_avatar(profile_id, *remote_profile.value, job->cancel);
    }
  }

  ensure_active();
  set_phase("stats");
  const auto stats_cache = database_.provider_cache(profile_id, "stats");
  if (!stats_cache || stats_cache->expires_at <= unix_time_seconds()) {
    const auto stats = provider.fetch_stats(
        *profile->provider_username,
        stats_cache ? stats_cache->validators : CacheValidators{}, job->cancel);
    if (stats.value.has_value()) {
      database_.put_provider_cache(
          profile_id, "stats", normalized_stats_json(*stats.value), stats.cache);
    }
  }

  ensure_active();
  set_phase("months");
  const auto archive_cache = database_.provider_cache(profile_id, "archives");
  if (!archive_cache || archive_cache->expires_at <= unix_time_seconds()) {
    auto archives = provider.fetch_available_months(
        *profile->provider_username,
        archive_cache ? archive_cache->validators : CacheValidators{}, job->cancel);
    if (archives.value.has_value()) {
      auto months_json = nlohmann::json(*archives.value);
      if (provider.type() == ProviderType::lichess) {
        months_json = nlohmann::json::array();
        // Once built, the month list itself contains only game-period metadata and
        // is safe to retain. Reuse it on later refreshes instead of persisting the
        // account creation timestamp merely to rebuild the same list.
        if (archive_cache.has_value()) {
          try {
            const auto cached = nlohmann::json::parse(archive_cache->payload_json);
            if (cached.is_array()) months_json = cached;
          } catch (...) {
          }
        }
        using namespace std::chrono;
        const auto today = year_month_day{floor<days>(system_clock::now())};
        const year_month end = today.year() / today.month();
        if (months_json.empty()) {
          year_month start = end;
          if (transient_provider_joined.has_value()) {
            const auto joined_days = floor<days>(
                sys_seconds{seconds{*transient_provider_joined}});
            const year_month_day joined{joined_days};
            start = joined.year() / joined.month();
          }
          for (auto value = start; value <= end; value += std::chrono::months{1}) {
            std::ostringstream name;
            name << std::setfill('0') << std::setw(4) << static_cast<int>(value.year())
                 << '-' << std::setw(2) << static_cast<unsigned>(value.month());
            months_json.push_back(name.str());
          }
        } else {
          std::ostringstream current;
          current << std::setfill('0') << std::setw(4) << static_cast<int>(end.year())
                  << '-' << std::setw(2) << static_cast<unsigned>(end.month());
          if (std::find(months_json.begin(), months_json.end(), current.str()) == months_json.end()) {
            months_json.push_back(current.str());
          }
        }
        if (archives.cache.fetched_at == 0) {
          archives.cache.fetched_at = unix_time_seconds();
          archives.cache.expires_at = archives.cache.fetched_at + 86400;
        }
      }
      database_.put_provider_cache(
          profile_id, "archives", months_json.dump(), archives.cache);
    }
  }

  ensure_active();
  set_phase("games");
  const auto query = provider_month_query(year, month);
  const auto month_name = provider_month_name(query);
  const auto month_cache = database_.provider_cache(profile_id, "month:" + month_name);
  if (!month_cache || month_cache->expires_at <= unix_time_seconds()) {
    const auto remote_games = provider.fetch_games(
        *profile->provider_username, query,
        month_cache ? month_cache->validators : CacheValidators{}, job->cancel);
    if (remote_games.value.has_value()) {
      std::vector<ProviderStoredGame> normalized;
      normalized.reserve(remote_games.value->size());
      for (const auto& game : *remote_games.value) {
        if (!game.completed || game.pgn.empty()) continue;
        const auto parsed = parse_pgn(game.pgn);
        if (!parsed.valid || parsed.game.moves.empty()) continue;
        normalized.push_back({.provider_game = game, .parsed_game = parsed.game});
      }
      database_.upsert_provider_games(
          profile_id, month_name, normalized, remote_games.cache);
    }
  }
}

void ProviderService::run_provider_create(
    const ProfileType type,
    std::string username,
    const std::shared_ptr<ProviderJob>& job) noexcept {
  try {
    auto provider = provider_for(type);
    auto result = provider->fetch_profile(username, {}, job->cancel);
    if (!result.value.has_value()) {
      throw ProviderException(HttpError::invalid_response, "provider returned no profile");
    }
    auto profile = database_.create_provider_profile(
        *result.value, result.cache, normalized_profile_json(*result.value));
    cache_provider_avatar(profile.id, *result.value, job->cancel);
    {
      std::lock_guard lock(job->state_mutex);
      job->profile_id = profile.id;
    }
    database_.set_provider_sync_state(
        profile.id, provider->type(), "syncing", {}, 0);
    try {
      sync_provider_resources(profile.id, *provider, 0, 0, job);
      database_.set_provider_sync_state(profile.id, provider->type(), "idle", {}, 0);
      finish_provider_job(job, "complete", provider_overview_json(profile.id));
    } catch (const ProviderException& warning) {
      database_.set_provider_sync_state(
          profile.id, provider->type(), "error", warning.what(),
          provider_scheduler_.retry_after_seconds(provider->type()));
      finish_provider_job(
          job, "complete", provider_overview_json(profile.id),
          http_error_name(warning.kind()), warning.what());
    }
  } catch (const ProviderException& error) {
    finish_provider_job(job, "error", {}, http_error_name(error.kind()), error.what());
  } catch (const std::exception& error) {
    finish_provider_job(job, "error", {}, "internal", error.what());
  } catch (...) {
    finish_provider_job(job, "error", {}, "internal", "unknown provider error");
  }
}

void ProviderService::run_provider_sync(
    std::string profile_id,
    const int year,
    const int month,
    const std::shared_ptr<ProviderJob>& job) noexcept {
  try {
    const auto profile = database_.profile(profile_id);
    if (!profile.has_value()) throw std::invalid_argument("profile not found");
    auto provider = provider_for(profile->type);
    database_.set_provider_sync_state(
        profile_id, provider->type(), "syncing", {}, 0);
    sync_provider_resources(profile_id, *provider, year, month, job);
    database_.set_provider_sync_state(profile_id, provider->type(), "idle", {}, 0);
    finish_provider_job(job, "complete", provider_overview_json(profile_id));
  } catch (const ProviderException& error) {
    try {
      const auto profile = database_.profile(profile_id);
      if (profile.has_value()) {
        const auto provider_type = static_cast<ProviderType>(profile->type);
        database_.set_provider_sync_state(
            profile_id, provider_type, "error", error.what(),
            provider_scheduler_.retry_after_seconds(provider_type));
      }
    } catch (...) {
    }
    finish_provider_job(
        job, "error", {}, http_error_name(error.kind()), error.what());
  } catch (const std::exception& error) {
    finish_provider_job(job, "error", {}, "internal", error.what());
  } catch (...) {
    finish_provider_job(job, "error", {}, "internal", "unknown provider error");
  }
}

std::string ProviderService::start_provider_profile_json(
    const ProfileType type, const std::string& username) {
  if (type != ProfileType::chess_com && type != ProfileType::lichess) {
    throw std::invalid_argument("online provider required");
  }
  validate_token(username, "provider username");
  auto job = std::make_shared<ProviderJob>();
  job->id = "provider-" + std::to_string(next_provider_job_id_++);
  {
    std::lock_guard lock(provider_jobs_mutex_);
    provider_jobs_.emplace(job->id, job);
  }
  job->worker = std::thread(
      [this, type, username, job] { run_provider_create(type, username, job); });
  return "{\"jobId\":\"" + escape_json(job->id) + "\"}";
}

std::string ProviderService::start_provider_sync_json(
    const std::string& profile_id, const int year, const int month) {
  validate_token(profile_id, "profile id");
  const auto profile = database_.profile(profile_id);
  if (!profile.has_value() || profile->type == ProfileType::local_pgn_fen) {
    throw std::invalid_argument("online profile not found");
  }
  (void)provider_month_query(year, month);
  auto job = std::make_shared<ProviderJob>();
  job->id = "provider-" + std::to_string(next_provider_job_id_++);
  job->profile_id = profile_id;
  {
    std::lock_guard lock(provider_jobs_mutex_);
    for (const auto& [id, current] : provider_jobs_) {
      (void)id;
      std::lock_guard state_lock(current->state_mutex);
      if (!current->finished && current->profile_id == profile_id) current->cancel->cancel();
    }
    provider_jobs_.emplace(job->id, job);
  }
  job->worker = std::thread(
      [this, profile_id, year, month, job] {
        run_provider_sync(profile_id, year, month, job);
      });
  return "{\"jobId\":\"" + escape_json(job->id) + "\"}";
}

std::string ProviderService::provider_job_status_json(const std::string& job_id) {
  validate_token(job_id, "provider job id");
  std::shared_ptr<ProviderJob> job;
  {
    std::lock_guard lock(provider_jobs_mutex_);
    const auto found = provider_jobs_.find(job_id);
    if (found == provider_jobs_.end()) throw std::runtime_error("provider job not found");
    job = found->second;
  }
  if (job->finished && job->worker.joinable()) job->worker.join();
  std::string output;
  {
    std::lock_guard lock(job->state_mutex);
    std::ostringstream json;
    json << "{\"jobId\":\"" << escape_json(job->id) << "\",\"state\":\""
         << escape_json(job->state) << "\",\"finished\":"
         << (job->finished ? "true" : "false") << ",\"profileId\":";
    if (job->profile_id.empty()) json << "null";
    else json << '"' << escape_json(job->profile_id) << '"';
    json << ",\"result\":" << (job->result_json.empty() ? "null" : job->result_json)
         << ",\"errorKind\":";
    if (job->error_kind.empty()) json << "null";
    else json << '"' << escape_json(job->error_kind) << '"';
    json << ",\"errorMessage\":";
    if (job->error_message.empty()) json << "null";
    else json << '"' << escape_json(job->error_message) << '"';
    json << '}';
    output = json.str();
  }
  if (job->finished) {
    std::lock_guard lock(provider_jobs_mutex_);
    provider_jobs_.erase(job_id);
  }
  return output;
}

void ProviderService::cancel_provider_job(const std::string& job_id) {
  validate_token(job_id, "provider job id");
  std::lock_guard lock(provider_jobs_mutex_);
  const auto found = provider_jobs_.find(job_id);
  if (found == provider_jobs_.end()) throw std::runtime_error("provider job not found");
  found->second->cancel->cancel();
}

std::string ProviderService::provider_overview_json(const std::string& profile_id) {
  validate_token(profile_id, "profile id");
  const auto profile = database_.profile(profile_id);
  if (!profile.has_value()) throw std::runtime_error("profile not found");
  const auto stats = database_.provider_cache(profile_id, "stats");
  const auto archives = database_.provider_cache(profile_id, "archives");
  nlohmann::json months = nlohmann::json::array();
  if (archives.has_value()) {
    try {
      months = nlohmann::json::parse(archives->payload_json);
    } catch (...) {
    }
  }
  for (const auto& month : database_.cached_months(profile_id)) {
    if (std::find(months.begin(), months.end(), month) == months.end()) months.push_back(month);
  }
  std::sort(months.begin(), months.end(), std::greater<>());
  const auto retry_after = profile->type == ProfileType::local_pgn_fen ? 0
      : provider_scheduler_.retry_after_seconds(static_cast<ProviderType>(profile->type));
  std::ostringstream json;
  json << "{\"profile\":" << profile_service_.profile_json(*profile)
       << ",\"stats\":" << (stats ? stats->payload_json : "[]")
       << ",\"availableMonths\":" << months.dump()
       << ",\"offlineReady\":" << (stats || !database_.cached_months(profile_id).empty()
                                          ? "true" : "false")
       << ",\"retryAfterSeconds\":" << retry_after << '}';
  return json.str();
}


}  // namespace kchess
