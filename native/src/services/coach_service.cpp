#include "services/coach_service.h"

#include <algorithm>
#include <chrono>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "ai/automatic/automatic_coach_trigger.h"
#include "ai/model_versions.h"
#include "ai/providers/gemini_provider.h"
#include "ai/teaching/personal_training_selector.h"
#include "ai/teaching/spaced_repetition_scheduler.h"
#include "chess/pgn.h"
#include "chess/move.h"
#include "chess/position_view.h"
#include "knowledge/knowledge_runtime.h"
#include "services/coach_profile_bridge.h"

namespace kchess {
// Section: Last engine hint, shared with the existing evidence retrieval hook
struct CoachEngineCacheEntry {
  std::string fen;
  std::string settings_key;
  std::string result;
  ai::CandidateMoveSnapshot candidates;
};

struct CoachHintCache {
  std::mutex mutex;
  CoachEngineCacheEntry hint;
  CoachEngineCacheEntry worst_move;
  CoachEngineCacheEntry fastest_loss;
};

struct CachedCoachEngineResult {
  std::string result;
  ai::CandidateMoveSnapshot candidates;
};

namespace {

// -----------------------------------------------------------------------------
// Section: Request mapping
// -----------------------------------------------------------------------------

using json = nlohmann::json;

std::int64_t coach_unix_time_seconds() {
  using namespace std::chrono;
  return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}

ai::CoachMode coach_mode(std::string_view value) {
  if (value == "explain") return ai::CoachMode::explain;
  if (value == "hint") return ai::CoachMode::hint;
  if (value == "compare") return ai::CoachMode::compare;
  if (value == "quiz") return ai::CoachMode::quiz;
  if (value == "plan") return ai::CoachMode::plan;
  if (value == "review") return ai::CoachMode::review;
  if (value == "teach") return ai::CoachMode::teach;
  return ai::CoachMode::answer;
}

ai::ResponseDepth response_depth(std::string_view value) {
  if (value == "concise") return ai::ResponseDepth::concise;
  if (value == "detailed") return ai::ResponseDepth::detailed;
  return ai::ResponseDepth::standard;
}

std::optional<std::string> optional_string(const json& value, const char* key) {
  if (!value.contains(key) || value[key].is_null()) return std::nullopt;
  const auto text = value[key].get<std::string>();
  return text.empty() ? std::nullopt : std::optional<std::string>(text);
}

bool fen_white_to_move(std::string_view fen) {
  const auto first_space = fen.find(' ');
  if (first_space == std::string_view::npos || first_space + 1 >= fen.size()) {
    return true;
  }
  return fen[first_space + 1] == 'w';
}

std::optional<double> root_expected_score(const json& value,
                                          std::string_view fen) {
  if (!value.contains("wdl") || !value["wdl"].is_object()) return std::nullopt;
  const auto& wdl = value["wdl"];
  if (!wdl.contains("wins") || !wdl.contains("draws") ||
      !wdl.contains("losses") || !wdl["wins"].is_number() ||
      !wdl["draws"].is_number() || !wdl["losses"].is_number()) {
    return std::nullopt;
  }
  const double wins = wdl["wins"].get<double>();
  const double draws = wdl["draws"].get<double>();
  const double losses = wdl["losses"].get<double>();
  const double total = wins + draws + losses;
  if (total <= 0.0) return std::nullopt;
  const double white_score = std::clamp((wins + 0.5 * draws) / total, 0.0, 1.0);
  return fen_white_to_move(fen) ? white_score : 1.0 - white_score;
}

ai::CandidateMoveSnapshot candidate_snapshot_from_json(
    const json& payload, std::string_view fen) {
  ai::CandidateMoveSnapshot snapshot;
  for (const auto& move : payload.value("candidates", json::array())) {
    if (!move.is_object()) continue;
    ai::CandidateLineInput line;
    line.rank = move.value("rank", 0);
    line.move_uci = move.value("uci", "");
    line.pv_uci = move.value("pv", std::vector<std::string>{});
    if (move.contains("evaluationCp") && move["evaluationCp"].is_number_integer())
      line.evaluation_cp = move["evaluationCp"].get<int>();
    if (move.contains("mateIn") && move["mateIn"].is_number_integer())
      line.mate_in = move["mateIn"].get<int>();
    line.expected_score = root_expected_score(move, fen);
    if (!line.move_uci.empty()) snapshot.root_lines.push_back(std::move(line));
  }
  return snapshot;
}

ai::CoachRequest parse_request(const json& value, Database& database) {
  ai::CoachRequest request;
  request.user_text = value.value("text", "");
  if (request.user_text.empty() || request.user_text.size() > 12000) {
    throw std::invalid_argument("Coach text must contain 1-12000 characters");
  }
  request.locale = value.value("locale", "en");
  request.mode = coach_mode(value.value("mode", "answer"));
  request.depth = response_depth(value.value("depth", "standard"));
  request.position_fen = optional_string(value, "positionFen");
  request.game_pgn = optional_string(value, "gamePgn");
  request.session_id = optional_string(value, "sessionId");
  request.user_move_uci = optional_string(value, "userMoveUci");
  request.surface = optional_string(value, "surface");
  request.context_id = optional_string(value, "contextId");
  if (value.contains("contextPly") && value["contextPly"].is_number_integer()) {
    request.context_ply = value["contextPly"].get<int>();
  }
  request.profile_id = optional_string(value, "profileId");
  if (!request.profile_id && request.session_id) {
    if (const auto active = database.active_profile()) request.profile_id = active->id;
  } else if (!request.profile_id && request.mode == ai::CoachMode::quiz) {
    if (const auto active = database.active_profile()) request.profile_id = active->id;
  }
  request.player_color = optional_string(value, "playerColor");
  request.hint_move_uci = optional_string(value, "hintMoveUci");
  request.variation_job_id = optional_string(value, "variationJobId");
  request.personal_training_requested = value.value("personalTraining", false);

  if (!request.game_pgn && request.context_id) {
    if (const auto game = database.game(*request.context_id)) {
      if (!game->pgn.empty()) request.game_pgn = game->pgn;
    }
  }
  return request;
}

void hydrate_coach_native_context_state(Database& database, ai::CoachRequest& request) {
  auto& state = request.native_context_state;

  if (request.context_id) {
    if (const auto game = database.game(*request.context_id)) {
      state.analysis_state = game->analyzed ? "available" : "unavailable";
      if (request.player_color) {
        const auto color = *request.player_color;
        if (color == "white") { state.user_rating = game->white_rating; state.opponent_rating = game->black_rating; }
        else if (color == "black") { state.user_rating = game->black_rating; state.opponent_rating = game->white_rating; }
      }
    }
  } else if (request.position_fen) {
    // Without a concrete game/config key we cannot claim a cache hit here.
    // Keep unknown rather than turning UI presence into analysis truth.
    state.analysis_state = "unknown";
  } else {
    state.analysis_state = "unavailable";
  }

  if (request.profile_id) {
    const auto owner = database.player_profile_owner_id(*request.profile_id);
    state.profile_state = database.ai_chess_profile_payload(owner).has_value() ? "available" : "unavailable";
    const auto sources = database.player_profile_game_sources(owner);
    state.history_state = sources.empty() ? "unavailable" : "available";
    if (request.context_id) {
      if (const auto source = database.player_profile_game_source(owner, *request.context_id)) {
        if (!state.user_rating) state.user_rating = source->player_rating;
        if (!state.opponent_rating) state.opponent_rating = source->opponent_rating;
      }
    } else if (!sources.empty()) {
      const auto& latest = sources.front();
      state.user_rating = latest.player_rating;
      state.opponent_rating = latest.opponent_rating;
    }
  } else {
    state.profile_state = "unavailable";
    state.history_state = "unavailable";
  }

  if (request.game_pgn) {
    const auto parsed = parse_pgn(*request.game_pgn);
    if (parsed.valid && !parsed.game.moves.empty()) state.last_move_uci = parsed.game.moves.back().uci;
  }
}

// -----------------------------------------------------------------------------
// Section: Personal practice hydration
// -----------------------------------------------------------------------------

void load_practice_progress(Database& database, ai::CoachRequest& request) {
  if (request.mode != ai::CoachMode::quiz || !request.profile_id) return;
  const auto owner = database.player_profile_owner_id(*request.profile_id);
  for (const auto& row : database.ai_coach_skill_progress(owner)) {
    request.practice_progress.emplace(row.motif_id, ai::PracticeProgress{
        .independent_successes = row.independent_successes,
        .verified_weak_attempts = row.verified_weak_attempts,
        .guided_successes = row.guided_successes,
        .success_streak = row.success_streak,
        .schedule_level = row.schedule_level,
        .last_practiced_at = row.last_practiced_at,
        .interval_seconds = row.interval_seconds,
        .next_practice_at = row.next_practice_at});
  }
}

void select_personal_quiz_position(Database& database,
                                   ai::CoachRequest& request) {
  if (request.mode != ai::CoachMode::quiz || !request.profile_id ||
      !request.personal_training_requested) {
    return;
  }

  const auto owner = database.player_profile_owner_id(*request.profile_id);
  const auto payload = database.ai_chess_profile_payload(owner);
  if (!payload) return;
  const auto profile = ai::chess_profile_from_json(*payload);
  if (!profile) return;
  const auto selected = ai::select_personal_training_position(
      *profile, request.practice_progress, coach_unix_time_seconds());
  if (!selected) return;

  request.position_fen = selected->example.fen;
  request.personal_training_position_selected = true;
  request.context_id = selected->example.game_id;
  request.context_ply = selected->example.ply;
  if (const auto game = database.game(selected->example.game_id);
      game && !game->pgn.empty()) {
    request.game_pgn = game->pgn;
  }
  if (const auto source = database.player_profile_game_source(
          owner, selected->example.game_id);
      source && (source->player_color == "white" ||
                 source->player_color == "black")) {
    request.player_color = source->player_color;
  }
}

// -----------------------------------------------------------------------------
// Section: Existing KChess evidence bridge
// -----------------------------------------------------------------------------

std::string hint_settings_key(Database& database) {
  const auto settings = database.settings();
  return settings.engine_id + ":" + std::to_string(settings.sideline_depth) + ":" +
      std::to_string(settings.sideline_multi_pv) + ":" + std::to_string(settings.sideline_threads) + ":" +
      std::to_string(settings.sideline_hash_mb);
}

CachedCoachEngineResult cached_hint(Database& database, AnalysisService& service,
                                    CoachHintCache& cache, const std::string& fen) {
  const auto settings_key = hint_settings_key(database);
  {
    std::lock_guard lock(cache.mutex);
    if (cache.hint.fen == fen && cache.hint.settings_key == settings_key &&
        !cache.hint.result.empty() && !cache.hint.candidates.root_lines.empty()) {
      return {.result = cache.hint.result, .candidates = cache.hint.candidates};
    }
  }

  // Never hold the cache mutex while Stockfish is running. Foreground engine
  // ownership already serializes the expensive work; the cache only protects
  // immutable snapshots shared with retrieval.
  const auto result = service.coach_hint_json(fen);
  const auto hint = json::parse(result);
  auto snapshot = candidate_snapshot_from_json(hint, fen);
  if (snapshot.root_lines.empty() && hint.contains("moves")) {
    json legacy = hint;
    legacy["candidates"] = hint["moves"];
    snapshot = candidate_snapshot_from_json(legacy, fen);
  }
  {
    std::lock_guard lock(cache.mutex);
    cache.hint = {
        .fen = fen,
        .settings_key = settings_key,
        .result = result,
        .candidates = snapshot,
    };
  }
  return {.result = result, .candidates = std::move(snapshot)};
}

CachedCoachEngineResult cached_extreme_move(
    Database& database, AnalysisService& service, CoachHintCache& cache,
    const std::string& fen, const bool fastest_loss) {
  const auto settings_key = hint_settings_key(database);
  {
    std::lock_guard lock(cache.mutex);
    const auto& entry = fastest_loss ? cache.fastest_loss : cache.worst_move;
    if (entry.fen == fen && entry.settings_key == settings_key &&
        !entry.result.empty() && !entry.candidates.root_lines.empty()) {
      return {.result = entry.result, .candidates = entry.candidates};
    }
  }

  const auto result = service.coach_extreme_move_json(fen, fastest_loss);
  const auto payload = json::parse(result);
  auto snapshot = candidate_snapshot_from_json(payload, fen);
  if (snapshot.root_lines.empty()) return {.result = result};

  {
    std::lock_guard lock(cache.mutex);
    auto& entry = fastest_loss ? cache.fastest_loss : cache.worst_move;
    entry = {
        .fen = fen,
        .settings_key = settings_key,
        .result = result,
        .candidates = snapshot,
    };
  }
  return {.result = result, .candidates = std::move(snapshot)};
}

json position_analysis(const ai::CoachRequest& request, AnalysisService& service) {
  json analysis;
  if (request.variation_job_id) {
    analysis = json::parse(service.variation_analysis_status_json(*request.variation_job_id));
    analysis["analyzedFen"] = analysis.value("fen", "");
  } else if (request.context_id && request.context_ply) {
    analysis = json::parse(service.move_analysis_status_json(*request.context_id, *request.context_ply));
  }
  if (!analysis.is_object() || !request.position_fen ||
      analysis.value("analyzedFen", "") != *request.position_fen) return json{};
  return analysis;
}

ai::EvidenceSources make_sources(Database& database, AnalysisService& analysis_service,
                                knowledge::KnowledgeRuntime& knowledge_runtime,
                                std::shared_ptr<CoachHintCache> hint_cache) {
  ai::EvidenceSources sources;
  sources.engine_candidates = [&database, &analysis_service, hint_cache](
      const ai::CoachRequest& request, const ai::QueryPlan& plan)
      -> std::optional<ai::EngineCandidateBundle> {
    // Only an explicit user's question may spend fresh engine work. Automatic
    // coaching consumes the analysis that triggered it and never starts a search.
    if (!request.position_fen || request.automatic_turn) return std::nullopt;
    try {
      if (request.verdict_challenge.has_value()) {
        std::string result;
        if (request.verdict_challenge->challenged_move_uci.has_value()) {
          result = analysis_service.coach_move_review_json(
              *request.position_fen,
              *request.verdict_challenge->challenged_move_uci);
        } else {
          result = analysis_service.coach_hint_json(*request.position_fen);
        }
        const auto payload = json::parse(result);
        auto snapshot = candidate_snapshot_from_json(payload, *request.position_fen);
        if (snapshot.root_lines.empty()) return std::nullopt;
        return ai::EngineCandidateBundle{
            .engine_evidence = {
                .kind = ai::EvidenceKind::engine,
                .payload = std::move(result),
                .confidence = 1.0},
            .candidate_snapshot = std::move(snapshot)};
      }
      if (plan.analysis_mode == ai::PositionAnalysisMode::worst_move ||
          plan.analysis_mode == ai::PositionAnalysisMode::fastest_loss) {
        auto cached = cached_extreme_move(
            database, analysis_service, *hint_cache, *request.position_fen,
            plan.analysis_mode == ai::PositionAnalysisMode::fastest_loss);
        if (cached.candidates.root_lines.empty()) return std::nullopt;
        return ai::EngineCandidateBundle{
            .engine_evidence = {
                .kind = ai::EvidenceKind::engine,
                .payload = std::move(cached.result),
                .confidence = 1.0},
            .candidate_snapshot = std::move(cached.candidates)};
      }
      auto cached = cached_hint(
          database, analysis_service, *hint_cache, *request.position_fen);
      if (cached.candidates.root_lines.empty()) return std::nullopt;
      return ai::EngineCandidateBundle{
          .engine_evidence = {
              .kind = ai::EvidenceKind::engine,
              .payload = std::move(cached.result),
              .confidence = 1.0},
          .candidate_snapshot = std::move(cached.candidates)};
    } catch (...) {
      return std::nullopt;
    }
  };
  sources.existing_candidates = [&database, &analysis_service, hint_cache](const ai::CoachRequest& request,
      const ai::QueryPlan&) -> std::optional<ai::CandidateMoveSnapshot> {
    const auto key = hint_settings_key(database);
    {
      std::lock_guard lock(hint_cache->mutex);
      if (request.position_fen && *request.position_fen == hint_cache->hint.fen &&
          key == hint_cache->hint.settings_key &&
          !hint_cache->hint.candidates.root_lines.empty()) {
        return hint_cache->hint.candidates;
      }
    }
    if (!request.position_fen) return std::nullopt;
    try {
      const auto analysis = position_analysis(request, analysis_service);
      if (!analysis.is_object()) return std::nullopt;
      ai::CandidateMoveSnapshot snapshot;
      for (const auto& raw : analysis.value("lines", json::array())) {
        ai::CandidateLineInput line;
        line.rank = raw.value("rank", 0);
        line.pv_uci = raw.value("moves", std::vector<std::string>{});
        if (line.pv_uci.empty()) continue;
        line.move_uci = line.pv_uci.front();
        (void)apply_legal_uci_move(*request.position_fen, line.move_uci);
        if (raw.contains("evaluationCp") && raw["evaluationCp"].is_number_integer())
          line.evaluation_cp = raw["evaluationCp"].get<int>();
        if (raw.contains("mateIn") && raw["mateIn"].is_number_integer())
          line.mate_in = raw["mateIn"].get<int>();
        line.expected_score = root_expected_score(raw, *request.position_fen);
        snapshot.root_lines.push_back(std::move(line));
      }
      if (!snapshot.root_lines.empty()) return snapshot;
    } catch (...) {
      return std::nullopt;
    }
    return std::nullopt;
  };
  sources.existing_analysis = [&analysis_service](
                                  const ai::CoachRequest& request,
                                  const ai::QueryPlan&) -> std::optional<ai::EvidenceItem> {
    if (!request.variation_job_id && (!request.context_id || !request.context_ply || *request.context_ply < 0)) {
      return std::nullopt;
    }
    try {
      const auto analysis = position_analysis(request, analysis_service);
      if (!analysis.is_object()) return std::nullopt;
      return ai::EvidenceItem{
          .kind = ai::EvidenceKind::existing_analysis,
          .payload = analysis.dump(),
          .confidence = 1.0,
      };
    } catch (...) {
      return std::nullopt;
    }
  };
  sources.completed_move_analysis = [&database, &analysis_service](
      const ai::CoachRequest& request,
      const std::string& original_fen) -> std::optional<ai::EvidenceItem> {
    if (!request.context_id || !request.context_ply ||
        *request.context_ply <= 0 || !request.position_fen ||
        !request.user_move_uci) return std::nullopt;
    try {
      const auto game = database.game(*request.context_id);
      const auto previous_ply = *request.context_ply - 1;
      if (!game || previous_ply >= static_cast<int>(game->moves.size()))
        return std::nullopt;
      const auto& move = game->moves[static_cast<std::size_t>(previous_ply)];
      if (move.ply_index != previous_ply || move.uci != *request.user_move_uci ||
          move.fen_before != original_fen ||
          move.fen_after != *request.position_fen) return std::nullopt;

      auto before_request = request;
      before_request.position_fen = original_fen;
      before_request.context_ply = previous_ply;
      before_request.variation_job_id.reset();
      const auto analysis = position_analysis(before_request, analysis_service);
      if (!analysis.is_object() || !analysis.value("qualityComplete", false) ||
          !analysis.value("classification", json{}).is_string() ||
          !analysis.value("expectedScoreBest", json{}).is_number() ||
          !analysis.value("expectedScorePlayed", json{}).is_number() ||
          !analysis.value("expectedScoreLoss", json{}).is_number() ||
          analysis.value("engineVersion", "").empty() ||
          analysis.value("configHash", "").empty()) return std::nullopt;
      return ai::EvidenceItem{
          .id = "analysis.completed_move.v1",
          .kind = ai::EvidenceKind::existing_analysis,
          .payload = analysis.dump(),
          .confidence = 1.0};
    } catch (...) {
      return std::nullopt;
    }
  };
  sources.opening = [&database](const ai::CoachRequest& request,
                                const ai::QueryPlan&) -> std::optional<ai::EvidenceItem> {
    if (!request.context_id) return std::nullopt;
    const auto game = database.game(*request.context_id);
    if (!game || (!game->opening_eco && !game->opening_name)) return std::nullopt;
    json payload;
    if (game->opening_eco) payload["eco"] = *game->opening_eco;
    if (game->opening_name) payload["name"] = *game->opening_name;
    return ai::EvidenceItem{
        .kind = ai::EvidenceKind::opening,
        .payload = payload.dump(),
        .confidence = 1.0,
    };
  };
  sources.user_profile = [&knowledge_runtime](const ai::CoachRequest& request,
                                     const ai::QueryPlan& plan) {
    return knowledge_runtime.coach_evidence(request, plan);
  };
  sources.practicality_player = [&database](const ai::CoachRequest& request,
                                            const ai::QueryPlan&) {
    return coach_practicality_player(database, request.profile_id);
  };
  return sources;
}

// -----------------------------------------------------------------------------
// Section: Response transport
// -----------------------------------------------------------------------------

std::string provider_status(const std::string& error_code) {
  if (error_code.empty()) return "provider_unavailable";
  if (error_code.find("automatic_") != std::string::npos) {
    return "provider_deferred";
  }
  if (error_code.find("rpd") != std::string::npos ||
      error_code.find("daily") != std::string::npos) {
    return "provider_daily_limit";
  }
  if (error_code.find("rpm") != std::string::npos ||
      error_code.find("tpm") != std::string::npos ||
      error_code.find("rate_limited") != std::string::npos ||
      error_code.find("backoff") != std::string::npos) {
    return "provider_rate_limited";
  }
  if (error_code == "gemini_output_empty" ||
      error_code.starts_with("gemini_response_")) {
    return "provider_response_invalid";
  }
  if (error_code == "quiz_candidate_evidence_unavailable") {
    return "evidence_unavailable";
  }
  if (error_code == "provider_unavailable" ||
      error_code == "remote_transport_unavailable" ||
      error_code.find("config_") != std::string::npos ||
      error_code.find("api_key_missing") != std::string::npos ||
      error_code.find("provider_disabled") != std::string::npos) {
    return "provider_unavailable";
  }
  return "provider_error";
}

bool renderable_native_answer(const ai::CoachResponse& response) {
  return !response.native_answer_kind.empty() && !response.board_moves.empty();
}

bool renderable_response(const ai::CoachResponse& response) {
  return !response.answer.empty() || !response.follow_up_question.empty() ||
      !response.client_actions.empty() || renderable_native_answer(response);
}

std::string response_status(const ai::CoachResponse& response) {
  if (!response.accepted) return "off_topic";
  if (response.action_fulfillment_required &&
      !response.action_fulfillment_passed) {
    return "validation_failed";
  }
  if (!response.safe_fallback_kind.empty()) return "safe_fallback";
  if (!response.validation_passed && !response.validation_issues.empty()) {
    return "validation_failed";
  }
  if (!response.provider_error_code.empty()) {
    return provider_status(response.provider_error_code);
  }
  if (!response.validation_passed) return "validation_failed";
  if (!renderable_response(response)) return "provider_unavailable";
  return "ok";
}

std::string trace_status(const ai::CoachPipelineTrace& trace) {
  if (!trace.accepted) return "off_topic";
  if (trace.action_fulfillment_required && !trace.action_fulfillment_passed) {
    return "validation_failed";
  }
  if (!trace.safe_fallback_kind.empty()) return "safe_fallback";
  if (!trace.validation_passed && !trace.validation_issues.empty()) {
    return "validation_failed";
  }
  if (!trace.provider_error_code.empty()) {
    return provider_status(trace.provider_error_code);
  }
  if (!trace.validation_passed) return "validation_failed";
  if (!trace.response_renderable) return "provider_unavailable";
  return "ok";
}

json response_value(const ai::CoachResponse& response) {
  json output = {
      {"accepted", response.accepted},
      {"answer", response.answer},
      {"nativeAnswerKind", response.native_answer_kind},
      {"followUpQuestion", response.follow_up_question},
      {"validationPassed", response.validation_passed},
      {"validationRepaired", response.validation_repaired},
      {"validationIssues", response.validation_issues},
      {"evidenceIds", response.evidence_references},
      {"providerErrorCode", response.provider_error_code},
      {"safeFallbackKind", response.safe_fallback_kind},
      {"actionFulfillment", {
          {"required", response.action_fulfillment_required},
          {"passed", response.action_fulfillment_passed},
          {"issues", response.action_fulfillment_issues},
      }},
  };
  if (response.chess_verdict) {
    const auto& verdict = *response.chess_verdict;
    output["chessVerdict"] = {
        {"authoritative", verdict.authoritative},
        {"positionScope", verdict.position_scope},
        {"positionVerdict", std::string(ai::position_verdict_name(verdict.position_verdict))},
        {"moveVerdict", std::string(ai::move_verdict_name(verdict.move_verdict))},
        {"evaluatedMoveUci", verdict.evaluated_move_uci.value_or("")},
        {"basis", verdict.basis},
    };
    if (verdict.white_evaluation_cp) {
      output["chessVerdict"]["whiteEvaluationCp"] = *verdict.white_evaluation_cp;
    }
    if (verdict.white_mate_in) {
      output["chessVerdict"]["whiteMateIn"] = *verdict.white_mate_in;
    }
    if (verdict.move_loss_cp) {
      output["chessVerdict"]["moveLossCp"] = *verdict.move_loss_cp;
    }
    if (verdict.move_loss_expected_score) {
      output["chessVerdict"]["moveLossExpectedScore"] =
          *verdict.move_loss_expected_score;
    }
  }
  output["boardMoves"] = json::array();
  output["focusSquares"] = json::array();
  output["clientActions"] = json::array();
  for (const auto& action : response.client_actions) {
    json value{{"id", action.id}};
    if (action.elo.has_value()) value["elo"] = *action.elo;
    if (action.color.has_value()) value["color"] = *action.color;
    output["clientActions"].push_back(std::move(value));
  }
  if (response.accepted && response.validation_passed) {
    for (const auto& move : response.board_moves) {
      if (!move.empty() && output["boardMoves"].size() < 2 &&
          std::find(output["boardMoves"].begin(), output["boardMoves"].end(),
                    json(move)) == output["boardMoves"].end()) {
        output["boardMoves"].push_back(move);
      }
    }
    for (const auto& move : response.recommendations) {
      if (response.answer_type != ai::CoachAnswerType::quiz &&
          !move.move_uci.empty() && output["boardMoves"].size() < 2 &&
          std::find(output["boardMoves"].begin(), output["boardMoves"].end(),
                    json(move.move_uci)) == output["boardMoves"].end()) {
        output["boardMoves"].push_back(move.move_uci);
      }
    }
    for (const auto& claim : response.claims) {
      if (response.answer_type == ai::CoachAnswerType::comparison &&
          claim.kind == ai::CoachClaimKind::engine_evaluation && output["boardMoves"].size() < 2 &&
          std::find(output["boardMoves"].begin(), output["boardMoves"].end(), json(claim.subject)) == output["boardMoves"].end())
        output["boardMoves"].push_back(claim.subject);
      if (claim.kind == ai::CoachClaimKind::piece_on_square && claim.value != "empty" &&
          output["focusSquares"].size() < 4) output["focusSquares"].push_back(claim.subject);
    }
  }
  output["status"] = response_status(response);
  return output;
}


// -----------------------------------------------------------------------------
// Section: Durable Coach session persistence
// -----------------------------------------------------------------------------

json chess_verdict_state_value(const ai::ChessVerdictContract& verdict) {
  json out{
      {"authoritative", verdict.authoritative},
      {"positionScope", verdict.position_scope},
      {"positionVerdict", static_cast<int>(verdict.position_verdict)},
      {"moveVerdict", static_cast<int>(verdict.move_verdict)},
      {"basis", verdict.basis},
  };
  if (verdict.evaluated_move_uci) out["evaluatedMoveUci"] = *verdict.evaluated_move_uci;
  if (verdict.white_evaluation_cp) out["whiteEvaluationCp"] = *verdict.white_evaluation_cp;
  if (verdict.white_mate_in) out["whiteMateIn"] = *verdict.white_mate_in;
  if (verdict.move_loss_cp) out["moveLossCp"] = *verdict.move_loss_cp;
  if (verdict.move_loss_expected_score)
    out["moveLossExpectedScore"] = *verdict.move_loss_expected_score;
  return out;
}

std::optional<ai::ChessVerdictContract> chess_verdict_state_from_value(
    const json& value) {
  if (!value.is_object()) return std::nullopt;
  ai::ChessVerdictContract verdict;
  verdict.authoritative = value.value("authoritative", false);
  verdict.position_scope = value.value("positionScope", "current_position");
  const int position = value.value("positionVerdict", 0);
  const int move = value.value("moveVerdict", 0);
  if (position >= static_cast<int>(ai::PositionVerdict::unknown) &&
      position <= static_cast<int>(ai::PositionVerdict::black_winning)) {
    verdict.position_verdict = static_cast<ai::PositionVerdict>(position);
  }
  if (move >= static_cast<int>(ai::MoveVerdict::unknown) &&
      move <= static_cast<int>(ai::MoveVerdict::losing)) {
    verdict.move_verdict = static_cast<ai::MoveVerdict>(move);
  }
  verdict.evaluated_move_uci = optional_string(value, "evaluatedMoveUci");
  if (value.contains("whiteEvaluationCp") && value["whiteEvaluationCp"].is_number_integer())
    verdict.white_evaluation_cp = value["whiteEvaluationCp"].get<int>();
  if (value.contains("whiteMateIn") && value["whiteMateIn"].is_number_integer())
    verdict.white_mate_in = value["whiteMateIn"].get<int>();
  if (value.contains("moveLossCp") && value["moveLossCp"].is_number_integer())
    verdict.move_loss_cp = value["moveLossCp"].get<int>();
  if (value.contains("moveLossExpectedScore") && value["moveLossExpectedScore"].is_number())
    verdict.move_loss_expected_score = value["moveLossExpectedScore"].get<double>();
  verdict.basis = value.value("basis", "");
  return verdict.authoritative ? std::optional<ai::ChessVerdictContract>{verdict}
                               : std::nullopt;
}

json move_attribution_state_value(const ai::CoachMoveAttribution& move) {
  json out{
      {"moverRole", move.mover_role},
      {"verifiedLearnerMove", move.verified_learner_move},
  };
  if (move.previous_fen) out["previousFen"] = *move.previous_fen;
  if (move.current_fen) out["currentFen"] = *move.current_fen;
  if (move.played_move_uci) out["playedMoveUci"] = *move.played_move_uci;
  if (move.mover_color) out["moverColor"] = *move.mover_color;
  if (move.learner_color) out["learnerColor"] = *move.learner_color;
  if (move.classification) out["classification"] = *move.classification;
  if (move.expected_score_before) out["expectedScoreBefore"] = *move.expected_score_before;
  if (move.expected_score_played) out["expectedScorePlayed"] = *move.expected_score_played;
  if (move.expected_score_loss) out["expectedScoreLoss"] = *move.expected_score_loss;
  return out;
}

std::optional<ai::CoachMoveAttribution> move_attribution_state_from_value(
    const json& value) {
  if (!value.is_object()) return std::nullopt;
  ai::CoachMoveAttribution move;
  move.previous_fen = optional_string(value, "previousFen");
  move.current_fen = optional_string(value, "currentFen");
  move.played_move_uci = optional_string(value, "playedMoveUci");
  move.mover_color = optional_string(value, "moverColor");
  move.learner_color = optional_string(value, "learnerColor");
  move.classification = optional_string(value, "classification");
  if (value.contains("expectedScoreBefore") && value["expectedScoreBefore"].is_number())
    move.expected_score_before = value["expectedScoreBefore"].get<double>();
  if (value.contains("expectedScorePlayed") && value["expectedScorePlayed"].is_number())
    move.expected_score_played = value["expectedScorePlayed"].get<double>();
  if (value.contains("expectedScoreLoss") && value["expectedScoreLoss"].is_number())
    move.expected_score_loss = value["expectedScoreLoss"].get<double>();
  move.mover_role = value.value("moverRole", "unknown");
  move.verified_learner_move = value.value("verifiedLearnerMove", false);
  if (!move.previous_fen && !move.current_fen && !move.played_move_uci &&
      !move.classification) return std::nullopt;
  return move;
}

json coach_session_state_value(const ai::CoachSessionState& state) {
  return json{
      {"profileId", state.profile_id.value_or("")},
      {"queryFamily", static_cast<int>(state.query_family)},
      {"profileScope", {
          {"topics", state.profile_scope.topics},
          {"timeControls", state.profile_scope.time_controls},
          {"phases", state.profile_scope.phases},
          {"playerColors", state.profile_scope.player_colors},
          {"needsEndgameMaterialType", state.profile_scope.needs_endgame_material_type},
          {"wantsProof", state.profile_scope.wants_proof},
          {"compareScopes", state.profile_scope.compare_scopes},
      }},
      {"needsProfile", state.needs_profile},
      {"currentBoard", state.current_board.value_or("")},
      {"currentTopic", static_cast<int>(state.current_topic)},
      {"currentGoal", state.current_goal},
      {"recentDialogue", state.recent_dialogue},
      {"lastClaim", state.last_claim},
      {"lastRecommendation", state.last_recommendation},
      {"referencedConcept", state.referenced_concept},
      {"unresolvedQuestion", state.unresolved_question},
      {"questionBoard", state.question_board},
      {"expectedMove", state.expected_move},
      {"expectedReply", state.expected_reply},
      {"exerciseSkillId", state.exercise_skill_id},
      {"hintBoard", state.hint_board},
      {"hintLevel", state.hint_level},
      {"scorePendingMoveQuestion", state.score_pending_move_question},
      {"acceptableMoves", state.acceptable_moves},
      {"lastAttemptStatus", state.last_attempt_status},
      {"lastAttemptClassification", state.last_attempt_classification},
      {"lastChessVerdict", state.last_chess_verdict
          ? chess_verdict_state_value(*state.last_chess_verdict) : json(nullptr)},
      {"lastMoveAttribution", state.last_move_attribution
          ? move_attribution_state_value(*state.last_move_attribution) : json(nullptr)},
      {"lastVerdictFen", state.last_verdict_fen.value_or("")},
  };
}

std::optional<ai::CoachSessionState> coach_session_state_from_value(
    const json& value, const std::string& profile_id) {
  if (!value.is_object()) return std::nullopt;
  ai::CoachSessionState state;
  state.profile_id = profile_id;
  const int family = value.value("queryFamily", 0);
  if (family >= static_cast<int>(ai::QueryFamily::unknown) &&
      family <= static_cast<int>(ai::QueryFamily::position)) {
    state.query_family = static_cast<ai::QueryFamily>(family);
  }
  if (value.contains("profileScope") && value["profileScope"].is_object()) {
    const auto& scope = value["profileScope"];
    state.profile_scope.topics = scope.value("topics", std::vector<std::string>{});
    state.profile_scope.time_controls =
        scope.value("timeControls", std::vector<std::string>{});
    state.profile_scope.phases = scope.value("phases", std::vector<std::string>{});
    state.profile_scope.player_colors =
        scope.value("playerColors", std::vector<std::string>{});
    state.profile_scope.needs_endgame_material_type =
        scope.value("needsEndgameMaterialType", false);
    state.profile_scope.wants_proof = scope.value("wantsProof", false);
    state.profile_scope.compare_scopes = scope.value("compareScopes", false);
  }
  state.needs_profile = value.value("needsProfile", false);
  if (const auto board = optional_string(value, "currentBoard")) {
    state.current_board = *board;
  }
  const int topic = value.value("currentTopic", 0);
  if (topic >= static_cast<int>(ai::CoachIntent::unknown) &&
      topic <= static_cast<int>(ai::CoachIntent::off_topic)) {
    state.current_topic = static_cast<ai::CoachIntent>(topic);
  }
  state.current_goal = value.value("currentGoal", "");
  state.recent_dialogue =
      value.value("recentDialogue", std::vector<std::string>{});
  if (state.recent_dialogue.size() > 8) {
    state.recent_dialogue.erase(
        state.recent_dialogue.begin(), state.recent_dialogue.end() - 8);
  }
  state.last_claim = value.value("lastClaim", "");
  state.last_recommendation = value.value("lastRecommendation", "");
  state.referenced_concept = value.value("referencedConcept", "");
  state.unresolved_question = value.value("unresolvedQuestion", "");
  state.question_board = value.value("questionBoard", "");
  state.expected_move = value.value("expectedMove", "");
  state.expected_reply = value.value("expectedReply", "");
  state.exercise_skill_id = value.value("exerciseSkillId", "");
  state.hint_board = value.value("hintBoard", "");
  state.hint_level = std::clamp(value.value("hintLevel", 0), 0, 4);
  state.score_pending_move_question =
      value.value("scorePendingMoveQuestion", false);
  state.acceptable_moves =
      value.value("acceptableMoves", std::vector<std::string>{});
  if (state.acceptable_moves.size() > 3) state.acceptable_moves.resize(3);
  state.last_attempt_status = value.value("lastAttemptStatus", "");
  state.last_attempt_classification =
      value.value("lastAttemptClassification", "");
  if (value.contains("lastChessVerdict"))
    state.last_chess_verdict = chess_verdict_state_from_value(value["lastChessVerdict"]);
  if (value.contains("lastMoveAttribution"))
    state.last_move_attribution =
        move_attribution_state_from_value(value["lastMoveAttribution"]);
  if (const auto fen = optional_string(value, "lastVerdictFen"))
    state.last_verdict_fen = *fen;
  return state.empty() ? std::nullopt
                       : std::optional<ai::CoachSessionState>(std::move(state));
}

void restore_persisted_coach_session(
    Database& database, ai::CoachOrchestrator& orchestrator,
    const std::optional<std::string>& session_id,
    const std::optional<std::string>& profile_id) {
  if (!session_id || !profile_id) return;
  if (orchestrator.session_state(session_id, profile_id).has_value()) {
    database.touch_coach_session(*profile_id, *session_id,
                                 coach_unix_time_seconds());
    return;
  }
  const auto record = database.coach_session(*profile_id, *session_id);
  if (!record) return;
  database.touch_coach_session(*profile_id, *session_id,
                               coach_unix_time_seconds());
  try {
    const auto persisted = json::parse(record->compact_state_json);
    if (auto state = coach_session_state_from_value(persisted, *profile_id)) {
      orchestrator.restore_session(*session_id, *state);
    }
  } catch (...) {
    // A damaged compact continuation snapshot must not make the durable chat
    // transcript unusable. The next successful turn rewrites a valid snapshot.
  }
}

void ensure_persistent_coach_session(
    Database& database, const ai::CoachRequest& request) {
  if (!request.session_id || !request.profile_id) return;
  database.ensure_coach_session(*request.profile_id, *request.session_id,
                                coach_unix_time_seconds());
}

void persist_coach_turn(Database& database, ai::CoachOrchestrator& orchestrator,
                        const ai::CoachRequest& request,
                        const ai::CoachResponse& response) {
  if (!request.session_id || !request.profile_id) return;
  const auto now = coach_unix_time_seconds();
  database.ensure_coach_session(*request.profile_id, *request.session_id, now);

  if (!request.automatic_turn && !request.user_text.empty()) {
    database.append_coach_session_message(
        *request.profile_id, *request.session_id, "user", request.user_text,
        "{}", false, now);
  }

  const bool durable_assistant_message =
      response.accepted &&
      (!response.answer.empty() || !response.native_answer_kind.empty() ||
       !response.safe_fallback_kind.empty());
  if (durable_assistant_message) {
    database.append_coach_session_message(
        *request.profile_id, *request.session_id, "assistant", response.answer,
        response_value(response).dump(), request.automatic_turn, now);
  }

  if (const auto state =
          orchestrator.session_state(request.session_id, request.profile_id)) {
    database.save_coach_session_state(
        *request.profile_id, *request.session_id,
        coach_session_state_value(*state).dump(), now);
  }
}

json coach_session_record_value(const CoachSessionRecord& record) {
  return json{
      {"id", record.id},
      {"sessionNumber", record.session_number},
      {"name", record.name},
      {"createdAt", record.created_at},
      {"updatedAt", record.updated_at},
      {"lastOpenedAt", record.last_opened_at},
  };
}

json coach_session_message_value(const CoachSessionMessageRecord& record) {
  json payload = json::object();
  try {
    const auto parsed = json::parse(record.payload_json);
    if (parsed.is_object()) payload = parsed;
  } catch (...) {
    // Transcript text stays usable even if old structured metadata is damaged.
  }
  return json{
      {"sequence", record.sequence},
      {"role", record.role},
      {"content", record.content},
      {"payload", std::move(payload)},
      {"automaticTurn", record.automatic_turn},
      {"createdAt", record.created_at},
  };
}

// -----------------------------------------------------------------------------
// Section: Automatic turn mapping
// -----------------------------------------------------------------------------

std::vector<std::string> reason_names(const ai::AutomaticCoachDecision& decision) {
  std::vector<std::string> names;
  names.reserve(decision.reasons.size());
  for (const auto reason : decision.reasons) {
    names.emplace_back(ai::automatic_coach_reason_name(reason));
  }
  return names;
}

json automatic_gate_value(const ai::AutomaticCoachDecision& decision) {
  return json{
      {"primaryReasonPresent", decision.primary_reason_present},
      {"criticalOverride", decision.critical_override},
      {"recencyBlocked", decision.recency_blocked},
      {"minimumTeachingValue", decision.minimum_teaching_value}};
}

bool strong_completed_move_classification(std::string_view classification) {
  return classification == "theory" || classification == "forced" ||
      classification == "brilliant" || classification == "critical" ||
      classification == "best" || classification == "excellent";
}

bool sound_completed_move_classification(std::string_view classification) {
  return classification == "good" || classification == "okay";
}

bool weak_completed_move_classification(std::string_view classification) {
  return classification == "inaccuracy" || classification == "mistake" ||
      classification == "blunder" || classification == "miss";
}

std::string automatic_prompt(const ai::AutomaticCoachDecision& decision,
                             const bool answering_question,
                             const bool user_played_move,
                             std::string_view native_classification) {
  std::ostringstream prompt;
  if (answering_question) {
    prompt << "The learner just played a legal answer to your open board question. "
              "Use the verified attempt feedback from session context before anything else. ";
    if (strong_completed_move_classification(native_classification)) {
      prompt << "Native completed-move analysis classified the learner's move as "
             << native_classification
             << ". Treat this as a successful strong answer even if it differed from "
                "the small candidate set retained when the quiz was asked. Start with "
                "clear positive feedback and explain one concrete supported reason; do "
                "not describe the move as merely legal or imply that it was inferior. ";
    } else if (sound_completed_move_classification(native_classification)) {
      prompt << "Native completed-move analysis classified the learner's move as "
             << native_classification
             << ". Treat it as a sound move and do not imply a mistake, but do not "
                "claim the exact exercise was solved unless session feedback marks a "
                "successful candidate. ";
    } else if (weak_completed_move_classification(native_classification)) {
      prompt << "Native completed-move analysis classified the learner's move as "
             << native_classification
             << ". Explain one concrete supported problem without shaming the learner. ";
    } else {
      prompt << "If session feedback says legal_alternative_not_graded, stay strictly "
                "neutral: do not imply the move was weak, inferior, or a mistake. ";
    }
    prompt << "Explain a native candidate or opponent resource only when supplied. "
              "Do not start another quiz in this reply.";
    return prompt.str();
  }
  prompt << (user_played_move ? "Coach the learner's verified played move naturally. "
                              : "Explain the selected line move without assuming the learner played it. ")
         << "Explain the concrete chess "
            "idea or opponent resource supported by the supplied position and analysis. ";
  if (decision.reasons.empty()) {
    prompt << "No special event was verified; do not assign a move label or invent a tactic. ";
  } else {
    prompt << "The native trigger reasons are: ";
    bool first = true;
    for (const auto reason : decision.reasons) {
      if (!first) prompt << ", ";
      prompt << ai::automatic_coach_reason_name(reason);
      first = false;
    }
    prompt << ". ";
  }
  prompt << "Use only supplied chess evidence. Ask a question only when the "
            "position offers a verified decision for the learner to examine; "
            "otherwise give a useful explanation without a question.";
  return prompt.str();
}

}  // namespace

// -----------------------------------------------------------------------------
// Section: Coach service lifecycle
// -----------------------------------------------------------------------------

CoachService::CoachService(Database& database, AnalysisService& analysis_service,
                           knowledge::KnowledgeRuntime& knowledge_runtime)
    : database_(database),
      analysis_service_(analysis_service),
      hint_cache_(std::make_shared<CoachHintCache>()),
      orchestrator_(make_sources(database, analysis_service, knowledge_runtime, hint_cache_),
                    ai::make_gemini_provider(),
                    [&database](const ai::CoachLearningAttempt& attempt) {
                      if (!attempt.profile_id || attempt.skill_id.empty()) return;
                      const auto owner =
                          database.player_profile_owner_id(*attempt.profile_id);
                      ai::PracticeProgress previous;
                      for (const auto& row : database.ai_coach_skill_progress(owner)) {
                        if (row.motif_id != attempt.skill_id) continue;
                        previous = ai::PracticeProgress{
                            .independent_successes = row.independent_successes,
                            .verified_weak_attempts = row.verified_weak_attempts,
                            .guided_successes = row.guided_successes,
                            .success_streak = row.success_streak,
                            .schedule_level = row.schedule_level,
                            .last_practiced_at = row.last_practiced_at,
                            .interval_seconds = row.interval_seconds,
                            .next_practice_at = row.next_practice_at};
                        break;
                      }
                      const auto scheduled = ai::schedule_practice_attempt(
                          previous, attempt.independent_success,
                          coach_unix_time_seconds());
                      database.record_ai_coach_skill_attempt(
                          owner, attempt.skill_id, attempt.independent_success,
                          scheduled.verified_weak_attempts,
                          scheduled.guided_successes, scheduled.success_streak,
                          scheduled.schedule_level, scheduled.interval_seconds,
                          scheduled.next_practice_at, scheduled.last_practiced_at);
                    },
                    [this](const ai::CoachPipelineTrace& trace) {
                      record_performance_trace(trace);
                    }) {}

CoachService::~CoachService() {
  std::vector<std::shared_ptr<CoachJob>> jobs;
  {
    std::lock_guard lock(jobs_mutex_);
    for (const auto& [id, job] : jobs_) {
      (void)id;
      job->cancelled = true;
      jobs.push_back(job);
    }
  }
  for (const auto& job : jobs) {
    if (job->worker.joinable()) job->worker.join();
  }
}

std::string CoachService::ask_json(const std::string& request_json) {
  return ask_json_impl(request_json, nullptr);
}

std::string CoachService::sessions_json(const std::string& profile_id) const {
  if (profile_id.empty()) {
    throw std::invalid_argument("Coach sessions require profile id");
  }
  json sessions = json::array();
  for (const auto& record : database_.coach_sessions(profile_id)) {
    sessions.push_back(coach_session_record_value(record));
  }
  return json{{"schema", "coach.sessions.v1"}, {"sessions", std::move(sessions)}}.dump();
}

std::string CoachService::create_session_json(const std::string& profile_id) {
  if (profile_id.empty()) {
    throw std::invalid_argument("Coach session requires profile id");
  }
  const auto record =
      database_.create_coach_session(profile_id, coach_unix_time_seconds());
  return json{{"schema", "coach.session.v1"},
              {"session", coach_session_record_value(record)}}.dump();
}

std::string CoachService::session_messages_json(
    const std::string& request_json) const {
  const auto request = json::parse(request_json);
  const auto profile_id = request.value("profileId", "");
  const auto session_id = request.value("sessionId", "");
  if (profile_id.empty() || session_id.empty()) {
    throw std::invalid_argument(
        "Coach session messages require profileId and sessionId");
  }
  const auto session = database_.coach_session(profile_id, session_id);
  if (!session) {
    throw std::invalid_argument("Coach session not found");
  }
  json messages = json::array();
  for (const auto& record :
       database_.coach_session_messages(profile_id, session_id)) {
    messages.push_back(coach_session_message_value(record));
  }
  database_.touch_coach_session(profile_id, session_id,
                                coach_unix_time_seconds());
  std::string current_position_fen;
  try {
    const auto state = json::parse(session->compact_state_json);
    current_position_fen = state.value("currentBoard", "");
  } catch (...) {
    // The transcript remains readable even if the continuation snapshot is bad.
  }
  return json{{"schema", "coach.session_messages.v1"},
              {"session", coach_session_record_value(*session)},
              {"currentPositionFen", current_position_fen},
              {"messages", std::move(messages)}}.dump();
}

std::string CoachService::rename_session_json(
    const std::string& request_json) {
  const auto request = json::parse(request_json);
  const auto profile_id = request.value("profileId", "");
  const auto session_id = request.value("sessionId", "");
  auto name = request.value("name", "");
  const auto first = name.find_first_not_of(" \t\r\n");
  const auto last = name.find_last_not_of(" \t\r\n");
  name = first == std::string::npos ? std::string{}
                                    : name.substr(first, last - first + 1);
  if (profile_id.empty() || session_id.empty() || name.empty()) {
    throw std::invalid_argument(
        "Coach session rename requires profileId, sessionId and name");
  }
  database_.rename_coach_session(profile_id, session_id, name,
                                 coach_unix_time_seconds());
  const auto session = database_.coach_session(profile_id, session_id);
  if (!session) throw std::invalid_argument("Coach session not found");
  return json{{"schema", "coach.session.v1"},
              {"session", coach_session_record_value(*session)}}.dump();
}

std::string CoachService::delete_session_json(
    const std::string& request_json) {
  const auto request = json::parse(request_json);
  const auto profile_id = request.value("profileId", "");
  const auto session_id = request.value("sessionId", "");
  if (profile_id.empty() || session_id.empty()) {
    throw std::invalid_argument(
        "Coach session delete requires profileId and sessionId");
  }
  database_.delete_coach_session(profile_id, session_id);
  return json{{"schema", "coach.session_delete.v1"},
              {"deleted", true},
              {"sessionId", session_id}}.dump();
}


std::string CoachService::ask_json_impl(
    const std::string& request_json, const std::atomic_bool* cancelled) {
  cancel_automatic_jobs_for_foreground();
  if (cancelled != nullptr && cancelled->load()) {
    return json{{"status", "cancelled"}}.dump();
  }
  const auto value = json::parse(request_json);
  auto request = parse_request(value, database_);
  hydrate_coach_native_context_state(database_, request);
  load_practice_progress(database_, request);
  select_personal_quiz_position(database_, request);
  ensure_persistent_coach_session(database_, request);
  restore_persisted_coach_session(
      database_, orchestrator_, request.session_id, request.profile_id);
  if (!acquire_execution_slot(JobKind::ask, cancelled)) {
    return json{{"status", "cancelled"}}.dump();
  }
  try {
    const auto response = orchestrator_.handle(request);
    persist_coach_turn(database_, orchestrator_, request, response);
    auto output = response_value(response);
    output["positionFen"] = request.position_fen.value_or("");
    release_execution_slot();
    return output.dump();
  } catch (...) {
    release_execution_slot();
    throw;
  }
}

std::string CoachService::context_json(const std::string& request_json) const {
  const auto value = json::parse(request_json);
  if (const auto fen = optional_string(value, "fen")) {
    return json{{"position", json::parse(position_view_json(*fen))}}.dump();
  }
  if (const auto pgn = optional_string(value, "pgn")) {
    const auto parsed = parse_pgn(*pgn);
    if (!parsed.valid) throw std::invalid_argument(parsed.error);
    json positions = json::array();
    json moves = json::array();
    positions.push_back(json::parse(position_view_json(parsed.game.initial_fen)));
    for (const auto& move : parsed.game.moves) {
      positions.push_back(json::parse(position_view_json(move.fen_after)));
      moves.push_back(move.uci);
    }
    return json{{"position", positions.front()},
                {"positions", std::move(positions)},
                {"moves", std::move(moves)},
                {"ply", -1}}.dump();
  }
  throw std::invalid_argument("Coach context requires FEN or PGN");
}

std::string CoachService::automatic_json(const std::string& request_json) {
  return automatic_json_impl(request_json, nullptr);
}

std::string CoachService::automatic_json_impl(
    const std::string& request_json, const std::atomic_bool* cancelled) {
  const auto value = json::parse(request_json);
  const auto context_id = optional_string(value, "contextId");
  const auto variation_id = optional_string(value, "variationJobId");
  const int ply = value.value("contextPly", 0);
  const auto game = context_id ? database_.game(*context_id) : std::nullopt;
  if (!variation_id && (!game || ply <= 0 || static_cast<std::size_t>(ply) > game->moves.size())) {
    return json{{"status", "skipped"}, {"triggered", false}, {"reasons", json::array()}}.dump();
  }

  ai::AutomaticCoachEvent event;
  std::string played_move;
  std::optional<double> expected_score_before;
  std::optional<double> expected_score_played;
  if (!variation_id) {
    event.previous_fen = ply == 1
        ? std::optional<std::string>(game->starting_fen)
        : std::optional<std::string>(game->moves[static_cast<std::size_t>(ply - 2)].fen_after);
    event.current_fen = game->moves[static_cast<std::size_t>(ply - 1)].fen_after;
    played_move = game->moves[static_cast<std::size_t>(ply - 1)].uci;
  }

  try {
    const auto analysis = json::parse(variation_id
        ? analysis_service_.variation_analysis_status_json(*variation_id)
        : analysis_service_.move_analysis_status_json(*context_id, ply));
    if (variation_id) {
      if (analysis.value("status", "") == "running" || analysis.value("classification", json{}).is_null())
        return json{{"status", "skipped"}, {"triggered", false}}.dump();
      event.current_fen = analysis.at("fen").get<std::string>();
      played_move = analysis.at("playedMove").get<std::string>();
      event.previous_fen = optional_string(value, "previousFen");
      if (!event.previous_fen || apply_legal_uci_move(*event.previous_fen, played_move).fen_after != *event.current_fen)
        return json{{"status", "skipped"}, {"triggered", false}}.dump();
    }
    if (analysis.contains("classification") && analysis["classification"].is_string()) {
      event.classification = analysis["classification"].get<std::string>();
    }
    if (analysis.contains("expectedScoreBefore") &&
        analysis["expectedScoreBefore"].is_number()) {
      expected_score_before = analysis["expectedScoreBefore"].get<double>();
    }
    if (analysis.contains("expectedScorePlayed") &&
        analysis["expectedScorePlayed"].is_number()) {
      expected_score_played = analysis["expectedScorePlayed"].get<double>();
    }
    if (analysis.contains("expectedScoreLoss") &&
        analysis["expectedScoreLoss"].is_number()) {
      event.expected_score_loss = analysis["expectedScoreLoss"].get<double>();
    }
  } catch (...) {
    // Phase/motif triggers remain available even before persisted engine analysis.
    if (variation_id) return json{{"status", "skipped"}, {"triggered", false}}.dump();
  }

  const auto requested_profile_id = optional_string(value, "profileId");
  const auto active_profile = database_.active_profile();
  const std::string profile_id = requested_profile_id.value_or(
      game ? game->profile_id : active_profile ? active_profile->id : "");
  if (!event.classification.empty() && !profile_id.empty()) {
    event.repeated_personal_mistake = coach_repeated_personal_mistake(
        database_, profile_id, event.classification);
  }

  const auto now_seconds = coach_unix_time_seconds();
  if (!profile_id.empty()) {
    const auto owner = database_.player_profile_owner_id(profile_id);
    double strongest_due = 0.0;
    for (const auto& row : database_.ai_coach_skill_progress(owner)) {
      const ai::PracticeProgress progress{
          .independent_successes = row.independent_successes,
          .verified_weak_attempts = row.verified_weak_attempts,
          .guided_successes = row.guided_successes,
          .success_streak = row.success_streak,
          .schedule_level = row.schedule_level,
          .last_practiced_at = row.last_practiced_at,
          .interval_seconds = row.interval_seconds,
          .next_practice_at = row.next_practice_at};
      strongest_due = std::max(
          strongest_due, ai::practice_due_priority(progress, now_seconds));
    }
    event.due_practice_relevance = std::clamp(strongest_due / 2.0, 0.0, 1.0);
  }

  const auto session_id = optional_string(value, "sessionId");
  const std::string automatic_history_key = session_id
      ? "session:" + *session_id
      : !profile_id.empty() ? "profile:" + profile_id : "global";
  event.seconds_since_last_automatic =
      seconds_since_last_automatic(automatic_history_key, now_seconds);

  auto decision = ai::AutomaticCoachTrigger{}.decide(event);
  bool user_played_move = false;
  const auto reported_before = optional_string(value, "playedMoveFenBefore");
  const auto reported_uci = optional_string(value, "playedMoveUci");
  if (reported_before && reported_uci && event.previous_fen &&
      event.current_fen && *reported_before == *event.previous_fen &&
      *reported_uci == played_move) {
    try {
      user_played_move = apply_legal_uci_move(*reported_before, *reported_uci)
                             .fen_after == *event.current_fen;
    } catch (...) {
      // A UI selection or mismatched stale board is not a completed move.
    }
  }
  const auto learner_color = optional_string(value, "playerColor");
  std::optional<std::string> mover_color;
  if (event.previous_fen) {
    mover_color = fen_white_to_move(*event.previous_fen) ? "white" : "black";
  }
  std::string mover_role = "unknown";
  if (mover_color && learner_color &&
      (*learner_color == "white" || *learner_color == "black")) {
    mover_role = *mover_color == *learner_color ? "learner" : "opponent";
  }
  const bool verified_learner_move = user_played_move && mover_role == "learner";
  const auto persistent_profile_id = profile_id.empty()
      ? std::nullopt
      : std::optional<std::string>(profile_id);
  restore_persisted_coach_session(
      database_, orchestrator_, session_id, persistent_profile_id);
  const bool answering_question = verified_learner_move &&
      orchestrator_.has_pending_move_question(
          session_id, profile_id.empty() ? std::nullopt
                                         : std::optional<std::string>(profile_id),
          event.previous_fen);
  if (answering_question) {
    decision.trigger = true;
    decision.teaching_value = std::max(decision.teaching_value, 0.9);
    decision.priority = decision.teaching_value;
  }
  const auto reasons = reason_names(decision);
  if (cancelled != nullptr && cancelled->load()) {
    return json{{"status", "cancelled"}, {"triggered", false},
                {"reasons", reasons}}.dump();
  }
  if (!decision.trigger) {
    return json{
        {"status", "skipped"},
        {"triggered", false},
        {"reasons", reasons},
        {"priority", decision.priority},
        {"teachingValue", {
            {"objectiveImportance", decision.objective_importance},
            {"personalRelevance", decision.personal_relevance},
            {"practiceRelevance", decision.practice_relevance},
            {"interruptionCost", decision.interruption_cost},
            {"value", decision.teaching_value}}},
        {"triggerGate", automatic_gate_value(decision)},
    }.dump();
  }

  ai::CoachRequest request;
  request.user_text = automatic_prompt(decision, answering_question,
                                       verified_learner_move, event.classification);
  // A move answering an open trainer question arrives through the Automatic
  // endpoint, but it is interactive foreground feedback, not an unsolicited
  // background Coach turn. Treat it as foreground for provider quota/teaching
  // policy so Automatic soft guards cannot silently swallow the learner reply.
  request.automatic_turn = !answering_question;
  request.locale = value.value("locale", "en");
  request.mode = ai::CoachMode::explain;
  request.depth = ai::ResponseDepth::concise;
  request.position_fen = event.current_fen;
  request.move_attribution = ai::CoachMoveAttribution{
      .previous_fen = event.previous_fen,
      .current_fen = event.current_fen,
      .played_move_uci = played_move.empty()
          ? std::nullopt
          : std::optional<std::string>(played_move),
      .mover_color = mover_color,
      .learner_color = learner_color,
      .mover_role = mover_role,
      .classification = event.classification.empty()
          ? std::nullopt
          : std::optional<std::string>(event.classification),
      .expected_score_before = expected_score_before,
      .expected_score_played = expected_score_played,
      .expected_score_loss = event.expected_score_loss,
      .verified_learner_move = verified_learner_move,
  };
  if (verified_learner_move) request.user_move_uci = played_move;
  if (verified_learner_move && !event.classification.empty()) {
    request.user_move_classification = event.classification;
  }
  request.user_move_success_confirmed = verified_learner_move &&
      strong_completed_move_classification(event.classification);
  request.user_move_error_confirmed = verified_learner_move &&
      !request.user_move_success_confirmed &&
      (weak_completed_move_classification(event.classification) ||
       event.expected_score_loss.value_or(0.0) >= 0.15);
  if (game && !game->pgn.empty()) request.game_pgn = game->pgn;
  request.session_id = session_id;
  hydrate_coach_native_context_state(database_, request);
  // The played move belongs to previous_fen, never to the current candidate root.
  request.surface = optional_string(value, "surface");
  request.context_id = context_id;
  request.context_ply = ply;
  request.profile_id = profile_id.empty() ? std::nullopt : std::optional<std::string>(profile_id);
  request.variation_job_id = variation_id;
  request.player_color = learner_color;
  ensure_persistent_coach_session(database_, request);

  if (cancelled != nullptr && cancelled->load()) {
    return json{{"status", "cancelled"}, {"triggered", false},
                {"reasons", reasons}}.dump();
  }

  const JobKind execution_kind =
      answering_question ? JobKind::ask : JobKind::automatic;
  if (!acquire_execution_slot(execution_kind, cancelled)) {
    return json{{"status", "cancelled"}, {"triggered", false},
                {"reasons", reasons}}.dump();
  }
  ai::CoachResponse response;
  try {
    if (cancelled != nullptr && cancelled->load()) {
      release_execution_slot();
      return json{{"status", "cancelled"}, {"triggered", false},
                  {"reasons", reasons}}.dump();
    }
    response = orchestrator_.handle(request);
    persist_coach_turn(database_, orchestrator_, request, response);
    release_execution_slot();
  } catch (...) {
    release_execution_slot();
    throw;
  }
  if (cancelled != nullptr && cancelled->load()) {
    return json{{"status", "cancelled"}, {"triggered", false},
                {"reasons", reasons}}.dump();
  }
  auto output = response_value(response);
  output["triggered"] = true;
  output["reasons"] = reasons;
  output["priority"] = decision.priority;
  output["teachingValue"] = json{
      {"objectiveImportance", decision.objective_importance},
      {"personalRelevance", decision.personal_relevance},
      {"practiceRelevance", decision.practice_relevance},
      {"interruptionCost", decision.interruption_cost},
      {"value", decision.teaching_value}};
  output["triggerGate"] = automatic_gate_value(decision);
  output["eventKind"] = event.classification;
  output["moveAttribution"] = {
      {"moverColor", mover_color.value_or("")},
      {"learnerColor", learner_color.value_or("")},
      {"moverRole", mover_role},
      {"playedMoveUci", played_move},
      {"verifiedLearnerMove", verified_learner_move},
  };
  output["positionFen"] = event.current_fen.value_or("");
  if (!answering_question && output.value("status", "") == "ok") {
    record_automatic_delivery(automatic_history_key, now_seconds);
  }
  return output.dump();
}

// -----------------------------------------------------------------------------
// Section: Automatic interruption history
// -----------------------------------------------------------------------------

std::optional<std::int64_t> CoachService::seconds_since_last_automatic(
    const std::string& key, const std::int64_t now_seconds) const {
  std::lock_guard lock(automatic_history_mutex_);
  const auto it = last_automatic_at_.find(key);
  if (it == last_automatic_at_.end()) return std::nullopt;
  return std::max<std::int64_t>(0, now_seconds - it->second);
}

void CoachService::record_automatic_delivery(
    const std::string& key, const std::int64_t now_seconds) const {
  std::lock_guard lock(automatic_history_mutex_);
  if (!last_automatic_at_.contains(key) && last_automatic_at_.size() >= 128) {
    const auto oldest = std::min_element(
        last_automatic_at_.begin(), last_automatic_at_.end(),
        [](const auto& a, const auto& b) { return a.second < b.second; });
    if (oldest != last_automatic_at_.end()) last_automatic_at_.erase(oldest);
  }
  last_automatic_at_[key] = std::max<std::int64_t>(0, now_seconds);
}

// -----------------------------------------------------------------------------
// Section: Foreground-first Coach execution scheduling
// -----------------------------------------------------------------------------

bool CoachService::acquire_execution_slot(
    const JobKind kind, const std::atomic_bool* cancelled) const {
  using namespace std::chrono;
  const bool foreground = kind != JobKind::automatic;
  const auto wait_started = steady_clock::now();
  std::unique_lock lock(execution_state_mutex_);
  if (foreground) {
    ++foreground_waiters_;
  } else {
    ++automatic_waiters_;
  }

  auto leave_wait_queue = [&]() {
    if (foreground) {
      --foreground_waiters_;
    } else {
      --automatic_waiters_;
    }
  };

  for (;;) {
    if (cancelled != nullptr && cancelled->load()) {
      leave_wait_queue();
      execution_cv_.notify_all();
      return false;
    }
    const bool foreground_has_priority = foreground || foreground_waiters_ == 0;
    if (!execution_active_ && foreground_has_priority) break;
    execution_cv_.wait_for(lock, milliseconds(25));
  }

  leave_wait_queue();
  execution_active_ = true;
  execution_active_kind_ = kind;
  lock.unlock();

  if (foreground) {
    const auto waited = static_cast<std::uint64_t>(
        duration_cast<milliseconds>(steady_clock::now() - wait_started).count());
    foreground_wait_count_.fetch_add(1);
    foreground_wait_total_ms_.fetch_add(waited);
    foreground_wait_last_ms_.store(waited);
  }
  return true;
}

void CoachService::release_execution_slot() const {
  {
    std::lock_guard lock(execution_state_mutex_);
    execution_active_ = false;
  }
  execution_cv_.notify_all();
}

// -----------------------------------------------------------------------------
// Section: Read-only Coach performance diagnostics
// -----------------------------------------------------------------------------

void CoachService::record_performance_trace(
    const ai::CoachPipelineTrace& trace) {
  std::lock_guard lock(performance_mutex_);
  ++performance_totals_.requests;
  performance_totals_.automatic_requests += trace.automatic_turn ? 1 : 0;
  performance_totals_.accepted_requests += trace.accepted ? 1 : 0;
  performance_totals_.validation_failures += trace.validation_passed ? 0 : 1;
  performance_totals_.action_fulfillment_failures +=
      trace.action_fulfillment_required && !trace.action_fulfillment_passed ? 1 : 0;
  performance_totals_.repaired_responses += trace.validation_repaired ? 1 : 0;
  performance_totals_.provider_errors += trace.provider_error_code.empty() ? 0 : 1;
  performance_totals_.provider_calls +=
      static_cast<std::uint64_t>(std::max(0, trace.provider_calls));
  performance_totals_.total_ms += trace.total_ms;
  performance_totals_.session_ms += trace.session_ms;
  performance_totals_.route_ms += trace.route_ms;
  performance_totals_.planning_ms += trace.planning_ms;
  performance_totals_.context_ms += trace.context_ms;
  performance_totals_.retrieval_ms += trace.retrieval_ms;
  performance_totals_.position_analysis_ms += trace.position_analysis_ms;
  performance_totals_.practicality_ms += trace.practicality_ms;
  performance_totals_.teaching_plan_ms += trace.teaching_plan_ms;
  performance_totals_.position_cache_requests +=
      trace.position_cache_requested ? 1 : 0;
  performance_totals_.position_cache_any_hits +=
      trace.position_cache_any_hit ? 1 : 0;
  performance_totals_.position_cache_full_hits +=
      trace.position_cache_full_hit ? 1 : 0;
  performance_totals_.response_cache_requests +=
      trace.response_cache_requested ? 1 : 0;
  performance_totals_.response_cache_hits +=
      trace.response_cache_hit ? 1 : 0;
  performance_totals_.provider_evidence_input_items +=
      trace.provider_evidence_input_count;
  performance_totals_.provider_evidence_selected_items +=
      trace.provider_evidence_count;
  performance_totals_.provider_exact_duplicates_dropped +=
      trace.provider_exact_duplicates_dropped;
  performance_totals_.provider_compacted_items +=
      trace.provider_compacted_items;
  performance_totals_.provider_compaction_savings_tokens +=
      trace.provider_compaction_savings_tokens;
  performance_totals_.provider_estimated_input_tokens +=
      trace.provider_estimated_input_tokens;
  performance_totals_.provider_estimated_selected_tokens +=
      trace.provider_estimated_selected_tokens;
  performance_totals_.provider_request_ms += trace.provider_request_ms;
  performance_totals_.response_cache_key_ms += trace.response_cache_key_ms;
  performance_totals_.provider_ms += trace.provider_ms;
  performance_totals_.validation_ms += trace.validation_ms;
  performance_totals_.repair_provider_ms += trace.repair_provider_ms;
  recent_performance_.push_back(trace);
  constexpr std::size_t kRecentTraceLimit = 12;
  while (recent_performance_.size() > kRecentTraceLimit) {
    recent_performance_.pop_front();
  }
}

std::string CoachService::performance_diagnostics_json() const {
  PerformanceTotals totals;
  std::deque<ai::CoachPipelineTrace> recent;
  {
    std::lock_guard lock(performance_mutex_);
    totals = performance_totals_;
    recent = recent_performance_;
  }

  auto trace_json = [](const ai::CoachPipelineTrace& trace) {
    return json{
        {"automaticTurn", trace.automatic_turn},
        {"accepted", trace.accepted},
        {"validationPassed", trace.validation_passed},
        {"validationRepaired", trace.validation_repaired},
        {"providerErrorCode", trace.provider_error_code},
        {"statusClass", trace_status(trace)},
        {"validationIssues", trace.validation_issues},
        {"safeFallbackKind", trace.safe_fallback_kind},
        {"fallbackReason", trace.fallback_reason},
        {"learnerAttemptStatus", trace.learner_attempt_status},
        {"learnerMoveClassification", trace.learner_move_classification},
        {"moveAttribution", {
            {"moverColor", trace.move_mover_color},
            {"learnerColor", trace.move_learner_color},
            {"moverRole", trace.move_mover_role},
            {"playedMoveUci", trace.move_played_uci},
            {"verifiedLearnerMove", trace.move_verified_learner},
        }},
        {"providerCalls", trace.provider_calls},
        {"interaction", {
            {"requestKind", trace.interaction_request_kind},
            {"answerIntent", trace.interaction_answer_intent},
            {"answerLlmUsed", trace.answer_llm_used},
            {"responseRenderable", trace.response_renderable},
            {"nativeAnswerKind", trace.native_answer_kind},
        }},
        {"chessVerdict", {
            {"authoritative", trace.verdict_authoritative},
            {"positionVerdict", trace.verdict_position},
            {"moveVerdict", trace.verdict_move},
            {"evaluatedMoveUci", trace.verdict_evaluated_move},
            {"basis", trace.verdict_basis},
            {"reviewOutcome", trace.verdict_review_outcome},
            {"reviewedMoveUci", trace.verdict_review_move},
            {"groundingRequired", trace.verdict_grounding_required},
            {"groundingAvailable", trace.verdict_grounding_available},
            {"groundingBlocked", trace.verdict_grounding_blocked},
        }},
        {"actionFulfillment", {
            {"required", trace.action_fulfillment_required},
            {"passed", trace.action_fulfillment_passed},
            {"requestedActions", trace.requested_actions},
            {"transportedClientActions", trace.transported_client_actions},
            {"issues", trace.action_fulfillment_issues},
        }},
        {"routing", {
            {"intent", trace.routed_intent},
            {"contextIntent", trace.context_intent},
            {"explicitCurrentIntent", trace.explicit_current_intent},
            {"analysisMode", trace.analysis_mode},
            {"analysisModeExplicit", trace.analysis_mode_explicit},
        }},
        {"evidencePlan", {
            {"confidence", trace.evidence_plan_confidence},
            {"sourceCount", trace.evidence_plan_source_count},
            {"needCount", trace.evidence_plan_need_count},
            {"freshness", trace.evidence_plan_freshness},
            {"interaction", trace.evidence_plan_interaction},
            {"eloTarget", trace.evidence_plan_elo_target},
            {"satisfiedNeeds", trace.evidence_needs_satisfied},
            {"missingNeeds", trace.evidence_needs_missing},
        }},
        {"aggregation", {
            {"selectedItems", trace.aggregated_evidence_count},
            {"duplicatesRemoved", trace.evidence_duplicates_removed},
            {"conflictsResolved", trace.evidence_conflicts_resolved},
        }},
        {"nativeGrounding", {
            {"candidateCount", trace.native_candidate_count},
            {"factCount", trace.native_fact_count},
            {"focusKind", trace.native_focus_kind},
        }},
        {"teaching", {
            {"objective", trace.teaching_objective},
            {"deliveryMode", trace.teaching_delivery_mode},
            {"skillId", trace.teaching_skill_id},
        }},
        {"evidence", {
            {"retrieved", trace.retrieved_evidence_count},
            {"final", trace.final_evidence_count},
            {"providerInput", trace.provider_evidence_input_count},
            {"providerVisible", trace.provider_evidence_count},
            {"exactDuplicatesDropped",
             trace.provider_exact_duplicates_dropped},
            {"compactedItems", trace.provider_compacted_items},
            {"estimatedCompactionSavingsTokens",
             trace.provider_compaction_savings_tokens},
            {"estimatedInputTokens",
             trace.provider_estimated_input_tokens},
            {"estimatedSelectedTokens",
             trace.provider_estimated_selected_tokens},
            {"budgetTokens", trace.provider_evidence_budget_tokens},
        }},
        {"positionCache", {
            {"requested", trace.position_cache_requested},
            {"anyHit", trace.position_cache_any_hit},
            {"fullHit", trace.position_cache_full_hit},
        }},
        {"responseCache", {
            {"requested", trace.response_cache_requested},
            {"hit", trace.response_cache_hit},
        }},
        {"timingMs", {
            {"total", trace.total_ms},
            {"session", trace.session_ms},
            {"route", trace.route_ms},
            {"planning", trace.planning_ms},
            {"context", trace.context_ms},
            {"retrieval", trace.retrieval_ms},
            {"positionAnalysis", trace.position_analysis_ms},
            {"practicality", trace.practicality_ms},
            {"teachingPlan", trace.teaching_plan_ms},
            {"providerRequest", trace.provider_request_ms},
            {"responseCacheKey", trace.response_cache_key_ms},
            {"provider", trace.provider_ms},
            {"validation", trace.validation_ms},
            {"repairProvider", trace.repair_provider_ms},
        }},
    };
  };

  json recent_json = json::array();
  for (const auto& trace : recent) recent_json.push_back(trace_json(trace));

  std::uint64_t running = 0;
  std::uint64_t running_ask = 0;
  std::uint64_t running_automatic = 0;
  std::uint64_t running_hint = 0;
  {
    std::lock_guard lock(jobs_mutex_);
    for (const auto& [id, job] : jobs_) {
      (void)id;
      if (job->finished.load()) continue;
      ++running;
      if (job->kind == JobKind::ask) ++running_ask;
      if (job->kind == JobKind::automatic) ++running_automatic;
      if (job->kind == JobKind::hint) ++running_hint;
    }
  }

  bool execution_active = false;
  JobKind execution_active_kind = JobKind::ask;
  std::uint64_t foreground_waiters = 0;
  std::uint64_t automatic_waiters = 0;
  {
    std::lock_guard lock(execution_state_mutex_);
    execution_active = execution_active_;
    execution_active_kind = execution_active_kind_;
    foreground_waiters = foreground_waiters_;
    automatic_waiters = automatic_waiters_;
  }
  const auto active_kind_name = [execution_active, execution_active_kind]() {
    if (!execution_active) return std::string("idle");
    if (execution_active_kind == JobKind::automatic) return std::string("automatic");
    if (execution_active_kind == JobKind::hint) return std::string("hint");
    return std::string("ask");
  }();

  const auto average = [requests = totals.requests](std::uint64_t value) {
    return requests == 0 ? 0.0
                         : static_cast<double>(value) /
                               static_cast<double>(requests);
  };
  const auto position_cache = orchestrator_.position_cache_stats();
  const auto response_cache = orchestrator_.response_cache_stats();
  return json{
      {"schema", "coach.performance.v1"},
      {"contracts", {
          {"statusSchema", "coach.status.v3"},
          {"promptVersion",
           std::string(ai::ModelVersionRegistry::version_of("CoachPrompt"))},
          {"responseSchema", std::string(ai::kCoachResponseSchemaVersion)},
      }},
      {"requests", totals.requests},
      {"automaticRequests", totals.automatic_requests},
      {"acceptedRequests", totals.accepted_requests},
      {"validationFailures", totals.validation_failures},
      {"actionFulfillmentFailures", totals.action_fulfillment_failures},
      {"repairedResponses", totals.repaired_responses},
      {"providerErrors", totals.provider_errors},
      {"providerCalls", totals.provider_calls},
      {"averageTimingMs", {
          {"total", average(totals.total_ms)},
          {"session", average(totals.session_ms)},
          {"route", average(totals.route_ms)},
          {"planning", average(totals.planning_ms)},
          {"context", average(totals.context_ms)},
          {"retrieval", average(totals.retrieval_ms)},
          {"positionAnalysis", average(totals.position_analysis_ms)},
          {"practicality", average(totals.practicality_ms)},
          {"teachingPlan", average(totals.teaching_plan_ms)},
          {"providerRequest", average(totals.provider_request_ms)},
          {"responseCacheKey", average(totals.response_cache_key_ms)},
          {"provider", average(totals.provider_ms)},
          {"validation", average(totals.validation_ms)},
          {"repairProvider", average(totals.repair_provider_ms)},
      }},
      {"positionCache", {
          {"requests", totals.position_cache_requests},
          {"anyHits", totals.position_cache_any_hits},
          {"fullHits", totals.position_cache_full_hits},
          {"anyHitRate", totals.position_cache_requests == 0
              ? 0.0
              : static_cast<double>(totals.position_cache_any_hits) /
                    static_cast<double>(totals.position_cache_requests)},
          {"fullHitRate", totals.position_cache_requests == 0
              ? 0.0
              : static_cast<double>(totals.position_cache_full_hits) /
                    static_cast<double>(totals.position_cache_requests)},
          {"entries", position_cache.entries},
          {"capacity", position_cache.capacity},
          {"evictions", position_cache.evictions},
      }},
      {"providerInput", {
          {"inputItems", totals.provider_evidence_input_items},
          {"selectedItems", totals.provider_evidence_selected_items},
          {"exactDuplicatesDropped",
           totals.provider_exact_duplicates_dropped},
          {"compactedItems", totals.provider_compacted_items},
          {"estimatedCompactionSavingsTokens",
           totals.provider_compaction_savings_tokens},
          {"estimatedInputTokens", totals.provider_estimated_input_tokens},
          {"estimatedSelectedTokens",
           totals.provider_estimated_selected_tokens},
          {"estimatedTokenReduction",
           totals.provider_estimated_input_tokens >=
                   totals.provider_estimated_selected_tokens
               ? totals.provider_estimated_input_tokens -
                     totals.provider_estimated_selected_tokens
               : 0},
      }},
      {"responseCache", {
          {"requests", totals.response_cache_requests},
          {"hits", totals.response_cache_hits},
          {"hitRate", totals.response_cache_requests == 0
              ? 0.0
              : static_cast<double>(totals.response_cache_hits) /
                    static_cast<double>(totals.response_cache_requests)},
          {"stores", response_cache.stores},
          {"entries", response_cache.entries},
          {"capacity", response_cache.capacity},
          {"evictions", response_cache.evictions},
          {"keyBuilds", response_cache.key_builds},
          {"keyBytes", response_cache.key_bytes},
      }},
      {"jobs", {
          {"running", running},
          {"ask", running_ask},
          {"automatic", running_automatic},
          {"hint", running_hint},
      }},
      {"scheduler", {
          {"policy", "foreground_first"},
          {"activeClass", active_kind_name},
          {"foregroundWaiters", foreground_waiters},
          {"automaticWaiters", automatic_waiters},
          {"automaticPreemptions", automatic_preemptions_.load()},
          {"foregroundWaitCount", foreground_wait_count_.load()},
          {"foregroundWaitTotalMs", foreground_wait_total_ms_.load()},
          {"foregroundWaitLastMs", foreground_wait_last_ms_.load()},
      }},
      {"recent", std::move(recent_json)},
  }.dump();
}

// -----------------------------------------------------------------------------
// Section: Non-blocking coach jobs
// -----------------------------------------------------------------------------

std::string CoachService::start_job(
    std::string request_json, const JobKind kind, std::string session_id) {
  auto job = std::make_shared<CoachJob>();
  job->id = "coach-" + std::to_string(next_job_id_++);
  job->kind = kind;
  job->session_id = std::move(session_id);
  job->request_generation = next_request_generation_++;
  {
    std::lock_guard lock(jobs_mutex_);
    const std::string session_key = job->session_id.empty()
                                        ? "__default_coach_session__"
                                        : job->session_id;
    const bool foreground = kind != JobKind::automatic;
    const std::string generation_key = session_key +
        (foreground ? "|foreground" : "|automatic");
    latest_request_generation_[generation_key] = job->request_generation;
    for (const auto& [existing_id, existing] : jobs_) {
      (void)existing_id;
      const std::string existing_session = existing->session_id.empty()
                                               ? "__default_coach_session__"
                                               : existing->session_id;
      const bool existing_foreground = existing->kind != JobKind::automatic;
      if (existing_session == session_key &&
          existing_foreground == foreground &&
          !existing->finished.load()) {
        existing->cancelled = true;
      }
    }
    jobs_.emplace(job->id, job);
  }
  execution_cv_.notify_all();
  job->worker = std::thread(
      [this, request_json = std::move(request_json), kind, job]() mutable {
        run_job(std::move(request_json), kind, job);
      });
  return json{{"jobId", job->id}}.dump();
}

std::string CoachService::start_ask_json(const std::string& request_json) {
  cancel_automatic_jobs_for_foreground();
  const auto session_id = optional_string(json::parse(request_json), "sessionId").value_or("");
  return start_job(request_json, JobKind::ask, session_id);
}

void CoachService::cancel_superseded_automatic_jobs(
    const std::string& session_id) {
  bool cancelled_any = false;
  {
    std::lock_guard lock(jobs_mutex_);
    for (const auto& [id, job] : jobs_) {
      (void)id;
      if (job->kind == JobKind::automatic &&
          job->session_id == session_id && !job->finished.load()) {
        if (!job->cancelled.exchange(true)) cancelled_any = true;
      }
    }
  }
  if (cancelled_any) execution_cv_.notify_all();
}

void CoachService::cancel_automatic_jobs_for_foreground() {
  std::uint64_t cancelled_count = 0;
  {
    std::lock_guard lock(jobs_mutex_);
    for (const auto& [id, job] : jobs_) {
      (void)id;
      if (job->kind != JobKind::automatic || job->finished.load()) continue;
      if (!job->cancelled.exchange(true)) ++cancelled_count;
    }
  }
  if (cancelled_count != 0) {
    automatic_preemptions_.fetch_add(cancelled_count);
    execution_cv_.notify_all();
  }
}

bool CoachService::is_latest_request(const CoachJob& job) const {
  std::lock_guard lock(jobs_mutex_);
  const std::string session_key = job.session_id.empty()
                                      ? "__default_coach_session__"
                                      : job.session_id;
  const std::string key = session_key +
      (job.kind == JobKind::automatic ? "|automatic" : "|foreground");
  const auto found = latest_request_generation_.find(key);
  return found != latest_request_generation_.end() && found->second == job.request_generation;
}

std::string CoachService::start_automatic_json(const std::string& request_json) {
  // Latest-position-wins: rapid move sequences must never build an LLM backlog.
  const auto session_id = optional_string(json::parse(request_json), "sessionId")
                              .value_or("");
  cancel_superseded_automatic_jobs(session_id);
  return start_job(request_json, JobKind::automatic, session_id);
}

std::string CoachService::start_hint_json(const std::string& request_json) {
  cancel_automatic_jobs_for_foreground();
  const auto session_id = optional_string(json::parse(request_json), "sessionId").value_or("");
  return start_job(request_json, JobKind::hint, session_id);
}

void CoachService::run_job(
    std::string request_json, const JobKind kind,
    const std::shared_ptr<CoachJob>& job) noexcept {
  try {
    if (job->cancelled.load()) {
      std::lock_guard lock(job->state_mutex);
      job->state = "cancelled";
      job->finished = true;
      return;
    }

    std::string result;
    if (kind == JobKind::automatic) {
      result = automatic_json_impl(request_json, &job->cancelled);
    } else if (kind == JobKind::hint) {
      const auto value = json::parse(request_json);
      const auto fen = optional_string(value, "fen");
      if (!fen) throw std::invalid_argument("Coach hint requires FEN");
      // Foreground hints share the Coach execution slot and outrank Automatic Coach.
      if (!acquire_execution_slot(JobKind::hint, &job->cancelled)) {
        std::lock_guard lock(job->state_mutex);
        job->state = "cancelled";
        job->finished = true;
        return;
      }
      try {
        result = cached_hint(database_, analysis_service_, *hint_cache_, *fen).result;
        release_execution_slot();
      } catch (...) {
        release_execution_slot();
        throw;
      }
    } else {
      result = ask_json_impl(request_json, &job->cancelled);
    }
    const bool latest = is_latest_request(*job);
    std::lock_guard lock(job->state_mutex);
    if (job->cancelled) {
      job->state = "cancelled";
    } else if (!latest) {
      // Cacheable native evidence may already have been produced, but stale
      // provider/Coach output must never be delivered to a newer board state.
      job->state = "superseded";
      job->result_json.clear();
    } else {
      job->state = "complete";
      job->result_json = result;
    }
  } catch (const std::exception& error) {
    std::lock_guard lock(job->state_mutex);
    job->state = job->cancelled ? "cancelled" : "error";
    if (!job->cancelled) job->error_message = error.what();
  } catch (...) {
    std::lock_guard lock(job->state_mutex);
    job->state = job->cancelled ? "cancelled" : "error";
    if (!job->cancelled) job->error_message = "unknown coach error";
  }
  job->finished = true;
}

std::string CoachService::job_status_json(const std::string& job_id) {
  std::shared_ptr<CoachJob> job;
  {
    std::lock_guard lock(jobs_mutex_);
    const auto found = jobs_.find(job_id);
    if (found == jobs_.end()) throw std::runtime_error("coach job not found");
    job = found->second;
  }

  if (job->finished && job->worker.joinable()) job->worker.join();

  json output;
  {
    std::lock_guard lock(job->state_mutex);
    output = {
        {"jobId", job->id},
        {"state", job->state},
        {"finished", job->finished.load()},
        {"result", job->result_json.empty() ? json(nullptr)
                                             : json::parse(job->result_json)},
        {"errorMessage", job->error_message.empty()
                             ? json(nullptr)
                             : json(job->error_message)},
    };
  }

  if (job->finished) {
    std::lock_guard lock(jobs_mutex_);
    jobs_.erase(job_id);
  }
  return output.dump();
}

void CoachService::cancel_job(const std::string& job_id) {
  std::lock_guard lock(jobs_mutex_);
  const auto found = jobs_.find(job_id);
  if (found == jobs_.end()) return;
  found->second->cancelled = true;
  execution_cv_.notify_all();
}

}  // namespace kchess
