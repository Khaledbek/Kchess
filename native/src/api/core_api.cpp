#include "kchess/core_api.h"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <new>
#include <optional>
#include <string>

#include "core/core.h"

namespace {

kchess::Core* core_from(const kc_core_handle handle) {
  return static_cast<kchess::Core*>(handle);
}

char* copy_string(const std::string& value) {
  auto* result = static_cast<char*>(std::malloc(value.size() + 1));
  if (result == nullptr) {
    return nullptr;
  }
  std::memcpy(result, value.c_str(), value.size() + 1);
  return result;
}

kc_status set_error(
    kchess::Core* core, const kc_status status, const std::string& message) noexcept {
  if (core != nullptr) {
    core->set_last_error(static_cast<int32_t>(status), message);
  }
  return status;
}

char* invalid_string_argument(kchess::Core* core, const char* message) noexcept {
  set_error(core, KC_STATUS_INVALID_ARGUMENT, message);
  return nullptr;
}

template <typename Function>
kc_status status_call(kchess::Core* core, Function&& function) noexcept {
  if (core == nullptr) {
    return KC_STATUS_INVALID_ARGUMENT;
  }
  try {
    function();
    core->set_last_error(KC_STATUS_OK, {});
    return KC_STATUS_OK;
  } catch (const std::invalid_argument& error) {
    return set_error(core, KC_STATUS_INVALID_ARGUMENT, error.what());
  } catch (const std::exception& error) {
    return set_error(core, KC_STATUS_INTERNAL_ERROR, error.what());
  } catch (...) {
    return set_error(core, KC_STATUS_INTERNAL_ERROR, "Unknown native error");
  }
}

template <typename Function>
char* string_call(kchess::Core* core, Function&& function) noexcept {
  if (core == nullptr) {
    return nullptr;
  }
  try {
    auto result = function();
    auto* copied = copy_string(result);
    if (copied == nullptr) {
      set_error(core, KC_STATUS_INTERNAL_ERROR, "Could not allocate native result string");
      return nullptr;
    }
    core->set_last_error(KC_STATUS_OK, {});
    return copied;
  } catch (const std::invalid_argument& error) {
    set_error(core, KC_STATUS_INVALID_ARGUMENT, error.what());
    return nullptr;
  } catch (const std::exception& error) {
    set_error(core, KC_STATUS_INTERNAL_ERROR, error.what());
    return nullptr;
  } catch (...) {
    set_error(core, KC_STATUS_INTERNAL_ERROR, "Unknown native error");
    return nullptr;
  }
}

}  // namespace

