#include "services/game_library_service.h"

#include <algorithm>
#include <cctype>
#include <ctime>
#include <iomanip>
#include <optional>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>

#include "chess/fen.h"
#include "chess/pgn.h"
#include "chess/move.h"
#include "chess/position_view.h"
#include "services/termination.h"

namespace kchess {
namespace {

std::string escape_json(const std::string& input) {
  std::ostringstream output;
  for (const unsigned char character : input) {
    switch (character) {
      case '"': output << "\\\""; break;
      case '\\': output << "\\\\"; break;
      case '\b': output << "\\b"; break;
      case '\f': output << "\\f"; break;
      case '\n': output << "\\n"; break;
      case '\r': output << "\\r"; break;
      case '\t': output << "\\t"; break;
      default:
        if (character < 0x20) {
          output << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                 << static_cast<int>(character) << std::dec;
        } else {
          output << character;
        }
    }
  }
  return output.str();
}

void validate_token(const std::string& value, const char* field) {
  if (value.empty() || value.size() > 128) {
    throw std::invalid_argument(std::string(field) + " must contain 1-128 characters");
  }
  if (std::any_of(value.begin(), value.end(), [](const unsigned char character) {
        return std::iscntrl(character) != 0;
      })) {
    throw std::invalid_argument(std::string(field) + " contains control characters");
  }
}

std::string normalized_collection_name(std::string value) {
  const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char c) {
    return std::isspace(c) != 0;
  });
  const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char c) {
    return std::isspace(c) != 0;
  }).base();
  if (first >= last) throw std::invalid_argument("Collection name is required");
  value = std::string(first, last);
  if (value.size() > 80) {
    throw std::invalid_argument("Collection name must contain at most 80 characters");
  }
  validate_token(value, "collection name");
  return value;
}

std::string lowercase(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char character) {
    return static_cast<char>(std::tolower(character));
  });
  return value;
}

void append_optional_int(std::ostringstream& json, const std::optional<int>& value) {
  if (value.has_value()) json << *value;
  else json << "null";
}

void append_optional_double(
    std::ostringstream& json, const std::optional<double>& value) {
  if (value.has_value()) json << std::fixed << std::setprecision(6) << *value;
  else json << "null";
}

void append_optional_string(
    std::ostringstream& json, const std::optional<std::string>& value) {
  if (value.has_value()) json << '"' << escape_json(*value) << '"';
  else json << "null";
}

bool valid_month(const std::string& month) {
  if (month.size() != 7 || month[4] != '-') return false;
  for (const std::size_t index : {0u, 1u, 2u, 3u, 5u, 6u}) {
    if (!std::isdigit(static_cast<unsigned char>(month[index]))) return false;
  }
  const int month_number = std::stoi(month.substr(5, 2));
  return month_number >= 1 && month_number <= 12;
}

}  // namespace

GameLibraryService::GameLibraryService(Database& database, ProfileService& profile_service)
    : database_(database), profile_service_(profile_service) {}

