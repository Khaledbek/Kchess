#pragma once

#include <string_view>

#include "analysis/move_classifier.h"
#include "analysis/move_classifier_sf19.h"

namespace kchess {

inline bool uses_stockfish19_classifier_contract(const std::string_view engine_version) {
  return engine_version.rfind("Stockfish 19", 0) == 0;
}

inline int current_classifier_version_for_engine(const std::string_view engine_version) {
  return uses_stockfish19_classifier_contract(engine_version)
      ? MoveClassifierSf19Config::version
      : MoveClassifierConfig::version;
}

inline bool current_classifier_contract(
    const std::string_view engine_version, const int classifier_version) {
  return classifier_version > 0
      && classifier_version == current_classifier_version_for_engine(engine_version);
}

}  // namespace kchess
