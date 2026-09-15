#pragma once

#include <cstddef>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "knowledge_graph_contract.h"

struct sqlite3;

namespace kchess::knowledge {

// -----------------------------------------------------------------------------
// Section: Traversal contract
// -----------------------------------------------------------------------------

enum class GraphDirection { kOutgoing, kIncoming, kBoth };

struct KnowledgeTraversalStep {
  KnowledgeEdge edge;
  KnowledgeNode node;
  std::size_t depth{0};
};

struct KnowledgeNodeLookup {
  std::optional<KnowledgeNodeKind> kind;
  std::map<std::string, std::string> text_properties;
  bool case_insensitive_text{true};
  std::size_t limit{100};
};

// -----------------------------------------------------------------------------
// Section: SQLite graph storage
// -----------------------------------------------------------------------------

// GraphStore is a focused SQLite view over the general Knowledge Graph tables.
// It stores routing structure and compact typed properties only. Source payloads
// remain in the existing authoritative KChess stores.
class GraphStore {
 public:
  explicit GraphStore(std::filesystem::path data_directory);
  ~GraphStore();

  GraphStore(const GraphStore&) = delete;
  GraphStore& operator=(const GraphStore&) = delete;

  void open();
  void close() noexcept;
  [[nodiscard]] bool is_open() const noexcept { return db_ != nullptr; }

  bool upsert_node(const KnowledgeNode& node);
  [[nodiscard]] std::optional<KnowledgeNode> node(
      const KnowledgeNodeId& id) const;
  bool remove_node(const KnowledgeNodeId& id);

  bool upsert_edge(const KnowledgeEdge& edge);
  [[nodiscard]] std::optional<KnowledgeEdge> edge(
      const KnowledgeEdgeId& id) const;
  bool remove_edge(const KnowledgeEdgeId& id);

  [[nodiscard]] std::vector<KnowledgeTraversalStep> adjacent(
      const KnowledgeNodeId& id, GraphDirection direction = GraphDirection::kBoth,
      std::optional<KnowledgeEdgeKind> kind = std::nullopt,
      std::size_t limit = 100) const;

  [[nodiscard]] std::vector<KnowledgeTraversalStep> traverse(
      const KnowledgeNodeId& seed, std::size_t max_depth,
      std::size_t max_nodes = 100,
      GraphDirection direction = GraphDirection::kBoth) const;

  [[nodiscard]] std::vector<KnowledgeNode> find_nodes(
      const KnowledgeNodeLookup& lookup) const;

 private:
  std::filesystem::path database_path_;
  sqlite3* db_{nullptr};
};

}  // namespace kchess::knowledge
