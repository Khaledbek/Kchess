#include <algorithm>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "providers/game_provider.h"

namespace {

using namespace kchess;

int assertions = 0;

void expect(const bool condition, const std::string& message) {
  ++assertions;
  if (!condition) throw std::runtime_error(message);
}

class FakeHttpClient final : public HttpClient {
 public:
  std::deque<HttpResponse> responses;
  std::vector<HttpRequest> requests;
  bool stream_in_small_chunks{false};
  bool cancel_after_first_chunk{false};

  HttpResponse get(const HttpRequest& request) override {
    requests.push_back(request);
    if (responses.empty()) {
      return {.error = HttpError::transport, .error_message = "missing fake response"};
    }
    auto response = std::move(responses.front());
    responses.pop_front();
    if (request.on_chunk && response.error == HttpError::none && response.status == 200) {
      const auto body = std::move(response.body);
      response.body.clear();
      const std::size_t step = stream_in_small_chunks ? 11U : body.size();
      int chunks = 0;
      for (std::size_t offset = 0; offset < body.size(); offset += step) {
        const auto count = std::min(step, body.size() - offset);
        if (!request.on_chunk(std::string_view(body).substr(offset, count))) {
          response.error = HttpError::cancelled;
          response.error_message = "fake stream cancelled";
          return response;
        }
        ++chunks;
        if (cancel_after_first_chunk && chunks == 1 && request.cancel_token) {
          request.cancel_token->cancel();
        }
      }
    }
    return response;
  }
};

HttpResponse json_response(std::string body, const int status = 200) {
  return {
      .status = status,
      .headers = {{"etag", "fixture-etag"}, {"cache-control", "max-age=300"}},
      .body = std::move(body),
  };
}

template <typename Function>
void expect_error(Function&& function, const HttpError expected, const std::string& name) {
  try {
    function();
    expect(false, name + " must fail");
  } catch (const ProviderException& error) {
    expect(error.kind() == expected, name + " returned wrong error class");
  }
}

void test_chess_com() {
  FakeHttpClient http;
  ProviderRequestScheduler scheduler;
  ChessComProvider provider(http, scheduler);
  auto cancel = std::make_shared<CancelToken>();
  http.responses.push_back(json_response(R"json({
    "username":"FixtureUser","player_id":42,"title":"GM","name":"Fixture Name",
    "avatar":"https://images.chesscomfiles.com/avatar.png","location":"Berlin",
    "country":"https://api.chess.com/pub/country/DE","joined":100,"last_online":200,
    "followers":12,"fide":2500,"status":"premium"
  })json"));
  const auto profile = provider.fetch_profile("fixture user", {"old", "date"}, cancel);
  expect(profile.value && profile.value->username == "FixtureUser", "Chess.com username");
  expect(profile.value->avatar_url.has_value(), "Chess.com optional avatar");
  expect(profile.value->country == "DE", "Chess.com country normalization");
  expect(http.requests.front().url.find("fixture%20user") != std::string::npos,
         "Chess.com username must be URL encoded");
  expect(http.requests.front().headers.at("If-None-Match") == "old",
         "Chess.com ETag validator");

  http.responses.push_back(json_response(R"json({"username":"NoAvatar"})json"));
  const auto no_avatar = provider.fetch_profile("NoAvatar", {}, cancel);
  expect(no_avatar.value && !no_avatar.value->avatar_url,
         "Missing Chess.com avatar stays absent");
  expect(no_avatar.value->fallback_asset == "provider_chesscom_fallback.png",
         "Chess.com fallback asset");

  http.responses.push_back(json_response(R"json({
    "chess_rapid":{"last":{"rating":1801},"best":{"rating":1900},
      "record":{"win":12,"loss":5,"draw":3}},
    "chess_blitz":{"last":{"rating":1700},"record":{"win":2,"loss":2,"draw":1}},
    "tactics":{"highest":{"rating":9999}}
  })json"));
  const auto stats = provider.fetch_stats("FixtureUser", {}, cancel);
  expect(stats.value && stats.value->performances.size() == 2,
         "Chess.com relevant stats only");
  const auto rapid = std::find_if(
      stats.value->performances.begin(), stats.value->performances.end(),
      [](const ProviderPerformance& value) { return value.key == "rapid"; });
  expect(rapid != stats.value->performances.end() && rapid->games == 20,
         "Chess.com game totals");

  const std::string pgn =
      "[Event \"Fixture\"]\n[White \"FixtureUser\"]\n[Black \"Other\"]\n"
      "[Result \"1-0\"]\n\n1. e4 e5 1-0";
  nlohmann::json game_fixture;
  game_fixture["games"] = nlohmann::json::array({
      {{"url", "https://www.chess.com/game/live/1"}, {"pgn", pgn},
       {"end_time", 1234}, {"time_control", "600+5"}, {"time_class", "rapid"},
       {"rules", "chess"},
       {"white", {{"username", "FixtureUser"}, {"rating", 1800}, {"result", "win"}}},
       {"black", {{"username", "Other"}, {"rating", 1790}, {"result", "checkmated"}}},
       {"accuracies", {{"white", 91.2}, {"black", 77.5}}}},
      {{"url", "https://www.chess.com/game/daily/2"}, {"pgn", pgn},
       {"time_class", "daily"},
       {"white", {{"username", "Other"}, {"result", "agreed"}}},
       {"black", {{"username", "FixtureUser"}, {"result", "agreed"}}}},
      {{"url", "https://www.chess.com/game/live/3"}, {"pgn", pgn},
       {"time_class", "blitz"},
       {"white", {{"username", "Other"}, {"result", "win"}}},
       {"black", {{"username", "FixtureUser"}, {"result", "resigned"}}}},
      {{"url", "https://www.chess.com/game/live/4"}, {"pgn", pgn},
       {"time_class", "bullet"},
       {"white", {{"username", "FixtureUser"}, {"result", "timeout"}}},
       {"black", {{"username", "Other"}, {"result", "win"}}}},
  });
  http.responses.push_back(json_response(game_fixture.dump()));
  const auto games = provider.fetch_games("FixtureUser", {.year = 2026, .month = 8}, {}, cancel);
  expect(games.value && games.value->size() == 4, "Chess.com games parsed");
  expect(games.value->at(0).profile_outcome == ProviderOutcome::win,
         "Chess.com win mapping");
  expect(games.value->at(1).profile_outcome == ProviderOutcome::draw,
         "Chess.com draw mapping");
  expect(games.value->at(2).profile_outcome == ProviderOutcome::loss,
         "Chess.com loss mapping");
  expect(games.value->at(0).provider_accuracy_white == 91.2,
         "Chess.com provider accuracy");
  expect(!games.value->at(1).provider_accuracy_white,
         "Chess.com absent accuracy stays absent");
  expect(games.value->at(0).time_control_type == TimeControlType::rapid
         && games.value->at(1).time_control_type == TimeControlType::daily
         && games.value->at(2).time_control_type == TimeControlType::blitz
         && games.value->at(3).time_control_type == TimeControlType::bullet,
         "Chess.com time class mapping");

  http.responses.push_back(json_response(R"json({"archives":[
    "https://api.chess.com/pub/player/FixtureUser/games/2026/07",
    "https://api.chess.com/pub/player/FixtureUser/games/2026/08"]})json"));
  const auto months = provider.fetch_available_months("FixtureUser", {}, cancel);
  expect(months.value && months.value->at(0) == "2026-07", "Chess.com archives");

