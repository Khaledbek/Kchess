#include "services/analysis_service.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <unordered_set>
#include <vector>

#include <nlohmann/json.hpp>

#include "analysis/accuracy.h"
#include "analysis/move_classifier.h"
#include "chess/move.h"
#include "core/settings_registry.h"
#include "diagnostics/logger.h"

namespace kchess {
namespace {

std::int64_t unix_time_seconds() {
  return std::chrono::duration_cast<std::chrono::seconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
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

void append_optional_int(std::ostringstream& json, const std::optional<int>& value) {
  if (value.has_value()) json << *value;
  else json << "null";
}

void append_optional_double(
    std::ostringstream& json, const std::optional<double>& value) {
  if (value.has_value()) json << std::fixed << std::setprecision(6) << *value;
  else json << "null";
}

std::string lowercase(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char character) {
    return static_cast<char>(std::tolower(character));
  });
  return value;
}

PositionEvaluation evaluation_of(const EngineLine& line) {
  return {
      .wdl = line.wdl,
      .evaluation_cp = line.evaluation_cp,
      .mate_in = line.mate_in,
  };
}

std::string fnv1a_hex(const std::string& value) {
  std::uint64_t hash = 14695981039346656037ULL;
  for (const unsigned char character : value) {
    hash ^= character;
    hash *= 1099511628211ULL;
  }
  std::ostringstream result;
  result << std::hex << std::setw(16) << std::setfill('0') << hash;
  return result.str();
}

std::string position_cache_engine_identity(const std::string& engine_version) {
  // A cache entry is only reusable with the exact embedded NNUE pair. The
  // filenames contain the official network hashes, so replacing a network
  // naturally creates a new cache namespace instead of reusing stale scores.
  return engine_version
      + "|nnue=nn-c288c895ea92.nnue+nn-37f18f62d772.nnue";
}

std::string canonical_position_cache_fen(const std::string& fen) {
  // The fullmove counter does not affect the legal position or Stockfish's
  // search. Keep the halfmove clock because it affects the fifty-move rule.
  std::istringstream input(fen);
  std::vector<std::string> fields;
  std::string field;
  while (input >> field) fields.push_back(field);
  if (fields.size() < 5) return fen;
  std::ostringstream result;
  for (std::size_t index = 0; index < 5; ++index) {
    if (index != 0) result << ' ';
    result << fields[index];
  }
  return result.str();
}

}  // namespace

AnalysisService::AnalysisService(
    Database& database, OpeningTheoryProvider& opening_theory)
    : database_(database), opening_theory_(&opening_theory) {}

AnalysisService::~AnalysisService() {
  std::vector<std::shared_ptr<AnalysisJob>> analysis_jobs;
  {
    std::lock_guard lock(jobs_mutex_);
    for (const auto& [game_id, job] : jobs_) {
      (void)game_id;
      job->cancel_requested = true;
      job->engine->cancel();
      analysis_jobs.push_back(job);
    }
  }
  for (const auto& job : analysis_jobs) {
    if (job->worker.joinable()) job->worker.join();
  }

  std::vector<std::shared_ptr<VariationJob>> variation_jobs;
  {
    std::lock_guard lock(variation_jobs_mutex_);
    for (const auto& [id, job] : variation_jobs_) {
      (void)id;
      job->engine->cancel();
      variation_jobs.push_back(job);
    }
  }
  for (const auto& job : variation_jobs) {
    if (job->worker.joinable()) job->worker.join();
  }
}

void AnalysisService::set_opening_theory_provider(
    OpeningTheoryProvider& opening_theory) noexcept {
  opening_theory_ = &opening_theory;
}

void AnalysisService::clear_engine_cache() {
  database_.clear_global_position_cache();
  diagnostics::info("cache", "global position cache cleared");
}

void AnalysisService::cancel_jobs_for_games(
    const std::vector<std::string>& game_ids) {
  std::vector<std::shared_ptr<AnalysisJob>> cancelled;
  {
    std::lock_guard lock(jobs_mutex_);
    for (const auto& game_id : game_ids) {
      const auto found = jobs_.find(game_id);
      if (found == jobs_.end()) continue;
      found->second->cancel_requested = true;
      found->second->engine->cancel();
      cancelled.push_back(found->second);
      jobs_.erase(found);
    }
  }
  for (const auto& job : cancelled) {
    if (job->worker.joinable()) job->worker.join();
  }
}