extern "C" {

int32_t kc_abi_version(void) { return KCHESS_CORE_ABI_VERSION; }

const char* kc_core_version(void) { return "0.6.0-phase6-function-fixes"; }

int32_t kc_smoke_test(const int32_t value) { return value + 1; }

kc_core_handle kc_core_create(const char* data_directory_utf8) {
  try {
    if (data_directory_utf8 == nullptr || data_directory_utf8[0] == '\0') {
      return nullptr;
    }
    const auto* utf8_path = reinterpret_cast<const char8_t*>(data_directory_utf8);
    return new kchess::Core(std::filesystem::path(utf8_path));
  } catch (...) {
    return nullptr;
  }
}

void kc_core_destroy(const kc_core_handle handle) {
  try {
    delete core_from(handle);
  } catch (...) {
  }
}

kc_status kc_core_initialize(const kc_core_handle handle) {
  return status_call(core_from(handle), [handle] { core_from(handle)->initialize(); });
}

const char* kc_core_last_error(const kc_core_handle handle) {
  auto* core = core_from(handle);
  return core == nullptr ? "Invalid core handle" : core->last_error().c_str();
}

kc_status kc_core_last_status(const kc_core_handle handle) {
  auto* core = core_from(handle);
  if (core == nullptr) return KC_STATUS_INVALID_ARGUMENT;
  return static_cast<kc_status>(core->last_status());
}

char* kc_profiles_json(const kc_core_handle handle) {
  return string_call(core_from(handle), [handle] { return core_from(handle)->profiles_json(); });
}

char* kc_create_profile_json(
    const kc_core_handle handle,
    const int32_t profile_type,
    const char* display_name_utf8,
    const char* provider_username_utf8) {
  auto* core = core_from(handle);
  if (core == nullptr) return nullptr;
  if (display_name_utf8 == nullptr || provider_username_utf8 == nullptr) {
    return invalid_string_argument(core, "Profile name and provider username are required");
  }
  return string_call(core, [=] {
    return core->create_profile_json(
        static_cast<kchess::ProfileType>(profile_type),
        display_name_utf8,
        provider_username_utf8);
  });
}

kc_status kc_set_active_profile(
    const kc_core_handle handle, const char* profile_id_utf8) {
  auto* core = core_from(handle);
  if (profile_id_utf8 == nullptr) {
    return set_error(core, KC_STATUS_INVALID_ARGUMENT, "Profile id is required");
  }
  return status_call(core, [=] { core->set_active_profile(profile_id_utf8); });
}

kc_status kc_delete_profile(
    const kc_core_handle handle, const char* profile_id_utf8) {
  auto* core = core_from(handle);
  if (profile_id_utf8 == nullptr)
    return set_error(core, KC_STATUS_INVALID_ARGUMENT, "Profile id is required");
  return status_call(core, [=] { core->delete_profile(profile_id_utf8); });
}

kc_status kc_merge_local_profile(
    const kc_core_handle handle,
    const char* source_profile_id_utf8,
    const char* target_profile_id_utf8) {
  auto* core = core_from(handle);
  if (source_profile_id_utf8 == nullptr || target_profile_id_utf8 == nullptr) {
    return set_error(core, KC_STATUS_INVALID_ARGUMENT, "Source and target profile ids are required");
  }
  return status_call(core, [=] {
    core->merge_local_profile(source_profile_id_utf8, target_profile_id_utf8);
  });
}

char* kc_active_profile_json(const kc_core_handle handle) {
  return string_call(
      core_from(handle), [handle] { return core_from(handle)->active_profile_json(); });
}

char* kc_app_settings_json(const kc_core_handle handle) {
  return string_call(core_from(handle), [handle] { return core_from(handle)->settings_json(); });
}

kc_status kc_set_engine_settings(
    const kc_core_handle handle,
    const int32_t depth,
    const int32_t multi_pv,
    const int32_t time_limit_seconds) {
  return status_call(core_from(handle), [=] {
    core_from(handle)->set_engine_settings(depth, multi_pv, time_limit_seconds);
  });
}

kc_status kc_set_analysis_depth_range(
    const kc_core_handle handle,
    const int32_t minimum_depth,
    const int32_t maximum_depth) {
  return status_call(core_from(handle), [=] {
    core_from(handle)->set_analysis_depth_range(minimum_depth, maximum_depth);
  });
}

kc_status kc_set_engine_resources(const kc_core_handle handle, const int32_t threads, const int32_t hash_mb) {
  return status_call(core_from(handle), [=] { core_from(handle)->set_engine_resources(threads, hash_mb); });
}

kc_status kc_set_show_board_arrows(const kc_core_handle handle, const int32_t enabled) {
  return status_call(
      core_from(handle), [=] { core_from(handle)->set_show_board_arrows(enabled != 0); });
}

kc_status kc_set_boolean_setting(
    const kc_core_handle handle, const char* key_utf8, const int32_t enabled) {
  if (key_utf8 == nullptr || key_utf8[0] == '\0') {
    return set_error(core_from(handle), KC_STATUS_INVALID_ARGUMENT, "Setting key is required");
  }
  return status_call(core_from(handle), [=] {
    core_from(handle)->set_boolean_setting(key_utf8, enabled != 0);
  });
}

kc_status kc_set_theme_mode(const kc_core_handle handle, const char* theme_mode_utf8) {
  if (theme_mode_utf8 == nullptr) {
    return set_error(core_from(handle), KC_STATUS_INVALID_ARGUMENT, "Theme mode is required");
  }
  return status_call(
      core_from(handle), [=] { core_from(handle)->set_theme_mode(theme_mode_utf8); });
}

kc_status kc_set_locale(const kc_core_handle handle, const char* locale_utf8) {
  if (locale_utf8 == nullptr) {
    return set_error(core_from(handle), KC_STATUS_INVALID_ARGUMENT, "Locale is required");
  }
  return status_call(core_from(handle), [=] { core_from(handle)->set_locale(locale_utf8); });
}

char* kc_games_json(const kc_core_handle handle) {
  return string_call(core_from(handle), [handle] { return core_from(handle)->games_json(); });
}

char* kc_favorite_games_json(const kc_core_handle handle) {
  return string_call(core_from(handle), [handle] {
    return core_from(handle)->favorite_games_json();
  });
}

char* kc_game_json(const kc_core_handle handle, const char* game_id_utf8) {
  if (game_id_utf8 == nullptr)
    return invalid_string_argument(core_from(handle), "Game id is required");
  return string_call(
      core_from(handle), [=] { return core_from(handle)->game_json(game_id_utf8); });
}

char* kc_import_pgn_json(const kc_core_handle handle, const char* pgn_utf8) {
  if (pgn_utf8 == nullptr)
    return invalid_string_argument(core_from(handle), "PGN is required");
  return string_call(
      core_from(handle), [=] { return core_from(handle)->import_pgn_json(pgn_utf8); });
}

char* kc_import_fen_json(
    const kc_core_handle handle,
    const char* fen_utf8,
    const char* display_name_utf8) {
  if (fen_utf8 == nullptr || display_name_utf8 == nullptr)
    return invalid_string_argument(core_from(handle), "FEN and display name are required");
  return string_call(core_from(handle), [=] {
    return core_from(handle)->import_fen_json(fen_utf8, display_name_utf8);
  });
}

char* kc_start_provider_profile_json(
    const kc_core_handle handle,
    const int32_t profile_type,
    const char* username_utf8) {
  if (username_utf8 == nullptr)
    return invalid_string_argument(core_from(handle), "Provider username is required");
  return string_call(core_from(handle), [=] {
    return core_from(handle)->start_provider_profile_json(
        static_cast<kchess::ProfileType>(profile_type), username_utf8);
  });
}

char* kc_start_provider_sync_json(
    const kc_core_handle handle,
    const char* profile_id_utf8,
    const int32_t year,
    const int32_t month) {
  if (profile_id_utf8 == nullptr)
    return invalid_string_argument(core_from(handle), "Profile id is required");
  return string_call(core_from(handle), [=] {
    return core_from(handle)->start_provider_sync_json(profile_id_utf8, year, month);
  });
}

char* kc_provider_job_status_json(
    const kc_core_handle handle, const char* job_id_utf8) {
  if (job_id_utf8 == nullptr)
    return invalid_string_argument(core_from(handle), "Job id is required");
  return string_call(core_from(handle), [=] {
    return core_from(handle)->provider_job_status_json(job_id_utf8);
  });
}

kc_status kc_cancel_provider_job(
    const kc_core_handle handle, const char* job_id_utf8) {
  if (job_id_utf8 == nullptr)
    return set_error(core_from(handle), KC_STATUS_INVALID_ARGUMENT, "Job id is required");
  return status_call(core_from(handle), [=] {
    core_from(handle)->cancel_provider_job(job_id_utf8);
  });
}

char* kc_provider_overview_json(
    const kc_core_handle handle, const char* profile_id_utf8) {
  if (profile_id_utf8 == nullptr)
    return invalid_string_argument(core_from(handle), "Profile id is required");
  return string_call(core_from(handle), [=] {
    return core_from(handle)->provider_overview_json(profile_id_utf8);
  });
}

kc_status kc_set_game_favorite(
    const kc_core_handle handle, const char* game_id_utf8, const int32_t enabled) {
  if (game_id_utf8 == nullptr)
    return set_error(core_from(handle), KC_STATUS_INVALID_ARGUMENT, "Game id is required");
  return status_call(core_from(handle), [=] {
    core_from(handle)->set_favorite(game_id_utf8, enabled != 0);
  });
}

char* kc_favorite_collections_json(const kc_core_handle handle) {
  return string_call(core_from(handle), [=] {
    return core_from(handle)->favorite_collections_json();
  });
}

char* kc_create_favorite_collection_json(
    const kc_core_handle handle, const char* name_utf8) {
  if (name_utf8 == nullptr)
    return invalid_string_argument(core_from(handle), "Collection name is required");
  return string_call(core_from(handle), [=] {
    return core_from(handle)->create_favorite_collection_json(name_utf8);
  });
}

kc_status kc_rename_favorite_collection(
    const kc_core_handle handle,
    const char* collection_id_utf8,
    const char* name_utf8) {
  if (collection_id_utf8 == nullptr || name_utf8 == nullptr) {
    return set_error(
        core_from(handle), KC_STATUS_INVALID_ARGUMENT,
        "Collection id and name are required");
  }
  return status_call(core_from(handle), [=] {
    core_from(handle)->rename_favorite_collection(collection_id_utf8, name_utf8);
  });
}

kc_status kc_delete_favorite_collection(
    const kc_core_handle handle, const char* collection_id_utf8) {
  if (collection_id_utf8 == nullptr)
    return set_error(
        core_from(handle), KC_STATUS_INVALID_ARGUMENT, "Collection id is required");
  return status_call(core_from(handle), [=] {
    core_from(handle)->delete_favorite_collection(collection_id_utf8);
  });
}

kc_status kc_set_game_favorite_collection(
    const kc_core_handle handle,
    const char* game_id_utf8,
    const char* collection_id_utf8) {
  if (game_id_utf8 == nullptr)
    return set_error(core_from(handle), KC_STATUS_INVALID_ARGUMENT, "Game id is required");
  return status_call(core_from(handle), [=] {
    const std::optional<std::string> collection =
        collection_id_utf8 == nullptr || collection_id_utf8[0] == '\0'
        ? std::nullopt
        : std::optional<std::string>(collection_id_utf8);
    core_from(handle)->set_favorite_collection(game_id_utf8, collection);
  });
}

kc_status kc_set_game_downloaded(
    const kc_core_handle handle, const char* game_id_utf8, const int32_t enabled) {
  if (game_id_utf8 == nullptr)
    return set_error(core_from(handle), KC_STATUS_INVALID_ARGUMENT, "Game id is required");
  return status_call(core_from(handle), [=] {
    core_from(handle)->set_downloaded(game_id_utf8, enabled != 0);
  });
}

kc_status kc_delete_local_game(
    const kc_core_handle handle, const char* game_id_utf8) {
  if (game_id_utf8 == nullptr)
    return set_error(core_from(handle), KC_STATUS_INVALID_ARGUMENT, "Game id is required");
  return status_call(core_from(handle), [=] {
    core_from(handle)->delete_local_game(game_id_utf8);
  });
}

kc_status kc_clear_cached_month(
    const kc_core_handle handle,
    const char* profile_id_utf8,
    const char* month_utf8) {
  if (profile_id_utf8 == nullptr || month_utf8 == nullptr) {
    return set_error(
        core_from(handle), KC_STATUS_INVALID_ARGUMENT, "Profile id and month are required");
  }
  return status_call(core_from(handle), [=] {
    core_from(handle)->clear_cached_month(profile_id_utf8, month_utf8);
  });
}

char* kc_start_analysis_json(const kc_core_handle handle, const char* game_id_utf8) {
  if (game_id_utf8 == nullptr) {
    return invalid_string_argument(core_from(handle), "Game id is required");
  }
  return string_call(
      core_from(handle), [=] { return core_from(handle)->start_analysis_json(game_id_utf8); });
}

char* kc_analysis_status_json(const kc_core_handle handle, const char* game_id_utf8) {
  if (game_id_utf8 == nullptr) {
    return invalid_string_argument(core_from(handle), "Game id is required");
  }
  return string_call(
      core_from(handle), [=] { return core_from(handle)->analysis_status_json(game_id_utf8); });
}

char* kc_start_move_refinement_json(
    const kc_core_handle handle, const char* game_id_utf8, const int32_t ply) {
  if (game_id_utf8 == nullptr)
    return invalid_string_argument(core_from(handle), "Game id is required");
  return string_call(core_from(handle), [=] {
    return core_from(handle)->start_move_refinement_json(game_id_utf8, ply);
  });
}

char* kc_move_analysis_status_json(
    const kc_core_handle handle, const char* game_id_utf8, const int32_t ply) {
  if (game_id_utf8 == nullptr)
    return invalid_string_argument(core_from(handle), "Game id is required");
  return string_call(core_from(handle), [=] {
    return core_from(handle)->move_analysis_status_json(game_id_utf8, ply);
  });
}

kc_status kc_cancel_analysis(
    const kc_core_handle handle, const char* game_id_utf8) {
  if (game_id_utf8 == nullptr)
    return set_error(core_from(handle), KC_STATUS_INVALID_ARGUMENT, "Game id is required");
  return status_call(
      core_from(handle), [=] { core_from(handle)->cancel_analysis(game_id_utf8); });
}

kc_status kc_clear_engine_cache(const kc_core_handle handle) {
  return status_call(core_from(handle), [=] { core_from(handle)->clear_engine_cache(); });
}

char* kc_start_variation_analysis_json(
    const kc_core_handle handle, const char* fen_utf8, const char* uci_utf8) {
  if (fen_utf8 == nullptr || uci_utf8 == nullptr)
    return invalid_string_argument(core_from(handle), "FEN and UCI move are required");
  return string_call(core_from(handle), [=] {
    return core_from(handle)->start_variation_analysis_json(fen_utf8, uci_utf8);
  });
}

char* kc_start_variation_analysis_with_settings_json(
    const kc_core_handle handle,
    const char* fen_utf8,
    const char* uci_utf8,
    const int32_t depth,
    const int32_t multi_pv,
    const int32_t threads,
    const int32_t hash_mb) {
  if (fen_utf8 == nullptr || uci_utf8 == nullptr)
    return invalid_string_argument(core_from(handle), "FEN and UCI move are required");
  return string_call(core_from(handle), [=] {
    return core_from(handle)->start_variation_analysis_with_settings_json(
        fen_utf8, uci_utf8, depth, multi_pv, threads, hash_mb);
  });
}

char* kc_variation_analysis_status_json(
    const kc_core_handle handle, const char* job_id_utf8) {
  if (job_id_utf8 == nullptr)
    return invalid_string_argument(core_from(handle), "Job id is required");
  return string_call(core_from(handle), [=] {
    return core_from(handle)->variation_analysis_status_json(job_id_utf8);
  });
}

kc_status kc_cancel_variation_analysis(
    const kc_core_handle handle, const char* job_id_utf8) {
  if (job_id_utf8 == nullptr)
    return set_error(core_from(handle), KC_STATUS_INVALID_ARGUMENT, "Job id is required");
  return status_call(core_from(handle), [=] {
    core_from(handle)->cancel_variation_analysis(job_id_utf8);
  });
}

void kc_string_free(char* value) { std::free(value); }

}  // extern "C"
