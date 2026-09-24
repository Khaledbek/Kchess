// -----------------------------------------------------------------------------
// Section: Validated bundled endgame definitions
// -----------------------------------------------------------------------------

#include "training/training_catalog.h"

#include <stdexcept>

namespace kchess {

const std::vector<TrainingExerciseDefinition>& training_catalog() {
  static const std::vector<TrainingExerciseDefinition> catalogue{
      {
          .id = "endgame_opposition",
          .category = "endgame",
          .goal = "win",
          .starting_fen = "8/4k3/8/3K4/4P3/8/8/8 w - - 0 1",
          .solution_san = {
              "Ke5", "Kd7", "Kf6", "Kd6", "e5+", "Kd5",
              "e6", "Kd6", "e7", "Kd7", "Kf7",
          },
      },
      {
          .id = "endgame_lucena",
          .category = "endgame",
          .goal = "win",
          .starting_fen = "3K4/3P1k2/8/8/8/8/8/r3R3 w - - 0 1",
          .solution_san = {
              "Re4", "Ra2", "Kc7", "Rc2+", "Kb6", "Rb2+",
              "Kc6", "Rc2+", "Kb5", "Rb2+", "Rb4",
          },
      },
      {
          .id = "endgame_philidor",
          .category = "endgame",
          .goal = "draw",
          .starting_fen = "8/4k3/r7/3KP3/8/8/8/7R b - - 0 1",
          .solution_san = {"Rb6", "e6", "Rb1", "Ke5", "Re1+"},
      },
  };
  return catalogue;
}

const TrainingExerciseDefinition& training_exercise(const std::string& id) {
  for (const auto& exercise : training_catalog()) {
    if (exercise.id == id) return exercise;
  }
  throw std::invalid_argument("Training exercise not found");
}

}  // namespace kchess