std::string GameLibraryService::game_record_json(
    const GameRecord& game, const bool include_moves) const {
  bool profile_is_black = false;
  std::string profile_color = "unknown";
  if (const auto profile = database_.profile(game.profile_id); profile.has_value()) {
    const auto identity = profile->provider_username.value_or(profile->display_name);
    const auto normalized_identity = lowercase(identity);
    if (normalized_identity == lowercase(game.black_name)) {
      profile_is_black = true;
      profile_color = "black";
    } else if (normalized_identity == lowercase(game.white_name)) {
      profile_color = "white";
    }
  }
  const auto provider_accuracy = profile_is_black
      ? game.provider_accuracy_black : game.provider_accuracy_white;
  const auto local_accuracy = profile_is_black
      ? game.local_accuracy_black : game.local_accuracy_white;
  const auto preferred_accuracy = local_accuracy.has_value() ? local_accuracy : provider_accuracy;

  std::ostringstream json;
  json << "{\"id\":\"" << escape_json(game.id) << "\",\"kind\":\""
       << escape_json(game.kind) << "\",\"whiteName\":\""
       << escape_json(game.white_name) << "\",\"blackName\":\""
       << escape_json(game.black_name) << "\",\"whiteRating\":";
  append_optional_int(json, game.white_rating);
  json << ",\"blackRating\":";
  append_optional_int(json, game.black_rating);
  json << ",\"result\":\"" << escape_json(game.result)
       << "\",\"event\":\"" << escape_json(game.event)
       << "\",\"site\":\"" << escape_json(game.site)
       << "\",\"date\":\"" << escape_json(game.date)
       << "\",\"timeControl\":\"" << escape_json(game.time_control)
       << "\",\"startingFen\":\"" << escape_json(game.starting_fen)
       << "\",\"createdAt\":" << game.created_at << ",\"endedAt\":" << game.ended_at
       << ",\"providerGameId\":";
  append_optional_string(json, game.provider_game_id);
  json << ",\"providerUrl\":";
  append_optional_string(json, game.provider_url);
  json << ",\"profileColor\":\"" << profile_color
       << "\",\"openingEco\":";
  append_optional_string(json, game.opening_eco);
  json << ",\"openingName\":";
  append_optional_string(json, game.opening_name);
  json << ",\"openingPly\":";
  append_optional_int(json, game.opening_ply);
  json << ",\"providerOutcome\":\"" << escape_json(game.provider_outcome)
       << "\",\"timeControlType\":\"" << escape_json(game.time_control_type)
       << "\",\"providerAccuracy\":";
  append_optional_double(json, provider_accuracy);
  json << ",\"localAccuracy\":";
  append_optional_double(json, local_accuracy);
  json << ",\"accuracy\":";
  append_optional_double(json, preferred_accuracy);
  json << ",\"accuracySource\":\""
       << (local_accuracy ? "local" : provider_accuracy ? "provider" : "none")
       << "\",\"favorite\":" << (game.favorite ? "true" : "false")
       << ",\"favoriteCollectionId\":";
  append_optional_string(json, game.favorite_collection_id);
  json << ",\"downloaded\":" << (game.downloaded ? "true" : "false")
       << ",\"analyzed\":" << (game.analyzed ? "true" : "false")
       << ",\"termination\":\"" << termination_bucket(game.pgn, game.result)
       << "\",\"isFixture\":false";
  if (include_moves) {
    nlohmann::json outcome = nullptr;
    if (!game.moves.empty()) {
      const auto& final_move = game.moves.back();
      const bool checkmate = final_move.san.ends_with('#')
          || final_move.san.ends_with("++");
      if (checkmate) {
        outcome = {
            {"result", final_move.side_to_move == "black" ? "0-1" : "1-0"},
            {"checkmate", true},
        };
      }
    }
    if (outcome.is_null()
        && (game.result == "1-0" || game.result == "0-1"
            || game.result == "1/2-1/2" || game.result == "½-½")) {
      outcome = {{"result", game.result}, {"checkmate", false}};
    }
    json << ",\"startingPosition\":" << position_view_json(game.starting_fen)
         << ",\"outcome\":" << outcome.dump()
         << ",\"pgn\":\"" << escape_json(game.pgn) << "\",\"moves\":[";
    for (std::size_t index = 0; index < game.moves.size(); ++index) {
      if (index != 0) json << ',';
      const auto& move = game.moves[index];
      json << "{\"plyIndex\":" << move.ply_index
           << ",\"moveNumber\":" << move.move_number
           << ",\"sideToMove\":\"" << escape_json(move.side_to_move)
           << "\",\"san\":\"" << escape_json(move.san)
           << "\",\"uci\":\"" << escape_json(move.uci)
           << "\",\"fenBefore\":\"" << escape_json(move.fen_before)
           << "\",\"fenAfter\":\"" << escape_json(move.fen_after)
           << "\",\"positionBefore\":" << position_view_json(move.fen_before)
           << ",\"positionAfter\":" << position_view_json(move.fen_after) << "}";
    }
    json << ']';
  }
  json << '}';
  return json.str();
}

std::string GameLibraryService::games_json() const {
  const auto profile = database_.active_profile();
  if (!profile.has_value()) return "[]";
  const auto games = database_.games(profile->id);
  std::ostringstream json;
  json << '[';
  for (std::size_t index = 0; index < games.size(); ++index) {
    if (index != 0) json << ',';
    json << game_record_json(games[index], false);
  }
  json << ']';
  return json.str();
}

std::string GameLibraryService::favorite_games_json() const {
  const auto games = database_.favorite_games();
  std::ostringstream json;
  json << '[';
  for (std::size_t index = 0; index < games.size(); ++index) {
    if (index != 0) json << ',';
    json << game_record_json(games[index], false);
  }
  json << ']';
  return json.str();
}

std::string GameLibraryService::game_json(const std::string& game_id) const {
  validate_token(game_id, "game id");
  const auto game = database_.game(game_id);
  if (!game.has_value()) throw std::runtime_error("Game not found");
  return game_record_json(*game, true);
}