AnalysisRequest AnalysisService::analysis_request(
    const std::string& fen, const AppSettings& settings) const {
  return AnalysisRequest{
      .fen = fen,
      .depth = settings.depth,
      .multi_pv = settings.multi_pv,
      .threads = settings.threads,
      .hash_mb = settings.hash_mb,
      .time_limit_seconds = settings.time_limit_seconds,
  };
}

std::string AnalysisService::analysis_config_hash(const AppSettings& settings) const {
  StockfishEngine engine;
  // Classifier/Accuracy/Book versions remain independent from the engine
  // settings and are checked by rebuild_classification().
  std::string config = engine.version();
  // Only settings marked cache_relevant participate. Visual/app-only settings
  // therefore never force an expensive Stockfish re-analysis.
  for (const auto& descriptor : kSettingsRegistry) {
    if (!descriptor.cache_relevant) continue;
    config += "|" + std::string(descriptor.cache_token) + "=";
    if (descriptor.key == kDepthSetting.key) {
      config += std::to_string(settings.depth);
    } else if (descriptor.key == kMultiPvSetting.key) {
      config += std::to_string(settings.multi_pv);
    } else if (descriptor.key == kTimeLimitSetting.key) {
      config += std::to_string(settings.time_limit_seconds);
    } else if (descriptor.key == kThreadsSetting.key) {
      config += std::to_string(settings.threads);
    } else if (descriptor.key == kHashMbSetting.key) {
      config += std::to_string(settings.hash_mb);
    }
  }
  // Position model v2 stores the engine result for the position BEFORE a
  // move at the same public ply index, plus one synthetic final-position slot.
  // Including this token prevents legacy after-move caches from being reused.
  config += "|positionModel=before-after-v2";
  config += "|classification=unknown-phase2";
  return "phase7-" + fnv1a_hex(config);
}

std::optional<PersistedAnalysis> AnalysisService::reusable_analysis(
    const std::string& game_id,
    const AppSettings& settings,
    const int requested_ply) const {
  const auto exact_hash = analysis_config_hash(settings);
  if (auto exact = database_.analysis(game_id, exact_hash, requested_ply); exact.has_value()) {
    return exact;
  }
  StockfishEngine engine;
  return database_.compatible_analysis(game_id, engine.version(), settings, requested_ply);
}

const char* AnalysisService::job_state_name(const AnalysisJobState state) noexcept {
  switch (state) {
    case AnalysisJobState::queued: return "queued";
    case AnalysisJobState::running: return "running";
    case AnalysisJobState::cancelling: return "cancelling";
    case AnalysisJobState::cancelled: return "cancelled";
    case AnalysisJobState::completed: return "completed";
    case AnalysisJobState::failed: return "failed";
  }
  return "failed";
}

AnalysisService::AnalysisJobState AnalysisService::persisted_job_state(
    const PersistedAnalysis& analysis) noexcept {
  if (analysis.status == "complete") return AnalysisJobState::completed;
  if (analysis.status == "cancelled") return AnalysisJobState::cancelled;
  if (analysis.status == "error") return AnalysisJobState::failed;
  return AnalysisJobState::running;
}

