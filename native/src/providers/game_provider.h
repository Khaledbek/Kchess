#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "http/http_client.h"
#include "providers/provider_models.h"
#include "providers/provider_request_scheduler.h"

namespace kchess {

struct ProviderGameQuery {
  int year{0};
  int month{0};
  std::int64_t since_ms{0};
  std::int64_t until_ms{0};
};

class GameProvider {
 public:
  virtual ~GameProvider() = default;
  virtual ProviderType type() const noexcept = 0;
  virtual ProviderResult<ProviderProfile> fetch_profile(
      const std::string& username,
      const CacheValidators& validators,
      const std::shared_ptr<CancelToken>& cancel) = 0;
  virtual ProviderResult<ProviderStats> fetch_stats(
      const std::string& username,
      const CacheValidators& validators,
      const std::shared_ptr<CancelToken>& cancel) = 0;
  virtual ProviderResult<std::vector<ProviderGame>> fetch_games(
      const std::string& username,
      const ProviderGameQuery& query,
      const CacheValidators& validators,
      const std::shared_ptr<CancelToken>& cancel) = 0;
  virtual ProviderResult<std::vector<std::string>> fetch_available_months(
      const std::string& username,
      const CacheValidators& validators,
      const std::shared_ptr<CancelToken>& cancel) = 0;
};

class ChessComProvider final : public GameProvider {
 public:
  ChessComProvider(HttpClient& http, ProviderRequestScheduler& scheduler)
      : http_(http), scheduler_(scheduler) {}
  ProviderType type() const noexcept override { return ProviderType::chess_com; }
  ProviderResult<ProviderProfile> fetch_profile(
      const std::string&, const CacheValidators&,
      const std::shared_ptr<CancelToken>&) override;
  ProviderResult<ProviderStats> fetch_stats(
      const std::string&, const CacheValidators&,
      const std::shared_ptr<CancelToken>&) override;
  ProviderResult<std::vector<ProviderGame>> fetch_games(
      const std::string&, const ProviderGameQuery&, const CacheValidators&,
      const std::shared_ptr<CancelToken>&) override;
  ProviderResult<std::vector<std::string>> fetch_available_months(
      const std::string&, const CacheValidators&,
      const std::shared_ptr<CancelToken>&) override;

 private:
  HttpClient& http_;
  ProviderRequestScheduler& scheduler_;
};

class LichessProvider final : public GameProvider {
 public:
  LichessProvider(HttpClient& http, ProviderRequestScheduler& scheduler)
      : http_(http), scheduler_(scheduler) {}
  ProviderType type() const noexcept override { return ProviderType::lichess; }
  ProviderResult<ProviderProfile> fetch_profile(
      const std::string&, const CacheValidators&,
      const std::shared_ptr<CancelToken>&) override;
  ProviderResult<ProviderStats> fetch_stats(
      const std::string&, const CacheValidators&,
      const std::shared_ptr<CancelToken>&) override;
  ProviderResult<std::vector<ProviderGame>> fetch_games(
      const std::string&, const ProviderGameQuery&, const CacheValidators&,
      const std::shared_ptr<CancelToken>&) override;
  ProviderResult<std::vector<std::string>> fetch_available_months(
      const std::string&, const CacheValidators&,
      const std::shared_ptr<CancelToken>&) override;

 private:
  HttpClient& http_;
  ProviderRequestScheduler& scheduler_;
};

class LocalPgnFenProvider final : public GameProvider {
 public:
  ProviderType type() const noexcept override { return ProviderType::local; }
  ProviderResult<ProviderProfile> fetch_profile(
      const std::string& name, const CacheValidators&,
      const std::shared_ptr<CancelToken>&) override;
  ProviderResult<ProviderStats> fetch_stats(
      const std::string&, const CacheValidators&,
      const std::shared_ptr<CancelToken>&) override;
  ProviderResult<std::vector<ProviderGame>> fetch_games(
      const std::string&, const ProviderGameQuery&, const CacheValidators&,
      const std::shared_ptr<CancelToken>&) override;
  ProviderResult<std::vector<std::string>> fetch_available_months(
      const std::string&, const CacheValidators&,
      const std::shared_ptr<CancelToken>&) override;
};

}  // namespace kchess
