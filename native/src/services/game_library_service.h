#pragma once

#include <optional>
#include <string>

#include "core/models.h"
#include "persistence/database.h"
#include "services/profile_service.h"

namespace kchess {

class GameLibraryService {
 public:
  GameLibraryService(Database& database, ProfileService& profile_service);

  std::string games_json() const;
  std::string game_json(const std::string& game_id) const;
  std::string import_pgn_json(const std::string& pgn);
  std::string import_fen_json(const std::string& fen, const std::string& display_name);

  void set_favorite(const std::string& game_id, bool value);
  std::string favorite_collections_json() const;
  std::string create_favorite_collection_json(const std::string& name);
  void rename_favorite_collection(
      const std::string& collection_id, const std::string& name);
  void delete_favorite_collection(const std::string& collection_id);
  void set_favorite_collection(
      const std::string& game_id, const std::optional<std::string>& collection_id);
  void set_downloaded(const std::string& game_id, bool value);
  void delete_local_game(const std::string& game_id);
  void clear_cached_month(const std::string& profile_id, const std::string& month);

 private:
  std::string game_record_json(const GameRecord& game, bool include_moves) const;

  Database& database_;
  ProfileService& profile_service_;
};

}  // namespace kchess
