#include "services/analysis_service.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <cmath>
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
#include "chess/position_view.h"
#include "core/settings_registry.h"
#include "diagnostics/logger.h"

namespace kchess {
namespace {

constexpr int kEngineThreadHardCap = 32;
constexpr int kLiveStableIterations = 3;
constexpr int kLiveStableEvalToleranceCp = 15;
constexpr int kPreanalysisStableIterations = 3;
constexpr int kPreanalysisStableEvalToleranceCp = 10;

constexpr double kPreanalysisBoundaryMargin = 0.02;
constexpr double kPreanalysisGapMargin = 0.04;

PositionEvaluation evaluation_of(const EngineLine& line);

bool near_threshold(const double value, const double threshold, const double margin) {
  return std::abs(value - threshold) <= margin;
}

bool difficult_preanalysis_position(
    const std::string& fen,
    const std::string& played_move,
    const AnalysisResult& result) {
  if (result.lines.empty()) return false;

  const MoveClassifierConfig classifier;
  const auto best_expected = expected_score_side_to_move(evaluation_of(result.lines.front()));
  std::optional<double> second_expected;
  if (result.lines.size() > 1) {
    second_expected = expected_score_side_to_move(evaluation_of(result.lines[1]));
  }

  const EngineLine* played_line = nullptr;
  for (const auto& line : result.lines) {
    if (line.best_move() == played_move) {
      played_line = &line;
      break;
    }
  }

  // Mate and verified sacrifices are exactly the positions where a shallow
  // convergence decision can damage Great/Brilliant/Miss classification.
  if (std::any_of(result.lines.begin(), result.lines.end(), [](const EngineLine& line) {
        return line.mate_in.has_value();
      })) {
    return true;
  }
  if (played_line != nullptr
      && material_sacrifice_in_pv(fen, played_line->moves, 8)) {
    return true;
  }

  if (!best_expected.has_value()) return false;

  if (second_expected.has_value()) {
    const double gap = std::max(0.0, *best_expected - *second_expected);
    if (gap >= classifier.brilliant_gap - kPreanalysisGapMargin
        || near_threshold(gap, classifier.critical_gap, kPreanalysisGapMargin)) {
      return true;
    }
  }
  if (result.lines.size() > 1
      && result.lines.front().evaluation_cp.has_value()
      && result.lines[1].evaluation_cp.has_value()) {
    const int cp_gap = std::max(
        0, *result.lines.front().evaluation_cp - *result.lines[1].evaluation_cp);
    if (cp_gap >= classifier.critical_cp_gap - 30) return true;
  }

  if (played_line == nullptr) {
    // The played move falling outside the scout candidates is already an
    // uncertainty signal: full configured MultiPV should verify it.
    return !played_move.empty();
  }

  const auto played_expected = expected_score_side_to_move(evaluation_of(*played_line));
  const auto loss = expected_score_loss(best_expected, played_expected);
  if (!loss.has_value()) return false;

  // Spend the full minimum budget near category boundaries, where a few
  // centipawns can change the user-visible label.
  return near_threshold(*loss, classifier.excellent_loss, kPreanalysisBoundaryMargin)
      || near_threshold(*loss, classifier.okay_loss, kPreanalysisBoundaryMargin)
      || near_threshold(*loss, classifier.miss_loss, kPreanalysisBoundaryMargin)
      || near_threshold(*loss, classifier.blunder_outcome_loss, kPreanalysisBoundaryMargin)
      || near_threshold(*loss, classifier.blunder_severe_loss, kPreanalysisBoundaryMargin);
}

int preanalysis_early_stop_min_depth(const AppSettings& settings) {
  // The user's minimum depth remains the hard ceiling for preparation. Quiet
  // positions may stop only after a substantial search and several stable
  // iterations. Depths below 12 are cheap enough that early-stop bookkeeping
  // is not worthwhile.
  if (settings.depth < 12) return settings.depth;
  return std::max(8, settings.depth - 6);
}

int maximum_worker_threads() {
  const unsigned hardware = std::thread::hardware_concurrency();
  if (hardware <= 1) return 1;
  // A single Stockfish worker may use at most half of the machine's logical
  // CPUs. The hard cap keeps the value inside Kchess' persisted setting range.
  const int half = std::max(1, static_cast<int>(hardware / 2U));
  return std::clamp(half, 1, kEngineThreadHardCap);
}

int live_refinement_threads(const AppSettings& settings) {
  return std::clamp(settings.threads, 1, maximum_worker_threads());
}

int live_early_stop_min_depth(const AppSettings& settings) {
  // Convergence needs one baseline iteration plus kLiveStableIterations
  // successful comparisons. Leave enough room for all of them to complete
  // strictly before the hard maximum depth. With the default 12..18 range,
  // this starts at depth 14, allowing stable depths 15, 16 and 17 to finish
  // the search early instead of making an early stop mathematically impossible.
  const int hard_max_depth = settings.depth;
  const int minimum_depth = std::min(settings.min_analysis_depth, hard_max_depth);
  const int latest_useful_start =
      hard_max_depth - kLiveStableIterations - 1;
  if (latest_useful_start < minimum_depth) {
    // There is not enough depth headroom to prove stability before max depth.
    // Starting at the maximum preserves the normal hard-depth behavior.
    return hard_max_depth;
  }

  const int preferred_start = std::max(16, settings.min_analysis_depth + 4);
  return std::clamp(preferred_start, minimum_depth, latest_useful_start);
}

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

std::string ranked_best_move(
    const std::vector<EngineLine>& lines, const std::string& fallback) {
  if (!lines.empty()) {
    const auto move = lines.front().best_move();
    if (!move.empty()) return move;
  }
  return fallback;
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

std::string position_cache_engine_identity(
    const std::string& engine_version, const bool adaptive_early_stop) {
  // A cache entry is only reusable with the exact embedded NNUE pair and the
  // same convergence policy. An adaptive search may intentionally stop below
  // the requested hard depth, so it must never satisfy a later strict search.
  // The filenames contain the official network hashes, so replacing a network
  // naturally creates a new cache namespace instead of reusing stale scores.
  return engine_version
      + "|nnue=nn-c288c895ea92.nnue+nn-37f18f62d772.nnue"
      + "|adaptive=" + (adaptive_early_stop ? "1" : "0");
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

std::string variation_position_cache_key(
    const std::string& fen, const AppSettings& settings) {
  // MultiPV is intentionally not part of the key. A wider result can satisfy
  // a later two-line classifier lookup; callers verify the available line
  // count before reusing it. Threads/hash do not change requested quality.
  return canonical_position_cache_fen(fen)
      + "|depth=" + std::to_string(settings.depth)
      + "|time=" + std::to_string(settings.time_limit_seconds);
}

MoveCategory classify_variation_move(
    const std::string& fen_before,
    const std::string& played_move,
    const std::string& fen_after,
    const AnalysisResult& before,
    const AnalysisResult& after,
    const TheoryMoveInfo& theory) {
  std::optional<double> best_expected;
  std::optional<double> played_expected;
  std::optional<double> second_expected;
  std::optional<int> best_cp;
  std::optional<int> played_cp;
  std::optional<int> second_cp;
  bool played_is_best = false;
  bool missed_forced_mate = false;
  bool allowed_forced_mate = false;
  bool material_sacrifice = false;
  bool only_move_tactical = false;
  bool forces_nontrivial_mate = false;
  const auto context = position_context(fen_before);

  if (!before.lines.empty()) {
    played_is_best = played_move == ranked_best_move(before.lines, before.best_move);
    best_expected = expected_score_side_to_move(evaluation_of(before.lines.front()));
    best_cp = before.lines.front().evaluation_cp;
    if (before.lines.size() > 1) {
      second_expected = expected_score_side_to_move(evaluation_of(before.lines[1]));
      second_cp = before.lines[1].evaluation_cp;
    }

    for (const auto& line : before.lines) {
      if (line.best_move() != played_move) continue;
      played_expected = expected_score_side_to_move(evaluation_of(line));
      played_cp = line.evaluation_cp;
      material_sacrifice = material_sacrifice_in_pv(fen_before, line.moves, 8);
      forces_nontrivial_mate = line.mate_in.has_value() && *line.mate_in > 1;
      break;
    }

    if (best_expected.has_value() && second_expected.has_value()) {
      const bool unique_by_expected = *best_expected - *second_expected >= 0.20;
      const bool unique_by_cp = best_cp.has_value() && second_cp.has_value()
          && *best_cp - *second_cp >= MoveClassifierConfig{}.critical_cp_gap;
      only_move_tactical = *best_expected >= 0.55
          && (unique_by_expected || unique_by_cp);
    }

    const auto best_mate = before.lines.front().mate_in;
    const bool best_has_mate = best_mate.has_value() && *best_mate > 0;
    if (!after.lines.empty()) {
      const auto after_mate = after.lines.front().mate_in;
      forces_nontrivial_mate = forces_nontrivial_mate
          || (after_mate.has_value() && *after_mate < -1);
      if (!material_sacrifice) {
        std::vector<std::string> played_pv;
        played_pv.reserve(after.lines.front().moves.size() + 1);
        played_pv.push_back(played_move);
        played_pv.insert(
            played_pv.end(), after.lines.front().moves.begin(), after.lines.front().moves.end());
        material_sacrifice = material_sacrifice_in_pv(fen_before, played_pv, 8);
      }
      missed_forced_mate = best_has_mate
          && (!after_mate.has_value() || *after_mate > 0);
      allowed_forced_mate = after_mate.has_value() && *after_mate > 0
          && (!best_mate.has_value() || *best_mate >= 0);
    }
  }

  if (!after.lines.empty()) {
    if (!played_expected.has_value()) {
      played_expected = expected_score_mover_after_move(evaluation_of(after.lines.front()));
    }
    if (!played_cp.has_value() && after.lines.front().evaluation_cp.has_value()) {
      played_cp = -*after.lines.front().evaluation_cp;
    }
  } else {
    const auto after_context = position_context(fen_after);
    if (after_context.legal_move_count == 0) {
      played_expected = after_context.in_check ? 1.0 : 0.5;
    }
  }

  return classify_move({
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
      .best_evaluation_cp = best_cp,
      .played_evaluation_cp = played_cp,
      .second_best_evaluation_cp = second_cp,
  });
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

  std::vector<std::shared_ptr<AnalysisJob>> refinement_jobs;
  {
    std::lock_guard lock(refinement_jobs_mutex_);
    for (const auto& [game_id, job] : refinement_jobs_) {
      (void)game_id;
      job->cancel_requested = true;
      job->engine->cancel();
      refinement_jobs.push_back(job);
    }
  }
  for (const auto& job : refinement_jobs) {
    if (job->worker.joinable()) job->worker.join();
  }

  stop_all_variation_jobs(true);
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

  std::vector<std::shared_ptr<AnalysisJob>> refinements;
  {
    std::lock_guard lock(refinement_jobs_mutex_);
    for (const auto& game_id : game_ids) {
      const auto found = refinement_jobs_.find(game_id);
      if (found == refinement_jobs_.end()) continue;
      found->second->cancel_requested = true;
      found->second->engine->cancel();
      refinements.push_back(found->second);
      refinement_jobs_.erase(found);
    }
  }
  for (const auto& job : refinements) {
    if (job->worker.joinable()) job->worker.join();
  }
}

AppSettings AnalysisService::preanalysis_budget_settings() const {
  auto budget = database_.settings();
  // min_analysis_depth is the maximum depth of the preparation pass.  Do not
  // copy the live maximum into budget.depth.  MultiPV, time, threads and hash
  // remain exactly as configured by the user.
  budget.depth = std::clamp(
      budget.min_analysis_depth, AppSettings::min_depth, AppSettings::max_depth);
  budget.threads = std::clamp(budget.threads, 1, maximum_worker_threads());
  return budget;
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
      .search_moves = {},
  };
}

AnalysisRequest AnalysisService::preanalysis_request(
    const std::string& fen, const AppSettings& budget) const {
  // Kept separate from live refinement on purpose.  Later scheduler stages may
  // choose cheaper per-position requests, but no pre-analysis request may ever
  // exceed this user-selected budget.
  return analysis_request(fen, budget);
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
    } else if (descriptor.key == kAdaptiveEarlyStopSetting.key) {
      config += settings.adaptive_early_stop ? "1" : "0";
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
  // A deeper/higher-quality saved run is authoritative for lower requests.
  // Resource-only changes (threads/hash) must not duplicate a game's saved
  // analysis.  Database compatibility still enforces engine version, depth,
  // MultiPV, time budget and strict-vs-adaptive semantics.
  StockfishEngine engine;
  return database_.compatible_analysis(
      game_id, engine.version(), settings, requested_ply);
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

PersistedAnalysis AnalysisService::stable_live_classification_snapshot(
    const std::string& game_id,
    const int ply,
    PersistedAnalysis analysis,
    const AnalysisJob* live_job) const {
  // Maximum-depth engine rows are persisted position-by-position so the eval
  // bar/PV can update live.  Classifications, however, are a game-wide
  // coherent snapshot because adjacent positions affect each move.  Until
  // the refinement queue finishes and rebuild_classification() commits the
  // complete maximum-depth snapshot, keep showing the last published
  // pre-analysis classification instead of exposing partial/null categories.
  if (live_job == nullptr
      || live_job->state.load() == AnalysisJobState::completed
      || live_job->published_classification_config_hash.empty()) {
    return analysis;
  }

  const auto published = database_.analysis(
      game_id, live_job->published_classification_config_hash, ply);
  if (!published.has_value()) return analysis;

  analysis.classification = published->classification;
  analysis.classifier_version = published->classifier_version;
  analysis.recommended_move = published->recommended_move;
  analysis.expected_score_before = published->expected_score_before;
  analysis.expected_score_best = published->expected_score_best;
  analysis.expected_score_played = published->expected_score_played;
  analysis.expected_score_loss = published->expected_score_loss;
  analysis.theory = published->theory;

  // Keep the live run's engine-depth metadata, but freeze all classification
  // counters/accuracy values to the same published snapshot as the move label.
  const int live_engine_depth = analysis.summary.engine_depth;
  analysis.summary = published->summary;
  analysis.summary.engine_depth = live_engine_depth;
  return analysis;
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
  const int live_depth = live_job != nullptr && live_job->engine != nullptr
      ? live_job->engine->current_depth()
      : 0;
  bool quality_complete = false;
  try {
    const auto current_settings = database_.settings();
    const auto compatible = database_.compatible_analysis(
        game_id, analysis.engine_version, current_settings, analysis.latest_ply);
    quality_complete = analysis.latest_ply >= 0
        && compatible.has_value()
        && compatible->config_hash == analysis.config_hash
        && (!analysis.lines.empty()
            || (analysis.classification.has_value()
                && *analysis.classification == MoveCategory::theory));
  } catch (...) {
  }
  json << ",\"currentPly\":" << live_current_slot
       << ",\"liveDepth\":" << live_depth
       << ",\"qualityComplete\":" << (quality_complete ? "true" : "false")
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
  std::string analyzed_fen;
  if (const auto game = database_.game(game_id); game.has_value()) {
    analyzed_fen = game->starting_fen;
    if (!game->moves.empty() && analysis.latest_ply > 0) {
      const auto move_index = std::min(
          static_cast<std::size_t>(analysis.latest_ply - 1),
          game->moves.size() - 1);
      analyzed_fen = game->moves[move_index].fen_after;
    }
  }
  const bool line_is_white = analyzed_fen.empty() || white_to_move(analyzed_fen);
  json << ",\"lines\":[";
  const auto line_count = std::min(
      analysis.lines.size(), static_cast<std::size_t>(database_.settings().multi_pv));
  for (std::size_t index = 0; index < line_count; ++index) {
    if (index != 0) json << ',';
    const auto& line = analysis.lines[index];
    json << "{\"rank\":" << line.rank << ",\"depth\":" << line.depth
         << ",\"evaluationCp\":";
    append_optional_int(json, line.evaluation_cp.has_value()
        ? std::optional<int>(line_is_white ? *line.evaluation_cp : -*line.evaluation_cp)
        : std::nullopt);
    json << ",\"mateIn\":";
    append_optional_int(json, line.mate_in.has_value()
        ? std::optional<int>(line_is_white ? *line.mate_in : -*line.mate_in)
        : std::nullopt);
    json << ",\"wdl\":";
    if (line.wdl.has_value()) {
      json << "{\"wins\":" << (line_is_white ? line.wdl->wins : line.wdl->losses)
           << ",\"draws\":" << line.wdl->draws
           << ",\"losses\":" << (line_is_white ? line.wdl->losses : line.wdl->wins)
           << '}';
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
    std::optional<int> best_cp;
    std::optional<int> played_cp;
    std::optional<int> second_cp;
    AccuracyEvaluation accuracy_best;
    AccuracyEvaluation accuracy_played_root;
    AccuracyEvaluation accuracy_played_after;
    AccuracyEvaluation accuracy_second;
    bool played_is_best = false;
    bool missed_forced_mate = false;
    bool allowed_forced_mate = false;
    bool material_sacrifice = false;
    bool only_move_tactical = false;
    bool forces_nontrivial_mate = false;
    std::string recommended_move;
    const auto context = position_context(move.fen_before);
    if (before.has_value() && !before->lines.empty()) {
      recommended_move = ranked_best_move(before->lines, before->best_move);
      played_is_best = move.uci == recommended_move;
      if (!recommended_move.empty()) {
        recommended_move = apply_legal_uci_move(
            move.fen_before, recommended_move).san;
      }
      best_expected = expected_score_side_to_move(evaluation_of(before->lines.front()));
      best_cp = before->lines.front().evaluation_cp;
      accuracy_best = {
          .evaluation_cp = before->lines.front().evaluation_cp,
          .mate_in = before->lines.front().mate_in,
          .depth = before->lines.front().depth,
      };
      if (before->lines.size() > 1) {
        second_expected = expected_score_side_to_move(evaluation_of(before->lines[1]));
        second_cp = before->lines[1].evaluation_cp;
        accuracy_second = {
            .evaluation_cp = before->lines[1].evaluation_cp,
            .mate_in = before->lines[1].mate_in,
            .depth = before->lines[1].depth,
        };
      }

      // Prefer the played move's score from the SAME root MultiPV search when
      // Stockfish returned it. Comparing two root alternatives from one search
      // is much more stable than comparing a before-position result with an
      // independently deepened after-position result.
      for (const auto& line : before->lines) {
        if (line.best_move() != move.uci) continue;
        played_expected = expected_score_side_to_move(evaluation_of(line));
        played_cp = line.evaluation_cp;
        accuracy_played_root = {
            .evaluation_cp = line.evaluation_cp,
            .mate_in = line.mate_in,
            .depth = line.depth,
        };
        material_sacrifice = material_sacrifice_in_pv(
            move.fen_before, line.moves, 8);
        forces_nontrivial_mate = line.mate_in.has_value() && *line.mate_in > 1;
        break;
      }

      if (best_expected.has_value() && second_expected.has_value()) {
        const bool unique_by_expected = *best_expected - *second_expected >= 0.20;
        const bool unique_by_cp = best_cp.has_value() && second_cp.has_value()
            && *best_cp - *second_cp >= MoveClassifierConfig{}.critical_cp_gap;
        only_move_tactical = *best_expected >= 0.55
            && (unique_by_expected || unique_by_cp);
      }
      const auto best_mate = before->lines.front().mate_in;
      const bool best_has_mate = best_mate.has_value() && *best_mate > 0;
      if (after.has_value() && !after->lines.empty()) {
        const auto after_mate = after->lines.front().mate_in;
        // After the move the perspective flips to the opponent. Negative mate
        // means the mover has a forced mate. This also verifies sacrificial
        // attacks even when the played move was not rank 1 in a shallower root
        // search.
        forces_nontrivial_mate = forces_nontrivial_mate
            || (after_mate.has_value() && *after_mate < -1);
        if (!material_sacrifice) {
          std::vector<std::string> played_pv;
          played_pv.reserve(after->lines.front().moves.size() + 1);
          played_pv.push_back(move.uci);
          played_pv.insert(
              played_pv.end(),
              after->lines.front().moves.begin(),
              after->lines.front().moves.end());
          material_sacrifice = material_sacrifice_in_pv(
              move.fen_before, played_pv, 8);
        }
        // A missing mate score means a previously forced mate was lost;
        // positive mate means the move allowed the opponent to force mate.
        missed_forced_mate = best_has_mate
            && (!after_mate.has_value() || *after_mate > 0);
        allowed_forced_mate = after_mate.has_value() && *after_mate > 0
            && (!best_mate.has_value() || *best_mate >= 0);
      }
    }
    if (after.has_value() && !after->lines.empty()) {
      const auto& after_line = after->lines.front();
      if (!played_expected.has_value()) {
        played_expected = expected_score_mover_after_move(evaluation_of(after_line));
      }
      if (!played_cp.has_value() && after_line.evaluation_cp.has_value()) {
        played_cp = -*after_line.evaluation_cp;
      }
      accuracy_played_after.depth = after_line.depth;
      if (after_line.evaluation_cp.has_value()) {
        accuracy_played_after.evaluation_cp = -*after_line.evaluation_cp;
      }
      if (after_line.mate_in.has_value() && *after_line.mate_in != 0) {
        // Engine scores are from the side-to-move perspective.  After the
        // played move the opponent is to move, so invert mate sign back to
        // the mover perspective used by Accuracy V3.
        accuracy_played_after.mate_in = -*after_line.mate_in;
      } else if (after_line.mate_in.has_value() && *after_line.mate_in == 0) {
        // The terminal adapter uses mate=0 when the opponent is already
        // checkmated. From the mover's perspective this is a completed mate.
        accuracy_played_after.evaluation_cp.reset();
        accuracy_played_after.mate_in = 1;
        if (accuracy_played_after.depth <= 0) {
          accuracy_played_after.depth = accuracy_best.depth;
        }
      } else if (after_line.depth == 0 && after_line.wdl.has_value()
                 && after_line.wdl->draws > 0
                 && after_line.wdl->wins == 0
                 && after_line.wdl->losses == 0) {
        // Stalemate / terminal draw. Accuracy itself does not consume WDL;
        // this is only a zero-cost terminal-state marker from the engine.
        accuracy_played_after.evaluation_cp = 0;
        accuracy_played_after.mate_in.reset();
        accuracy_played_after.depth = accuracy_best.depth;
      }
    }
    if (!played_expected.has_value()) {
      // Terminal positions need no engine PV. Score the actual board state
      // directly, including the final move even when its post-move slot was
      // satisfied by the engine adapter's zero-cost terminal result.
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
        .best_evaluation_cp = best_cp,
        .played_evaluation_cp = played_cp,
        .second_best_evaluation_cp = second_cp,
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
    accuracy_moves.push_back({
        .theory = theory.is_theory,
        .played_is_best = played_is_best,
        .legal_move_count = context.legal_move_count,
        .best = accuracy_best,
        .played_root = accuracy_played_root,
        .played_after = accuracy_played_after,
        .second_best = accuracy_second,
    });
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
    const std::shared_ptr<AnalysisJob>& job,
    const int preferred_slot) noexcept {
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
                  << " preanalysisBudgetDepth=" << settings.depth
                  << " preanalysisBudgetMultiPv=" << settings.multi_pv
                  << " threads=" << settings.threads
                  << " hashMb=" << settings.hash_mb
                  << " time=" << settings.time_limit_seconds;
      diagnostics::info("analysis", log_message.str());
    }
    const std::string engine_version = job->engine->version();
    const std::string cache_engine_identity = position_cache_engine_identity(engine_version, settings.adaptive_early_stop);
    bool engine_started = false;

    std::unordered_set<int> completed_slots(
        completed_position_slots.begin(), completed_position_slots.end());
    const bool is_fen_position_only = positions.size() == 1;

    // For PGN preparation, slot i is the position before move i. Keep the
    // actually played move beside that slot so the adaptive scheduler can
    // decide whether the cheap two-line root search already contains enough
    // information for classification.
    std::vector<std::string> played_move_by_slot(positions.size());
    if (!is_fen_position_only) {
      if (const auto game = database_.game(game_id); game.has_value()) {
        for (const auto& move : game->moves) {
          if (move.ply_index < 0
              || move.ply_index >= static_cast<int>(played_move_by_slot.size())) {
            continue;
          }
          played_move_by_slot[static_cast<std::size_t>(move.ply_index)] = move.uci;
        }
      }
    }

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

    // Preparation-pass optimization: Theory moves need no Stockfish work of
    // their own. Skip only position slots whose adjacent moves are both
    // Theory, so the boundary position required by the first non-Theory move
    // is still analyzed normally. This is book-driven, never based on a fixed
    // opening move count, so an early amateur deviation immediately resumes
    // normal engine analysis.
    std::vector<int> theory_skipped_slots;
    if (!is_fen_position_only) {
      if (const auto game = database_.game(game_id); game.has_value()) {
        std::vector<bool> theory_moves(game->moves.size(), false);
        const auto metadata = opening_theory_->metadata();
        for (std::size_t index = 0; index < game->moves.size(); ++index) {
          const auto& move = game->moves[index];
          if (metadata.max_ply > 0
              && static_cast<std::uint32_t>(move.ply_index) < metadata.max_ply) {
            theory_moves[index] =
                opening_theory_->lookup(move.fen_before, move.uci).is_theory;
          }
        }
        for (int slot = 0; slot < static_cast<int>(positions.size()); ++slot) {
          const bool previous_requires_engine = slot > 0
              && !theory_moves[static_cast<std::size_t>(slot - 1)];
          const bool next_requires_engine = slot < static_cast<int>(theory_moves.size())
              && !theory_moves[static_cast<std::size_t>(slot)];
          if (!previous_requires_engine && !next_requires_engine) {
            theory_skipped_slots.push_back(slot);
          }
        }
      }
    }

    for (const int slot : theory_skipped_slots) {
      if (completed_slots.contains(slot)) continue;
      const std::string cache_fen = canonical_position_cache_fen(positions[slot]);
      AnalysisResult skipped_result;
      // Reuse a previous checkpoint when one exists, but do not run Stockfish
      // merely to populate an eval for a move that is classified as Theory.
      if (settings.use_global_analysis_cache) {
        if (auto checkpoint = database_.best_position_checkpoint(
                cache_fen, cache_engine_identity, settings.depth, settings.multi_pv);
            checkpoint.has_value()) {
          skipped_result = std::move(*checkpoint);
        }
      }
      completed_slots.insert(slot);
      // A Theory classification still needs an authoritative move_analysis row.
      // Without this empty placeholder persist_classifications() has nothing to
      // update, so the move disappears from both the UI badge and Theory totals.
      database_.persist_engine_result(
          game_id, config_hash, slot, contiguous_completed_moves(),
          skipped_result, unix_time_seconds());
    }
    job->completed_moves = contiguous_completed_moves();
    if (!theory_skipped_slots.empty()) {
      rebuild_classification(game_id, config_hash, true, job->completed_moves - 1);
    }

    std::vector<int> analysis_order;
    analysis_order.reserve(positions.size());
    const auto push_slot = [&](const int slot) {
      if (slot < 0 || slot >= static_cast<int>(positions.size())) return;
      if (std::find(analysis_order.begin(), analysis_order.end(), slot) == analysis_order.end()) {
        analysis_order.push_back(slot);
      }
    };
    if (preferred_slot >= 0) {
      push_slot(preferred_slot);
      push_slot(preferred_slot + 1);
    }
    for (int slot = 0; slot < static_cast<int>(positions.size()); ++slot) push_slot(slot);

    for (const int ply : analysis_order) {
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
        const auto context = position_context(positions[ply]);
        if (context.legal_move_count == 0) {
          // Terminal checkmate/stalemate positions have no candidate move and
          // therefore need no Stockfish search. Persist an empty terminal
          // result so the final played move can still be classified from the
          // board state.
          result = AnalysisResult{};
        } else {
          if (!engine_started) {
            job->engine->start();
            job->engine->new_game();
            engine_started = true;
          }

          // Adaptive preparation stage 1: at the user's full minimum depth,
          // calculate at most the two most important root alternatives first.
          // Two lines are enough to distinguish Best/Great and, when the
          // actually played move is among them, to score that move from the
          // same root search. The user's MultiPV remains the hard ceiling.
          auto scout_settings = settings;
          scout_settings.multi_pv = std::max(
              1, std::min({settings.multi_pv, context.legal_move_count, 2}));

          std::optional<AnalysisResult> scout;
          if (settings.use_global_analysis_cache) {
            scout = database_.compatible_position_analysis(
                cache_fen, cache_engine_identity, scout_settings);
          }
          if (!scout.has_value()) {
            auto scout_request = preanalysis_request(positions[ply], scout_settings);
            scout_request.multi_pv = scout_settings.multi_pv;
            if (scout_settings.depth >= 12) {
              scout_request.dynamic_early_stop = settings.adaptive_early_stop;
              scout_request.early_stop_min_depth =
                  preanalysis_early_stop_min_depth(scout_settings);
              scout_request.early_stop_stable_iterations =
                  kPreanalysisStableIterations;
              scout_request.early_stop_eval_tolerance_cp =
                  kPreanalysisStableEvalToleranceCp;
              scout_request.early_stop_require_cp_scores = true;
            }
            scout = job->engine->analyze(scout_request);
            if (scout->interrupted) {
              if (job->cancel_requested) {
                job->state = AnalysisJobState::cancelled;
                database_.set_analysis_status(game_id, config_hash, "cancelled");
                job->engine->stop();
                job->finished = true;
                return;
              }
              continue;
            }
            if (settings.use_global_analysis_cache) {
              database_.persist_position_analysis(
                  cache_fen, cache_engine_identity, scout_settings, *scout,
                  unix_time_seconds());
            }
          }

          bool needs_full_lines = false;
          const std::string played_move = ply >= 0
                  && ply < static_cast<int>(played_move_by_slot.size())
              ? played_move_by_slot[static_cast<std::size_t>(ply)]
              : std::string{};
          bool played_is_in_scout = !played_move.empty() && std::any_of(
              scout->lines.begin(), scout->lines.end(), [&](const EngineLine& line) {
                return line.best_move() == played_move;
              });

          // Adaptive preparation stage 2: when the played move is outside the
          // two scout candidates, do not immediately repeat the whole root
          // search with the user's larger MultiPV. Ask Stockfish to search only
          // the played root move at the scout's reached depth. This gives the
          // classifier a same-position score at a fraction of the cost while
          // preserving the existing full verification for genuinely difficult
          // tactical/boundary positions below.
          if (settings.multi_pv > scout_settings.multi_pv
              && !played_move.empty() && !played_is_in_scout) {
            auto played_request = preanalysis_request(positions[ply], settings);
            played_request.depth = std::max(1, scout->reached_depth);
            played_request.multi_pv = 1;
            played_request.dynamic_early_stop = false;
            played_request.search_moves = {played_move};
            auto played_result = job->engine->analyze(played_request);
            if (played_result.interrupted) {
              if (job->cancel_requested) {
                job->state = AnalysisJobState::cancelled;
                database_.set_analysis_status(game_id, config_hash, "cancelled");
                job->engine->stop();
                job->finished = true;
                return;
              }
              continue;
            }

            const bool targeted_played_move = !played_result.lines.empty()
                && played_result.lines.front().best_move() == played_move;
            if (targeted_played_move) {
              auto played_line = std::move(played_result.lines.front());
              played_line.rank = static_cast<int>(scout->lines.size()) + 1;
              scout->lines.push_back(std::move(played_line));
              scout->nodes = std::max(scout->nodes, played_result.nodes);
              played_is_in_scout = true;
              diagnostics::debug(
                  "analysis", "game=" + game_id + " slot=" + std::to_string(ply)
                      + " targetedPlayedMove=1 move=" + played_move
                      + " depth=" + std::to_string(played_request.depth));
            } else {
              // A legal game move should always survive a searchmoves root
              // restriction. Fall back to the previous full-MultiPV path if
              // Stockfish ever cannot return that targeted line.
              needs_full_lines = true;
              diagnostics::debug(
                  "analysis", "game=" + game_id + " slot=" + std::to_string(ply)
                      + " targetedPlayedMove=0 fallbackFull=1 move=" + played_move);
            }
          }

          const bool difficult_position = difficult_preanalysis_position(
              positions[ply], played_move, *scout);
          // Step 5: difficult classifications receive the user's full minimum
          // budget. This is intentionally selective: quiet, clear positions
          // keep the cheap scout result, while tactical/mate/sacrifice and
          // boundary cases are verified without early termination.
          const bool needs_full_verification = difficult_position
              && (scout->converged_early
                  || settings.multi_pv > scout_settings.multi_pv
                  || scout->reached_depth < settings.depth);

          if (needs_full_lines || needs_full_verification) {
            auto full_request = preanalysis_request(positions[ply], settings);
            full_request.multi_pv = std::max(
                1, std::min(full_request.multi_pv, context.legal_move_count));
            // Deliberately leave dynamic_early_stop disabled. The whole point
            // of this second pass is to settle a user-visible classification
            // that the cheap scout could not establish confidently.
            result = job->engine->analyze(full_request);
            if (result->interrupted) {
              if (job->cancel_requested) {
                job->state = AnalysisJobState::cancelled;
                database_.set_analysis_status(game_id, config_hash, "cancelled");
                job->engine->stop();
                job->finished = true;
                return;
              }
              continue;
            }
            if (settings.use_global_analysis_cache) {
              database_.persist_position_analysis(
                  cache_fen, cache_engine_identity, settings, *result,
                  unix_time_seconds());
            }
            diagnostics::debug(
                "analysis", "game=" + game_id + " slot=" + std::to_string(ply)
                    + " adaptiveLines=" + std::to_string(scout_settings.multi_pv)
                    + "->" + std::to_string(full_request.multi_pv)
                    + " difficult=" + (difficult_position ? "1" : "0"));
          } else {
            result = std::move(scout);
            diagnostics::debug(
                "analysis", "game=" + game_id + " slot=" + std::to_string(ply)
                    + " adaptiveLines=" + std::to_string(scout_settings.multi_pv)
                    + " difficult=" + (difficult_position ? "1" : "0"));
          }
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

      if (!is_fen_position_only && preferred_slot >= 0
          && completed_slots.contains(preferred_slot)
          && completed_slots.contains(preferred_slot + 1)) {
        rebuild_classification(game_id, config_hash, true, preferred_slot);
      } else if (!is_fen_position_only && completed_moves > 0) {
        rebuild_classification(
            game_id, config_hash, true, completed_moves - 1);
      }
    }
    rebuild_classification(game_id, config_hash, true);
    database_.set_analysis_status(game_id, config_hash, "complete");
    // This completed run supersedes every older/lower saved run for the game.
    // Position-cache entries remain available independently.
    database_.prune_game_analyses_except(game_id, config_hash);
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


void AnalysisService::run_refinement_queue(
    const std::string& game_id,
    const std::string& config_hash,
    AppSettings settings,
    std::vector<std::string> positions,
    std::vector<int> completed_position_slots,
    const std::shared_ptr<AnalysisJob>& job) noexcept {
  try {
    job->state = AnalysisJobState::running;
    // There is exactly one live Stockfish instance. Respect the user-selected
    // Hash value here as well; the Engine settings must describe both
    // preparation and live refinement. Threads are still capped to a safe
    // per-worker share of the machine so the UI/OS remains responsive.
    settings.hash_mb = std::clamp(
        settings.hash_mb, kHashMbSetting.min_int, kHashMbSetting.max_int);
    settings.threads = live_refinement_threads(settings);

    const std::string engine_version = job->engine->version();
    const std::string cache_engine_identity = position_cache_engine_identity(engine_version, settings.adaptive_early_stop);
    job->engine->start();
    job->engine->new_game();

    std::unordered_set<int> completed_slots(
        completed_position_slots.begin(), completed_position_slots.end());
    const bool is_fen_position_only = positions.size() == 1;

    auto contiguous_completed_moves = [&]() {
      if (is_fen_position_only) return completed_slots.contains(0) ? 1 : 0;
      int next_slot = 0;
      while (next_slot < static_cast<int>(positions.size())
             && completed_slots.contains(next_slot)) {
        ++next_slot;
      }
      return std::max(0, next_slot - 1);
    };
    job->completed_moves = contiguous_completed_moves();

    // Theory moves do not need maximum-depth engine work: the classifier
    // returns Theory directly. A position slot can be skipped only when every
    // adjacent move that depends on it is theory; this preserves the boundary
    // evaluation needed for the first non-theory move after the opening.
    std::vector<int> theory_skipped_slots;
    if (!is_fen_position_only) {
      if (const auto game = database_.game(game_id); game.has_value()) {
        std::vector<bool> theory_moves(game->moves.size(), false);
        const auto metadata = opening_theory_->metadata();
        for (std::size_t index = 0; index < game->moves.size(); ++index) {
          const auto& move = game->moves[index];
          if (metadata.max_ply > 0
              && static_cast<std::uint32_t>(move.ply_index) < metadata.max_ply) {
            theory_moves[index] =
                opening_theory_->lookup(move.fen_before, move.uci).is_theory;
          }
        }
        for (int slot = 0; slot < static_cast<int>(positions.size()); ++slot) {
          const bool previous_requires_engine = slot > 0
              && !theory_moves[static_cast<std::size_t>(slot - 1)];
          const bool next_requires_engine = slot < static_cast<int>(theory_moves.size())
              && !theory_moves[static_cast<std::size_t>(slot)];
          if (!previous_requires_engine && !next_requires_engine) {
            theory_skipped_slots.push_back(slot);
          }
        }
      }
    }

    for (const int slot : theory_skipped_slots) {
      if (!completed_slots.insert(slot).second) continue;
      // Copy the best already-calculated minimum/checkpoint result into this
      // maximum run when available. This costs no Stockfish time but keeps the
      // eval bar populated while the move itself remains Theory.
      const std::string cache_fen = canonical_position_cache_fen(positions[slot]);
      AnalysisResult skipped_result;
      if (auto checkpoint = database_.best_position_checkpoint(
              cache_fen, cache_engine_identity, settings.depth, settings.multi_pv);
          checkpoint.has_value()) {
        skipped_result = std::move(*checkpoint);
      }
      database_.persist_engine_result(
          game_id, config_hash, slot, contiguous_completed_moves(),
          skipped_result, unix_time_seconds());
    }
    job->completed_moves = contiguous_completed_moves();
    auto next_slot = [&]() -> int {
      if (positions.empty()) return -1;
      int focus = job->requested_position_slot.load();
      focus = std::clamp(focus, 0, static_cast<int>(positions.size()) - 1);

      // Priority: selected position, position after the selected move, all
      // following positions, then walk backwards. Recomputed after every
      // completed/interrupted search, so a new selection takes effect at once.
      if (!completed_slots.contains(focus)) return focus;
      if (focus + 1 < static_cast<int>(positions.size())
          && !completed_slots.contains(focus + 1)) {
        return focus + 1;
      }
      for (int slot = focus + 2; slot < static_cast<int>(positions.size()); ++slot) {
        if (!completed_slots.contains(slot)) return slot;
      }
      for (int slot = focus - 1; slot >= 0; --slot) {
        if (!completed_slots.contains(slot)) return slot;
      }
      return -1;
    };

    while (!job->cancel_requested) {
      const int slot = next_slot();
      if (slot < 0) break;

      job->current_position_slot = slot;
      const auto generation = job->target_generation.load();
      const std::string cache_fen = canonical_position_cache_fen(positions[slot]);
      std::optional<AnalysisResult> result;

      if (settings.use_global_analysis_cache) {
        result = database_.compatible_position_analysis(
            cache_fen, cache_engine_identity, settings);
      }

      if (!result.has_value()) {
        try {
          auto request = analysis_request(positions[slot], settings);
          request.dynamic_early_stop = settings.adaptive_early_stop;
          request.early_stop_min_depth = live_early_stop_min_depth(settings);
          request.early_stop_stable_iterations = kLiveStableIterations;
          request.early_stop_eval_tolerance_cp = kLiveStableEvalToleranceCp;
          result = job->engine->analyze(request);
        } catch (const std::exception&) {
          // A retarget can arrive before Stockfish emitted its first PV. That
          // is a normal preemption, not a failed analysis job.
          if (!job->cancel_requested && job->target_generation.load() != generation) {
            continue;
          }
          throw;
        }
      }

      if (!result.has_value()) continue;

      if (result->interrupted) {
        if (!result->lines.empty() && result->reached_depth > 0) {
          // Keep an interrupted search as a lightweight position checkpoint,
          // not as a completed maximum-analysis row. This preserves correct
          // completion semantics (including time-limited searches) while a
          // later visit can immediately show the best depth already reached.
          auto checkpoint_settings = settings;
          checkpoint_settings.depth = std::max(1, result->reached_depth);
          database_.persist_position_analysis(
              cache_fen, cache_engine_identity, checkpoint_settings,
              *result, unix_time_seconds());
        }
        if (job->cancel_requested) break;
        continue;
      }

      if (result->converged_early) {
        diagnostics::debug(
            "analysis", "game=" + game_id + " slot=" + std::to_string(slot)
                + " converged early at depth=" + std::to_string(result->reached_depth));
      }

      if (settings.use_global_analysis_cache) {
        database_.persist_position_analysis(
            cache_fen, cache_engine_identity, settings, *result, unix_time_seconds());
      }
      completed_slots.insert(slot);
      const int completed_moves = contiguous_completed_moves();
      job->completed_moves = completed_moves;
      database_.persist_engine_result(
          game_id, config_hash, slot, completed_moves, *result, unix_time_seconds());

      // Do not publish a partially recomputed classification here. Engine
      // results remain live, while move categories are published once, after
      // every position in this refinement queue has finished.
    }

    if (job->cancel_requested) {
      job->state = AnalysisJobState::cancelled;
      database_.set_analysis_status(game_id, config_hash, "cancelled");
      diagnostics::info("analysis", "game=" + game_id + " refinement worker cancelled");
    } else {
      rebuild_classification(game_id, config_hash, true);
      database_.set_analysis_status(game_id, config_hash, "complete");
      database_.prune_game_analyses_except(game_id, config_hash);
      job->state = AnalysisJobState::completed;
      diagnostics::info("analysis", "game=" + game_id + " refinement queue completed");
    }
  } catch (const std::exception& error) {
    const bool cancelled = job->cancel_requested;
    job->state = cancelled ? AnalysisJobState::cancelled : AnalysisJobState::failed;
    try {
      database_.set_analysis_status(
          game_id, config_hash, cancelled ? "cancelled" : "error",
          cancelled ? std::string{} : error.what());
    } catch (...) {
    }
    if (!cancelled) {
      diagnostics::error(
          "analysis", "game=" + game_id + " refinement worker failed: " + error.what());
    }
  } catch (...) {
    job->state = job->cancel_requested
        ? AnalysisJobState::cancelled
        : AnalysisJobState::failed;
    try {
      database_.set_analysis_status(
          game_id, config_hash, job->cancel_requested ? "cancelled" : "error",
          job->cancel_requested ? std::string{} : "Unknown refinement worker error");
    } catch (...) {
    }
  }

  job->current_position_slot = -1;
  job->engine->stop();
  job->finished = true;
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
    for (const auto& move : game->moves) {
      positions.push_back(move.fen_after);
    }
  }
  if (positions.empty()) throw std::invalid_argument("Game has no analyzable positions");

  // A main-line analysis owns the Stockfish work slot. If a sideline engine
  // is still resident, release it before starting or resuming this worker.
  stop_all_variation_jobs(true);

  const auto settings = preanalysis_budget_settings();
  StockfishEngine version_probe;
  const auto config_hash = analysis_config_hash(settings);
  if (auto reusable = reusable_analysis(game_id, settings);
      reusable.has_value() && reusable->status == "complete") {
    diagnostics::info("cache", "game=" + game_id + " complete analysis reused");
    rebuild_classification(game_id, reusable->config_hash);
    database_.prune_game_analyses_except(game_id, reusable->config_hash);
    return analysis_json(game_id, database_.analysis(
        game_id, reusable->config_hash).value());
  }
  version_probe.validate_available();
  const int public_total_plies = game->kind == "fen"
      ? 1
      : static_cast<int>(game->moves.size());
  auto persisted = database_.prepare_analysis(
      game_id, config_hash, version_probe.version(), public_total_plies,
      settings.depth, settings.multi_pv, settings.time_limit_seconds,
      settings.adaptive_early_stop);
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
  if (!result.has_value()) {
    const auto minimum_settings = preanalysis_budget_settings();
    result = reusable_analysis(game_id, minimum_settings);
  }
  if (!result.has_value()) throw std::runtime_error("Analysis job not found");
  return analysis_json(game_id, *result, live_job.get());
}

std::string AnalysisService::move_analysis_status_json(const std::string& game_id, const int ply) {
  validate_token(game_id, "game id");
  if (ply < 0) throw std::invalid_argument("ply must be non-negative");

  // Prefer a maximum-quality live refinement when that position already has
  // a result. Keep the refinement job attached even before the first maximum
  // result is persisted, so callers continue polling and can see live depth.
  std::shared_ptr<AnalysisJob> refinement_job;
  {
    std::lock_guard lock(refinement_jobs_mutex_);
    const auto job = refinement_jobs_.find(game_id);
    if (job != refinement_jobs_.end()) {
      refinement_job = job->second;
      if (auto refined = database_.analysis(game_id, refinement_job->config_hash, ply);
          refined.has_value() && (!refined->lines.empty() || refined->latest_ply == ply)) {
        auto visible = stable_live_classification_snapshot(
            game_id, ply, std::move(*refined), refinement_job.get());
        return analysis_json(game_id, visible, refinement_job.get());
      }
    }
  }

  auto maximum_settings = database_.settings();
  if (auto refined = reusable_analysis(game_id, maximum_settings, ply);
      refined.has_value() && (!refined->lines.empty() || refined->latest_ply == ply)) {
    auto visible = stable_live_classification_snapshot(
        game_id, ply, std::move(*refined), refinement_job.get());
    return analysis_json(game_id, visible, refinement_job.get());
  }

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
    const auto minimum_settings = preanalysis_budget_settings();
    result = reusable_analysis(game_id, minimum_settings, ply);
  }
  if (!result.has_value() && refinement_job != nullptr) {
    result = database_.analysis(game_id, refinement_job->config_hash, ply);
  }
  if (!result.has_value()) throw std::runtime_error("Analysis job not found");

  if (refinement_job != nullptr) {
    live_job = refinement_job;
    const auto game = database_.game(game_id);
    if (game.has_value()) {
      std::string fen;
      if (game->kind == "fen") {
        if (ply == 0) fen = game->starting_fen;
      } else if (ply == 0) {
        fen = game->starting_fen;
      } else if (ply <= static_cast<int>(game->moves.size())) {
        fen = game->moves[static_cast<std::size_t>(ply - 1)].fen_after;
      }
      if (!fen.empty()) {
        const auto requested = database_.settings();
        const auto checkpoint = database_.best_position_checkpoint(
            canonical_position_cache_fen(fen),
            position_cache_engine_identity(
                refinement_job->engine->version(), requested.adaptive_early_stop),
            requested.depth, requested.multi_pv);
        const int persisted_depth = result->lines.empty()
            ? 0 : result->lines.front().depth;
        if (checkpoint.has_value() && !checkpoint->lines.empty()
            && checkpoint->lines.front().depth > persisted_depth) {
          result->best_move = checkpoint->best_move;
          result->lines = checkpoint->lines;
          result->latest_ply = ply;
        }
      }
    }
  }
  if (refinement_job != nullptr && live_job == refinement_job) {
    *result = stable_live_classification_snapshot(
        game_id, ply, std::move(*result), refinement_job.get());
  }
  return analysis_json(game_id, *result, live_job.get());
}

std::string AnalysisService::start_move_refinement_json(
    const std::string& game_id, const int ply) {
  validate_token(game_id, "game id");
  if (ply < 0) throw std::invalid_argument("ply must be non-negative");
  const auto game = database_.game(game_id);
  if (!game.has_value()) throw std::runtime_error("Game not found");

  // Main-line refinement and sideline analysis share the same live-analysis
  // budget. Returning to the main line first stops the temporary sideline
  // search so Stockfish never competes with itself for CPU or RAM.
  stop_all_variation_jobs(true);

  auto settings = database_.settings();
  if (settings.depth <= settings.min_analysis_depth) {
    return move_analysis_status_json(game_id, ply);
  }

  std::vector<std::string> positions;
  if (game->kind == "fen") {
    positions.push_back(game->starting_fen);
  } else {
    positions.reserve(game->moves.size() + 1);
    positions.push_back(game->starting_fen);
    for (const auto& move : game->moves) {
      positions.push_back(move.fen_after);
    }
  }
  if (ply >= static_cast<int>(positions.size())) {
    throw std::invalid_argument("ply exceeds game position count");
  }

  StockfishEngine version_probe;
  version_probe.validate_available();
  if (auto reusable = reusable_analysis(game_id, settings, ply);
      reusable.has_value() && reusable->status == "complete") {
    rebuild_classification(game_id, reusable->config_hash);
    database_.prune_game_analyses_except(game_id, reusable->config_hash);
    return analysis_json(game_id, *reusable);
  }
  const auto config_hash = analysis_config_hash(settings);
  const int public_total_plies = game->kind == "fen"
      ? 1
      : static_cast<int>(game->moves.size());
  auto persisted = database_.prepare_analysis(
      game_id, config_hash, version_probe.version(), public_total_plies,
      settings.depth, settings.multi_pv, settings.time_limit_seconds,
      settings.adaptive_early_stop);

  // The minimum/background pass must never compete with live refinement for
  // CPU/RAM. The first user-driven refinement takes ownership of Stockfish.
  std::shared_ptr<AnalysisJob> minimum_job;
  {
    std::lock_guard lock(jobs_mutex_);
    const auto existing = jobs_.find(game_id);
    if (existing != jobs_.end()) {
      minimum_job = existing->second;
      if (!minimum_job->finished) {
        minimum_job->cancel_requested = true;
        minimum_job->engine->cancel();
      }
      jobs_.erase(existing);
    }
  }
  if (minimum_job && minimum_job->worker.joinable()) minimum_job->worker.join();

  // Only one live-refinement worker is allowed globally. Close a worker from
  // another game before activating this game. For the same game/config, keep
  // the existing worker and merely retarget it -- no thread/engine churn.
  std::vector<std::shared_ptr<AnalysisJob>> stale_jobs;
  std::shared_ptr<AnalysisJob> active;
  {
    std::lock_guard lock(refinement_jobs_mutex_);
    for (auto it = refinement_jobs_.begin(); it != refinement_jobs_.end();) {
      const bool same_job = it->first == game_id
          && it->second->config_hash == config_hash
          && !it->second->finished;
      if (same_job) {
        active = it->second;
        ++it;
        continue;
      }
      auto stale = it->second;
      if (!stale->finished) {
        stale->cancel_requested = true;
        stale->engine->cancel();
      }
      stale_jobs.push_back(stale);
      it = refinement_jobs_.erase(it);
    }
  }
  for (const auto& stale : stale_jobs) {
    if (stale->worker.joinable()) stale->worker.join();
  }

  if (active != nullptr) {
    active->requested_position_slot = ply;
    active->target_generation.fetch_add(1);
    // Preempt the old position immediately. The worker persists any partial
    // PV it already has and recomputes its queue around the newly selected ply.
    const int current = active->current_position_slot.load();
    if (current >= 0 && current != ply) active->engine->cancel();

    return move_analysis_status_json(game_id, ply);
  }

  // If the whole maximum run is already complete, there is no worker to start.
  if (persisted.status == "complete") {
    if (auto ready = database_.analysis(game_id, config_hash, ply); ready.has_value()) {
      return analysis_json(game_id, *ready);
    }
    return analysis_json(game_id, persisted);
  }

  auto job = std::make_shared<AnalysisJob>();
  job->engine = std::make_shared<StockfishEngine>();
  job->config_hash = config_hash;
  if (auto published = reusable_analysis(game_id, preanalysis_budget_settings());
      published.has_value()) {
    job->published_classification_config_hash = published->config_hash;
  } else {
    job->published_classification_config_hash =
        analysis_config_hash(preanalysis_budget_settings());
  }
  job->requested_position_slot = ply;
  job->target_generation = 1;
  auto completed_position_slots = database_.analyzed_position_slots(game_id, config_hash);
  job->worker = std::thread(
      [this, game_id, config_hash, settings, positions = std::move(positions),
       completed_position_slots = std::move(completed_position_slots), job]() mutable {
        run_refinement_queue(
            game_id, config_hash, settings, std::move(positions),
            std::move(completed_position_slots), job);
      });
  {
    std::lock_guard lock(refinement_jobs_mutex_);
    refinement_jobs_[game_id] = job;
  }

  return move_analysis_status_json(game_id, ply);
}

void AnalysisService::cancel_analysis(const std::string& game_id) {
  validate_token(game_id, "game id");
  std::vector<std::shared_ptr<AnalysisJob>> jobs;
  {
    std::lock_guard lock(jobs_mutex_);
    const auto found = jobs_.find(game_id);
    if (found != jobs_.end()) jobs.push_back(found->second);
  }
  {
    std::lock_guard lock(refinement_jobs_mutex_);
    const auto found = refinement_jobs_.find(game_id);
    if (found != refinement_jobs_.end()) jobs.push_back(found->second);
  }
  if (jobs.empty()) throw std::runtime_error("Analysis job not found");
  for (const auto& job : jobs) {
    job->state = AnalysisJobState::cancelling;
    job->cancel_requested = true;
    job->engine->cancel();
  }
}

void AnalysisService::delete_analysis(const std::string& game_id) {
  validate_token(game_id, "game id");
  if (!database_.game(game_id).has_value()) {
    throw std::runtime_error("Game not found");
  }

  std::vector<std::shared_ptr<AnalysisJob>> stale_jobs;
  {
    std::lock_guard lock(jobs_mutex_);
    const auto found = jobs_.find(game_id);
    if (found != jobs_.end()) {
      stale_jobs.push_back(found->second);
      jobs_.erase(found);
    }
  }
  {
    std::lock_guard lock(refinement_jobs_mutex_);
    const auto found = refinement_jobs_.find(game_id);
    if (found != refinement_jobs_.end()) {
      stale_jobs.push_back(found->second);
      refinement_jobs_.erase(found);
    }
  }
  for (const auto& job : stale_jobs) {
    if (!job->finished) {
      job->state = AnalysisJobState::cancelling;
      job->cancel_requested = true;
      job->engine->cancel();
    }
  }
  for (const auto& job : stale_jobs) {
    if (job->worker.joinable()) job->worker.join();
  }

  database_.delete_game_analyses(game_id);
  diagnostics::info("analysis", "game=" + game_id + " saved analysis deleted");
}


void AnalysisService::stop_all_mainline_analysis_jobs() noexcept {
  std::vector<std::shared_ptr<AnalysisJob>> stale_jobs;
  {
    std::lock_guard lock(jobs_mutex_);
    for (auto& [game_id, job] : jobs_) {
      (void)game_id;
      if (!job->finished) {
        job->state = AnalysisJobState::cancelling;
        job->cancel_requested = true;
        job->engine->cancel();
      }
      stale_jobs.push_back(job);
    }
    jobs_.clear();
  }
  {
    std::lock_guard lock(refinement_jobs_mutex_);
    for (auto& [game_id, job] : refinement_jobs_) {
      (void)game_id;
      if (!job->finished) {
        job->state = AnalysisJobState::cancelling;
        job->cancel_requested = true;
        job->engine->cancel();
      }
      stale_jobs.push_back(job);
    }
    refinement_jobs_.clear();
  }
  for (const auto& stale : stale_jobs) {
    if (stale->worker.joinable()) stale->worker.join();
  }
}

void AnalysisService::stop_all_variation_jobs(const bool stop_engine) noexcept {
  std::vector<std::shared_ptr<VariationJob>> stale_jobs;
  std::shared_ptr<StockfishEngine> engine_to_stop;
  {
    std::lock_guard lock(variation_jobs_mutex_);
    for (auto& [job_id, job] : variation_jobs_) {
      (void)job_id;
      if (!job->finished) {
        job->cancel_requested = true;
        job->engine->cancel();
      }
      stale_jobs.push_back(job);
    }
    variation_jobs_.clear();
    if (stop_engine) {
      variation_position_results_.clear();
      engine_to_stop = std::move(variation_engine_);
    }
  }
  for (const auto& stale : stale_jobs) {
    if (stale->worker.joinable()) stale->worker.join();
  }
  if (engine_to_stop != nullptr) engine_to_stop->stop();
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
  // Validate/apply the move before touching the current worker. An illegal
  // sideline attempt must not kill the analysis that is already visible.
  const auto applied = apply_legal_uci_move(fen, uci);

  // A sideline is live-only and owns the single Stockfish work slot. Stop
  // both the main-line worker and any previous sideline search before the new
  // legal board position is accepted. This prevents hidden background engines.
  stop_all_mainline_analysis_jobs();
  // Cancel/join only the previous search. Keep the sideline Stockfish instance
  // and ephemeral position cache alive so consecutive variation moves retain
  // loaded NNUE networks, Hash/TT state and the previous position evaluation.
  stop_all_variation_jobs(false);
  settings.threads = std::clamp(settings.threads, 1, maximum_worker_threads());
  auto job = std::make_shared<VariationJob>();
  job->id = "variation-" + std::to_string(next_variation_job_id_++);
  job->played_move = applied.uci;
  job->played_san = applied.san;
  job->fen_before = fen;
  job->fen = applied.fen_after;
  {
    std::lock_guard lock(variation_jobs_mutex_);
    if (variation_engine_ == nullptr) {
      variation_engine_ = std::make_shared<StockfishEngine>();
      variation_engine_->validate_available();
    }
    job->engine = variation_engine_;
  }
  job->worker = std::thread([this, job, settings = std::move(settings)] {
    try {
      if (job->cancel_requested) throw std::runtime_error("Variation analysis cancelled");
      job->engine->start();

      // Keep the live UX unchanged: first analyze the position AFTER the
      // sideline move so evaluation/PV immediately correspond to the board the
      // user is looking at. Classification is finalized afterwards.
      auto after_request = analysis_request(job->fen, settings);
      after_request.cancel_requested = &job->cancel_requested;
      auto after = job->engine->analyze(after_request);
      {
        std::lock_guard lock(job->state_mutex);
        job->result = after;
      }
      if (job->cancel_requested || after.interrupted) {
        std::lock_guard lock(job->state_mutex);
        job->status = "paused";
        job->finished = true;
        return;
      }

      // Preserve the finished after-position result for the next sideline ply.
      // This cache is in-memory only and is cleared when variation mode ends.
      {
        std::lock_guard lock(variation_jobs_mutex_);
        variation_position_results_[variation_position_cache_key(job->fen, settings)] = after;
      }

      // A move category compares the played move with the best alternatives in
      // the BEFORE position. Normally that position is exactly the previous
      // sideline result, so only the first sideline ply needs an extra search.
      auto before_settings = settings;
      const auto before_context = position_context(job->fen_before);
      before_settings.multi_pv = std::max(
          1, std::min({settings.multi_pv, before_context.legal_move_count, 2}));

      std::optional<AnalysisResult> before;
      const auto before_key = variation_position_cache_key(job->fen_before, before_settings);
      {
        std::lock_guard lock(variation_jobs_mutex_);
        const auto cached = variation_position_results_.find(before_key);
        if (cached != variation_position_results_.end()
            && (before_context.legal_move_count == 0
                || static_cast<int>(cached->second.lines.size()) >= before_settings.multi_pv)) {
          before = cached->second;
        }
      }

      if (!before.has_value() && settings.use_global_analysis_cache) {
        before = database_.compatible_position_analysis(
            canonical_position_cache_fen(job->fen_before),
            position_cache_engine_identity(
                job->engine->version(), before_settings.adaptive_early_stop),
            before_settings);
      }

      if (!before.has_value()) {
        // Do not expose engine->current_result() while this second search is
        // running: it belongs to the BEFORE position. The UI keeps displaying
        // the completed after-position PV until classification is ready.
        job->expose_live_result = false;
        auto before_request = analysis_request(job->fen_before, before_settings);
        before_request.cancel_requested = &job->cancel_requested;
        before = job->engine->analyze(before_request);
        if (job->cancel_requested || before->interrupted) {
          std::lock_guard lock(job->state_mutex);
          job->status = "paused";
          job->finished = true;
          return;
        }
        {
          std::lock_guard lock(variation_jobs_mutex_);
          variation_position_results_[before_key] = *before;
        }
      }

      TheoryMoveInfo theory;
      if (opening_theory_ != nullptr) {
        theory = opening_theory_->lookup(job->fen_before, job->played_move);
      }
      const auto category = classify_variation_move(
          job->fen_before, job->played_move, job->fen, *before, after, theory);
      {
        std::lock_guard lock(job->state_mutex);
        job->classification = category;
        job->status = "complete";
      }
    } catch (const std::exception& error) {
      std::lock_guard lock(job->state_mutex);
      if (job->cancel_requested) {
        job->status = "paused";
        job->error.clear();
      } else {
        job->status = "error";
        job->error = error.what();
      }
    } catch (...) {
      std::lock_guard lock(job->state_mutex);
      if (job->cancel_requested) {
        job->status = "paused";
        job->error.clear();
      } else {
        job->status = "error";
        job->error = "Unknown variation analysis error";
      }
    }
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

  std::string status;
  std::string error;
  AnalysisResult result;
  std::optional<MoveCategory> classification;
  {
    std::lock_guard lock(job->state_mutex);
    status = job->status;
    error = job->error;
    result = job->result;
    classification = job->classification;
  }
  // Side-line analysis is genuinely live: while Stockfish is thinking, expose
  // its latest complete PV iteration instead of waiting for the hard depth.
  if (status == "running" && job->expose_live_result.load()) {
    auto live = job->engine->current_result();
    if (!live.lines.empty()) result = std::move(live);
  }

  nlohmann::json json{
      {"jobId", job->id},
      {"status", status},
      {"playedMove", job->played_move},
      {"playedSan", job->played_san},
      {"fen", job->fen},
      {"position", nlohmann::json::parse(position_view_json(job->fen))},
      {"error", error.empty() ? nlohmann::json(nullptr) : nlohmann::json(error)},
      {"bestMove", result.best_move},
      {"liveDepth", result.reached_depth},
      {"moverEvaluationCp", nullptr},
      {"moverMateIn", nullptr},
      {"classification", classification.has_value()
          ? nlohmann::json(move_category_name(*classification)) : nlohmann::json(nullptr)},
      {"lines", nlohmann::json::array()},
  };
  if (!result.lines.empty()) {
    const auto& principal = result.lines.front();
    if (principal.evaluation_cp.has_value()) {
      json["moverEvaluationCp"] = -*principal.evaluation_cp;
    }
    if (principal.mate_in.has_value()) json["moverMateIn"] = -*principal.mate_in;
  }
  for (const auto& line : result.lines) {
    const bool line_is_white = white_to_move(job->fen);
    nlohmann::json line_json{
        {"rank", line.rank},
        {"depth", line.depth},
        {"evaluationCp", line.evaluation_cp.has_value()
            ? nlohmann::json(line_is_white ? *line.evaluation_cp : -*line.evaluation_cp)
            : nlohmann::json(nullptr)},
        {"mateIn", line.mate_in.has_value()
            ? nlohmann::json(line_is_white ? *line.mate_in : -*line.mate_in)
            : nlohmann::json(nullptr)},
        {"nodes", line.nodes},
        {"moves", line.moves},
        {"wdl", nullptr},
    };
    if (line.wdl.has_value()) {
      line_json["wdl"] = {
          {"wins", line_is_white ? line.wdl->wins : line.wdl->losses},
          {"draws", line.wdl->draws},
          {"losses", line_is_white ? line.wdl->losses : line.wdl->wins},
      };
    }
    json["lines"].push_back(std::move(line_json));
  }
  return json.dump();
}

void AnalysisService::cancel_variation_analysis(const std::string& job_id) {
  validate_token(job_id, "variation job id");
  std::shared_ptr<VariationJob> job;
  {
    std::lock_guard lock(variation_jobs_mutex_);
    const auto found = variation_jobs_.find(job_id);
    if (found == variation_jobs_.end()) return;
    job = found->second;
    job->cancel_requested = true;
    job->engine->cancel();
  }
  if (job->worker.joinable()) job->worker.join();
  std::shared_ptr<StockfishEngine> engine_to_stop;
  {
    std::lock_guard lock(variation_jobs_mutex_);
    const auto found = variation_jobs_.find(job_id);
    if (found != variation_jobs_.end() && found->second == job) {
      variation_jobs_.erase(found);
    }
    // Explicit cancellation means the caller is leaving the active sideline.
    // Do not keep its potentially large Hash allocation next to main-line
    // Stockfish. Consecutive sideline moves use start_variation_job_json(),
    // which cancels only the search and never comes through this path.
    if (variation_jobs_.empty() && variation_engine_ == job->engine) {
      variation_position_results_.clear();
      engine_to_stop = std::move(variation_engine_);
    }
  }
  if (engine_to_stop != nullptr) engine_to_stop->stop();
}

}  // namespace kchess