std::string GameLibraryService::query_games_json(const std::string& query_text) const {
  const auto profile = database_.active_profile();
  if (!profile.has_value()) return "[]";
  const auto query = nlohmann::json::parse(query_text);
  const auto search = lowercase(query.value("search", std::string{}));
  const auto outcome = query.value("outcome", std::string{"all"});
  const auto color = query.value("color", std::string{"all"});
  const auto month = query.value("month", std::string{});
  const bool favorite_only = query.value("favoriteOnly", false);
  const bool apply_month = query.value("applyMonth", false);
  const auto sort = query.value("sort", std::string{"newest"});
  const auto controls = query.value("timeControls", std::vector<std::string>{});
  const bool require_analyzed = std::ranges::find(controls, "analyzed") != controls.end();
  const bool require_not_analyzed =
      std::ranges::find(controls, "notAnalyzed") != controls.end();
  std::vector<std::string> time_controls;
  for (const auto& value : controls) {
    if (value != "analyzed" && value != "notAnalyzed") time_controls.push_back(value);
  }

  const auto identity = lowercase(
      profile->provider_username.value_or(profile->display_name));
  auto games = database_.games(profile->id);
  games.erase(std::remove_if(games.begin(), games.end(), [&](const GameRecord& game) {
    if (favorite_only && !game.favorite) return true;
    if (!search.empty() && lowercase(game.white_name).find(search) == std::string::npos
        && lowercase(game.black_name).find(search) == std::string::npos) return true;
    if (outcome != "all" && game.provider_outcome != outcome) return true;
    if (color == "white" && lowercase(game.white_name) != identity) return true;
    if (color == "black" && lowercase(game.black_name) != identity) return true;
    if (!time_controls.empty()
        && std::ranges::find(time_controls, game.time_control_type) == time_controls.end()) {
      return true;
    }
    if (require_analyzed && !game.analyzed) return true;
    if (require_not_analyzed && game.analyzed) return true;
    if (apply_month && !month.empty() && game.ended_at > 0) {
      const std::time_t timestamp = static_cast<std::time_t>(game.ended_at);
      std::tm utc{};
#if defined(_WIN32)
      gmtime_s(&utc, &timestamp);
#else
      gmtime_r(&timestamp, &utc);
#endif
      std::ostringstream value;
      value << std::setfill('0') << std::setw(4) << utc.tm_year + 1900
            << '-' << std::setw(2) << utc.tm_mon + 1;
      if (value.str() != month) return true;
    }
    return false;
  }), games.end());

  std::ranges::sort(games, [&](const GameRecord& left, const GameRecord& right) {
    if (sort == "oldest") return left.ended_at < right.ended_at;
    if (sort == "accuracyHigh" || sort == "accuracyLow") {
      const auto accuracy = [&](const GameRecord& game) -> double {
        const bool black = lowercase(game.black_name) == identity;
        const auto local = black ? game.local_accuracy_black : game.local_accuracy_white;
        const auto provider = black ? game.provider_accuracy_black : game.provider_accuracy_white;
        return local.value_or(provider.value_or(sort == "accuracyHigh" ? -1.0 : 101.0));
      };
      return sort == "accuracyHigh" ? accuracy(left) > accuracy(right)
                                     : accuracy(left) < accuracy(right);
    }
    return left.ended_at > right.ended_at;
  });

  std::ostringstream json;
  json << '[';
  for (std::size_t index = 0; index < games.size(); ++index) {
    if (index != 0) json << ',';
    json << game_record_json(games[index], false);
  }
  json << ']';
  return json.str();
}

std::string GameLibraryService::resolve_board_move_json(
    const std::string& game_id,
    const std::string& fen,
    const std::string& source,
    const std::string& target,
    const int first_candidate_ply) const {
  validate_token(game_id, "game id");
  if (source.size() != 2 || target.size() != 2) {
    throw std::invalid_argument("Board squares must use algebraic coordinates");
  }
  const auto game = database_.game(game_id);
  if (!game.has_value()) throw std::runtime_error("Game not found");

  const auto applied = apply_legal_uci_move(fen, source + target);
  std::optional<int> main_line_ply;
  const int start = std::max(0, first_candidate_ply);
  for (int index = start; index < static_cast<int>(game->moves.size()); ++index) {
    const auto& recorded = game->moves[static_cast<std::size_t>(index)];
    if (recorded.uci == applied.uci && same_chess_position(recorded.fen_before, fen)) {
      main_line_ply = index;
      break;
    }
  }

  nlohmann::json json{
      {"uci", applied.uci},
      {"san", applied.san},
      {"fenAfter", applied.fen_after},
      {"positionAfter", nlohmann::json::parse(position_view_json(applied.fen_after))},
      {"mainLinePly", main_line_ply.has_value()
          ? nlohmann::json(*main_line_ply) : nlohmann::json(nullptr)},
  };
  return json.dump();
}

