#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>

#include "persistence/database.h"

namespace {

using namespace kchess;
int assertions = 0;

void expect(const bool condition, const char* message) {
  ++assertions;
  if (!condition) throw std::runtime_error(message);
}

}  // namespace

int main() {
  const auto directory = std::filesystem::temp_directory_path()
      / "kchess_phase4_provider_persistence";
  try {
    std::filesystem::remove_all(directory);
    ProviderProfile remote_profile{
        .provider = ProviderType::chess_com,
        .username = "FixtureUser",
        .display_name = "Fixture User",
        .title = "GM",
        .fallback_asset = "provider_chesscom_fallback.png",
        .joined = 100,
        .provider_specific_id = "42",
    };
    const ResponseCacheInfo profile_cache{
        .etag = "profile-etag", .last_modified = "yesterday",
        .fetched_at = 1000, .expires_at = 1300};
    std::string profile_id;
    std::string game_id;
    {
      Database database(directory);
      database.open_and_migrate();
      const auto profile = database.create_provider_profile(
          remote_profile, profile_cache, "{\"username\":\"FixtureUser\"}");
      profile_id = profile.id;
      expect(profile.provider_specific_id == "42", "provider profile id persisted");
      const auto cached_profile = database.provider_cache(profile.id, "profile");
      expect(cached_profile && cached_profile->validators.etag == "profile-etag",
             "profile validators persisted");
      database.put_provider_cache(
          profile.id, "stats", "[{\"key\":\"rapid\"}]",
          {.etag = "stats-etag", .fetched_at = 1001, .expires_at = 1301});

      ParsedGame parsed;
      parsed.raw_pgn = "[Result \"1-0\"]\n\n1. e4 e5 1-0";
      parsed.initial_fen = "start";
      parsed.moves.push_back({
          .ply_index = 0, .move_number = 1, .side_to_move = "white",
          .san = "e4", .uci = "e2e4", .fen_before = "before", .fen_after = "after"});
      ProviderGame provider_game{
          .provider = ProviderType::chess_com,
          .provider_game_id = "https://www.chess.com/game/live/fixture",
          .url = "https://www.chess.com/game/live/fixture",
          .pgn = parsed.raw_pgn,
          .white_username = "FixtureUser",
          .black_username = "Other",
          .white_rating = 1800,
          .black_rating = 1790,
          .result = "1-0",
          .profile_outcome = ProviderOutcome::win,
          .ended_at = 1234,
          .time_control = "600+5",
          .time_control_type = TimeControlType::rapid,
          .rules = "chess",
          .provider_accuracy_white = 91.2,
      };
      const std::vector<ProviderStoredGame> batch{{provider_game, parsed}};
      expect(database.upsert_provider_games(
                 profile.id, "2026-07", batch,
                 {.etag = "month-etag", .fetched_at = 1002, .expires_at = 1302}) == 1,
             "first provider game inserted");
      expect(database.upsert_provider_games(
                 profile.id, "2026-07", batch,
                 {.etag = "month-etag-2", .fetched_at = 1003, .expires_at = 1303}) == 0,
             "repeat provider game deduplicated");
      auto games = database.games(profile.id);
      expect(games.size() == 1 && games.front().provider_outcome == "win",
             "normalized provider outcome persisted");
      expect(games.front().provider_accuracy_white == 91.2,
             "provider accuracy persisted separately");
      game_id = games.front().id;
      database.set_favorite(profile.id, game_id, true);
      database.set_downloaded(profile.id, game_id, true);
      games = database.games(profile.id);
      expect(games.front().favorite && games.front().downloaded,
             "favorite and download are independent local state");
    }
    {
      Database reopened(directory);
      reopened.open_and_migrate();
      const auto profile = reopened.profile(profile_id);
      const auto games = reopened.games(profile_id);
      const auto game = reopened.game(game_id);
      expect(profile && profile->display_name == "Fixture User",
             "provider profile available offline after restart");
      expect(games.size() == 1 && games.front().favorite && games.front().downloaded,
             "provider game and local state available offline");
      expect(game && game->moves.size() == 1 && game->moves.front().uci == "e2e4",
             "normalized game opens through existing move model");
      const auto stats = reopened.provider_cache(profile_id, "stats");
      const auto month = reopened.provider_cache(profile_id, "month:2026-07");
      expect(stats && month && month->validators.etag == "month-etag-2",
             "stats and month caches survive restart");
    }
    std::filesystem::remove_all(directory);
    std::cout << "All " << assertions
              << " KChess phase-4 provider persistence assertions passed.\n";
    return EXIT_SUCCESS;
  } catch (const std::exception& error) {
    std::filesystem::remove_all(directory);
    std::cerr << "Provider persistence test failed: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
