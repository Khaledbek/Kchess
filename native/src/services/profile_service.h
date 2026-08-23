#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "core/models.h"
#include "persistence/database.h"

namespace kchess {

class ProfileService {
 public:
  ProfileService(Database& database, std::filesystem::path data_directory);

  std::string profiles_json() const;
  std::string create_profile_json(
      ProfileType type,
      const std::string& display_name,
      const std::string& provider_username);
  void set_active_profile(const std::string& profile_id);
  std::string active_profile_json() const;
  Profile require_local_profile() const;
  Profile ensure_local_profile();
  Profile require_active_profile() const;

  std::string profile_json(const Profile& profile) const;
  void delete_profile_storage(const Profile& profile);
  void merge_local_profile(const std::string& source_profile_id, const std::string& target_profile_id);

 private:
  Database& database_;
  std::filesystem::path data_directory_;
};

}  // namespace kchess
