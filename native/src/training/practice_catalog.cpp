// -----------------------------------------------------------------------------
// Section: Bundled catalogues; all parsing and tree construction are native
// -----------------------------------------------------------------------------
#include "training/practice_catalog.h"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>
#include "chess/position_view.h"

namespace kchess {
namespace {
const char* kStudies =
#include "training/data/studies.inc"
;
const char* kOpenings[] = {
#include "training/data/openings_a.inc"
#include "training/data/openings_b.inc"
#include "training/data/openings_c.inc"
#include "training/data/openings_d.inc"
#include "training/data/openings_e.inc"
};
std::string trim(std::string value) {
  const auto first = value.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) return {};
  return value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
}
std::string lower(std::string value) {
  for (char& c : value) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return value;
}
std::string first_san(const std::string& pgn) {
  std::istringstream stream(pgn);
  for (std::string token; stream >> token;) {
    const auto start = token.find_first_not_of("0123456789.");
    if (start != std::string::npos) return token.substr(start);
  }
  return {};
}
int movetext_plies(const std::string& pgn) {
  std::istringstream input(pgn);
  int count = 0;
  for (std::string token; input >> token;) {
    if (token == "*" || token == "1-0" || token == "0-1" || token == "1/2-1/2") continue;
    if (token.find_first_not_of("0123456789.") != std::string::npos) ++count;
  }
  return count;
}
}

PracticeCatalog::PracticeCatalog() : studies_(nlohmann::json::parse(kStudies)) {}
const nlohmann::json& PracticeCatalog::studies() const { return studies_; }
nlohmann::json PracticeCatalog::study(const std::string& id) const {
  for (const auto& section : studies_.at("sections")) {
    int number = 0;
    for (const auto& candidate : section.at("studies")) {
      ++number;
      if (candidate.at("id") != id) continue;
      auto item = candidate;
      item["number"] = number;
      item["section"] = section.at("id");
      return item;
    }
  }
  throw std::invalid_argument("Unknown study");
}

void PracticeCatalog::load_openings() {
  if (!nodes_.empty()) return;
  nodes_.push_back({0, -1, 0, "", {}});
  std::map<std::pair<int, std::string>, int> siblings;
  for (const auto* source : kOpenings) {
    std::istringstream input(source);
    std::string row;
    while (std::getline(input, row)) {
      std::istringstream columns(row);
      Opening value{static_cast<int>(openings_.size()) + 1, "", "", ""};
      if (!std::getline(columns, value.eco, '\t') ||
          !std::getline(columns, value.name, '\t') ||
          !std::getline(columns, value.pgn, '\t')) continue;
      value.pgn = trim(value.pgn);
      if (value.pgn.empty()) continue;
      value.plies = movetext_plies(value.pgn);
      openings_.push_back(value);
      std::vector<std::string> path{first_san(value.pgn)};
      std::string name = value.name;
      std::replace(name.begin(), name.end(), ':', ',');
      std::istringstream names(name);
      for (std::string part; std::getline(names, part, ',');) {
        if (!trim(part).empty()) path.push_back(trim(part));
      }
      int parent = 0;
      for (const auto& name_part : path) {
        const auto key = std::make_pair(parent, name_part);
        const auto found = siblings.find(key);
        if (found == siblings.end()) {
          const int id = static_cast<int>(nodes_.size());
          nodes_[parent].children.push_back(id);
          nodes_.push_back({id, parent, value.id, name_part, {}});
          siblings.emplace(key, id);
          parent = id;
        } else {
          parent = found->second;
          auto& selected = openings_.at(nodes_[parent].opening - 1);
          // The shortest legal continuation represents the entry point;
          // compare ply counts rather than SAN spelling or byte lengths.
          if (value.plies < selected.plies) nodes_[parent].opening = value.id;
        }
      }
    }
  }
}

const ParsedGame& PracticeCatalog::line(int id) {
  load_openings();
  if (id <= 0 || id > static_cast<int>(openings_.size())) {
    throw std::invalid_argument("Unknown opening");
  }
  if (const auto found = parsed_.find(id); found != parsed_.end()) return found->second;
  auto result = parse_pgn(openings_[id - 1].pgn + " *");
  if (!result.valid || result.game.moves.empty()) throw std::runtime_error("Invalid opening line");
  return parsed_.emplace(id, std::move(result.game)).first->second;
}

nlohmann::json PracticeCatalog::opening(int id) {
  const auto& game = line(id);
  const auto& value = openings_.at(id - 1);
  // The setup line in notation: a scenario card shows the moves it starts from.
  std::vector<std::string> moves;
  for (const auto& move : game.moves) moves.push_back(move.san);
  return {{"id", id}, {"name", value.name}, {"eco", value.eco}, {"moves", moves},
          {"fen", game.moves.back().fen_after},
          {"position", nlohmann::json::parse(position_view_json(game.moves.back().fen_after))}};
}

nlohmann::json PracticeCatalog::openings(int parent, const std::string& query) {
  load_openings();
  std::vector<int> selected;
  if (query.empty()) {
    if (parent < 0 || parent >= static_cast<int>(nodes_.size())) throw std::invalid_argument("Unknown node");
    selected = nodes_[parent].children;
  } else {
    const auto normalized_query = lower(query);
    for (const auto& node : nodes_) {
      if (node.id == 0) continue;
      const auto& value = openings_.at(node.opening - 1);
      if (lower(node.name + " " + value.eco).find(normalized_query) != std::string::npos) {
        selected.push_back(node.id);
        if (selected.size() == 20) break;
      }
    }
  }
  std::stable_sort(selected.begin(), selected.end(), [&](int a, int b) {
    if (parent == 0 && nodes_[a].children.size() != nodes_[b].children.size())
      return nodes_[a].children.size() > nodes_[b].children.size();
    return nodes_[a].name < nodes_[b].name;
  });
  auto result = nlohmann::json::array();
  for (int id : selected) {
    const auto& node = nodes_[id];
    auto item = opening(node.opening);
    item["openingId"] = node.opening;
    item["id"] = node.id;
    item["name"] = node.name;
    item["parentId"] = node.parent;
    item["childCount"] = node.children.size();
    item["progressKey"] = "opening_" + std::to_string(node.opening);
    result.push_back(std::move(item));
  }
  return result;
}

int PracticeCatalog::matching(const std::string& eco, const std::string& name) {
  // Matching needs catalogue metadata, not parsed PGNs or board previews.
  load_openings();
  const auto normalized_name = lower(name);
  int fallback = 0;
  for (const auto& value : openings_) {
    if (value.eco == eco && value.name == name) return value.id;
    if (!fallback && ((!eco.empty() && value.eco == eco) ||
        (!name.empty() && lower(value.name).find(normalized_name) != std::string::npos))) fallback = value.id;
  }
  return fallback;
}
}  // namespace kchess
