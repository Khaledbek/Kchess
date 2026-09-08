#include "engine/stockfish19_result_coherence.h"

#include <algorithm>
#include <utility>

namespace kchess {

bool usable_stockfish19_engine_move(const std::string& move) {
  return !move.empty() && move != "(none)" && move != "0000";
}

std::vector<EngineLine> coherent_stockfish19_ranked_lines(
    const std::map<int, EngineLine>& ranked_lines,
    const std::map<std::string, EngineLine>& latest_lines_by_move,
    const std::string& final_best_move) {
  std::vector<EngineLine> current;
  current.reserve(ranked_lines.size());
  for (const auto& [rank, line] : ranked_lines) {
    (void)rank;
    current.push_back(line);
  }
  if (current.empty() || !usable_stockfish19_engine_move(final_best_move)) {
    return current;
  }

  // SF19 may finish immediately after a PV/rank reshuffle. The final bestmove
  // callback is authoritative, while the latest rank slots can still represent
  // the previous layout. Retaining the latest PV for each root move lets KChess
  // expose one coherent result to both the board arrow and the classifier.
  const auto current_best = std::find_if(
      current.begin(), current.end(), [&](const EngineLine& line) {
        return line.best_move() == final_best_move;
      });

  EngineLine best_line;
  bool have_best_line = false;
  if (current_best != current.end()) {
    best_line = *current_best;
    have_best_line = true;
  } else {
    const auto historical = latest_lines_by_move.find(final_best_move);
    if (historical != latest_lines_by_move.end()) {
      best_line = historical->second;
      have_best_line = true;
    }
  }
  if (!have_best_line) return current;

  const std::size_t requested_line_count = current.size();
  std::vector<EngineLine> coherent;
  coherent.reserve(requested_line_count);
  best_line.rank = 1;
  coherent.push_back(std::move(best_line));

  for (const auto& line : current) {
    if (line.best_move().empty() || line.best_move() == final_best_move) continue;
    auto copy = line;
    copy.rank = static_cast<int>(coherent.size()) + 1;
    coherent.push_back(std::move(copy));
    if (coherent.size() >= requested_line_count) break;
  }
  return coherent;
}

}  // namespace kchess
