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
  // Every named opening family (Sicilian Defense, Caro-Kann Defense, ...), most
  // variations first. The tree's own top level is the first move (e4, d4), which
  // names no opening, so a scenario dashboard starts one level down.
  nlohmann::json families();
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
  nlohmann::json describe(const std::vector<int>& selected);
  std::vector<Opening> openings_;
  std::vector<Node> nodes_;
  std::map<int, ParsedGame> parsed_;
  nlohmann::json studies_;
};
}  // namespace kchess
