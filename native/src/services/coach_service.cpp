#include "services/coach_service.h"

#include <algorithm>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "ai/automatic/automatic_coach_trigger.h"
#include "ai/providers/gemini_provider.h"
#include "chess/pgn.h"
#include "chess/move.h"
#include "chess/position_view.h"
#include "services/coach_profile_bridge.h"

namespace kchess {
// Section: Last engine hint, shared with the existing evidence retrieval hook
struct CoachHintCache {
  std::mutex mutex;
  std::string fen;
  std::string settings_key;
  std::string result;
  ai::CandidateMoveSnapshot candidates;
};

namespace {

// -----------------------------------------------------------------------------
// Section: Request mapping
// -----------------------------------------------------------------------------

using json = nlohmann::json;

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
  request.player_color = optional_string(value, "playerColor");
  request.hint_move_uci = optional_string(value, "hintMoveUci");
  request.variation_job_id = optional_string(value, "variationJobId");

  if (!request.game_pgn && request.context_id) {
    if (const auto game = database.game(*request.context_id)) {
      if (!game->pgn.empty()) request.game_pgn = game->pgn;
    }
  }
  return request;
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

std::string cached_hint(Database& database, AnalysisService& service,
                        CoachHintCache& cache, const std::string& fen) {
  const auto settings_key = hint_settings_key(database);
  std::lock_guard lock(cache.mutex);
  if (cache.fen == fen && cache.settings_key == settings_key && !cache.result.empty())
    return cache.result;
  const auto result = service.coach_hint_json(fen);
  const auto hint = json::parse(result);
  ai::CandidateMoveSnapshot snapshot;
  for (const auto& move : hint.value("candidates", hint.value("moves", json::array()))) {
    ai::CandidateLineInput line;
    line.rank = move.value("rank", 0);
    line.move_uci = move.value("uci", "");
    line.pv_uci = move.value("pv", std::vector<std::string>{});
    if (move.contains("evaluationCp") && move["evaluationCp"].is_number_integer())
      line.evaluation_cp = move["evaluationCp"].get<int>();
    if (move.contains("mateIn") && move["mateIn"].is_number_integer())
      line.mate_in = move["mateIn"].get<int>();
    snapshot.root_lines.push_back(std::move(line));
  }
  cache.fen = fen;
  cache.settings_key = settings_key;
  cache.result = result;
  cache.candidates = std::move(snapshot);
  return result;
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
                                std::shared_ptr<CoachHintCache> hint_cache) {
  ai::EvidenceSources sources;
  sources.engine_candidates = [&database, &analysis_service, hint_cache](
      const ai::CoachRequest& request, const ai::QueryPlan&)
      -> std::optional<ai::EngineCandidateBundle> {
    // Only an explicit user's question may spend fresh engine work. Automatic
    // coaching consumes the analysis that triggered it and never starts a search.
    if (!request.position_fen || request.automatic_turn) return std::nullopt;
    try {
      const auto result = cached_hint(database, analysis_service, *hint_cache, *request.position_fen);
      std::lock_guard lock(hint_cache->mutex);
      return ai::EngineCandidateBundle{
          .engine_evidence = {.kind = ai::EvidenceKind::engine, .payload = result, .confidence = 1.0},
          .candidate_snapshot = hint_cache->candidates};
    } catch (...) {
      return std::nullopt;
    }
  };
  sources.existing_candidates = [&database, &analysis_service, hint_cache](const ai::CoachRequest& request,
      const ai::QueryPlan&) -> std::optional<ai::CandidateMoveSnapshot> {
    const auto key = hint_settings_key(database);
    {
      std::lock_guard lock(hint_cache->mutex);
      if (request.position_fen && *request.position_fen == hint_cache->fen &&
          key == hint_cache->settings_key && !hint_cache->candidates.root_lines.empty()) {
        return hint_cache->candidates;
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
  sources.user_profile = [&database](const ai::CoachRequest& request,
                                     const ai::QueryPlan&) {
    return coach_profile_evidence(database, request.profile_id);
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

json response_value(const ai::CoachResponse& response) {
  json output = {
      {"accepted", response.accepted},
      {"answer", response.answer},
      {"followUpQuestion", response.follow_up_question},
      {"validationPassed", response.validation_passed},
      {"validationRepaired", response.validation_repaired},
      {"validationIssues", response.validation_issues},
      {"evidenceIds", response.evidence_references},
  };
  output["boardMoves"] = json::array();
  output["focusSquares"] = json::array();
  if (response.accepted && response.validation_passed) {
    for (const auto& move : response.recommendations) {
      if (response.answer_type != ai::CoachAnswerType::quiz &&
          !move.move_uci.empty() && output["boardMoves"].size() < 2) {
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
  if (!response.accepted) {
    output["status"] = "off_topic";
  } else if (!response.validation_passed) {
    output["status"] = "validation_failed";
  } else if (response.answer.empty()) {
    output["status"] = "provider_unavailable";
  } else {
    output["status"] = "ok";
  }
  return output;
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

std::string automatic_prompt(const ai::AutomaticCoachDecision& decision) {
  std::ostringstream prompt;
  prompt << "Coach the just-played move naturally in one concise comment. "
            "Explain the practical chess idea instead of reciting engine telemetry. "
            "The native trigger reasons are: ";
  bool first = true;
  for (const auto reason : decision.reasons) {
    if (!first) prompt << ", ";
    prompt << ai::automatic_coach_reason_name(reason);
    first = false;
  }
  prompt << ". Use only supplied chess evidence. If useful, end with one short question "
            "that makes the player think about this position.";
  return prompt.str();
}

}  // namespace

// -----------------------------------------------------------------------------
// Section: Coach service lifecycle
// -----------------------------------------------------------------------------

CoachService::CoachService(Database& database, AnalysisService& analysis_service)
    : database_(database),
      analysis_service_(analysis_service),
      hint_cache_(std::make_shared<CoachHintCache>()),
      orchestrator_(make_sources(database, analysis_service, hint_cache_),
                    ai::make_gemini_provider()) {}

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

std::string CoachService::ask_json(const std::string& request_json) const {
  const auto value = json::parse(request_json);
  auto request = parse_request(value, database_);
  std::lock_guard lock(execution_mutex_);
  auto output = response_value(orchestrator_.handle(request));
  output["positionFen"] = request.position_fen.value_or("");
  return output.dump();
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

std::string CoachService::automatic_json(const std::string& request_json) const {
  return automatic_json_impl(request_json, nullptr);
}

std::string CoachService::automatic_json_impl(
    const std::string& request_json, const std::atomic_bool* cancelled) const {
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
    if (analysis.contains("expectedScoreLoss") &&
        analysis["expectedScoreLoss"].is_number()) {
      event.expected_score_loss = analysis["expectedScoreLoss"].get<double>();
    }
  } catch (...) {
    // Phase/motif triggers remain available even before persisted engine analysis.
    if (variation_id) return json{{"status", "skipped"}, {"triggered", false}}.dump();
  }

  const auto requested_profile_id = optional_string(value, "profileId");
  const std::string profile_id =
      requested_profile_id.value_or(game ? game->profile_id : "");
  if (!event.classification.empty() && !profile_id.empty()) {
    event.repeated_personal_mistake = coach_repeated_personal_mistake(
        database_, profile_id, event.classification);
  }

  const auto decision = ai::AutomaticCoachTrigger{}.decide(event);
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
    }.dump();
  }

  ai::CoachRequest request;
  request.user_text = automatic_prompt(decision);
  request.automatic_turn = true;
  request.locale = value.value("locale", "en");
  request.mode = ai::CoachMode::explain;
  request.depth = ai::ResponseDepth::concise;
  request.position_fen = event.current_fen;
  if (game && !game->pgn.empty()) request.game_pgn = game->pgn;
  request.session_id = optional_string(value, "sessionId");
  // The played move belongs to previous_fen, never to the current candidate root.
  request.surface = optional_string(value, "surface");
  request.context_id = context_id;
  request.context_ply = ply;
  request.profile_id = profile_id.empty() ? std::nullopt : std::optional<std::string>(profile_id);
  request.variation_job_id = variation_id;
  request.player_color = optional_string(value, "playerColor");

  if (cancelled != nullptr && cancelled->load()) {
    return json{{"status", "cancelled"}, {"triggered", false},
                {"reasons", reasons}}.dump();
  }

  ai::CoachResponse response;
  {
    std::lock_guard lock(execution_mutex_);
    if (cancelled != nullptr && cancelled->load()) {
      return json{{"status", "cancelled"}, {"triggered", false},
                  {"reasons", reasons}}.dump();
    }
    response = orchestrator_.handle(request);
  }
  if (cancelled != nullptr && cancelled->load()) {
    return json{{"status", "cancelled"}, {"triggered", false},
                {"reasons", reasons}}.dump();
  }
  auto output = response_value(response);
  output["triggered"] = true;
  output["reasons"] = reasons;
  output["priority"] = decision.priority;
  output["eventKind"] = event.classification;
  output["positionFen"] = event.current_fen.value_or("");
  return output.dump();
}

// -----------------------------------------------------------------------------
// Section: Non-blocking coach jobs
// -----------------------------------------------------------------------------

std::string CoachService::start_job(
    std::string request_json, const JobKind kind) {
  auto job = std::make_shared<CoachJob>();
  job->id = "coach-" + std::to_string(next_job_id_++);
  job->kind = kind;
  {
    std::lock_guard lock(jobs_mutex_);
    jobs_.emplace(job->id, job);
  }
  job->worker = std::thread(
      [this, request_json = std::move(request_json), kind, job]() mutable {
        run_job(std::move(request_json), kind, job);
      });
  return json{{"jobId", job->id}}.dump();
}

std::string CoachService::start_ask_json(const std::string& request_json) {
  return start_job(request_json, JobKind::ask);
}

void CoachService::cancel_superseded_automatic_jobs() {
  std::lock_guard lock(jobs_mutex_);
  for (const auto& [id, job] : jobs_) {
    (void)id;
    if (job->kind == JobKind::automatic && !job->finished.load()) {
      job->cancelled = true;
    }
  }
}

std::string CoachService::start_automatic_json(const std::string& request_json) {
  // Latest-position-wins: rapid move sequences must never build an LLM backlog.
  cancel_superseded_automatic_jobs();
  return start_job(request_json, JobKind::automatic);
}

std::string CoachService::start_hint_json(const std::string& request_json) {
  return start_job(request_json, JobKind::hint);
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
      // Serialize hints with coach turns and retain only one position's result.
      std::lock_guard execution_lock(execution_mutex_);
      result = cached_hint(database_, analysis_service_, *hint_cache_, *fen);
    } else {
      result = ask_json(request_json);
    }
    std::lock_guard lock(job->state_mutex);
    if (job->cancelled) {
      job->state = "cancelled";
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
}

}  // namespace kchess