std::string AnalysisService::analysis_json(
    const std::string& game_id, const PersistedAnalysis& analysis,
    const AnalysisJob* live_job) const {
  std::ostringstream json;
  const auto job_state = live_job != nullptr
      ? live_job->state.load()
      : persisted_job_state(analysis);
  const int live_current_slot = live_job != nullptr
      ? live_job->current_position_slot.load()
      : analysis.latest_ply;
  const int live_completed_moves = live_job != nullptr
      ? std::max(analysis.completed_plies, live_job->completed_moves.load())
      : analysis.completed_plies;

  json << "{\"gameId\":\"" << escape_json(game_id) << "\",\"status\":\""
       << escape_json(analysis.status) << "\",\"jobState\":\""
       << job_state_name(job_state) << "\",\"completedPlies\":"
       << live_completed_moves << ",\"totalPlies\":" << analysis.total_plies
       << ",\"progress\":";
  if (analysis.total_plies <= 0) json << '0';
  else json << std::fixed << std::setprecision(3)
            << static_cast<double>(live_completed_moves) / analysis.total_plies;
  json << ",\"currentPly\":" << live_current_slot
       << ",\"bestMove\":\"" << escape_json(analysis.best_move)
       << "\",\"engineVersion\":\"" << escape_json(analysis.engine_version)
       << "\",\"configHash\":\"" << escape_json(analysis.config_hash)
       << "\",\"error\":";
  if (analysis.error.empty()) json << "null";
  else json << '"' << escape_json(analysis.error) << '"';
  json << ",\"recommendedMove\":\"" << escape_json(analysis.recommended_move)
       << "\",\"classification\":";
  if (analysis.classification.has_value()) {
    json << '"' << move_category_name(*analysis.classification) << '"';
  } else {
    json << "null";
  }
  json << ",\"classifierVersion\":" << analysis.classifier_version
       << ",\"expectedScoreBefore\":";
  append_optional_double(json, analysis.expected_score_before);
  json << ",\"expectedScoreBest\":";
  append_optional_double(json, analysis.expected_score_best);
  json << ",\"expectedScorePlayed\":";
  append_optional_double(json, analysis.expected_score_played);
  json << ",\"expectedScoreLoss\":";
  append_optional_double(json, analysis.expected_score_loss);
  json << ",\"theory\":";
  if (analysis.theory.has_value()) {
    json << "{\"games\":" << analysis.theory->games
         << ",\"whiteWins\":" << analysis.theory->white_wins
         << ",\"draws\":" << analysis.theory->draws
         << ",\"blackWins\":" << analysis.theory->black_wins << '}';
  } else {
    json << "null";
  }
  json << ",\"lines\":[";
  const auto line_count = std::min(
      analysis.lines.size(), static_cast<std::size_t>(database_.settings().multi_pv));
  for (std::size_t index = 0; index < line_count; ++index) {
    if (index != 0) json << ',';
    const auto& line = analysis.lines[index];
    json << "{\"rank\":" << line.rank << ",\"depth\":" << line.depth
         << ",\"evaluationCp\":";
    append_optional_int(json, line.evaluation_cp);
    json << ",\"mateIn\":";
    append_optional_int(json, line.mate_in);
    json << ",\"wdl\":";
    if (line.wdl.has_value()) {
      json << "{\"wins\":" << line.wdl->wins << ",\"draws\":" << line.wdl->draws
           << ",\"losses\":" << line.wdl->losses << '}';
    } else {
      json << "null";
    }
    json << ",\"nodes\":" << line.nodes << ",\"moves\":[";
    for (std::size_t move_index = 0; move_index < line.moves.size(); ++move_index) {
      if (move_index != 0) json << ',';
      json << '"' << escape_json(line.moves[move_index]) << '"';
    }
    json << "]}";
  }
  json << ']';
  {
    const auto& summary = analysis.summary;
    std::string profile_side = "unknown";
    const auto profile = database_.active_profile();
    const auto game = database_.game(game_id);
    if (profile.has_value() && game.has_value()) {
      const auto display_name = lowercase(profile->display_name);
      const auto provider_name = profile->provider_username.has_value()
          ? lowercase(*profile->provider_username) : std::string{};
      const auto matches = [&](const std::string& player_name) {
        const auto normalized = lowercase(player_name);
        return normalized == display_name
            || (!provider_name.empty() && normalized == provider_name);
      };
      if (matches(game->white_name) && !matches(game->black_name)) {
        profile_side = "white";
      } else if (matches(game->black_name) && !matches(game->white_name)) {
        profile_side = "black";
      }
    }
    auto append_player = [&](const PlayerAnalysisSummary& player) {
      json << "{\"theory\":" << player.theory
           << ",\"brilliant\":" << player.brilliant << ",\"critical\":" << player.critical
           << ",\"best\":" << player.best
           << ",\"excellent\":" << player.excellent << ",\"okay\":" << player.okay
           << ",\"miss\":" << player.miss << ",\"mistake\":" << player.mistake
           << ",\"blunder\":" << player.blunder << ",\"totalMoves\":"
           << player.total_moves << ",\"analyzedMoves\":" << player.analyzed_moves
           << ",\"localAccuracy\":";
      append_optional_double(json, player.local_accuracy);
      json << '}';
    };
    json << ",\"summary\":{\"profileSide\":\"" << profile_side
         << "\",\"classifierVersion\":" << summary.classifier_version
         << ",\"accuracyAlgorithmVersion\":" << summary.accuracy_algorithm_version
         << ",\"openingBookVersion\":\"" << escape_json(summary.opening_book_version)
         << "\",\"engineDepth\":" << summary.engine_depth
         << ",\"engineVersion\":\"" << escape_json(analysis.engine_version)
         << "\",\"white\":";
    append_player(summary.white);
    json << ",\"black\":";
    append_player(summary.black);
    json << '}';
  }
  json << '}';
  return json.str();
}

