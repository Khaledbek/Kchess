// -----------------------------------------------------------------------------
// Section: Native service orchestration
// -----------------------------------------------------------------------------

#include "core/core.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <utility>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <nlohmann/json.hpp>

#include "diagnostics/logger.h"
#include "engine/stockfish_factory.h"
#include "persistence/sqlite_write_priority.h"
#include "theory/opening_name_index.h"

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

struct ProcessRuntimeDiagnosticsState {
  std::mutex mutex;
  bool initialized{false};
  std::chrono::steady_clock::time_point started_at{};
  std::chrono::steady_clock::time_point last_sample_at{};
  std::uint64_t started_cpu_100ns{0};
  std::uint64_t last_cpu_100ns{0};
  bool cpu_baseline_available{false};
  unsigned int logical_processors{1};
};

ProcessRuntimeDiagnosticsState& process_runtime_diagnostics_state() {
  static ProcessRuntimeDiagnosticsState state;
  return state;
}

#ifdef _WIN32
std::optional<std::uint64_t> current_process_cpu_100ns() {
  FILETIME creation{};
  FILETIME exit{};
  FILETIME kernel{};
  FILETIME user{};
  if (!GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernel, &user)) {
    return std::nullopt;
  }
  const auto to_uint64 = [](const FILETIME value) {
    ULARGE_INTEGER converted{};
    converted.LowPart = value.dwLowDateTime;
    converted.HighPart = value.dwHighDateTime;
    return static_cast<std::uint64_t>(converted.QuadPart);
  };
  return to_uint64(kernel) + to_uint64(user);
}
#endif

void initialize_process_runtime_diagnostics() {
  auto& state = process_runtime_diagnostics_state();
  std::lock_guard lock(state.mutex);
  if (state.initialized) return;

  const auto now = std::chrono::steady_clock::now();
  state.started_at = now;
  state.last_sample_at = now;
#ifdef _WIN32
  if (const auto cpu = current_process_cpu_100ns(); cpu.has_value()) {
    state.started_cpu_100ns = *cpu;
    state.last_cpu_100ns = *cpu;
    state.cpu_baseline_available = true;
  }
  SYSTEM_INFO system_info{};
  GetSystemInfo(&system_info);
  state.logical_processors =
      std::max(1u, static_cast<unsigned int>(system_info.dwNumberOfProcessors));
#endif
  state.initialized = true;
}

