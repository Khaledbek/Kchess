// -----------------------------------------------------------------------------
// Section: Bundled training catalogue
// -----------------------------------------------------------------------------

#pragma once

#include <string>
#include <vector>

namespace kchess {

struct TrainingExerciseDefinition {
  std::string id;
  std::string category;
  std::string goal;
  std::string starting_fen;
  std::vector<std::string> solution_san;
};

const std::vector<TrainingExerciseDefinition>& training_catalog();
const TrainingExerciseDefinition& training_exercise(const std::string& id);

}  // namespace kchess
