#include "providers/game_provider.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <optional>
#include <sstream>
#include <unordered_set>

#include <nlohmann/json.hpp>

#include "providers/provider_common.h"

namespace kchess {
namespace {

using nlohmann::json;

template <typename T>
std::optional<T> optional_value(const json& value, const char* key) {
  const auto found = value.find(key);
  if (found == value.end() || found->is_null()) return std::nullopt;
  try {
    return found->get<T>();
  } catch (const json::exception&) {
    return std::nullopt;
  }
}

json parse_json(const std::string& body, const std::string& resource) {
  try {
    return json::parse(body);
  } catch (const json::exception& error) {
    throw ProviderException(
        HttpError::invalid_response,
        "invalid Chess.com " + resource + " JSON: " + error.what());
  }
}

TimeControlType chess_com_time_control(const std::string& value) {
  if (value == "daily") return TimeControlType::daily;
  if (value == "rapid") return TimeControlType::rapid;
  if (value == "blitz") return TimeControlType::blitz;
  if (value == "bullet") return TimeControlType::bullet;
  return TimeControlType::unknown;
}

bool is_draw_code(const std::string& value) {
  static const std::unordered_set<std::string> codes{
      "agreed", "repetition", "stalemate", "insufficient", "50move",
      "timevsinsufficient", "draw"};
  return codes.contains(value);
}

ProviderOutcome chess_com_outcome(const std::string& code) {
  if (code == "win") return ProviderOutcome::win;
  if (is_draw_code(code)) return ProviderOutcome::draw;
  if (!code.empty()) return ProviderOutcome::loss;
  return ProviderOutcome::unknown;
}

std::string pgn_result(const std::string& white, const std::string& black) {
  if (white == "win") return "1-0";
  if (black == "win") return "0-1";
  if (is_draw_code(white) || is_draw_code(black)) return "1/2-1/2";
  return "*";
}

std::string lowercase(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char character) {
    return static_cast<char>(std::tolower(character));
  });
  return value;
}

ResponseCacheInfo cache_or_default(const HttpResponse& response) {
  return response_cache_info(response);
}

}  // namespace

ProviderResult<ProviderProfile> ChessComProvider::fetch_profile(
    const std::string& username,
    const CacheValidators& validators,
    const std::shared_ptr<CancelToken>& cancel) {
  auto request = provider_request(
      "https://api.chess.com/pub/player/" + url_encode(username),
      "application/json", validators, cancel);
  request.max_body_bytes = 2U * 1024U * 1024U;
  const auto response = scheduler_.get(type(), http_, request);
  ProviderResult<ProviderProfile> result{.cache = cache_or_default(response)};
  if (!accept_provider_response(response)) {
    result.not_modified = true;
    return result;
  }
  const auto data = parse_json(response.body, "profile");
  const auto canonical = optional_value<std::string>(data, "username");
  if (!canonical.has_value() || canonical->empty()) {
    throw ProviderException(HttpError::invalid_response, "Chess.com profile has no username");
  }
  ProviderProfile profile;
  profile.provider = type();
  profile.username = *canonical;
  // Privacy boundary: online profiles use only the provider username as identity.
  // Real names and other real-world profile metadata are deliberately ignored.
  profile.display_name = *canonical;
  profile.title = optional_value<std::string>(data, "title");
  profile.avatar_url = optional_value<std::string>(data, "avatar");
  profile.fallback_asset = "provider_chesscom_fallback.png";
  profile.fide = optional_value<int>(data, "fide");
  result.value = std::move(profile);
  return result;
}

ProviderResult<ProviderStats> ChessComProvider::fetch_stats(
    const std::string& username,
    const CacheValidators& validators,
    const std::shared_ptr<CancelToken>& cancel) {
  auto request = provider_request(
      "https://api.chess.com/pub/player/" + url_encode(username) + "/stats",
      "application/json", validators, cancel);
  request.max_body_bytes = 4U * 1024U * 1024U;
  const auto response = scheduler_.get(type(), http_, request);
  ProviderResult<ProviderStats> result{.cache = cache_or_default(response)};
  if (!accept_provider_response(response)) {
    result.not_modified = true;
    return result;
  }
  const auto data = parse_json(response.body, "stats");
  ProviderStats stats{.provider = type()};
  for (const auto& [key, value] : data.items()) {
    if (!(key == "chess_bullet" || key == "chess_blitz" || key == "chess_rapid"
          || key == "chess_daily" || key.starts_with("chess960"))
        || !value.is_object()) {
      continue;
    }
    ProviderPerformance performance;
    performance.key = key.starts_with("chess_") ? key.substr(6) : key;
    if (const auto last = value.find("last"); last != value.end() && last->is_object()) {
      performance.current_rating = optional_value<int>(*last, "rating");
    }
    if (const auto best = value.find("best"); best != value.end() && best->is_object()) {
      performance.best_rating = optional_value<int>(*best, "rating");
    }
    if (const auto record = value.find("record");
        record != value.end() && record->is_object()) {
      performance.wins = optional_value<int>(*record, "win");
      performance.losses = optional_value<int>(*record, "loss");
      performance.draws = optional_value<int>(*record, "draw");
      if (performance.wins && performance.losses && performance.draws) {
        performance.games = *performance.wins + *performance.losses + *performance.draws;
      }
    }
    stats.performances.push_back(std::move(performance));
  }
  result.value = std::move(stats);
  return result;
}

