#include "providers/game_provider.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <optional>
#include <sstream>

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
        "invalid Lichess " + resource + " JSON: " + error.what());
  }
}

std::string lowercase(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char character) {
    return static_cast<char>(std::tolower(character));
  });
  return value;
}

TimeControlType lichess_time_control(const std::string& value) {
  if (value == "bullet" || value == "ultraBullet") return TimeControlType::bullet;
  if (value == "blitz") return TimeControlType::blitz;
  if (value == "rapid") return TimeControlType::rapid;
  if (value == "classical") return TimeControlType::classical;
  if (value == "correspondence") return TimeControlType::correspondence;
  return TimeControlType::unknown;
}

std::string player_name(const json& player, const std::string& fallback) {
  if (const auto user = player.find("user"); user != player.end() && user->is_object()) {
    if (const auto name = optional_value<std::string>(*user, "name"); name.has_value()) {
      return *name;
    }
    if (const auto id = optional_value<std::string>(*user, "id"); id.has_value()) return *id;
  }
  return optional_value<std::string>(player, "name").value_or(fallback);
}

std::optional<double> player_accuracy(const json& player) {
  const auto analysis = player.find("analysis");
  return analysis != player.end() && analysis->is_object()
      ? optional_value<double>(*analysis, "accuracy")
      : std::nullopt;
}

bool is_finished_status(const std::string& status) {
  return !(status == "created" || status == "started");
}

ProviderGame parse_game(const json& game, const std::string& username) {
  const auto id = optional_value<std::string>(game, "id");
  const auto pgn = optional_value<std::string>(game, "pgn");
  const auto players = game.find("players");
  if (!id.has_value() || id->empty() || !pgn.has_value() || pgn->empty()
      || players == game.end() || !players->is_object()) {
    throw ProviderException(HttpError::invalid_response, "Lichess game is missing id, PGN or players");
  }
  const auto white = players->find("white");
  const auto black = players->find("black");
  if (white == players->end() || black == players->end()
      || !white->is_object() || !black->is_object()) {
    throw ProviderException(HttpError::invalid_response, "Lichess game player data is invalid");
  }
  const auto white_name = player_name(*white, "White");
  const auto black_name = player_name(*black, "Black");
  const auto winner = optional_value<std::string>(game, "winner").value_or("");
  const auto status = optional_value<std::string>(game, "status").value_or("");
  const std::string result = winner == "white" ? "1-0" : winner == "black" ? "0-1"
      : is_finished_status(status) ? "1/2-1/2" : "*";
  ProviderOutcome outcome = ProviderOutcome::unknown;
  const auto requested = lowercase(username);
  if (lowercase(white_name) == requested) {
    outcome = winner == "white" ? ProviderOutcome::win
        : winner == "black" ? ProviderOutcome::loss
        : is_finished_status(status) ? ProviderOutcome::draw : ProviderOutcome::unknown;
  } else if (lowercase(black_name) == requested) {
    outcome = winner == "black" ? ProviderOutcome::win
        : winner == "white" ? ProviderOutcome::loss
        : is_finished_status(status) ? ProviderOutcome::draw : ProviderOutcome::unknown;
  }
  ProviderGame result_game;
  result_game.provider = ProviderType::lichess;
  result_game.provider_game_id = *id;
  result_game.url = "https://lichess.org/" + *id;
  result_game.pgn = *pgn;
  result_game.white_username = white_name;
  result_game.black_username = black_name;
  result_game.white_rating = optional_value<int>(*white, "rating");
  result_game.black_rating = optional_value<int>(*black, "rating");
  result_game.result = result;
  result_game.profile_outcome = outcome;
  result_game.ended_at = optional_value<std::int64_t>(game, "lastMoveAt").value_or(0) / 1000;
  const auto speed = optional_value<std::string>(game, "speed").value_or("");
  result_game.time_control_type = lichess_time_control(speed);
  result_game.rules = optional_value<std::string>(game, "variant").value_or("standard");
  if (const auto clock = game.find("clock"); clock != game.end() && clock->is_object()) {
    const auto initial = optional_value<int>(*clock, "initial");
    const auto increment = optional_value<int>(*clock, "increment");
    if (initial && increment) {
      result_game.time_control = std::to_string(*initial) + "+" + std::to_string(*increment);
    }
  } else if (const auto days = optional_value<int>(game, "daysPerTurn"); days.has_value()) {
    result_game.time_control = std::to_string(*days) + "d";
  }
  result_game.provider_accuracy_white = player_accuracy(*white);
  result_game.provider_accuracy_black = player_accuracy(*black);
  if (const auto opening = game.find("opening");
      opening != game.end() && opening->is_object()) {
    result_game.eco = optional_value<std::string>(*opening, "eco");
  }
  if (const auto arena = game.find("arenaTour"); arena != game.end() && arena->is_object()) {
    result_game.tournament = optional_value<std::string>(*arena, "id");
  } else if (const auto swiss = game.find("swissTour");
             swiss != game.end() && swiss->is_object()) {
    result_game.tournament = optional_value<std::string>(*swiss, "id");
  }
  result_game.completed = is_finished_status(status);
  return result_game;
}

}  // namespace