nlohmann::json process_runtime_diagnostics_json() {
  initialize_process_runtime_diagnostics();
#ifdef _WIN32
  auto& state = process_runtime_diagnostics_state();
  std::lock_guard lock(state.mutex);
  const auto cpu = current_process_cpu_100ns();
  if (!cpu.has_value() || !state.cpu_baseline_available) {
    return {{"schema", "process.runtime.v1"}, {"supported", false}};
  }

  const auto now = std::chrono::steady_clock::now();
  const auto window_json = [&](
                               const std::chrono::steady_clock::time_point from,
                               const std::uint64_t from_cpu) {
    const double wall_ms =
        std::chrono::duration<double, std::milli>(now - from).count();
    const double cpu_ms =
        static_cast<double>(*cpu - from_cpu) / 10000.0;
    const double core_equivalent_percent =
        wall_ms > 0.0 ? (cpu_ms / wall_ms) * 100.0 : 0.0;
    const double logical_capacity_percent =
        core_equivalent_percent / static_cast<double>(state.logical_processors);
    return nlohmann::json{
        {"windowMs", wall_ms},
        {"cpuCoreEquivalentPercent", core_equivalent_percent},
        {"cpuLogicalCapacityPercent",
         std::clamp(logical_capacity_percent, 0.0, 100.0)},
    };
  };

  DWORD handle_count = 0;
  const bool handle_count_available =
      GetProcessHandleCount(GetCurrentProcess(), &handle_count) != 0;
  auto result = nlohmann::json{
      {"schema", "process.runtime.v1"},
      {"supported", true},
      {"logicalProcessors", state.logical_processors},
      {"processCpuTimeMs", static_cast<double>(*cpu) / 10000.0},
      {"sinceCoreCreate", window_json(state.started_at, state.started_cpu_100ns)},
      {"sinceLastInspectorSample",
       window_json(state.last_sample_at, state.last_cpu_100ns)},
  };
  if (handle_count_available) {
    result["processHandleCount"] = handle_count;
  }
  state.last_sample_at = now;
  state.last_cpu_100ns = *cpu;
  return result;
#else
  return {{"schema", "process.runtime.v1"}, {"supported", false}};
#endif
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
      opening_names_(std::make_unique<UnavailableOpeningNameIndex>()),
      opening_lines_(std::make_unique<UnavailableOpeningLineGraph>()),
      analysis_service_(database_, *opening_theory_),
      statistics_service_(database_),
      knowledge_runtime_(database_, statistics_service_, data_directory_),
      player_profile_service_(database_, analysis_service_, provider_service_, knowledge_runtime_),
      bot_service_(database_),
      coach_service_(database_, analysis_service_, knowledge_runtime_),
      training_service_(database_),
      practice_service_(database_, training_service_, bot_service_) {
  initialize_process_runtime_diagnostics();
  diagnostics::configure_logging(data_directory_);
  diagnostics::info("core", "Core created");
}

Core::~Core() = default;

void Core::initialize() {
  diagnostics::info("core", "Initialization started");
  using Clock = std::chrono::steady_clock;
  const auto initialization_started = Clock::now();
  const auto elapsed_ms = [](const auto started) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               Clock::now() - started)
        .count();
  };
  nlohmann::json startup = {
      {"schema", "core.startup.v1"},
      {"databaseOpenAndMigrateMs", 0},
      {"openingBookLoadMs", 0},
      {"openingNamesLoadMs", 0},
      {"openingLinesLoadMs", 0},
      {"pendingOpeningClassification",
       {{"limit", 256}, {"processed", 0}, {"durationMs", 0}}},
      {"knowledgeRuntimeOpenMs", 0},
      {"profileWorkerStartMs", 0},
      {"totalMs", 0},
  };

  auto phase_started = Clock::now();
  database_.open_and_migrate();
  startup["databaseOpenAndMigrateMs"] = elapsed_ms(phase_started);
  diagnostics::set_enabled(database_.settings().diagnostic_logging);

  phase_started = Clock::now();
  const auto book_path = data_directory_ / "opening_book.kcb";
  if (std::filesystem::exists(book_path)) {
    try {
      opening_theory_ = std::make_unique<KcbOpeningTheoryProvider>(book_path);
    } catch (const std::exception& error) {
      opening_theory_ = std::make_unique<UnavailableOpeningTheoryProvider>();
      last_error_ = std::string("Opening book disabled: ") + error.what();
    }
  }
  startup["openingBookLoadMs"] = elapsed_ms(phase_started);
  analysis_service_.set_opening_theory_provider(*opening_theory_);

  phase_started = Clock::now();
  const auto names_path = data_directory_ / "opening_names.kco";
  if (std::filesystem::exists(names_path)) {
    try {
      opening_names_ = std::make_unique<KcoOpeningNameIndex>(names_path);
    } catch (const std::exception& error) {
      opening_names_ = std::make_unique<UnavailableOpeningNameIndex>();
      last_error_ = std::string("Opening names disabled: ") + error.what();
    }
  }
  startup["openingNamesLoadMs"] = elapsed_ms(phase_started);

  phase_started = Clock::now();
  const auto lines_path = data_directory_ / "opening_lines.kcl";
  diagnostics::info(
      "core",
      std::string("Opening lines runtime path: ") + lines_path.string()
          + " | exists=" + (std::filesystem::exists(lines_path) ? "true" : "false"));
  if (std::filesystem::exists(lines_path)) {
    try {
      opening_lines_ = std::make_unique<KclOpeningLineGraph>(lines_path);
      const auto names_metadata = opening_names_->metadata();
      if (names_metadata.entry_count > 0
          && (opening_lines_->metadata().node_count != names_metadata.entry_count
              || opening_lines_->position_key_fingerprint()
                  != opening_names_->position_key_fingerprint())) {
        // KCL owns training topology; KCO is only optional naming metadata.
        // A stale/differently scoped name index must never disable an otherwise
        // valid training graph. Keep both readers alive, let unmatched KCO
        // lookups simply return no label, and surface the contract drift only
        // through diagnostics. This also allows future KCL/KCO coverage to
        // evolve independently while retaining the shared position-key codec.
        diagnostics::warning(
            "core",
            "KCL/KCO coverage differs; opening training remains enabled and "
            "destination names may be unavailable for unmatched graph nodes");
      }
    } catch (const std::exception& error) {
      opening_lines_ = std::make_unique<UnavailableOpeningLineGraph>(error.what());
      last_error_ = std::string("Opening lines disabled: ") + error.what();
    }
  } else {
    opening_lines_ = std::make_unique<UnavailableOpeningLineGraph>(
        std::string("opening_lines.kcl is missing from runtime path: ")
            + lines_path.string());
  }
  startup["openingLinesLoadMs"] = elapsed_ms(phase_started);
  practice_service_.set_opening_sources(*opening_lines_, *opening_names_, *opening_theory_);

  // Keep the legacy bounded opening-classification maintenance visible in the
  // inspector. It is synchronous startup work, so its measured duration is
  // essential when diagnosing a slow first frame.
  phase_started = Clock::now();
  const int classified_openings = classify_pending_openings(256);
  startup["pendingOpeningClassification"] = {
      {"limit", 256},
      {"processed", classified_openings},
      {"durationMs", elapsed_ms(phase_started)},
  };

  phase_started = Clock::now();
  try {
    knowledge_runtime_.open();
  } catch (const std::exception& error) {
    diagnostics::warning(
        "core", std::string("Knowledge Graph runtime disabled: ") + error.what());
  } catch (...) {
    diagnostics::warning("core", "Knowledge Graph runtime disabled");
  }
  startup["knowledgeRuntimeOpenMs"] = elapsed_ms(phase_started);

  phase_started = Clock::now();
  player_profile_service_.start();
  startup["profileWorkerStartMs"] = elapsed_ms(phase_started);
  initialized_ = true;
  startup["totalMs"] = elapsed_ms(initialization_started);
  startup_diagnostics_json_ = startup.dump();
  diagnostics::info("core", "Initialization complete");
}