ProviderResult<std::vector<ProviderGame>> ChessComProvider::fetch_games(
    const std::string& username,
    const ProviderGameQuery& query,
    const CacheValidators& validators,
    const std::shared_ptr<CancelToken>& cancel) {
  if (query.year < 2000 || query.month < 1 || query.month > 12) {
    throw std::invalid_argument("invalid Chess.com archive month");
  }
  std::ostringstream url;
  url << "https://api.chess.com/pub/player/" << url_encode(username) << "/games/"
      << std::setw(4) << std::setfill('0') << query.year << '/'
      << std::setw(2) << std::setfill('0') << query.month;
  auto request = provider_request(url.str(), "application/json", validators, cancel);
  const auto response = scheduler_.get(type(), http_, request);
  ProviderResult<std::vector<ProviderGame>> result{.cache = cache_or_default(response)};
  // Chess.com may return 404 for a valid profile/month combination with no
  // published archive. Profile existence was validated independently.
  if (response.error == HttpError::not_found) {
    result.value = std::vector<ProviderGame>{};
    return result;
  }
  if (!accept_provider_response(response)) {
    result.not_modified = true;
    return result;
  }
  const auto data = parse_json(response.body, "games");
  const auto games = data.find("games");
  if (games == data.end() || !games->is_array()) {
    throw ProviderException(HttpError::invalid_response, "Chess.com games array is missing");
  }
  const auto requested_username = lowercase(username);
  std::vector<ProviderGame> normalized;
  normalized.reserve(games->size());
  for (const auto& game : *games) {
    if (!game.is_object()) continue;
    const auto pgn = optional_value<std::string>(game, "pgn");
    const auto url_value = optional_value<std::string>(game, "url");
    if (!pgn.has_value() || !url_value.has_value() || pgn->empty() || url_value->empty()) continue;
    const auto white = game.find("white");
    const auto black = game.find("black");
    if (white == game.end() || black == game.end() || !white->is_object() || !black->is_object()) {
      continue;
    }
    const auto white_name = optional_value<std::string>(*white, "username").value_or("White");
    const auto black_name = optional_value<std::string>(*black, "username").value_or("Black");
    const auto white_code = optional_value<std::string>(*white, "result").value_or("");
    const auto black_code = optional_value<std::string>(*black, "result").value_or("");
    ProviderOutcome outcome = ProviderOutcome::unknown;
    if (lowercase(white_name) == requested_username) outcome = chess_com_outcome(white_code);
    else if (lowercase(black_name) == requested_username) outcome = chess_com_outcome(black_code);
    ProviderGame item;
    item.provider = type();
    item.provider_game_id = *url_value;
    item.url = *url_value;
    item.pgn = *pgn;
    item.white_username = white_name;
    item.black_username = black_name;
    item.white_rating = optional_value<int>(*white, "rating");
    item.black_rating = optional_value<int>(*black, "rating");
    item.result = pgn_result(white_code, black_code);
    item.profile_outcome = outcome;
    item.ended_at = optional_value<std::int64_t>(game, "end_time").value_or(0);
    item.time_control = optional_value<std::string>(game, "time_control").value_or("");
    const auto time_class = optional_value<std::string>(game, "time_class").value_or("");
    item.time_control_type = chess_com_time_control(time_class);
    item.rules = optional_value<std::string>(game, "rules").value_or("chess");
    if (const auto accuracies = game.find("accuracies");
        accuracies != game.end() && accuracies->is_object()) {
      item.provider_accuracy_white = optional_value<double>(*accuracies, "white");
      item.provider_accuracy_black = optional_value<double>(*accuracies, "black");
    }
    item.eco = optional_value<std::string>(game, "eco");
    item.tournament = optional_value<std::string>(game, "tournament");
    item.match = optional_value<std::string>(game, "match");
    normalized.push_back(std::move(item));
  }
  result.value = std::move(normalized);
  return result;
}

ProviderResult<std::vector<std::string>> ChessComProvider::fetch_available_months(
    const std::string& username,
    const CacheValidators& validators,
    const std::shared_ptr<CancelToken>& cancel) {
  auto request = provider_request(
      "https://api.chess.com/pub/player/" + url_encode(username) + "/games/archives",
      "application/json", validators, cancel);
  request.max_body_bytes = 4U * 1024U * 1024U;
  const auto response = scheduler_.get(type(), http_, request);
  ProviderResult<std::vector<std::string>> result{.cache = cache_or_default(response)};
  if (!accept_provider_response(response)) {
    result.not_modified = true;
    return result;
  }
  const auto data = parse_json(response.body, "archives");
  const auto archives = data.find("archives");
  if (archives == data.end() || !archives->is_array()) {
    throw ProviderException(HttpError::invalid_response, "Chess.com archives array is missing");
  }
  std::vector<std::string> months;
  for (const auto& value : *archives) {
    if (!value.is_string()) continue;
    const auto url_value = value.get<std::string>();
    if (url_value.size() >= 7) {
      const auto month = url_value.substr(url_value.size() - 7);
      if (month[4] == '/') months.push_back(month.substr(0, 4) + '-' + month.substr(5));
    }
  }
  result.value = std::move(months);
  return result;
}

}  // namespace kchess
