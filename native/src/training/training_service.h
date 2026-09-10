// -----------------------------------------------------------------------------
// Section: Native training workflow
// -----------------------------------------------------------------------------

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>

#include "persistence/database.h"
#include "training/training_catalog.h"

namespace kchess {

class TrainingService {
 public:
  explicit TrainingService(Database& database) : database_(database) {}

  std::string overview_json() const;
  std::string start_attempt_json(const std::string& exercise_id);
  std::string play_move_json(
      const std::string& attempt_id,
      const std::string& source,
      const std::string& target);

 private:
  struct Attempt {
    const TrainingExerciseDefinition* exercise{nullptr};
    std::size_t ply{0};
    std::string fen;
    bool had_error{false};
  };

  TrainingProgressRecord progress_for(const std::string& exercise_id) const;
  TrainingProgressRecord complete_attempt(const Attempt& attempt);
  std::string next_attempt_id();

  Database& database_;
  std::unordered_map<std::string, Attempt> attempts_;
  std::uint64_t next_attempt_{1};
};

}  // namespace kchess