void AnalysisService::rebuild_classification(
    const std::string& game_id,
    const std::string& config_hash,
    const bool force,
    const int through_ply) {
  const std::string book_version = opening_theory_->source_version();
  if (!force && database_.classification_is_current(
          game_id, config_hash, MoveClassifierConfig::version,
          AccuracyConfig::version, book_version)) {
    return;
  }
  const auto game = database_.game(game_id);
  if (!game.has_value()) throw std::runtime_error("Game not found during classification");
  std::vector<MoveClassificationRecord> records;
  std::vector<AccuracyMove> white_moves;
  std::vector<AccuracyMove> black_moves;
  const auto book_metadata = opening_theory_->metadata();
  for (const auto& move : game->moves) {
    if (through_ply >= 0 && move.ply_index > through_ply) break;
    // Position slot i is the position before move i; slot i+1 is the
    // resulting position after that move.  This makes move 0 identical to
    // every other move and gives it a real Stockfish "before" evaluation.
    const auto before = database_.analysis(game_id, config_hash, move.ply_index);
    const auto after = database_.analysis(game_id, config_hash, move.ply_index + 1);
    TheoryMoveInfo theory;
    if (book_metadata.max_ply > 0
        && static_cast<std::uint32_t>(move.ply_index) < book_metadata.max_ply) {
      theory = opening_theory_->lookup(move.fen_before, move.uci);
    }
    std::optional<double> best_expected;
    std::optional<double> played_expected;
    std::optional<double> second_expected;
    bool played_is_best = false;
    bool missed_forced_mate = false;
    bool allowed_forced_mate = false;
    bool material_sacrifice = false;
    bool only_move_tactical = false;
    bool forces_nontrivial_mate = false;
    std::string recommended_move;
    const auto context = position_context(move.fen_before);
    if (before.has_value() && !before->lines.empty()) {
      recommended_move = before->best_move;
      played_is_best = move.uci == before->best_move;
      if (!recommended_move.empty()) {
        recommended_move = apply_legal_uci_move(
            move.fen_before, recommended_move).san;
      }
      best_expected = expected_score_side_to_move(evaluation_of(before->lines.front()));
      if (before->lines.size() > 1) {
        second_expected = expected_score_side_to_move(evaluation_of(before->lines[1]));
      }
      if (played_is_best) {
        material_sacrifice = material_sacrifice_in_pv(
            move.fen_before, before->lines.front().moves);
      }
      if (best_expected.has_value() && second_expected.has_value()) {
        only_move_tactical = *best_expected >= 0.55
            && *best_expected - *second_expected >= 0.20;
      }
      const bool best_has_mate = before->lines.front().mate_in.value_or(0) > 0;
      forces_nontrivial_mate = played_is_best
          && before->lines.front().mate_in.value_or(0) > 1;
      if (after.has_value() && !after->lines.empty()) {
        const int after_mate = after->lines.front().mate_in.value_or(0);
        missed_forced_mate = best_has_mate && after_mate >= 0;
        allowed_forced_mate = after_mate > 0
            && before->lines.front().mate_in.value_or(1) >= 0;
      }
    }
    if (after.has_value() && !after->lines.empty()) {
      played_expected = expected_score_mover_after_move(evaluation_of(after->lines.front()));
    } else if (after.has_value()) {
      // A terminal position legitimately has no PV.  Score it directly from
      // the board state so the final mating/stalemating move can still be
      // classified instead of becoming unknown.
      const auto after_context = position_context(move.fen_after);
      if (after_context.legal_move_count == 0) {
        played_expected = after_context.in_check ? 1.0 : 0.5;
      }
    }
    const auto loss = expected_score_loss(best_expected, played_expected);
    const auto category = classify_move({
        .theory = theory.is_theory,
        .played_is_best = played_is_best,
        .material_sacrifice = material_sacrifice,
        .only_move_tactical = only_move_tactical,
        .missed_forced_mate = missed_forced_mate,
        .allowed_forced_mate = allowed_forced_mate,
        .was_in_check_before_move = context.in_check,
        .forces_nontrivial_mate = forces_nontrivial_mate,
        .legal_move_count = context.legal_move_count,
        .best_expected_score = best_expected,
        .played_expected_score = played_expected,
        .second_best_expected_score = second_expected,
    });
    records.push_back({
        .ply = move.ply_index,
        .classification = category,
        .classifier_version = MoveClassifierConfig::version,
        .expected_score_before = best_expected,
        .expected_score_best = best_expected,
        .expected_score_played = played_expected,
        .expected_score_loss = loss,
        .recommended_move = recommended_move,
        .theory = theory,
    });
    {
      std::ostringstream log_message;
      log_message << "game=" << game_id << " ply=" << move.ply_index
                  << " category=" << move_category_name(category);
      if (loss.has_value()) log_message << " loss=" << std::fixed << std::setprecision(4) << *loss;
      diagnostics::debug("classifier", log_message.str());
    }
    auto& accuracy_moves = move.side_to_move == "black" ? black_moves : white_moves;
    accuracy_moves.push_back({.category = category, .loss = loss});
  }
  database_.persist_classifications(
      game_id, config_hash, records, game_accuracy(white_moves), game_accuracy(black_moves),
      MoveClassifierConfig::version, AccuracyConfig::version, book_version,
      through_ply < 0);
}

