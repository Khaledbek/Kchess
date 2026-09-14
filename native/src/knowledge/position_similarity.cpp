#include "position_similarity.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

#include "ai/position/position_features.h"
#include "theory/position_key.h"

namespace kchess::knowledge {
namespace {

// -----------------------------------------------------------------------------
// Section: Feature encoding helpers
// -----------------------------------------------------------------------------

float normalized(const int value, const float maximum) {
  if (maximum <= 0.0F) return 0.0F;
  return std::clamp(static_cast<float>(value) / maximum, 0.0F, 1.5F);
}

void append(std::vector<float>& values, const float value,
            const float weight = 1.0F) {
  values.push_back(std::isfinite(value) ? value * weight : 0.0F);
}

void append_file_mask(std::vector<float>& values,
                      const std::vector<int>& files, const float weight) {
  for (int file = 0; file < 8; ++file) {
    append(values,
           std::find(files.begin(), files.end(), file) == files.end() ? 0.0F
                                                                      : 1.0F,
           weight);
  }
}

void append_signed_square_mask(std::vector<float>& values,
                               const std::vector<int>& white_squares,
                               const std::vector<int>& black_squares,
                               const float weight) {
  for (int square = 0; square < 64; ++square) {
    const bool white = std::find(white_squares.begin(), white_squares.end(),
                                 square) != white_squares.end();
    const bool black = std::find(black_squares.begin(), black_squares.end(),
                                 square) != black_squares.end();
    append(values, white == black ? 0.0F : (white ? 1.0F : -1.0F), weight);
  }
}

void append_king_square_mask(std::vector<float>& values,
                             const int white_square, const int black_square,
                             const float weight) {
  for (int square = 0; square < 64; ++square) {
    const bool white = white_square == square;
    const bool black = black_square == square;
    append(values, white == black ? 0.0F : (white ? 1.0F : -1.0F), weight);
  }
}

void append_side(std::vector<float>& values,
                 const ai::SidePositionFeatures& side) {
  // Material and piece inventory.
  append(values, normalized(side.pieces.pawns, 8.0F), 0.70F);
  append(values, normalized(side.pieces.knights, 4.0F), 0.55F);
  append(values, normalized(side.pieces.bishops, 4.0F), 0.55F);
  append(values, normalized(side.pieces.rooks, 4.0F), 0.60F);
  append(values, normalized(side.pieces.queens, 2.0F), 0.60F);
  append(values, normalized(side.material_cp, 5000.0F), 0.55F);

  // Development/activity/space. These are deterministic board facts from the
  // existing PositionFeatureExtractor, not engine evaluations.
  append(values, normalized(side.minor_pieces_off_home, 4.0F), 0.35F);
  append(values, normalized(side.mobility_squares, 64.0F), 0.30F);
  append(values, normalized(side.activity_squares, 64.0F), 0.35F);
  append(values, normalized(side.space_squares, 32.0F), 0.35F);
  append(values, normalized(side.defended_pieces, 15.0F), 0.25F);
  append_file_mask(values, side.semi_open_files, 0.22F);

  const auto& strategic = side.strategic;
  append(values, normalized(strategic.pawns.isolated_pawns, 8.0F), 0.45F);
  append(values, normalized(strategic.pawns.doubled_pawns, 8.0F), 0.45F);
  append(values, normalized(strategic.pawns.connected_pawns, 8.0F), 0.40F);

  append(values, normalized(strategic.king.pawn_shield, 3.0F), 0.45F);
  append(values, normalized(strategic.king.exposed_files, 4.0F), 0.45F);
  append(values, normalized(strategic.king.attacked_zone_squares, 8.0F),
         0.45F);
  append(values, normalized(strategic.king.enemy_attackers, 6.0F), 0.45F);

  append(values,
         normalized(static_cast<int>(strategic.weak_square_candidates.size()),
                    16.0F),
         0.22F);
  append(values,
         normalized(static_cast<int>(strategic.outpost_candidates.size()),
                    16.0F),
         0.22F);
  append(values,
         normalized(static_cast<int>(strategic.bad_piece_candidates.size()),
                    8.0F),
         0.20F);
  append(values,
         normalized(static_cast<int>(strategic.bad_knight_candidates.size()),
                    4.0F),
         0.18F);
  append(values,
         normalized(static_cast<int>(strategic.bad_bishop_candidates.size()),
                    4.0F),
         0.18F);

  append(values, normalized(strategic.light_square_pawns, 8.0F), 0.16F);
  append(values, normalized(strategic.dark_square_pawns, 8.0F), 0.16F);
  append(values, normalized(strategic.light_square_bishops, 4.0F), 0.14F);
  append(values, normalized(strategic.dark_square_bishops, 4.0F), 0.14F);

  const auto& weaknesses = side.weakness_facts;
  append(values,
         normalized(
             static_cast<int>(weaknesses.backward_pawn_candidates.size()),
             8.0F),
         0.20F);
  append(values,
         normalized(static_cast<int>(weaknesses.loose_piece_candidates.size()),
                    12.0F),
         0.20F);
  append(values,
         normalized(
             static_cast<int>(weaknesses.unprotected_pawn_candidates.size()),
             8.0F),
         0.20F);
  append(values,
         normalized(
             static_cast<int>(weaknesses.overloaded_defender_candidates.size()),
             8.0F),
         0.18F);
  append(values, weaknesses.weak_back_rank_candidate ? 1.0F : 0.0F, 0.18F);
}

int center_pawn_count(const ai::PositionFeatures& features) {
  constexpr int kD4 = 27;
  constexpr int kE4 = 28;
  constexpr int kD5 = 35;
  constexpr int kE5 = 36;
  const auto is_center = [](const int square) {
    return square == kD4 || square == kE4 || square == kD5 || square == kE5;
  };
  int count = 0;
  for (const int square : features.white.strategic.pawns.pawn_squares) {
    if (is_center(square)) ++count;
  }
  for (const int square : features.black.strategic.pawns.pawn_squares) {
    if (is_center(square)) ++count;
  }
  return count;
}

std::string canonical_position_key(std::string_view fen) {
  const auto key = stockfish_position_key(std::string(fen));
  std::ostringstream output;
  output << "sf18:" << std::hex << std::setfill('0') << std::setw(16) << key;
  return output.str();
}

std::optional<std::string> string_property(const KnowledgeNode& node,
                                           std::string_view key) {
  const auto it = node.properties.find(std::string(key));
  if (it == node.properties.end()) return std::nullopt;
  const auto* value = std::get_if<std::string>(&it->second);
  if (value == nullptr || value->empty()) return std::nullopt;
  return *value;
}

}  // namespace

// -----------------------------------------------------------------------------
// Section: Deterministic feature encoder
// -----------------------------------------------------------------------------

std::vector<float> PositionFeatureVectorEncoder::encode(
    const ai::PositionFeatures& features) const {
  std::vector<float> values;
  values.reserve(320);

  // Global board state and file topology.
  append(values, features.white_to_move ? 1.0F : -1.0F, 0.18F);
  append(values, normalized(features.side_to_move_legal_moves, 64.0F), 0.22F);
  append_file_mask(values, features.open_files, 0.30F);

  // Preserve exact pawn geometry without duplicating FEN/PGN payloads.
  append_signed_square_mask(values,
                            features.white.strategic.pawns.pawn_squares,
                            features.black.strategic.pawns.pawn_squares, 1.0F);
  append_signed_square_mask(values,
                            features.white.strategic.pawns.passed_pawn_squares,
                            features.black.strategic.pawns.passed_pawn_squares,
                            0.65F);
  append_king_square_mask(values, features.white.strategic.king.king_square,
                          features.black.strategic.king.king_square, 0.50F);

  // Color-specific structure and activity remain separate dimensions so two
  // superficially similar structures with opposite material/king conditions do
  // not collapse into the same vector.
  append_side(values, features.white);
  append_side(values, features.black);

  const int non_pawn_pieces =
      features.white.pieces.knights + features.white.pieces.bishops +
      features.white.pieces.rooks + features.white.pieces.queens +
      features.black.pieces.knights + features.black.pieces.bishops +
      features.black.pieces.rooks + features.black.pieces.queens;
  const int total_pawns =
      features.white.pieces.pawns + features.black.pieces.pawns;
  const int material_balance =
      features.white.material_cp - features.black.material_cp;

  // Explicit phase/center dimensions improve retrieval for endgame and
  // open/closed-center questions even though their raw ingredients are also
  // represented above.
  append(values, normalized(non_pawn_pieces, 14.0F), 0.45F);
  append(values, normalized(total_pawns, 16.0F), 0.30F);
  append(values,
         std::clamp(static_cast<float>(material_balance) / 4000.0F, -1.5F,
                    1.5F),
         0.40F);
  append(values, normalized(center_pawn_count(features), 4.0F), 0.45F);
  append(values, features.white.pieces.queens > 0 ? 1.0F : 0.0F, 0.25F);
  append(values, features.black.pieces.queens > 0 ? 1.0F : 0.0F, 0.25F);

  return values;
}

std::vector<float> PositionFeatureVectorEncoder::encode_fen(
    const std::string_view fen) const {
  ai::PositionFeatureExtractor extractor;
  return encode(extractor.extract(std::string(fen)));
}

std::string PositionFeatureVectorEncoder::source_version(
    const std::string_view fen) const {
  return "position_features:" + std::string(kPositionFeatureModelVersion) + ":" +
         canonical_position_key(fen);
}

// -----------------------------------------------------------------------------
// Section: Position-vector persistence and search
// -----------------------------------------------------------------------------

PositionSimilarityIndex::PositionSimilarityIndex(GraphStore& graph,
                                                 VectorIndex& vectors)
    : graph_(graph), vectors_(vectors) {}

std::string PositionSimilarityIndex::upsert_position(
    const KnowledgeNodeId& position_node_id, const std::string_view fen,
    const std::int64_t observed_at_ms) {
  if (position_node_id.empty() || fen.empty() || observed_at_ms < 0) {
    throw std::invalid_argument("invalid position similarity upsert input");
  }

  const auto owner = graph_.node(position_node_id);
  if (!owner || owner->kind != KnowledgeNodeKind::kPosition) {
    throw std::invalid_argument("position similarity owner is not a Position node");
  }

  const auto expected_key = canonical_position_key(fen);
  const auto stored_key = string_property(*owner, "position_key");
  if (!stored_key || *stored_key != expected_key) {
    throw std::invalid_argument("position FEN does not match canonical node identity");
  }

  auto values = encoder_.encode_fen(fen);
  if (values.empty()) {
    throw std::runtime_error("position feature encoder returned an empty vector");
  }

  KnowledgeVectorRecord record;
  record.metadata.owner_kind = EmbeddingOwnerKind::kNode;
  record.metadata.owner_id = position_node_id.value;
  record.metadata.vector_space = KnowledgeVectorSpace::kChessPosition;
  record.metadata.model_id = std::string(kPositionFeatureModelId);
  record.metadata.model_version = std::string(kPositionFeatureModelVersion);
  record.metadata.dimensions = static_cast<std::uint32_t>(values.size());
  record.metadata.source_version = encoder_.source_version(fen);
  record.metadata.updated_at_ms = observed_at_ms;
  record.metadata.embedding_id = make_knowledge_embedding_id(
      record.metadata.owner_kind, record.metadata.owner_id,
      record.metadata.vector_space, record.metadata.model_id,
      record.metadata.model_version);

  const auto existing = vectors_.metadata(record.metadata.embedding_id);
  record.metadata.created_at_ms =
      existing ? existing->created_at_ms : observed_at_ms;
  record.values = std::move(values);
  vectors_.upsert(record);
  return record.metadata.embedding_id;
}

bool PositionSimilarityIndex::remove_position(
    const KnowledgeNodeId& position_node_id) {
  if (position_node_id.empty()) return false;
  const auto embedding_id = make_knowledge_embedding_id(
      EmbeddingOwnerKind::kNode, position_node_id.value,
      KnowledgeVectorSpace::kChessPosition, kPositionFeatureModelId,
      kPositionFeatureModelVersion);
  return vectors_.remove(embedding_id);
}

bool PositionSimilarityIndex::is_current_position_vector(
    const KnowledgeEmbeddingMetadata& metadata,
    const KnowledgeNode& owner) const {
  if (metadata.owner_kind != EmbeddingOwnerKind::kNode ||
      metadata.vector_space != KnowledgeVectorSpace::kChessPosition ||
      metadata.model_id != kPositionFeatureModelId ||
      metadata.model_version != kPositionFeatureModelVersion ||
      owner.kind != KnowledgeNodeKind::kPosition) {
    return false;
  }
  const auto key = string_property(owner, "position_key");
  if (!key) return false;
  const std::string expected =
      "position_features:" + std::string(kPositionFeatureModelVersion) + ":" +
      *key;
  return metadata.source_version == expected;
}

std::vector<PositionSimilarityMatch> PositionSimilarityIndex::similar_to_fen(
    const std::string_view fen, const std::size_t limit,
    const float minimum_score,
    const std::optional<KnowledgeNodeId> exclude_node) const {
  if (fen.empty() || limit == 0 || !std::isfinite(minimum_score)) return {};

  const auto values = encoder_.encode_fen(fen);
  if (values.empty()) return {};

  KnowledgeVectorQuery query;
  query.vector_space = KnowledgeVectorSpace::kChessPosition;
  query.model_id = std::string(kPositionFeatureModelId);
  query.model_version = std::string(kPositionFeatureModelVersion);
  query.values = values;
  // Fetch a bounded surplus because stale/wrong-owner records and the explicit
  // excluded node are filtered after vector search.
  query.limit = std::min<std::size_t>(100, std::max<std::size_t>(limit * 3, limit));
  query.minimum_score = minimum_score;

  std::vector<PositionSimilarityMatch> result;
  result.reserve(limit);
  for (const auto& match : vectors_.search(query)) {
    if (match.metadata.owner_kind != EmbeddingOwnerKind::kNode) continue;
    KnowledgeNodeId node_id{match.metadata.owner_id};
    if (exclude_node && node_id == *exclude_node) continue;
    const auto owner = graph_.node(node_id);
    if (!owner || !is_current_position_vector(match.metadata, *owner)) continue;
    result.push_back({.position_node_id = std::move(node_id),
                      .embedding_id = match.metadata.embedding_id,
                      .score = match.score});
    if (result.size() >= limit) break;
  }
  return result;
}

}  // namespace kchess::knowledge