  http.responses.push_back(json_response("", 304));
  const auto cached = provider.fetch_profile("FixtureUser", {}, cancel);
  expect(cached.not_modified && !cached.value, "Chess.com 304 uses cache");
  http.responses.push_back(json_response("{}", 404));
  const auto empty_month = provider.fetch_games(
      "FixtureUser", {.year = 2026, .month = 9}, {}, cancel);
  expect(empty_month.value && empty_month.value->empty(),
         "Chess.com missing month is an empty archive, not a missing user");
}

void test_lichess() {
  FakeHttpClient http;
  ProviderRequestScheduler scheduler;
  LichessProvider provider(http, scheduler);
  auto cancel = std::make_shared<CancelToken>();
  http.responses.push_back(json_response(R"json({
    "id":"fixtureid","username":"FixtureLichess","title":"IM","flair":"symbols.star",
    "createdAt":100000,"seenAt":200000,"disabled":false,"tosViolation":false,
    "count":{"all":20,"win":10,"loss":6,"draw":4},
    "perfs":{"rapid":{"rating":2100,"games":20,"prog":5}},
    "profile":{"firstName":"Ada","lastName":"Lovelace","flag":"GB","location":"London"}
  })json"));
  const auto profile = provider.fetch_profile("FixtureLichess", {}, cancel);
  expect(profile.value && profile.value->flair == "symbols.star", "Lichess flair");
  expect(profile.value && !profile.value->avatar_url, "Lichess has no invented avatar");
  expect(profile.value->joined == 100 && profile.value->last_online == 200,
         "Lichess timestamps normalized to seconds");

  const auto perf = R"json({"perf":{"glicko":{"rating":2111.4},"nb":22,"progress":4},
    "stat":{"highest":{"int":2200},"lowest":{"int":1800},
    "count":{"all":22,"win":12,"loss":7,"draw":3}}})json";
  for (int index = 0; index < 5; ++index) http.responses.push_back(json_response(perf));
  const auto stats = provider.fetch_stats("FixtureLichess", {}, cancel);
  expect(stats.value && stats.value->performances.size() == 5, "Lichess perf endpoints");
  expect(stats.value->performances.front().best_rating == 2200,
         "Lichess highest rating");

  const std::string ndjson =
      R"json({"id":"abc123","lastMoveAt":10000,"speed":"rapid","variant":"standard","winner":"white","status":"mate","pgn":"[Result \"1-0\"]\n\n1. e4 e5 1-0","clock":{"initial":600,"increment":5},"players":{"white":{"user":{"name":"FixtureLichess"},"rating":2100,"analysis":{"accuracy":93.5}},"black":{"user":{"name":"Other"},"rating":2090}},"opening":{"eco":"C20"}})json"
      "\n"
      R"json({"id":"def456","lastMoveAt":20000,"speed":"blitz","variant":"standard","status":"draw","pgn":"[Result \"1/2-1/2\"]\n\n1. d4 d5 1/2-1/2","players":{"white":{"user":{"name":"Other"}},"black":{"user":{"name":"FixtureLichess"}}}})json"
      "\n";
  http.stream_in_small_chunks = true;
  http.responses.push_back(json_response(ndjson));
  const auto games = provider.fetch_games(
      "FixtureLichess", {.since_ms = 1, .until_ms = 30'000}, {}, cancel);
  expect(games.value && games.value->size() == 2, "Lichess NDJSON streaming");
  expect(games.value->front().profile_outcome == ProviderOutcome::win,
         "Lichess win mapping");
  expect(games.value->back().profile_outcome == ProviderOutcome::draw,
         "Lichess draw mapping");
  expect(games.value->front().provider_accuracy_white == 93.5,
         "Lichess optional accuracy");
  expect(!games.value->back().provider_accuracy_white,
         "Lichess missing accuracy");

  auto stream_cancel = std::make_shared<CancelToken>();
  http.cancel_after_first_chunk = true;
  http.responses.push_back(json_response(ndjson));
  expect_error(
      [&] {
        (void)provider.fetch_games(
            "FixtureLichess", {.since_ms = 1, .until_ms = 30'000}, {}, stream_cancel);
      },
      HttpError::cancelled, "Lichess stream cancellation");
}

void test_errors_and_rate_limit() {
  for (const auto [status, expected] : std::vector<std::pair<int, HttpError>>{
           {404, HttpError::not_found}, {410, HttpError::gone}, {500, HttpError::server}}) {
    FakeHttpClient http;
    ProviderRequestScheduler scheduler;
    ChessComProvider provider(http, scheduler);
    http.responses.push_back(json_response("{}", status));
    expect_error(
        [&] { (void)provider.fetch_profile("missing", {}, std::make_shared<CancelToken>()); },
        expected, "HTTP status mapping");
  }
  {
    FakeHttpClient http;
    ProviderRequestScheduler scheduler;
    ChessComProvider provider(http, scheduler);
    http.responses.push_back(json_response("{invalid"));
    expect_error(
        [&] { (void)provider.fetch_profile("bad", {}, std::make_shared<CancelToken>()); },
        HttpError::invalid_response, "invalid JSON");
  }
  {
    FakeHttpClient http;
    ProviderRequestScheduler scheduler;
    ChessComProvider provider(http, scheduler);
    http.responses.push_back({.error = HttpError::timeout, .error_message = "fixture timeout"});
    expect_error(
        [&] { (void)provider.fetch_profile("slow", {}, std::make_shared<CancelToken>()); },
        HttpError::timeout, "timeout");
  }
  {
    FakeHttpClient http;
    ProviderRequestScheduler scheduler;
    LichessProvider provider(http, scheduler);
    http.responses.push_back({.status = 429, .headers = {{"retry-after", "2"}}});
    expect_error(
        [&] { (void)provider.fetch_profile("limited", {}, std::make_shared<CancelToken>()); },
        HttpError::rate_limited, "Lichess 429");
    expect(scheduler.retry_after_seconds(ProviderType::lichess) >= 60,
           "Lichess cooldown is at least one minute");
  }
}

}  // namespace

int main() {
  try {
    test_chess_com();
    test_lichess();
    test_errors_and_rate_limit();
    std::cout << "All " << assertions << " KChess phase-4 provider assertions passed.\n";
    return EXIT_SUCCESS;
  } catch (const std::exception& error) {
    std::cerr << "Provider test failed: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