void AnalysisService::run_analysis(
    const std::string& game_id,
    const std::string& config_hash,
    AppSettings settings,
    std::vector<std::string> positions,
    std::vector<int> completed_position_slots,
    const std::shared_ptr<AnalysisJob>& job) noexcept {
  try {
    if (job->cancel_requested) {
      job->state = AnalysisJobState::cancelled;
      database_.set_analysis_status(game_id, config_hash, "cancelled");
      job->finished = true;
      return;
    }
    job->state = AnalysisJobState::running;
    {
      std::ostringstream log_message;
      log_message << "game=" << game_id << " started positions=" << positions.size()
                  << " resumedSlots=" << completed_position_slots.size()
                  << " depth=" << settings.depth << " multipv=" << settings.multi_pv
                  << " time=" << settings.time_limit_seconds;
      diagnostics::info("analysis", log_message.str());
    }
    const std::string engine_version = job->engine->version();
    const std::string cache_engine_identity = position_cache_engine_identity(engine_version);
    bool engine_started = false;

    std::unordered_set<int> completed_slots(
        completed_position_slots.begin(), completed_position_slots.end());
    const bool is_fen_position_only = positions.size() == 1;

    auto contiguous_completed_moves = [&]() {
      if (is_fen_position_only) {
        return completed_slots.contains(0) ? 1 : 0;
      }
      // A PGN move i is fully analyzable only when both its pre-move slot i
      // and post-move slot i+1 exist. Therefore N completed moves require
      // the contiguous position prefix 0..N.
      int next_slot = 0;
      while (next_slot < static_cast<int>(positions.size())
             && completed_slots.contains(next_slot)) {
        ++next_slot;
      }
      return std::max(0, next_slot - 1);
    };

    job->completed_moves = contiguous_completed_moves();
    for (int ply = 0; ply < static_cast<int>(positions.size()); ++ply) {
      if (completed_slots.contains(ply)) continue;
      job->current_position_slot = ply;

      if (job->cancel_requested) {
        job->state = AnalysisJobState::cancelled;
        database_.set_analysis_status(game_id, config_hash, "cancelled");
        job->engine->stop();
        job->finished = true;
        return;
      }

      const std::string cache_fen = canonical_position_cache_fen(positions[ply]);
      std::optional<AnalysisResult> result;
      if (settings.use_global_analysis_cache) {
        result = database_.compatible_position_analysis(
            cache_fen, cache_engine_identity, settings);
      }
      if (result.has_value()) {
        diagnostics::debug(
            "cache", "game=" + game_id + " slot=" + std::to_string(ply) + " hit");
      } else {
        diagnostics::debug(
            "cache", "game=" + game_id + " slot=" + std::to_string(ply)
                + (settings.use_global_analysis_cache ? " miss" : " disabled"));
        if (!engine_started) {
          job->engine->start();
          job->engine->new_game();
          engine_started = true;
        }
        result = job->engine->analyze(analysis_request(positions[ply], settings));
        if (settings.use_global_analysis_cache) {
          database_.persist_position_analysis(
              cache_fen, cache_engine_identity, settings, *result, unix_time_seconds());
        }
      }

      completed_slots.insert(ply);
      const int completed_moves = contiguous_completed_moves();
      job->completed_moves = completed_moves;
      database_.persist_engine_result(
          game_id, config_hash, ply, completed_moves, *result, unix_time_seconds());

      diagnostics::debug(
          "analysis", "game=" + game_id + " slot=" + std::to_string(ply)
              + " completedMoves=" + std::to_string(completed_moves));

      if (!is_fen_position_only && completed_moves > 0) {
        rebuild_classification(
            game_id, config_hash, true, completed_moves - 1);
      }
    }
    rebuild_classification(game_id, config_hash, true);
    database_.set_analysis_status(game_id, config_hash, "complete");
    job->state = AnalysisJobState::completed;
    diagnostics::info("analysis", "game=" + game_id + " completed");
    job->current_position_slot = static_cast<int>(positions.size()) - 1;
    job->engine->stop();
    job->finished = true;
  } catch (const std::exception& error) {
    const bool cancelled = job->cancel_requested;
    if (cancelled) {
      diagnostics::info("analysis", "game=" + game_id + " cancelled");
    } else {
      diagnostics::error("analysis", "game=" + game_id + " failed: " + error.what());
    }
    job->state = cancelled ? AnalysisJobState::cancelled : AnalysisJobState::failed;
    try {
      database_.set_analysis_status(
          game_id, config_hash, cancelled ? "cancelled" : "error",
          cancelled ? std::string{} : error.what());
    } catch (...) {
    }
    job->engine->stop();
    job->finished = true;
  } catch (...) {
    diagnostics::error("analysis", "game=" + game_id + " failed: unknown error");
    job->state = job->cancel_requested
        ? AnalysisJobState::cancelled
        : AnalysisJobState::failed;
    try {
      database_.set_analysis_status(game_id, config_hash, "error", "Unknown engine error");
    } catch (...) {
    }
    job->engine->stop();
    job->finished = true;
  }
}