std::string GameLibraryService::import_pgn_json(const std::string& pgn) {
  const auto profile = profile_service_.ensure_local_profile();
  const auto parsed = parse_pgn(pgn);
  if (!parsed.valid) throw std::invalid_argument("Invalid PGN: " + parsed.error);
  if (parsed.game.moves.empty()) throw std::invalid_argument("PGN contains no main-line moves");
  const auto id = database_.import_pgn(profile.id, parsed.game);
  return game_json(id);
}

std::string GameLibraryService::import_fen_json(
    const std::string& fen, const std::string& display_name) {
  const auto profile = profile_service_.ensure_local_profile();
  validate_token(display_name, "position name");
  const auto validation = validate_fen(fen);
  if (!validation.valid) throw std::invalid_argument("Invalid FEN: " + validation.error);
  const auto id = database_.import_fen(profile.id, validation.normalized, display_name);
  return game_json(id);
}

void GameLibraryService::set_favorite(const std::string& game_id, const bool value) {
  validate_token(game_id, "game id");
  const auto profile = database_.active_profile();
  if (!profile.has_value()) throw std::runtime_error("no active profile");
  database_.set_favorite(profile->id, game_id, value);
}

std::string GameLibraryService::favorite_collections_json() const {
  const auto profile = database_.active_profile();
  if (!profile.has_value()) return "[]";
  const auto collections = database_.favorite_collections(profile->id);
  std::ostringstream json;
  json << '[';
  for (std::size_t index = 0; index < collections.size(); ++index) {
    if (index != 0) json << ',';
    const auto& collection = collections[index];
    json << "{\"id\":\"" << escape_json(collection.id)
         << "\",\"name\":\"" << escape_json(collection.name)
         << "\",\"gameCount\":" << collection.game_count
         << ",\"createdAt\":" << collection.created_at << '}';
  }
  json << ']';
  return json.str();
}

std::string GameLibraryService::create_favorite_collection_json(
    const std::string& name) {
  const auto profile = database_.active_profile();
  if (!profile.has_value()) throw std::runtime_error("no active profile");
  const auto collection = database_.create_favorite_collection(
      profile->id, normalized_collection_name(name));
  std::ostringstream json;
  json << "{\"id\":\"" << escape_json(collection.id)
       << "\",\"name\":\"" << escape_json(collection.name)
       << "\",\"gameCount\":0,\"createdAt\":" << collection.created_at << '}';
  return json.str();
}

void GameLibraryService::rename_favorite_collection(
    const std::string& collection_id, const std::string& name) {
  validate_token(collection_id, "collection id");
  const auto profile = database_.active_profile();
  if (!profile.has_value()) throw std::runtime_error("no active profile");
  database_.rename_favorite_collection(
      profile->id, collection_id, normalized_collection_name(name));
}

void GameLibraryService::delete_favorite_collection(
    const std::string& collection_id) {
  validate_token(collection_id, "collection id");
  const auto profile = database_.active_profile();
  if (!profile.has_value()) throw std::runtime_error("no active profile");
  database_.delete_favorite_collection(profile->id, collection_id);
}

void GameLibraryService::set_favorite_collection(
    const std::string& game_id,
    const std::optional<std::string>& collection_id) {
  validate_token(game_id, "game id");
  if (collection_id.has_value()) validate_token(*collection_id, "collection id");
  const auto profile = database_.active_profile();
  if (!profile.has_value()) throw std::runtime_error("no active profile");
  database_.set_favorite_collection(profile->id, game_id, collection_id);
}

void GameLibraryService::set_downloaded(const std::string& game_id, const bool value) {
  validate_token(game_id, "game id");
  const auto profile = database_.active_profile();
  if (!profile.has_value()) throw std::runtime_error("no active profile");
  // ABI-compatible entry point: a "download" is now just a global favorite
  // placed in the Downloads collection, not a separate persistence state.
  database_.set_downloaded(profile->id, game_id, value);
}

void GameLibraryService::delete_local_game(const std::string& game_id) {
  validate_token(game_id, "game id");
  const auto profile = database_.active_profile();
  if (!profile.has_value()) throw std::runtime_error("no active profile");
  // Local/imported entries are identified by the game itself, not by the
  // profile type. This allows PGN/FEN imports inside Chess.com/Lichess
  // libraries while provider games remain protected by Database::delete_local_game.
  database_.delete_local_game(profile->id, game_id);
}

void GameLibraryService::clear_cached_month(
    const std::string& profile_id, const std::string& month) {
  validate_token(profile_id, "profile id");
  if (!valid_month(month)) {
    throw std::invalid_argument("Invalid month; expected YYYY-MM");
  }
  const auto active = database_.active_profile();
  if (!active.has_value() || active->id != profile_id ||
      active->type == ProfileType::local_pgn_fen) {
    throw std::runtime_error("Provider profile is not active");
  }
  database_.clear_cached_month(profile_id, month);
}

}  // namespace kchess
