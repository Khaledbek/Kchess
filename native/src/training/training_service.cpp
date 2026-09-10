// -----------------------------------------------------------------------------
// Section: Training DTO serialization
// -----------------------------------------------------------------------------

#include "training/training_service.h"

#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <utility>

#include <nlohmann/json.hpp>

#include "chess/move.h"
#include "chess/position_view.h"

namespace kchess {
namespace {

constexpr int kMasteryThreshold = 3;

nlohmann::json progress_json(const TrainingProgressRecord& progress) {
  return {
      {"exerciseId", progress.exercise_id},
      {"isMastered", progress.mastered},
      {"successStreak", progress.success_streak},
      {"successCount", progress.success_count},
      {"attemptCount", progress.attempt_count},
      {"lastAttemptAt", progress.last_attempt_at.has_value()
          ? nlohmann::json(*progress.last_attempt_at)
          : nlohmann::json(nullptr)},
  };
}

std::string next_exercise_id(const TrainingExerciseDefinition& exercise) {
  const auto& catalogue = training_catalog();
  for (std::size_t index = 0; index < catalogue.size(); ++index) {
    if (catalogue[index].id != exercise.id) continue;
    for (++index; index < catalogue.size(); ++index) {
      if (catalogue[index].category == exercise.category) {
        return catalogue[index].id;
      }
    }
    break;
  }
  return {};
}

nlohmann::json exercise_json(
    const TrainingExerciseDefinition& exercise,
    const TrainingProgressRecord& progress) {
  const auto next_id = next_exercise_id(exercise);
  return {
      {"id", exercise.id},
      {"category", exercise.category},
      {"goal", exercise.goal},
      {"startingFen", exercise.starting_fen},
      {"solverColor", white_to_move(exercise.starting_fen) ? "white" : "black"},
      {"solverMoveCount", (exercise.solution_san.size() + 1) / 2},
      {"nextExerciseId",
       next_id.empty() ? nlohmann::json(nullptr) : nlohmann::json(next_id)},
      {"progress", progress_json(progress)},
  };
}

nlohmann::json position_json(const std::string& fen) {
  return nlohmann::json::parse(position_view_json(fen));
}

std::int64_t unix_time_seconds() {
  return std::chrono::duration_cast<std::chrono::seconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

}  // namespace

// -----------------------------------------------------------------------------
// Section: Catalogue and progress queries
// -----------------------------------------------------------------------------

TrainingProgressRecord TrainingService::progress_for(
    const std::string& exercise_id) const {
  for (const auto& progress : database_.training_progress()) {
    if (progress.exercise_id == exercise_id) return progress;
  }
  return {.exercise_id = exercise_id};
}

std::string TrainingService::overview_json() const {
  const auto stored = database_.training_progress();
  nlohmann::json exercises = nlohmann::json::array();
  nlohmann::json categories = nlohmann::json::object();

  for (const auto& exercise : training_catalog()) {
    auto progress = TrainingProgressRecord{.exercise_id = exercise.id};
    const auto found =
        std::find_if(stored.begin(), stored.end(), [&](const auto& entry) {
          return entry.exercise_id == exercise.id;
        });
    if (found != stored.end()) progress = *found;
    exercises.push_back(exercise_json(exercise, progress));

    auto& category = categories[exercise.category];
    if (category.is_null()) {
      category = {{"mastered", 0}, {"total", 0}, {"solved", 0}};
    }
    category["total"] = category["total"].get<int>() + 1;
    category["solved"] = category["solved"].get<int>() + progress.success_count;
    if (progress.mastered) {
      category["mastered"] = category["mastered"].get<int>() + 1;
    }
  }

  return nlohmann::json{
      {"masteryThreshold", kMasteryThreshold},
      {"exercises", std::move(exercises)},
      {"categories", std::move(categories)},
  }.dump();
}

// -----------------------------------------------------------------------------
// Section: Attempt lifecycle and native move validation
// -----------------------------------------------------------------------------

std::string TrainingService::next_attempt_id() {
  return "training-" + std::to_string(next_attempt_++);
}

std::string TrainingService::start_attempt_json(const std::string& exercise_id) {
  const auto& exercise = training_exercise(exercise_id);
  const auto attempt_id = next_attempt_id();
  attempts_[attempt_id] = Attempt{
      .exercise = &exercise,
      .fen = exercise.starting_fen,
  };
  return nlohmann::json{
      {"attemptId", attempt_id},
      {"exercise", exercise_json(exercise, progress_for(exercise.id))},
      {"position", position_json(exercise.starting_fen)},
      {"ply", 0},
      {"solverMovesPlayed", 0},
      {"status", "active"},
  }.dump();
}

TrainingProgressRecord TrainingService::complete_attempt(const Attempt& attempt) {
  auto progress = progress_for(attempt.exercise->id);
  const bool success = !attempt.had_error;
  progress.attempt_count += 1;
  progress.last_attempt_at = unix_time_seconds();
  if (success) {
    progress.success_count += 1;
    progress.success_streak = std::min(kMasteryThreshold, progress.success_streak + 1);
    progress.mastered = progress.mastered || progress.success_streak >= kMasteryThreshold;
  } else {
    progress.success_streak = 0;
  }
  database_.put_training_progress(progress);
  return progress;
}

std::string TrainingService::play_move_json(
    const std::string& attempt_id,
    const std::string& source,
    const std::string& target) {
  const auto found = attempts_.find(attempt_id);
  if (found == attempts_.end()) {
    throw std::invalid_argument("Training attempt not found");
  }
  auto& attempt = found->second;
  const auto& solution = attempt.exercise->solution_san;
  if (attempt.ply >= solution.size()) {
    throw std::invalid_argument("Training attempt is already complete");
  }

  std::optional<AppliedMove> played;
  try {
    played = apply_legal_uci_move(attempt.fen, source + target);
  } catch (const std::invalid_argument&) {
  }
  if (!played.has_value() || played->san != solution[attempt.ply]) {
    attempt.had_error = true;
    return nlohmann::json{
        {"attemptId", attempt_id},
        {"accepted", false},
        {"status", "wrong"},
        {"ply", attempt.ply},
        {"solverMovesPlayed", (attempt.ply + 1) / 2},
        {"position", position_json(attempt.fen)},
    }.dump();
  }

  attempt.fen = played->fen_after;
  attempt.ply += 1;
  const auto player_position = position_json(attempt.fen);
  nlohmann::json reply = nullptr;
  if (attempt.ply < solution.size()) {
    const auto response = find_legal_san_move(attempt.fen, solution[attempt.ply]);
    if (!response.has_value()) {
      throw std::runtime_error("Invalid bundled training line");
    }
    attempt.fen = response->fen_after;
    attempt.ply += 1;
    reply = {
        {"uci", response->uci},
        {"san", response->san},
        {"positionAfter", position_json(attempt.fen)},
    };
  }

  const bool completed = attempt.ply >= solution.size();
  const bool clean = !attempt.had_error;
  nlohmann::json result{
      {"attemptId", attempt_id},
      {"accepted", true},
      {"status", completed ? "completed" : "active"},
      {"ply", attempt.ply},
      {"solverMovesPlayed", (attempt.ply + 1) / 2},
      {"position", position_json(attempt.fen)},
      {"playerPosition", std::move(player_position)},
      {"reply", std::move(reply)},
      {"clean", completed ? nlohmann::json(clean) : nlohmann::json(nullptr)},
  };
  if (completed) {
    result["progress"] = progress_json(complete_attempt(attempt));
    attempts_.erase(found);
  }
  return result.dump();
}

}  // namespace kchess