std::string AnalysisService::start_analysis_json(const std::string& game_id) {
  validate_token(game_id, "game id");
  const auto game = database_.game(game_id);
  if (!game.has_value()) throw std::runtime_error("Game not found");
  std::vector<std::string> positions;
  if (game->kind == "fen") {
    positions.push_back(game->starting_fen);
  } else {
    // Slot 0 is the true position before the first move.  Each following
    // slot is the result after one played half-move, so move i is always
    // evaluated from slots i -> i+1.
    positions.reserve(game->moves.size() + 1);
    positions.push_back(game->starting_fen);
    for (const auto& move : game->moves) positions.push_back(move.fen_after);
  }
  if (positions.empty()) throw std::invalid_argument("Game has no analyzable positions");

  const auto settings = database_.settings();
  StockfishEngine version_probe;
  const auto config_hash = analysis_config_hash(settings);
  if (auto reusable = reusable_analysis(game_id, settings);
      reusable.has_value() && reusable->status == "complete") {
    diagnostics::info("cache", "game=" + game_id + " complete analysis reused");
    rebuild_classification(game_id, reusable->config_hash);
    return analysis_json(game_id, database_.analysis(
        game_id, reusable->config_hash).value());
  }
  version_probe.validate_available();
  const int public_total_plies = game->kind == "fen"
      ? 1
      : static_cast<int>(game->moves.size());
  auto persisted = database_.prepare_analysis(
      game_id, config_hash, version_probe.version(), public_total_plies,
      settings.depth, settings.multi_pv, settings.time_limit_seconds);
  if (persisted.status == "complete") {
    rebuild_classification(game_id, config_hash);
    return analysis_json(game_id, database_.analysis(game_id, config_hash).value());
  }

  {
    std::lock_guard lock(jobs_mutex_);
    auto existing = jobs_.find(game_id);
    if (existing != jobs_.end()
        && (existing->second->finished || existing->second->config_hash != config_hash)) {
      if (!existing->second->finished) {
        existing->second->cancel_requested = true;
        existing->second->engine->cancel();
      }
      if (existing->second->worker.joinable()) existing->second->worker.join();
      jobs_.erase(existing);
      existing = jobs_.end();
    }
    if (existing == jobs_.end()) {
      auto job = std::make_shared<AnalysisJob>();
      job->engine = std::make_shared<StockfishEngine>();
      job->config_hash = config_hash;
      // Resume is position-exact, not progress-counter based. Persisted
      // engine rows survive cancellation/errors, and only missing position
      // slots are sent to Stockfish on the next start.
      auto completed_position_slots =
          database_.analyzed_position_slots(game_id, config_hash);
      if (!completed_position_slots.empty()) {
        diagnostics::info(
            "analysis", "game=" + game_id + " resuming cachedSlots="
                + std::to_string(completed_position_slots.size()));
      }
      job->worker = std::thread(
          [this, game_id, config_hash, settings, positions = std::move(positions),
           completed_position_slots = std::move(completed_position_slots), job]() mutable {
            run_analysis(
                game_id, config_hash, settings, std::move(positions),
                std::move(completed_position_slots), job);
          });
      jobs_.emplace(game_id, std::move(job));
    }
  }
  return analysis_json(game_id, persisted);
}