ProviderResult<ProviderProfile> LichessProvider::fetch_profile(
    const std::string& username,
    const CacheValidators& validators,
    const std::shared_ptr<CancelToken>& cancel) {
  auto request = provider_request(
      "https://lichess.org/api/user/" + url_encode(username),
      "application/json", validators, cancel);
  request.max_body_bytes = 2U * 1024U * 1024U;
  const auto response = scheduler_.get(type(), http_, request);
  ProviderResult<ProviderProfile> result{.cache = response_cache_info(response)};
  if (!accept_provider_response(response)) {
    result.not_modified = true;
    return result;
  }
  const auto data = parse_json(response.body, "profile");
  const auto canonical = optional_value<std::string>(data, "username");
  if (!canonical || canonical->empty()) {
    throw ProviderException(HttpError::invalid_response, "Lichess profile has no username");
  }
  ProviderProfile profile;
  profile.provider = type();
  profile.username = *canonical;
  profile.display_name = *canonical;
  profile.title = optional_value<std::string>(data, "title");
  profile.fallback_asset = "provider_lichess_fallback.png";
  profile.flair = optional_value<std::string>(data, "flair");
  // Used only transiently to build the month selector on first sync. It is not
  // persisted, cached in normalized profile JSON, or exposed to Flutter.
  if (const auto created = optional_value<std::int64_t>(data, "createdAt")) {
    profile.joined = *created / 1000;
  }
  profile.disabled = optional_value<bool>(data, "disabled").value_or(false);
  profile.tos_violation = optional_value<bool>(data, "tosViolation").value_or(false);
  if (const auto count = data.find("count"); count != data.end() && count->is_object()) {
    profile.games = optional_value<int>(*count, "all");
    profile.wins = optional_value<int>(*count, "win");
    profile.losses = optional_value<int>(*count, "loss");
    profile.draws = optional_value<int>(*count, "draw");
  }
  if (const auto play_time = data.find("playTime");
      play_time != data.end() && play_time->is_object()) {
    profile.play_time_seconds = optional_value<std::int64_t>(*play_time, "total");
  }
  if (const auto perfs = data.find("perfs"); perfs != data.end() && perfs->is_object()) {
    for (const auto& [key, value] : perfs->items()) {
      if (!value.is_object() || key == "puzzle") continue;
      ProviderPerformance performance;
      performance.key = key;
      performance.current_rating = optional_value<int>(value, "rating");
      performance.games = optional_value<int>(value, "games");
      performance.rating_progress = optional_value<int>(value, "prog");
      profile.rating_summary.push_back(std::move(performance));
    }
  }
  result.value = std::move(profile);
  return result;
}

