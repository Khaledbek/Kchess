#ifndef KCHESS_CORE_API_H
#define KCHESS_CORE_API_H

#include <stdint.h>

#if defined(_WIN32)
#  if defined(KCHESS_CORE_BUILD)
#    define KCHESS_API __declspec(dllexport)
#  else
#    define KCHESS_API __declspec(dllimport)
#  endif
#else
#  define KCHESS_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef void* kc_core_handle;

// Increment only when the C ABI changes incompatibly. Additive exports may
// keep the same ABI version. Flutter validates this before creating Core.
#define KCHESS_CORE_ABI_VERSION 3

typedef enum kc_status {
  KC_STATUS_OK = 0,
  KC_STATUS_INVALID_ARGUMENT = 1,
  KC_STATUS_NOT_FOUND = 2,
  KC_STATUS_DATABASE_ERROR = 3,
  KC_STATUS_ENGINE_UNAVAILABLE = 4,
  KC_STATUS_INTERNAL_ERROR = 5
} kc_status;

KCHESS_API int32_t kc_abi_version(void);
KCHESS_API const char* kc_core_version(void);
KCHESS_API int32_t kc_smoke_test(int32_t value);

KCHESS_API kc_core_handle kc_core_create(const char* data_directory_utf8);
KCHESS_API void kc_core_destroy(kc_core_handle handle);
KCHESS_API kc_status kc_core_initialize(kc_core_handle handle);
KCHESS_API const char* kc_core_last_error(kc_core_handle handle);
KCHESS_API kc_status kc_core_last_status(kc_core_handle handle);

KCHESS_API char* kc_profiles_json(kc_core_handle handle);
KCHESS_API char* kc_create_profile_json(
    kc_core_handle handle,
    int32_t profile_type,
    const char* display_name_utf8,
    const char* provider_username_utf8);
KCHESS_API kc_status kc_set_active_profile(
    kc_core_handle handle,
    const char* profile_id_utf8);
KCHESS_API kc_status kc_delete_profile(
    kc_core_handle handle,
    const char* profile_id_utf8);
KCHESS_API kc_status kc_merge_local_profile(
    kc_core_handle handle,
    const char* source_profile_id_utf8,
    const char* target_profile_id_utf8);
KCHESS_API char* kc_active_profile_json(kc_core_handle handle);

KCHESS_API char* kc_app_settings_json(kc_core_handle handle);
KCHESS_API kc_status kc_set_engine_settings(
    kc_core_handle handle,
    int32_t depth,
    int32_t multi_pv,
    int32_t time_limit_seconds);
KCHESS_API kc_status kc_set_analysis_depth_range(
    kc_core_handle handle, int32_t minimum_depth, int32_t maximum_depth);
KCHESS_API kc_status kc_set_engine_resources(
    kc_core_handle handle, int32_t threads, int32_t hash_mb);
KCHESS_API kc_status kc_set_show_board_arrows(
    kc_core_handle handle,
    int32_t enabled);
KCHESS_API kc_status kc_set_boolean_setting(
    kc_core_handle handle,
    const char* key_utf8,
    int32_t enabled);
KCHESS_API kc_status kc_set_theme_mode(
    kc_core_handle handle,
    const char* theme_mode_utf8);
KCHESS_API kc_status kc_set_locale(
    kc_core_handle handle,
    const char* locale_utf8);

KCHESS_API char* kc_games_json(kc_core_handle handle);
KCHESS_API char* kc_games_query_json(
    kc_core_handle handle,
    const char* query_json_utf8);
KCHESS_API char* kc_favorite_games_json(kc_core_handle handle);
KCHESS_API char* kc_game_json(
    kc_core_handle handle,
    const char* game_id_utf8);
KCHESS_API char* kc_resolve_board_move_json(
    kc_core_handle handle,
    const char* game_id_utf8,
    const char* fen_utf8,
    const char* source_utf8,
    const char* target_utf8,
    int32_t first_candidate_ply);
KCHESS_API char* kc_import_pgn_json(
    kc_core_handle handle,
    const char* pgn_utf8);