int Core::classify_pending_openings(const int limit) {
  if (opening_names_->max_ply() == 0) return 0;  // Index unavailable.
  int processed = 0;
  for (const auto& pending : database_.games_needing_opening(limit)) {
    classify_game_opening(pending.first);
    ++processed;
  }
  return processed;
}

void Core::classify_game_opening(const std::string& game_id) {
  if (opening_names_->max_ply() == 0) return;
  const auto game = database_.game(game_id);
  if (!game.has_value() || game->opening_ply.has_value()) return;

  std::optional<OpeningName> opening;
  try {
    if (!game->moves.empty()) {
      opening = classify_opening(*opening_names_, game->moves);
    }
  } catch (const std::exception&) {
    opening.reset();
  }
  if (opening.has_value()) {
    database_.set_game_opening(game_id, opening->eco, opening->name, opening->ply);
  } else {
    database_.set_game_opening(game_id, std::nullopt, std::nullopt, 0);
  }
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
  player_profile_service_.pause_engine_work();
  provider_service_.cancel_jobs_for_other_profiles(profile_id);
  profile_service_.set_active_profile(profile_id);
  player_profile_service_.notify_profile_changed();
}

void Core::delete_profile(const std::string& profile_id) {
  validate_token(profile_id, "profile id");
  const auto profile = database_.profile(profile_id);
  if (!profile.has_value()) throw std::runtime_error("Profile not found");

  player_profile_service_.pause_engine_work();
  provider_service_.cancel_jobs_for_profile(profile_id);

  const auto game_ids = database_.profile_game_ids(profile_id);
  analysis_service_.cancel_jobs_for_games(game_ids);

  profile_service_.delete_profile_storage(*profile);
  player_profile_service_.notify_profile_changed();
}