std::string AnalysisService::analysis_status_json(const std::string& game_id) {
  validate_token(game_id, "game id");
  std::optional<PersistedAnalysis> result;
  std::shared_ptr<AnalysisJob> live_job;
  {
    std::lock_guard lock(jobs_mutex_);
    const auto job = jobs_.find(game_id);
    if (job != jobs_.end()) {
      live_job = job->second;
      result = database_.analysis(game_id, job->second->config_hash);
    }
  }
  if (!result.has_value()) result = reusable_analysis(game_id, database_.settings());
  if (!result.has_value()) throw std::runtime_error("Analysis job not found");
  return analysis_json(game_id, *result, live_job.get());
}

std::string AnalysisService::move_analysis_status_json(const std::string& game_id, const int ply) {
  validate_token(game_id, "game id");
  if (ply < 0) throw std::invalid_argument("ply must be non-negative");
  std::optional<PersistedAnalysis> result;
  std::shared_ptr<AnalysisJob> live_job;
  {
    std::lock_guard lock(jobs_mutex_);
    const auto job = jobs_.find(game_id);
    if (job != jobs_.end()) {
      live_job = job->second;
      result = database_.analysis(game_id, job->second->config_hash, ply);
    }
  }
  if (!result.has_value()) {
    result = reusable_analysis(game_id, database_.settings(), ply);
  }
  if (!result.has_value()) throw std::runtime_error("Analysis job not found");
  return analysis_json(game_id, *result, live_job.get());
}

void AnalysisService::cancel_analysis(const std::string& game_id) {
  validate_token(game_id, "game id");
  std::shared_ptr<AnalysisJob> job;
  {
    std::lock_guard lock(jobs_mutex_);
    const auto found = jobs_.find(game_id);
    if (found == jobs_.end()) throw std::runtime_error("Analysis job not found");
    job = found->second;
  }
  job->state = AnalysisJobState::cancelling;
  job->cancel_requested = true;
  job->engine->cancel();
}


void AnalysisService::reap_finished_variation_jobs() {
  std::vector<std::shared_ptr<VariationJob>> stale_jobs;
  {
    std::lock_guard lock(variation_jobs_mutex_);
    for (auto iterator = variation_jobs_.begin(); iterator != variation_jobs_.end();) {
      if (iterator->second->finished) {
        stale_jobs.push_back(iterator->second);
        iterator = variation_jobs_.erase(iterator);
      } else {
        ++iterator;
      }
    }
  }
  for (const auto& stale : stale_jobs) {
    if (stale->worker.joinable()) stale->worker.join();
  }
}

std::string AnalysisService::start_variation_analysis_json(
    const std::string& fen, const std::string& uci) {
  return start_variation_job_json(fen, uci, database_.settings());
}