KCHESS_API char* kc_import_fen_json(
    kc_core_handle handle,
    const char* fen_utf8,
    const char* display_name_utf8);
KCHESS_API char* kc_start_provider_profile_json(
    kc_core_handle handle,
    int32_t profile_type,
    const char* username_utf8);
KCHESS_API char* kc_start_provider_sync_json(
    kc_core_handle handle,
    const char* profile_id_utf8,
    int32_t year,
    int32_t month);
KCHESS_API char* kc_provider_job_status_json(
    kc_core_handle handle,
    const char* job_id_utf8);
KCHESS_API kc_status kc_cancel_provider_job(
    kc_core_handle handle,
    const char* job_id_utf8);
KCHESS_API char* kc_provider_overview_json(
    kc_core_handle handle,
    const char* profile_id_utf8);
KCHESS_API char* kc_statistics_overview_json(kc_core_handle handle);
KCHESS_API char* kc_statistics_openings_json(kc_core_handle handle);
KCHESS_API char* kc_statistics_terminations_json(kc_core_handle handle);
KCHESS_API char* kc_statistics_phases_json(kc_core_handle handle);
KCHESS_API kc_status kc_set_game_favorite(
    kc_core_handle handle,
    const char* game_id_utf8,
    int32_t enabled);
KCHESS_API char* kc_favorite_collections_json(kc_core_handle handle);
KCHESS_API char* kc_create_favorite_collection_json(
    kc_core_handle handle,
    const char* name_utf8);
KCHESS_API kc_status kc_rename_favorite_collection(
    kc_core_handle handle,
    const char* collection_id_utf8,
    const char* name_utf8);
KCHESS_API kc_status kc_delete_favorite_collection(
    kc_core_handle handle,
    const char* collection_id_utf8);
KCHESS_API kc_status kc_set_game_favorite_collection(
    kc_core_handle handle,
    const char* game_id_utf8,
    const char* collection_id_utf8);
KCHESS_API kc_status kc_set_game_downloaded(
    kc_core_handle handle,
    const char* game_id_utf8,
    int32_t enabled);
KCHESS_API kc_status kc_delete_local_game(
    kc_core_handle handle,
    const char* game_id_utf8);
KCHESS_API kc_status kc_clear_cached_month(
    kc_core_handle handle,
    const char* profile_id_utf8,
    const char* month_utf8);
KCHESS_API char* kc_start_analysis_json(
    kc_core_handle handle,
    const char* game_id_utf8);
KCHESS_API char* kc_analysis_status_json(
    kc_core_handle handle,
    const char* game_id_utf8);
KCHESS_API char* kc_start_move_refinement_json(
    kc_core_handle handle, const char* game_id_utf8, int32_t ply);
KCHESS_API char* kc_move_analysis_status_json(
    kc_core_handle handle,
    const char* game_id_utf8,
    int32_t ply);
KCHESS_API kc_status kc_cancel_analysis(
    kc_core_handle handle,
    const char* game_id_utf8);
KCHESS_API kc_status kc_delete_analysis(
    kc_core_handle handle,
    const char* game_id_utf8);
KCHESS_API kc_status kc_clear_engine_cache(kc_core_handle handle);
KCHESS_API char* kc_start_variation_analysis_json(
    kc_core_handle handle,
    const char* fen_utf8,
    const char* uci_utf8);
KCHESS_API char* kc_start_variation_analysis_with_settings_json(
    kc_core_handle handle,
    const char* fen_utf8,
    const char* uci_utf8,
    int32_t depth,
    int32_t multi_pv,
    int32_t threads,
    int32_t hash_mb);
KCHESS_API char* kc_variation_analysis_status_json(
    kc_core_handle handle,
    const char* job_id_utf8);
KCHESS_API kc_status kc_cancel_variation_analysis(
    kc_core_handle handle,
    const char* job_id_utf8);

KCHESS_API void kc_string_free(char* value);

#ifdef __cplusplus
}
#endif

#endif
