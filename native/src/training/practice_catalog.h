#pragma once
// -----------------------------------------------------------------------------
// Section: Native practice catalogue and opening hierarchy
// -----------------------------------------------------------------------------
#include <map>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "chess/pgn.h"

namespace kchess {
class PracticeCatalog {
 public:
  PracticeCatalog();
  const nlohmann::json& studies() const;
  nlohmann::json study(const std::string& id) const;
  nlohmann::json openings(int parent, const std::string& query);
  nlohmann::json opening(int id);
  int matching(const std::string& eco, const std::string& name);
  const ParsedGame& line(int id);

 private:
  struct Opening { int id; std::string eco, name, pgn; int plies{0}; };
  struct Node {
    int id, parent, opening;
    std::string name;
    std::vector<int> children;
  };
  void load_openings();
  std::vector<Opening> openings_;
  std::vector<Node> nodes_;
  std::map<int, ParsedGame> parsed_;
  nlohmann::json studies_;
};
}  // namespace kchess