void Core::merge_local_profile(
    const std::string& source_profile_id,
    const std::string& target_profile_id) {
  validate_token(source_profile_id, "source profile id");
  validate_token(target_profile_id, "target profile id");

  player_profile_service_.pause_engine_work();
  provider_service_.cancel_jobs_for_profile(source_profile_id);
  const auto game_ids = database_.profile_game_ids(source_profile_id);
  analysis_service_.cancel_jobs_for_games(game_ids);

  profile_service_.merge_local_profile(source_profile_id, target_profile_id);
  player_profile_service_.notify_profile_changed();
}

std::string Core::active_profile_json() {
  return profile_service_.active_profile_json();
}

std::string Core::player_profile_json() {
  return player_profile_service_.snapshot_json();
}

std::string Core::settings_json() {
  return settings_service_.settings_json();
}

void Core::set_engine_settings(
    const int depth, const int multi_pv, const int time_limit_seconds) {
  settings_service_.set_engine_settings(depth, multi_pv, time_limit_seconds);
}

void Core::set_analysis_depth_range(
    const int minimum_depth, const int maximum_depth) {
  settings_service_.set_analysis_depth_range(minimum_depth, maximum_depth);
}

void Core::set_engine_resources(const int threads, const int hash_mb) {
  settings_service_.set_engine_resources(threads, hash_mb);
}