std::string AnalysisService::start_variation_analysis_with_settings_json(
    const std::string& fen,
    const std::string& uci,
    const int depth,
    const int multi_pv,
    const int threads,
    const int hash_mb) {
  if (depth < AppSettings::min_depth || depth > AppSettings::max_depth) {
    throw std::invalid_argument("depth must be between 1 and 64");
  }
  if (multi_pv < AppSettings::min_multi_pv ||
      multi_pv > AppSettings::max_multi_pv) {
    throw std::invalid_argument("multiPv must be between 1 and 8");
  }
  if (threads < kThreadsSetting.min_int || threads > kThreadsSetting.max_int) {
    throw std::invalid_argument("threads must be between 1 and 32");
  }
  if (hash_mb < kHashMbSetting.min_int || hash_mb > kHashMbSetting.max_int) {
    throw std::invalid_argument("hashMb must be between 16 and 2048");
  }

  auto settings = database_.settings();
  settings.depth = depth;
  settings.multi_pv = multi_pv;
  settings.threads = threads;
  settings.hash_mb = hash_mb;
  return start_variation_job_json(fen, uci, std::move(settings));
}

std::string AnalysisService::start_variation_job_json(
    const std::string& fen,
    const std::string& uci,
    AppSettings settings) {
  reap_finished_variation_jobs();

  const auto applied = apply_legal_uci_move(fen, uci);
  auto job = std::make_shared<VariationJob>();
  job->id = "variation-" + std::to_string(next_variation_job_id_++);
  job->played_move = applied.uci;
  job->played_san = applied.san;
  job->fen = applied.fen_after;
  job->engine = std::make_shared<StockfishEngine>();
  job->engine->validate_available();
  job->worker = std::thread([this, job, settings = std::move(settings)] {
    try {
      job->engine->start();
      job->engine->new_game();
      auto result = job->engine->analyze(analysis_request(job->fen, settings));
      {
        std::lock_guard lock(job->state_mutex);
        job->result = std::move(result);
        job->status = "complete";
      }
    } catch (const std::exception& error) {
      std::lock_guard lock(job->state_mutex);
      job->status = "error";
      job->error = error.what();
    } catch (...) {
      std::lock_guard lock(job->state_mutex);
      job->status = "error";
      job->error = "Unknown variation analysis error";
    }
    job->engine->stop();
    job->finished = true;
  });
  {
    std::lock_guard lock(variation_jobs_mutex_);
    variation_jobs_.emplace(job->id, job);
  }
  return variation_analysis_status_json(job->id);
}

std::string AnalysisService::variation_analysis_status_json(const std::string& job_id) {
  validate_token(job_id, "variation job id");
  std::shared_ptr<VariationJob> job;
  {
    std::lock_guard lock(variation_jobs_mutex_);
    const auto found = variation_jobs_.find(job_id);
    if (found == variation_jobs_.end()) throw std::runtime_error("Variation job not found");
    job = found->second;
  }
  std::lock_guard lock(job->state_mutex);
  nlohmann::json json{
      {"jobId", job->id},
      {"status", job->status},
      {"playedMove", job->played_move},
      {"playedSan", job->played_san},
      {"fen", job->fen},
      {"error", job->error.empty() ? nlohmann::json(nullptr) : nlohmann::json(job->error)},
      {"bestMove", job->result.best_move},
      {"moverEvaluationCp", nullptr},
      {"moverMateIn", nullptr},
      {"lines", nlohmann::json::array()},
  };
  if (!job->result.lines.empty()) {
    const auto& principal = job->result.lines.front();
    if (principal.evaluation_cp.has_value()) {
      json["moverEvaluationCp"] = -*principal.evaluation_cp;
    }
    if (principal.mate_in.has_value()) json["moverMateIn"] = -*principal.mate_in;
  }
  for (const auto& line : job->result.lines) {
    nlohmann::json line_json{
        {"rank", line.rank},
        {"depth", line.depth},
        {"evaluationCp", line.evaluation_cp.has_value()
            ? nlohmann::json(*line.evaluation_cp) : nlohmann::json(nullptr)},
        {"mateIn", line.mate_in.has_value()
            ? nlohmann::json(*line.mate_in) : nlohmann::json(nullptr)},
        {"nodes", line.nodes},
        {"moves", line.moves},
        {"wdl", nullptr},
    };
    if (line.wdl.has_value()) {
      line_json["wdl"] = {
          {"wins", line.wdl->wins},
          {"draws", line.wdl->draws},
          {"losses", line.wdl->losses},
      };
    }
    json["lines"].push_back(std::move(line_json));
  }
  return json.dump();
}

}  // namespace kchess