ProviderResult<ProviderStats> LichessProvider::fetch_stats(
    const std::string& username,
    const CacheValidators&,
    const std::shared_ptr<CancelToken>& cancel) {
  ProviderResult<ProviderStats> result;
  ProviderStats stats{.provider = type()};
  for (const auto perf : std::array<const char*, 5>{
           "bullet", "blitz", "rapid", "classical", "correspondence"}) {
    auto request = provider_request(
        "https://lichess.org/api/user/" + url_encode(username) + "/perf/" + perf,
        "application/json", {}, cancel);
    request.max_body_bytes = 4U * 1024U * 1024U;
    const auto response = scheduler_.get(type(), http_, request);
    if (response.error == HttpError::not_found) continue;
    if (!accept_provider_response(response)) continue;
    result.cache = response_cache_info(response);
    const auto data = parse_json(response.body, std::string("performance ") + perf);
    ProviderPerformance value;
    value.key = perf;
    if (const auto perf_json = data.find("perf");
        perf_json != data.end() && perf_json->is_object()) {
      value.games = optional_value<int>(*perf_json, "nb");
      value.rating_progress = optional_value<int>(*perf_json, "progress");
      if (const auto glicko = perf_json->find("glicko");
          glicko != perf_json->end() && glicko->is_object()) {
        if (const auto rating = optional_value<double>(*glicko, "rating"); rating) {
          value.current_rating = static_cast<int>(*rating + 0.5);
        }
      }
    }
    if (const auto stat = data.find("stat"); stat != data.end() && stat->is_object()) {
      if (const auto highest = stat->find("highest");
          highest != stat->end() && highest->is_object()) {
        value.best_rating = optional_value<int>(*highest, "int");
      }
      if (const auto lowest = stat->find("lowest");
          lowest != stat->end() && lowest->is_object()) {
        value.lowest_rating = optional_value<int>(*lowest, "int");
      }
      if (const auto count = stat->find("count");
          count != stat->end() && count->is_object()) {
        value.games = optional_value<int>(*count, "all");
        value.wins = optional_value<int>(*count, "win");
        value.losses = optional_value<int>(*count, "loss");
        value.draws = optional_value<int>(*count, "draw");
      }
    }
    stats.performances.push_back(std::move(value));
  }
  if (result.cache.fetched_at == 0) {
    result.cache.fetched_at = unix_time_seconds_provider();
    result.cache.expires_at = result.cache.fetched_at + 300;
  }
  result.value = std::move(stats);
  return result;
}

ProviderResult<std::vector<ProviderGame>> LichessProvider::fetch_games(
    const std::string& username,
    const ProviderGameQuery& query,
    const CacheValidators& validators,
    const std::shared_ptr<CancelToken>& cancel) {
  std::ostringstream url;
  url << "https://lichess.org/api/games/user/" << url_encode(username)
      << "?since=" << query.since_ms << "&until=" << query.until_ms
      << "&pgnInJson=true&clocks=false&evals=false&opening=true&accuracy=true"
         "&literate=false&ongoing=false&finished=true&sort=dateDesc";
  auto request = provider_request(url.str(), "application/x-ndjson", validators, cancel);
  request.max_body_bytes = 512U * 1024U * 1024U;
  std::vector<ProviderGame> games;
  std::string pending;
  std::string parse_error;
  request.on_chunk = [&](const std::string_view chunk) {
    if (cancel && cancel->is_cancelled()) return false;
    pending.append(chunk);
    if (pending.size() > 8U * 1024U * 1024U) {
      parse_error = "Lichess NDJSON line exceeded size limit";
      return false;
    }
    std::size_t line_end = 0;
    while ((line_end = pending.find('\n')) != std::string::npos) {
      auto line = pending.substr(0, line_end);
      pending.erase(0, line_end + 1);
      if (!line.empty() && line.back() == '\r') line.pop_back();
      if (line.empty()) continue;
      try {
        games.push_back(parse_game(json::parse(line), username));
      } catch (const std::exception& error) {
        parse_error = error.what();
        return false;
      }
    }
    return true;
  };
  const auto response = scheduler_.get(type(), http_, request);
  ProviderResult<std::vector<ProviderGame>> result{.cache = response_cache_info(response)};
  if (!parse_error.empty()) {
    throw ProviderException(HttpError::invalid_response, parse_error);
  }
  if (!accept_provider_response(response)) {
    result.not_modified = true;
    return result;
  }
  if (!pending.empty()) {
    if (pending.back() == '\r') pending.pop_back();
    if (!pending.empty()) {
      try {
        games.push_back(parse_game(json::parse(pending), username));
      } catch (const std::exception& error) {
        throw ProviderException(
            HttpError::invalid_response,
            std::string("invalid Lichess NDJSON tail: ") + error.what());
      }
    }
  }
  result.value = std::move(games);
  return result;
}

ProviderResult<std::vector<std::string>> LichessProvider::fetch_available_months(
    const std::string&, const CacheValidators&, const std::shared_ptr<CancelToken>&) {
  return {.value = std::vector<std::string>{}};
}

}  // namespace kchess