void Core::set_sideline_engine_settings(
    const int depth, const int multi_pv, const int threads, const int hash_mb) {
  settings_service_.set_sideline_engine_settings(depth, multi_pv, threads, hash_mb);
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

void Core::set_engine_id(const std::string& engine_id) {
  if (engine_id != "stockfish18" && engine_id != "stockfish19") {
    throw std::invalid_argument("invalid engine id");
  }
  const auto current_engine_id = database_.settings().engine_id;
  if (current_engine_id == engine_id) return;

  // Prove that the candidate engine can initialize with its expected NNUE
  // before stopping any live work or persisting the new selection. A missing
  // or invalid Stockfish 19 runtime asset therefore leaves Stockfish 18 (and
  // vice versa) fully selected and usable instead of creating a half-switched
  // application state.
  const auto candidate = create_stockfish_engine(engine_id);
  candidate->validate_available();
  candidate->start();
  candidate->stop();

  // Stop only live engine work after the candidate has passed its runtime
  // probe. Existing Stockfish 18/19 analysis rows remain untouched and
  // continue to be separated by their engine version/config hash.
  analysis_service_.prepare_for_engine_change();
  settings_service_.set_engine_id(engine_id);
}

std::string Core::games_json() {
  classify_pending_openings(256);
  return game_library_service_.games_json();
}

std::string Core::initial_games_json() {
  classify_pending_openings(256);
  return game_library_service_.initial_games_json();
}

std::string Core::query_games_json(const std::string& query_json) {
  classify_pending_openings(256);
  return game_library_service_.query_games_json(query_json);
}

std::string Core::favorite_games_json() {
  classify_pending_openings(256);
  return game_library_service_.favorite_games_json();
}

std::string Core::game_json(const std::string& game_id) {
  classify_game_opening(game_id);
  return game_library_service_.game_json(game_id);
}

std::string Core::resolve_board_move_json(
    const std::string& game_id,
    const std::string& fen,
    const std::string& source,
    const std::string& target,
    const int first_candidate_ply) {
  return game_library_service_.resolve_board_move_json(
      game_id, fen, source, target, first_candidate_ply);
}

std::string Core::resolve_free_board_move_json(
    const std::string& fen,
    const std::string& source,
    const std::string& target) {
  return game_library_service_.resolve_free_board_move_json(fen, source, target);
}

std::string Core::board_promotion_options_json(
    const std::string& fen,
    const std::string& source,
    const std::string& target) {
  return game_library_service_.board_promotion_options_json(fen, source, target);
}

std::string Core::import_pgn_json(const std::string& pgn) {
  auto result = game_library_service_.import_pgn_json(pgn);
  classify_pending_openings(64);
  player_profile_service_.notify_profile_changed();
  return result;
}

std::string Core::import_fen_json(
    const std::string& fen, const std::string& display_name) {
  auto result = game_library_service_.import_fen_json(fen, display_name);
  classify_pending_openings(64);
  player_profile_service_.notify_profile_changed();
  return result;
}

std::string Core::start_provider_profile_json(
    const ProfileType type, const std::string& username) {
  return provider_service_.start_provider_profile_json(type, username);
}

std::string Core::start_scout_report_json(
    const ProfileType type, const std::string& username) {
  return provider_service_.start_scout_report_json(type, username);
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

std::string Core::statistics_overview_json() {
  return statistics_service_.overview_json();
}

std::string Core::statistics_accuracy_json(const std::string& time_control) {
  if (const auto profile = database_.active_profile();
      profile && !analysis_service_.has_active_work()) {
    // Make the first phase rows available promptly when a user opens
    // Statistics, then let the existing profile worker finish older cache rows.
    analysis_service_.refresh_cached_accuracy_for_statistics(profile->id, 4);
  }
  return statistics_service_.accuracy_json(time_control);
}

void Core::start_background_analysis() {
  // PlayerProfileService is started during native initialization. Keep this
  // compatibility endpoint for older Flutter clients without a second worker.
  player_profile_service_.start();
}

std::string Core::background_analysis_status_json() {
  const bool enabled = database_.app_setting("backgroundAnalysisEnabled").value_or("true") == "true";
  const auto profile = database_.active_profile();
  if (!profile) {
    return nlohmann::json{{"enabled", enabled}, {"state", "noProfile"},
                          {"analysedGames", 0}, {"totalGames", 0},
                          {"sourceRevision", database_.statistics_source_revision()}}.dump();
  }
  const auto owner = database_.player_profile_owner_id(profile->id);
  const auto progress = database_.ai_profile_queue_progress(owner);
  const auto history = provider_service_.player_profile_history_progress(owner);
  const bool complete = history.complete && progress.relevant > 0 &&
      progress.resolved_relevant >= progress.relevant &&
      progress.queued == 0 && progress.engine_pending == 0;
  return nlohmann::json{{"enabled", enabled},
                        {"state", !enabled ? "disabled" : complete ? "complete" : "running"},
                        {"analysedGames", progress.resolved_relevant},
                        {"totalGames", progress.relevant},
                        {"sourceRevision", database_.statistics_source_revision()}}.dump();
}

void Core::set_background_analysis_enabled(const bool enabled) {
  database_.set_setting("backgroundAnalysisEnabled", enabled ? "true" : "false");
  if (enabled) player_profile_service_.notify_profile_changed();
  else player_profile_service_.pause_engine_work();
}

std::string Core::statistics_openings_json(const std::string& time_control) {
  // This is an explicit user request for opening statistics, so finish the
  // remaining local backfill now instead of waiting for another app launch.
  classify_pending_openings(0);
  return statistics_service_.openings_json(time_control);
}

std::string Core::statistics_terminations_json() {
  return statistics_service_.terminations_json();
}

std::string Core::statistics_phases_json() {
  return statistics_service_.phases_json();
}

std::string Core::statistics_timeline_json(const std::string& query_json) {
  return statistics_service_.timeline_json(game_library_service_.query_games_json(query_json));
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
  player_profile_service_.notify_profile_changed();
}

void Core::clear_cached_month(
    const std::string& profile_id, const std::string& month) {
  game_library_service_.clear_cached_month(profile_id, month);
  player_profile_service_.notify_profile_changed();
}

std::string Core::start_analysis_json(const std::string& game_id) {
  player_profile_service_.pause_engine_work();
  return analysis_service_.start_analysis_json(game_id);
}

std::string Core::analysis_status_json(const std::string& game_id) {
  return analysis_service_.analysis_status_json(game_id);
}

std::string Core::move_analysis_status_json(
    const std::string& game_id, const int ply) {
  return analysis_service_.move_analysis_status_json(game_id, ply);
}

std::string Core::start_move_refinement_json(
    const std::string& game_id, const int ply) {
  player_profile_service_.pause_engine_work();
  return analysis_service_.start_move_refinement_json(game_id, ply);
}

void Core::cancel_analysis(const std::string& game_id) {
  analysis_service_.cancel_analysis(game_id);
}

void Core::delete_analysis(const std::string& game_id) {
  analysis_service_.delete_analysis(game_id);
  player_profile_service_.notify_profile_changed();
}

void Core::clear_engine_cache() {
  analysis_service_.clear_engine_cache();
}

std::string Core::start_variation_analysis_json(
    const std::string& fen, const std::string& uci) {
  player_profile_service_.pause_engine_work();
  return analysis_service_.start_variation_analysis_json(fen, uci);
}

std::string Core::start_variation_analysis_with_settings_json(
    const std::string& fen,
    const std::string& uci,
    const int depth,
    const int multi_pv,
    const int threads,
    const int hash_mb) {
  player_profile_service_.pause_engine_work();
  return analysis_service_.start_variation_analysis_with_settings_json(
      fen, uci, depth, multi_pv, threads, hash_mb);
}

std::string Core::variation_analysis_status_json(const std::string& job_id) {
  return analysis_service_.variation_analysis_status_json(job_id);
}

void Core::cancel_variation_analysis(const std::string& job_id) {
  analysis_service_.cancel_variation_analysis(job_id);
}

// -----------------------------------------------------------------------------
// Section: Local bot play orchestration
// -----------------------------------------------------------------------------

std::string Core::create_bot_game_json(
    const int requested_elo, const std::string& player_color) {
  return bot_service_.create_game_json(requested_elo, player_color);
}

std::string Core::active_bot_game_json() const {
  return bot_service_.active_game_json();
}

std::string Core::bot_game_json(const std::string& game_id) const {
  validate_token(game_id, "bot game id");
  return bot_service_.game_json(game_id);
}

std::string Core::bot_games_json() const {
  return bot_service_.games_json();
}

std::string Core::bot_game_analysis_game_json(const std::string& game_id) {
  validate_token(game_id, "bot game id");
  const auto bot_game = database_.bot_game(game_id);
  if (!bot_game.has_value()) throw std::invalid_argument("Bot game not found");
  if (bot_game->status == "active") {
    throw std::runtime_error("Finish the bot game before opening analysis");
  }
  if (bot_game->moves.empty()) {
    throw std::runtime_error("Bot game has no moves to analyse");
  }

  if (bot_game->analysis_game_id.has_value()) {
    if (database_.game(*bot_game->analysis_game_id).has_value()) {
      classify_game_opening(*bot_game->analysis_game_id);
      return game_library_service_.game_json(*bot_game->analysis_game_id);
    }
  }

  const auto imported = nlohmann::json::parse(
      game_library_service_.import_pgn_json(bot_service_.analysis_pgn(game_id)));
  const auto analysis_game_id = imported.at("id").get<std::string>();
  database_.set_bot_game_analysis_game_id(game_id, analysis_game_id);
  classify_game_opening(analysis_game_id);
  player_profile_service_.notify_profile_changed();
  return game_library_service_.game_json(analysis_game_id);
}

std::string Core::record_bot_game_move_json(
    const std::string& game_id,
    const std::string& expected_fen_before,
    const std::string& uci) {
  validate_token(game_id, "bot game id");
  return bot_service_.record_game_move_json(game_id, expected_fen_before, uci);
}

std::string Core::record_bot_game_move_from_ply_json(
    const std::string& game_id,
    const int base_ply,
    const std::string& expected_fen_before,
    const std::string& uci) {
  validate_token(game_id, "bot game id");
  return bot_service_.record_game_move_from_ply_json(
      game_id, base_ply, expected_fen_before, uci);
}

void Core::resign_bot_game(const std::string& game_id) {
  validate_token(game_id, "bot game id");
  bot_service_.resign_game(game_id);
}

void Core::abort_bot_game(const std::string& game_id) {
  validate_token(game_id, "bot game id");
  bot_service_.abort_game(game_id);
}

void Core::delete_bot_game(const std::string& game_id) {
  validate_token(game_id, "bot game id");
  bot_service_.delete_game(game_id);
}

void Core::set_bot_game_show_eval_bar(
    const std::string& game_id, const bool enabled) {
  validate_token(game_id, "bot game id");
  bot_service_.set_game_show_eval_bar(game_id, enabled);
}

std::string Core::start_bot_move_json(
    const std::string& fen, const int requested_elo) {
  return bot_service_.start_move_json(fen, requested_elo);
}

std::string Core::bot_move_status_json(const std::string& job_id) {
  validate_token(job_id, "bot move job id");
  return bot_service_.move_status_json(job_id);
}

void Core::cancel_bot_move(const std::string& job_id) {
  validate_token(job_id, "bot move job id");
  bot_service_.cancel_move(job_id);
}

// -----------------------------------------------------------------------------
// Section: Native training facade
// -----------------------------------------------------------------------------

std::string Core::coach_ask_json(const std::string& request_json) {
  return coach_service_.ask_json(request_json);
}

std::string Core::coach_performance_diagnostics_json() const {
  return coach_service_.performance_diagnostics_json();
}

std::string Core::coach_context_json(const std::string& request_json) {
  return coach_service_.context_json(request_json);
}

std::string Core::coach_automatic_json(const std::string& request_json) {
  return coach_service_.automatic_json(request_json);
}

std::string Core::start_coach_ask_json(const std::string& request_json) {
  return coach_service_.start_ask_json(request_json);
}

std::string Core::start_coach_automatic_json(const std::string& request_json) {
  return coach_service_.start_automatic_json(request_json);
}

std::string Core::start_coach_hint_json(const std::string& request_json) {
  return coach_service_.start_hint_json(request_json);
}

std::string Core::coach_job_status_json(const std::string& job_id) {
  validate_token(job_id, "coach job id");
  return coach_service_.job_status_json(job_id);
}

std::string Core::coach_sessions_json(const std::string& profile_id) const {
  validate_token(profile_id, "profile id");
  return coach_service_.sessions_json(profile_id);
}

std::string Core::create_coach_session_json(const std::string& profile_id) {
  validate_token(profile_id, "profile id");
  return coach_service_.create_session_json(profile_id);
}

std::string Core::coach_session_messages_json(
    const std::string& request_json) const {
  return coach_service_.session_messages_json(request_json);
}

std::string Core::rename_coach_session_json(const std::string& request_json) {
  return coach_service_.rename_session_json(request_json);
}

std::string Core::delete_coach_session_json(const std::string& request_json) {
  return coach_service_.delete_session_json(request_json);
}

void Core::cancel_coach_job(const std::string& job_id) {
  validate_token(job_id, "coach job id");
  coach_service_.cancel_job(job_id);
}

std::string Core::knowledge_inspector_json(const std::string& request_json) {
  const auto inspector_payload = knowledge_runtime_.inspector_json(request_json);
  auto inspector = nlohmann::json::parse(inspector_payload, nullptr, false);
  if (inspector.is_discarded() || !inspector.is_object()) {
    return inspector_payload;
  }

  // The Graph Inspector is also the developer-facing live background
  // diagnostic surface. Attach the authoritative native profile-preparation
  // snapshot even when KnowledgeRuntime itself is busy, so `status=busy` says
  // what work is occupying the app instead of being an opaque lock state.
  const auto background_payload = player_profile_service_.diagnostics_json();
  auto background = nlohmann::json::parse(background_payload, nullptr, false);
  if (!background.is_discarded() && background.is_object()) {
    inspector["profileBackground"] = background;
    inspector["samplingGuard"] = {
        {"historicalHardCap", background.value("samplingMaximumGames", 0)},
        {"recommendedSample", background.value("historicalSampleBudget", 0)},
        {"selectedHistoricalSample", background.value("historicalSampleGames", 0)},
        {"eligibleHistoricalGames", background.value("samplingEligibleGames", 0)},
        {"excludedShortGames", background.value("samplingExcludedGames", 0)},
        {"minimumEligiblePlies", background.value("samplingMinimumPlies", 0)},
        {"coverage", background.value("samplingCoverage", 0.0)},
        {"capped", background.value("samplingCapped", false)},
        {"initialPreparationComplete",
         background.value("initialPreparationComplete", false)},
        {"postBootstrapLearningMayExceedHistoricalCap", true},
    };
  }
  const auto startup_diagnostics = nlohmann::json::parse(
      startup_diagnostics_json_, nullptr, false);
  if (!startup_diagnostics.is_discarded() && startup_diagnostics.is_object()) {
    inspector["coreStartup"] = startup_diagnostics;
  }

  const auto analysis_performance = nlohmann::json::parse(
      analysis_service_.performance_diagnostics_json(), nullptr, false);
  if (!analysis_performance.is_discarded() && analysis_performance.is_object()) {
    inspector["analysisPerformance"] = analysis_performance;
  }
  const auto statistics_performance = nlohmann::json::parse(
      statistics_service_.performance_diagnostics_json(), nullptr, false);
  if (!statistics_performance.is_discarded() && statistics_performance.is_object()) {
    inspector["statisticsPerformance"] = statistics_performance;
  }
  const auto coach_performance = nlohmann::json::parse(
      coach_service_.performance_diagnostics_json(), nullptr, false);
  if (!coach_performance.is_discarded() && coach_performance.is_object()) {
    inspector["coachPerformance"] = coach_performance;
  }

  inspector["processRuntime"] = process_runtime_diagnostics_json();

  const auto sqlite_priority = persistence::sqlite_write_priority_gate().snapshot();
  inspector["sqliteWritePriority"] = {
      {"schema", "sqlite.write_priority.v1"},
      {"foregroundWaiters", sqlite_priority.foreground_waiters},
      {"foregroundSessions", sqlite_priority.foreground_sessions},
      {"backgroundWriters", sqlite_priority.background_writers},
      {"foregroundWaitCount", sqlite_priority.foreground_wait_count},
      {"foregroundWaitTotalMs", sqlite_priority.foreground_wait_total_ms},
      {"backgroundWaitCount", sqlite_priority.background_wait_count},
      {"backgroundWaitTotalMs", sqlite_priority.background_wait_total_ms},
  };
  return inspector.dump();
}

std::string Core::training_overview_json() const {
  return practice_service_.overview(training_service_.overview_json());
}

std::string Core::practice_command_json(const std::string& request) {
  return practice_service_.command(request);
}

std::string Core::start_training_attempt_json(const std::string& exercise_id) {
  validate_token(exercise_id, "training exercise id");
  return training_service_.start_attempt_json(exercise_id);
}

std::string Core::play_training_move_json(
    const std::string& attempt_id,
    const std::string& source,
    const std::string& target) {
  validate_token(attempt_id, "training attempt id");
  return training_service_.play_move_json(attempt_id, source, target);
}

}  // namespace kchess
