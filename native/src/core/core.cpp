#include "core/core.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <utility>

#include "diagnostics/logger.h"

namespace kchess {
namespace {

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

}  // namespace

Core::Core(std::filesystem::path data_directory)
    : data_directory_(std::move(data_directory)),
      database_(data_directory_),
      profile_service_(database_, data_directory_),
      game_library_service_(database_, profile_service_),
      settings_service_(database_),
      provider_service_(database_, profile_service_, data_directory_),
      opening_theory_(std::make_unique<UnavailableOpeningTheoryProvider>()),
      analysis_service_(database_, *opening_theory_) {
  diagnostics::configure_logging(data_directory_);
  diagnostics::info("core", "Core created");
}

Core::~Core() = default;

void Core::initialize() {
  diagnostics::info("core", "Initialization started");
  database_.open_and_migrate();
  diagnostics::set_enabled(database_.settings().diagnostic_logging);
  const auto book_path = data_directory_ / "opening_book.kcb";
  if (std::filesystem::exists(book_path)) {
    try {
      opening_theory_ = std::make_unique<KcbOpeningTheoryProvider>(book_path);
    } catch (const std::exception& error) {
      opening_theory_ = std::make_unique<UnavailableOpeningTheoryProvider>();
      last_error_ = std::string("Opening book disabled: ") + error.what();
    }
  }
  analysis_service_.set_opening_theory_provider(*opening_theory_);
  initialized_ = true;
  diagnostics::info("core", "Initialization complete");
}

std::string Core::profiles_json() {
  return profile_service_.profiles_json();
}

std::string Core::create_profile_json(
    const ProfileType type,
    const std::string& display_name,
    const std::string& provider_username) {
  return profile_service_.create_profile_json(type, display_name, provider_username);
}

void Core::set_active_profile(const std::string& profile_id) {
  validate_token(profile_id, "profile id");
  provider_service_.cancel_jobs_for_other_profiles(profile_id);
  profile_service_.set_active_profile(profile_id);
}

void Core::delete_profile(const std::string& profile_id) {
  validate_token(profile_id, "profile id");
  const auto profile = database_.profile(profile_id);
  if (!profile.has_value()) throw std::runtime_error("Profile not found");

  provider_service_.cancel_jobs_for_profile(profile_id);

  const auto game_ids = database_.profile_game_ids(profile_id);
  analysis_service_.cancel_jobs_for_games(game_ids);

  profile_service_.delete_profile_storage(*profile);
}

std::string Core::active_profile_json() {
  return profile_service_.active_profile_json();
}

std::string Core::settings_json() {
  return settings_service_.settings_json();
}

void Core::set_engine_settings(
    const int depth, const int multi_pv, const int time_limit_seconds) {
  settings_service_.set_engine_settings(depth, multi_pv, time_limit_seconds);
}

void Core::set_engine_resources(const int threads, const int hash_mb) {
  settings_service_.set_engine_resources(threads, hash_mb);
}

void Core::set_show_board_arrows(const bool enabled) {
  settings_service_.set_show_board_arrows(enabled);
}

void Core::set_boolean_setting(const std::string& key, const bool enabled) {
  settings_service_.set_boolean_setting(key, enabled);
}

void Core::set_theme_mode(const std::string& mode) {
  settings_service_.set_theme_mode(mode);
}

void Core::set_locale(const std::string& locale) {
  settings_service_.set_locale(locale);
}

std::string Core::games_json() {
  return game_library_service_.games_json();
}

std::string Core::game_json(const std::string& game_id) {
  return game_library_service_.game_json(game_id);
}

std::string Core::import_pgn_json(const std::string& pgn) {
  return game_library_service_.import_pgn_json(pgn);
}

std::string Core::import_fen_json(
    const std::string& fen, const std::string& display_name) {
  return game_library_service_.import_fen_json(fen, display_name);
}

std::string Core::start_provider_profile_json(
    const ProfileType type, const std::string& username) {
  return provider_service_.start_provider_profile_json(type, username);
}

std::string Core::start_provider_sync_json(
    const std::string& profile_id, const int year, const int month) {
  return provider_service_.start_provider_sync_json(profile_id, year, month);
}

std::string Core::provider_job_status_json(const std::string& job_id) {
  return provider_service_.provider_job_status_json(job_id);
}

void Core::cancel_provider_job(const std::string& job_id) {
  provider_service_.cancel_provider_job(job_id);
}

std::string Core::provider_overview_json(const std::string& profile_id) {
  return provider_service_.provider_overview_json(profile_id);
}

void Core::set_favorite(const std::string& game_id, const bool value) {
  game_library_service_.set_favorite(game_id, value);
}

std::string Core::favorite_collections_json() {
  return game_library_service_.favorite_collections_json();
}

std::string Core::create_favorite_collection_json(const std::string& name) {
  return game_library_service_.create_favorite_collection_json(name);
}

void Core::rename_favorite_collection(
    const std::string& collection_id, const std::string& name) {
  game_library_service_.rename_favorite_collection(collection_id, name);
}

void Core::delete_favorite_collection(const std::string& collection_id) {
  game_library_service_.delete_favorite_collection(collection_id);
}

void Core::set_favorite_collection(
    const std::string& game_id,
    const std::optional<std::string>& collection_id) {
  game_library_service_.set_favorite_collection(game_id, collection_id);
}

void Core::set_downloaded(const std::string& game_id, const bool value) {
  game_library_service_.set_downloaded(game_id, value);
}

void Core::delete_local_game(const std::string& game_id) {
  game_library_service_.delete_local_game(game_id);
}

void Core::clear_cached_month(
    const std::string& profile_id, const std::string& month) {
  game_library_service_.clear_cached_month(profile_id, month);
}

std::string Core::start_analysis_json(const std::string& game_id) {
  return analysis_service_.start_analysis_json(game_id);
}

std::string Core::analysis_status_json(const std::string& game_id) {
  return analysis_service_.analysis_status_json(game_id);
}

std::string Core::move_analysis_status_json(
    const std::string& game_id, const int ply) {
  return analysis_service_.move_analysis_status_json(game_id, ply);
}

void Core::cancel_analysis(const std::string& game_id) {
  analysis_service_.cancel_analysis(game_id);
}

void Core::clear_engine_cache() {
  analysis_service_.clear_engine_cache();
}

std::string Core::start_variation_analysis_json(
    const std::string& fen, const std::string& uci) {
  return analysis_service_.start_variation_analysis_json(fen, uci);
}

std::string Core::start_variation_analysis_with_settings_json(
    const std::string& fen,
    const std::string& uci,
    const int depth,
    const int multi_pv,
    const int threads,
    const int hash_mb) {
  return analysis_service_.start_variation_analysis_with_settings_json(
      fen, uci, depth, multi_pv, threads, hash_mb);
}

std::string Core::variation_analysis_status_json(const std::string& job_id) {
  return analysis_service_.variation_analysis_status_json(job_id);
}

}  // namespace kchess
