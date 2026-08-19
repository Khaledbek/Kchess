#include "services/profile_service.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <iomanip>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <utility>

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

std::string profile_type_name(const ProfileType type) {
  switch (type) {
    case ProfileType::chess_com: return "chessCom";
    case ProfileType::lichess: return "lichess";
    case ProfileType::local_pgn_fen: return "localPgnFen";
  }
  return "localPgnFen";
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

void append_optional_int(std::ostringstream& json, const std::optional<int>& value) {
  if (value.has_value()) json << *value;
  else json << "null";
}

void append_optional_int64(
    std::ostringstream& json, const std::optional<std::int64_t>& value) {
  if (value.has_value()) json << *value;
  else json << "null";
}

void append_optional_string(
    std::ostringstream& json, const std::optional<std::string>& value) {
  if (value.has_value()) json << '"' << escape_json(*value) << '"';
  else json << "null";
}

}  // namespace

ProfileService::ProfileService(Database& database, std::filesystem::path data_directory)
    : database_(database), data_directory_(std::move(data_directory)) {}

std::string ProfileService::profile_json(const Profile& profile) const {
  std::ostringstream json;
  json << "{\"id\":\"" << escape_json(profile.id) << "\",\"type\":\""
       << profile_type_name(profile.type) << "\",\"displayName\":\""
       << escape_json(profile.display_name) << "\",\"providerUsername\":";
  if (profile.provider_username.has_value()) {
    json << "\"" << escape_json(*profile.provider_username) << "\"";
  } else {
    json << "null";
  }
  json << ",\"avatarAsset\":\"" << escape_json(profile.avatar_asset) << "\",\"title\":";
  append_optional_string(json, profile.title);
  json << ",\"avatarUrl\":";
  append_optional_string(json, profile.avatar_url);
  json << ",\"avatarFile\":";
  append_optional_string(json, profile.avatar_file);
  json << ",\"flair\":";
  append_optional_string(json, profile.flair);
  json << ",\"fide\":";
  append_optional_int(json, profile.fide);
  json << ",\"providerGames\":";
  append_optional_int(json, profile.provider_games);
  json << ",\"providerWins\":";
  append_optional_int(json, profile.provider_wins);
  json << ",\"providerLosses\":";
  append_optional_int(json, profile.provider_losses);
  json << ",\"providerDraws\":";
  append_optional_int(json, profile.provider_draws);
  json << ",\"playTimeSeconds\":";
  append_optional_int64(json, profile.play_time_seconds);
  json << ",\"providerDisabled\":" << (profile.provider_disabled ? "true" : "false")
       << ",\"providerTosViolation\":"
       << (profile.provider_tos_violation ? "true" : "false")
       << ",\"profileFetchedAt\":" << profile.profile_fetched_at
       << ",\"createdAt\":" << profile.created_at
       << ",\"lastOpenedAt\":" << profile.last_opened_at << '}';
  return json.str();
}

std::string ProfileService::profiles_json() const {
  const auto profiles = database_.profiles();
  std::ostringstream json;
  json << '[';
  for (std::size_t index = 0; index < profiles.size(); ++index) {
    if (index != 0) json << ',';
    json << profile_json(profiles[index]);
  }
  json << ']';
  return json.str();
}

std::string ProfileService::create_profile_json(
    const ProfileType type,
    const std::string& display_name,
    const std::string& provider_username) {
  validate_token(display_name, "display name");
  std::optional<std::string> username;
  std::string avatar;
  switch (type) {
    case ProfileType::chess_com:
      validate_token(provider_username, "provider username");
      username = provider_username;
      avatar = "provider_chesscom_fallback.png";
      break;
    case ProfileType::lichess:
      validate_token(provider_username, "provider username");
      username = provider_username;
      avatar = "provider_lichess_fallback.png";
      break;
    case ProfileType::local_pgn_fen:
      avatar = "profile_unknown.png";
      break;
    default:
      throw std::invalid_argument("unsupported profile type");
  }
  return profile_json(database_.create_profile(type, display_name, username, avatar));
}

void ProfileService::set_active_profile(const std::string& profile_id) {
  validate_token(profile_id, "profile id");
  database_.set_active_profile(profile_id);
}

std::string ProfileService::active_profile_json() const {
  const auto profile = database_.active_profile();
  return profile.has_value() ? profile_json(*profile) : "null";
}

Profile ProfileService::require_local_profile() const {
  const auto profile = database_.active_profile();
  if (!profile.has_value()) throw std::runtime_error("No active profile");
  if (profile->type != ProfileType::local_pgn_fen) {
    throw std::invalid_argument("PGN/FEN import requires a local profile");
  }
  return *profile;
}

void ProfileService::delete_profile_storage(const Profile& profile) {
  database_.delete_profile(profile.id);
  if (!profile.avatar_file.has_value()) return;

  std::error_code error;
  const auto avatar = std::filesystem::weakly_canonical(*profile.avatar_file, error);
  if (error) return;
  const auto avatar_directory =
      std::filesystem::weakly_canonical(data_directory_ / "avatars", error);
  if (!error && avatar.parent_path() == avatar_directory
      && avatar.filename() == profile.id + ".img") {
    std::filesystem::remove(avatar, error);
  }
}

}  // namespace kchess
