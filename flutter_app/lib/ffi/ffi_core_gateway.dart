import 'dart:convert';
import 'dart:ffi';
import 'dart:io';

import 'package:ffi/ffi.dart';
import 'package:flutter/services.dart';
import 'package:path_provider/path_provider.dart';

import '../models/models.dart';
import 'core_gateway.dart';

typedef _AbiVersionNative = Int32 Function();
typedef _AbiVersionDart = int Function();
typedef _CoreCreateNative = Pointer<Void> Function(Pointer<Utf8>);
typedef _CoreCreateDart = Pointer<Void> Function(Pointer<Utf8>);
typedef _CoreDestroyNative = Void Function(Pointer<Void>);
typedef _CoreDestroyDart = void Function(Pointer<Void>);
typedef _StatusNoArgsNative = Int32 Function(Pointer<Void>);
typedef _StatusNoArgsDart = int Function(Pointer<Void>);
typedef _StringNoArgsNative = Pointer<Utf8> Function(Pointer<Void>);
typedef _StringNoArgsDart = Pointer<Utf8> Function(Pointer<Void>);
typedef _StringArgNative = Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>);
typedef _StringArgDart = Pointer<Utf8> Function(Pointer<Void>, Pointer<Utf8>);
typedef _StringTwoArgsNative = Pointer<Utf8> Function(
  Pointer<Void>,
  Pointer<Utf8>,
  Pointer<Utf8>,
);
typedef _StringTwoArgsDart = Pointer<Utf8> Function(
  Pointer<Void>,
  Pointer<Utf8>,
  Pointer<Utf8>,
);
typedef _StartVariationWithSettingsNative = Pointer<Utf8> Function(
  Pointer<Void>,
  Pointer<Utf8>,
  Pointer<Utf8>,
  Int32,
  Int32,
  Int32,
  Int32,
);
typedef _StartVariationWithSettingsDart = Pointer<Utf8> Function(
  Pointer<Void>,
  Pointer<Utf8>,
  Pointer<Utf8>,
  int,
  int,
  int,
  int,
);
typedef _StringIntArgNative = Pointer<Utf8> Function(
  Pointer<Void>,
  Pointer<Utf8>,
  Int32,
);
typedef _StringIntArgDart = Pointer<Utf8> Function(
  Pointer<Void>,
  Pointer<Utf8>,
  int,
);
typedef _ResolveBoardMoveNative = Pointer<Utf8> Function(
  Pointer<Void>,
  Pointer<Utf8>,
  Pointer<Utf8>,
  Pointer<Utf8>,
  Pointer<Utf8>,
  Int32,
);
typedef _ResolveBoardMoveDart = Pointer<Utf8> Function(
  Pointer<Void>,
  Pointer<Utf8>,
  Pointer<Utf8>,
  Pointer<Utf8>,
  Pointer<Utf8>,
  int,
);
typedef _StatusStringNative = Int32 Function(Pointer<Void>, Pointer<Utf8>);
typedef _StatusStringDart = int Function(Pointer<Void>, Pointer<Utf8>);
typedef _StatusTwoStringsNative = Int32 Function(
  Pointer<Void>,
  Pointer<Utf8>,
  Pointer<Utf8>,
);
typedef _StatusTwoStringsDart = int Function(
  Pointer<Void>,
  Pointer<Utf8>,
  Pointer<Utf8>,
);
typedef _StatusIntNative = Int32 Function(Pointer<Void>, Int32);
typedef _StatusIntDart = int Function(Pointer<Void>, int);
typedef _StatusTwoIntsNative = Int32 Function(Pointer<Void>, Int32, Int32);
typedef _StatusTwoIntsDart = int Function(Pointer<Void>, int, int);
typedef _StatusThreeIntsNative = Int32 Function(
  Pointer<Void>,
  Int32,
  Int32,
  Int32,
);
typedef _StatusThreeIntsDart = int Function(Pointer<Void>, int, int, int);
typedef _StartProviderProfileNative = Pointer<Utf8> Function(
  Pointer<Void>,
  Int32,
  Pointer<Utf8>,
);
typedef _StartProviderProfileDart = Pointer<Utf8> Function(
  Pointer<Void>,
  int,
  Pointer<Utf8>,
);
typedef _StartProviderSyncNative = Pointer<Utf8> Function(
  Pointer<Void>,
  Pointer<Utf8>,
  Int32,
  Int32,
);
typedef _StartProviderSyncDart = Pointer<Utf8> Function(
  Pointer<Void>,
  Pointer<Utf8>,
  int,
  int,
);
typedef _StatusStringIntNative = Int32 Function(
  Pointer<Void>,
  Pointer<Utf8>,
  Int32,
);
typedef _StatusStringIntDart = int Function(Pointer<Void>, Pointer<Utf8>, int);
typedef _CreateProfileNative = Pointer<Utf8> Function(
  Pointer<Void>,
  Int32,
  Pointer<Utf8>,
  Pointer<Utf8>,
);
typedef _CreateProfileDart = Pointer<Utf8> Function(
  Pointer<Void>,
  int,
  Pointer<Utf8>,
  Pointer<Utf8>,
);
typedef _FreeStringNative = Void Function(Pointer<Utf8>);
typedef _FreeStringDart = void Function(Pointer<Utf8>);

class FfiCoreGateway implements CoreGateway {
  static const int _supportedAbiVersion = 3;

  FfiCoreGateway._(this._library, this._dataDirectory) {
    try {
      _abiVersion = _library.lookupFunction<_AbiVersionNative, _AbiVersionDart>(
        'kc_abi_version',
      );
    } on ArgumentError {
      throw const CoreGatewayException(
        'Native Kchess core is too old: kc_abi_version is missing. '
        'Rebuild or replace the native core.',
      );
    }
    _create = _library.lookupFunction<_CoreCreateNative, _CoreCreateDart>(
      'kc_core_create',
    );
    _destroy = _library.lookupFunction<_CoreDestroyNative, _CoreDestroyDart>(
      'kc_core_destroy',
    );
    _initialize = _library
        .lookupFunction<_StatusNoArgsNative, _StatusNoArgsDart>(
          'kc_core_initialize',
        );
    _lastError = _library
        .lookupFunction<_StringNoArgsNative, _StringNoArgsDart>(
          'kc_core_last_error',
        );
    _lastStatus = _library
        .lookupFunction<_StatusNoArgsNative, _StatusNoArgsDart>(
          'kc_core_last_status',
        );
    _profiles = _library.lookupFunction<_StringNoArgsNative, _StringNoArgsDart>(
      'kc_profiles_json',
    );
    _activeProfile = _library
        .lookupFunction<_StringNoArgsNative, _StringNoArgsDart>(
          'kc_active_profile_json',
        );
    _createProfile = _library
        .lookupFunction<_CreateProfileNative, _CreateProfileDart>(
          'kc_create_profile_json',
        );
    _setActiveProfile = _library
        .lookupFunction<_StatusStringNative, _StatusStringDart>(
          'kc_set_active_profile',
        );
    _deleteProfile = _library
        .lookupFunction<_StatusStringNative, _StatusStringDart>(
          'kc_delete_profile',
        );
    _mergeLocalProfile = _library
        .lookupFunction<_StatusTwoStringsNative, _StatusTwoStringsDart>(
          'kc_merge_local_profile',
        );
    _settings = _library.lookupFunction<_StringNoArgsNative, _StringNoArgsDart>(
      'kc_app_settings_json',
    );
    _setArrows = _library.lookupFunction<_StatusIntNative, _StatusIntDart>(
      'kc_set_show_board_arrows',
    );
    _setBooleanSetting = _library
        .lookupFunction<_StatusStringIntNative, _StatusStringIntDart>(
          'kc_set_boolean_setting',
        );
    _setAnalysisDepthRange = _library
        .lookupFunction<_StatusTwoIntsNative, _StatusTwoIntsDart>(
          'kc_set_analysis_depth_range',
        );
    _setEngineSettings = _library
        .lookupFunction<_StatusThreeIntsNative, _StatusThreeIntsDart>(
          'kc_set_engine_settings',
        );
    _setEngineResources = _library
        .lookupFunction<_StatusTwoIntsNative, _StatusTwoIntsDart>(
          'kc_set_engine_resources',
        );
    _setTheme = _library.lookupFunction<_StatusStringNative, _StatusStringDart>(
      'kc_set_theme_mode',
    );
    _setLocale = _library
        .lookupFunction<_StatusStringNative, _StatusStringDart>(
          'kc_set_locale',
        );
    _statisticsOverview = _library
        .lookupFunction<_StringNoArgsNative, _StringNoArgsDart>(
          'kc_statistics_overview_json',
        );
    _statisticsOpenings = _library
        .lookupFunction<_StringNoArgsNative, _StringNoArgsDart>(
          'kc_statistics_openings_json',
        );
    _statisticsTerminations = _library
        .lookupFunction<_StringNoArgsNative, _StringNoArgsDart>(
          'kc_statistics_terminations_json',
        );
    _statisticsPhases = _library
        .lookupFunction<_StringNoArgsNative, _StringNoArgsDart>(
          'kc_statistics_phases_json',
        );
    _games = _library.lookupFunction<_StringNoArgsNative, _StringNoArgsDart>(
      'kc_games_json',
    );
    _queryGames = _library.lookupFunction<_StringArgNative, _StringArgDart>(
      'kc_games_query_json',
    );
    _favoriteGames = _library
        .lookupFunction<_StringNoArgsNative, _StringNoArgsDart>(
          'kc_favorite_games_json',
        );
    _game = _library.lookupFunction<_StringArgNative, _StringArgDart>(
      'kc_game_json',
    );
    _resolveBoardMove = _library
        .lookupFunction<_ResolveBoardMoveNative, _ResolveBoardMoveDart>(
          'kc_resolve_board_move_json',
        );
    _importPgn = _library.lookupFunction<_StringArgNative, _StringArgDart>(
      'kc_import_pgn_json',
    );
    _importFen = _library
        .lookupFunction<_StringTwoArgsNative, _StringTwoArgsDart>(
          'kc_import_fen_json',
        );
    _startAnalysis = _library.lookupFunction<_StringArgNative, _StringArgDart>(
      'kc_start_analysis_json',
    );
    _analysisStatus = _library.lookupFunction<_StringArgNative, _StringArgDart>(
      'kc_analysis_status_json',
    );
    _startMoveRefinement = _library
        .lookupFunction<_StringIntArgNative, _StringIntArgDart>(
          'kc_start_move_refinement_json',
        );
    _moveAnalysisStatus = _library
        .lookupFunction<_StringIntArgNative, _StringIntArgDart>(
          'kc_move_analysis_status_json',
        );
    _cancelAnalysis = _library
        .lookupFunction<_StatusStringNative, _StatusStringDart>(
          'kc_cancel_analysis',
        );
    _deleteAnalysis = _library
        .lookupFunction<_StatusStringNative, _StatusStringDart>(
          'kc_delete_analysis',
        );
    _clearEngineCache = _library
        .lookupFunction<_StatusNoArgsNative, _StatusNoArgsDart>(
          'kc_clear_engine_cache',
        );
    _startVariationAnalysis = _library
        .lookupFunction<_StringTwoArgsNative, _StringTwoArgsDart>(
          'kc_start_variation_analysis_json',
        );
    _startVariationAnalysisWithSettings = _library
        .lookupFunction<
          _StartVariationWithSettingsNative,
          _StartVariationWithSettingsDart
        >('kc_start_variation_analysis_with_settings_json');
    _variationAnalysisStatus = _library
        .lookupFunction<_StringArgNative, _StringArgDart>(
          'kc_variation_analysis_status_json',
        );
    _cancelVariationAnalysis = _library
        .lookupFunction<_StatusStringNative, _StatusStringDart>(
          'kc_cancel_variation_analysis',
        );
    _startProviderProfile = _library
        .lookupFunction<_StartProviderProfileNative, _StartProviderProfileDart>(
          'kc_start_provider_profile_json',
        );
    _startProviderSync = _library
        .lookupFunction<_StartProviderSyncNative, _StartProviderSyncDart>(
          'kc_start_provider_sync_json',
        );
    _providerJobStatus = _library
        .lookupFunction<_StringArgNative, _StringArgDart>(
          'kc_provider_job_status_json',
        );
    _cancelProviderJob = _library
        .lookupFunction<_StatusStringNative, _StatusStringDart>(
          'kc_cancel_provider_job',
        );
    _providerOverview = _library
        .lookupFunction<_StringArgNative, _StringArgDart>(
          'kc_provider_overview_json',
        );
    _setGameFavorite = _library
        .lookupFunction<_StatusStringIntNative, _StatusStringIntDart>(
          'kc_set_game_favorite',
        );
    _favoriteCollections = _library
        .lookupFunction<_StringNoArgsNative, _StringNoArgsDart>(
          'kc_favorite_collections_json',
        );
    _createFavoriteCollection = _library
        .lookupFunction<_StringArgNative, _StringArgDart>(
          'kc_create_favorite_collection_json',
        );
    _renameFavoriteCollection = _library
        .lookupFunction<_StatusTwoStringsNative, _StatusTwoStringsDart>(
          'kc_rename_favorite_collection',
        );
    _deleteFavoriteCollection = _library
        .lookupFunction<_StatusStringNative, _StatusStringDart>(
          'kc_delete_favorite_collection',
        );
    _setGameFavoriteCollection = _library
        .lookupFunction<_StatusTwoStringsNative, _StatusTwoStringsDart>(
          'kc_set_game_favorite_collection',
        );
    _setGameDownloaded = _library
        .lookupFunction<_StatusStringIntNative, _StatusStringIntDart>(
          'kc_set_game_downloaded',
        );
    _deleteLocalGame = _library
        .lookupFunction<_StatusStringNative, _StatusStringDart>(
          'kc_delete_local_game',
        );
    _clearCachedMonth = _library
        .lookupFunction<_StatusTwoStringsNative, _StatusTwoStringsDart>(
          'kc_clear_cached_month',
        );
    _freeString = _library.lookupFunction<_FreeStringNative, _FreeStringDart>(
      'kc_string_free',
    );
  }

  static Future<FfiCoreGateway> create() async {
    const configuredPath = String.fromEnvironment('KCHESS_CORE_PATH');
    final library = configuredPath.isNotEmpty
        ? DynamicLibrary.open(configuredPath)
        : DynamicLibrary.open(
            Platform.isWindows ? 'kchess_core.dll' : 'libkchess_core.so',
          );
    final directory = await getApplicationSupportDirectory();
    await _installBundledAsset(directory.path, 'opening_book.kcb');
    await _installBundledAsset(directory.path, 'opening_names.kco');
    return FfiCoreGateway._(library, directory.path);
  }

  static Future<void> _installBundledAsset(
    String directoryPath,
    String assetName,
  ) async {
    final data = await rootBundle.load('assets/$assetName');
    final bytes = data.buffer.asUint8List(
      data.offsetInBytes,
      data.lengthInBytes,
    );
    final destination = File(
      '$directoryPath${Platform.pathSeparator}$assetName',
    );
    if (await destination.exists()) {
      final current = await destination.readAsBytes();
      if (current.length == bytes.length) {
        var equal = true;
        for (var index = 0; index < bytes.length; index++) {
          if (current[index] != bytes[index]) {
            equal = false;
            break;
          }
        }
        if (equal) return;
      }
    }
    final temporary = File('${destination.path}.tmp');
    await temporary.writeAsBytes(bytes, flush: true);
    if (await destination.exists()) await destination.delete();
    await temporary.rename(destination.path);
  }

  final DynamicLibrary _library;
  final String _dataDirectory;
  late final _AbiVersionDart _abiVersion;
  late final _CoreCreateDart _create;
  late final _CoreDestroyDart _destroy;
  late final _StatusNoArgsDart _initialize;
  late final _StringNoArgsDart _lastError;
  late final _StatusNoArgsDart _lastStatus;
  late final _StringNoArgsDart _profiles;
  late final _StringNoArgsDart _activeProfile;
  late final _CreateProfileDart _createProfile;
  late final _StatusStringDart _setActiveProfile;
  late final _StatusStringDart _deleteProfile;
  late final _StatusTwoStringsDart _mergeLocalProfile;
  late final _StringNoArgsDart _settings;
  late final _StatusIntDart _setArrows;
  late final _StatusStringIntDart _setBooleanSetting;
  late final _StatusTwoIntsDart _setAnalysisDepthRange;
  late final _StatusThreeIntsDart _setEngineSettings;
  late final _StatusTwoIntsDart _setEngineResources;
  late final _StatusStringDart _setTheme;
  late final _StatusStringDart _setLocale;
  late final _StringNoArgsDart _games;
  late final _StringArgDart _queryGames;
  late final _StringNoArgsDart _favoriteGames;
  late final _StringArgDart _game;
  late final _ResolveBoardMoveDart _resolveBoardMove;
  late final _StringArgDart _importPgn;
  late final _StringTwoArgsDart _importFen;
  late final _StringArgDart _startAnalysis;
  late final _StringArgDart _analysisStatus;
  late final _StringIntArgDart _startMoveRefinement;
  late final _StringIntArgDart _moveAnalysisStatus;
  late final _StatusStringDart _cancelAnalysis;
  late final _StatusStringDart _deleteAnalysis;
  late final _StatusNoArgsDart _clearEngineCache;
  late final _StringTwoArgsDart _startVariationAnalysis;
  late final _StartVariationWithSettingsDart
  _startVariationAnalysisWithSettings;
  late final _StringArgDart _variationAnalysisStatus;
  late final _StatusStringDart _cancelVariationAnalysis;
  late final _StartProviderProfileDart _startProviderProfile;
  late final _StartProviderSyncDart _startProviderSync;
  late final _StringArgDart _providerJobStatus;
  late final _StatusStringDart _cancelProviderJob;
  late final _StringArgDart _providerOverview;
  late final _StringNoArgsDart _statisticsOverview;
  late final _StringNoArgsDart _statisticsOpenings;
  late final _StringNoArgsDart _statisticsTerminations;
  late final _StringNoArgsDart _statisticsPhases;
  late final _StatusStringIntDart _setGameFavorite;
  late final _StringNoArgsDart _favoriteCollections;
  late final _StringArgDart _createFavoriteCollection;
  late final _StatusTwoStringsDart _renameFavoriteCollection;
  late final _StatusStringDart _deleteFavoriteCollection;
  late final _StatusTwoStringsDart _setGameFavoriteCollection;
  late final _StatusStringIntDart _setGameDownloaded;
  late final _StatusStringDart _deleteLocalGame;
  late final _StatusTwoStringsDart _clearCachedMonth;
  late final _FreeStringDart _freeString;
  Pointer<Void> _handle = nullptr;
  String? _activeProviderJobId;

  @override
  Future<void> initialize() async {
    final nativeAbiVersion = _abiVersion();
    if (nativeAbiVersion != _supportedAbiVersion) {
      throw CoreGatewayException(
        'Incompatible native core ABI: expected $_supportedAbiVersion, '
        'got $nativeAbiVersion. Rebuild or replace the Kchess native core.',
      );
    }

    final path = _dataDirectory.toNativeUtf8();
    try {
      _handle = _create(path);
    } finally {
      malloc.free(path);
    }
    if (_handle == nullptr) {
      throw const CoreGatewayException('Could not create native core');
    }
    _checkStatus(_initialize(_handle));
  }

  @override
  Future<List<AppProfile>> profiles() async =>
      _readList(_profiles(_handle), (json) => AppProfile.fromJson(json));

  @override
  Future<AppProfile?> activeProfile() async {
    final value = _readJson(_activeProfile(_handle));
    return value == null
        ? null
        : AppProfile.fromJson(value as Map<String, Object?>);
  }

  @override
  Future<AppProfile> createProfile({
    required ProfileType type,
    required String displayName,
    required String providerUsername,
  }) async {
    if (type != ProfileType.localPgnFen) {
      final username = providerUsername.toNativeUtf8();
      try {
        final started =
            _readJson(
                  _startProviderProfile(_handle, type.nativeValue, username),
                )!
                as Map<String, Object?>;
        final overview = await _waitProviderJob(started['jobId']! as String);
        return overview.profile;
      } finally {
        malloc.free(username);
      }
    }
    final name = displayName.toNativeUtf8();
    final username = providerUsername.toNativeUtf8();
    try {
      final value = _readJson(
        _createProfile(_handle, type.nativeValue, name, username),
      );
      return AppProfile.fromJson(value! as Map<String, Object?>);
    } finally {
      malloc.free(name);
      malloc.free(username);
    }
  }

  @override
  Future<void> setActiveProfile(String profileId) => _withNativeString(
    profileId,
    (value) => _checkStatus(_setActiveProfile(_handle, value)),
  );

  @override
  Future<void> deleteProfile(String profileId) => _withNativeString(
    profileId,
    (value) => _checkStatus(_deleteProfile(_handle, value)),
  );

  @override
  Future<void> mergeLocalProfile(
    String sourceProfileId,
    String targetProfileId,
  ) async {
    final source = sourceProfileId.toNativeUtf8();
    final target = targetProfileId.toNativeUtf8();
    try {
      _checkStatus(_mergeLocalProfile(_handle, source, target));
    } finally {
      malloc.free(source);
      malloc.free(target);
    }
  }

  @override
  Future<ProviderOverview> providerOverview(String profileId) =>
      _withNativeString(profileId, (value) {
        final json =
            _readJson(_providerOverview(_handle, value))!
                as Map<String, Object?>;
        return ProviderOverview.fromJson(json);
      });

  @override
  Future<ProviderOverview> syncProvider(
    String profileId, {
    int year = 0,
    int month = 0,
  }) async {
    final value = profileId.toNativeUtf8();
    late final String jobId;
    try {
      final started =
          _readJson(_startProviderSync(_handle, value, year, month))!
              as Map<String, Object?>;
      jobId = started['jobId']! as String;
    } finally {
      malloc.free(value);
    }
    return _waitProviderJob(jobId);
  }

  @override
  Future<AppSettings> settings() async => AppSettings.fromJson(
    _readJson(_settings(_handle))! as Map<String, Object?>,
  );

  @override
  Future<void> setAnalysisDepthRange({
    required int minimumDepth,
    required int maximumDepth,
  }) async =>
      _checkStatus(_setAnalysisDepthRange(_handle, minimumDepth, maximumDepth));

  @override
  Future<void> setEngineSettings({
    required int depth,
    required int multiPv,
    required int timeLimitSeconds,
  }) async => _checkStatus(
    _setEngineSettings(_handle, depth, multiPv, timeLimitSeconds),
  );

  @override
  Future<void> setEngineResources({
    required int threads,
    required int hashMb,
  }) async => _checkStatus(_setEngineResources(_handle, threads, hashMb));

  @override
  Future<void> setShowBoardArrows(bool enabled) async =>
      _checkStatus(_setArrows(_handle, enabled ? 1 : 0));

  @override
  Future<void> setBooleanSetting(String key, bool enabled) => _withNativeString(
    key,
    (value) =>
        _checkStatus(_setBooleanSetting(_handle, value, enabled ? 1 : 0)),
  );

  @override
  Future<void> setThemeMode(AppThemeMode mode) => _withNativeString(
    mode.name,
    (value) => _checkStatus(_setTheme(_handle, value)),
  );

  @override
  Future<void> setLocale(String locale) => _withNativeString(
    locale,
    (value) => _checkStatus(_setLocale(_handle, value)),
  );

  @override
  Future<StatisticsOverview> statisticsOverview() async =>
      StatisticsOverview.fromJson(
        _readJson(_statisticsOverview(_handle))! as Map<String, Object?>,
      );

  @override
  Future<OpeningsStats> openingsStats() async => OpeningsStats.fromJson(
    _readJson(_statisticsOpenings(_handle))! as Map<String, Object?>,
  );

  @override
  Future<TerminationStats> terminationStats() async => TerminationStats.fromJson(
    _readJson(_statisticsTerminations(_handle))! as Map<String, Object?>,
  );

  @override
  Future<PhaseStats> phaseStats() async => PhaseStats.fromJson(
    _readJson(_statisticsPhases(_handle))! as Map<String, Object?>,
  );

  @override
  Future<List<GameSummary>> games() async =>
      _readList(_games(_handle), (json) => GameSummary.fromJson(json));

  @override
  Future<List<GameSummary>> queryGames(GameQuery query) => _withNativeString(
    jsonEncode(query.toJson()),
    (value) => _readList(
      _queryGames(_handle, value),
      (json) => GameSummary.fromJson(json),
    ),
  );

  @override
  Future<List<GameSummary>> favoriteGames() async =>
      _readList(_favoriteGames(_handle), (json) => GameSummary.fromJson(json));

  @override
  Future<GameDetail> game(String gameId) => _withNativeString(gameId, (value) {
    final json = _readJson(_game(_handle, value))! as Map<String, Object?>;
    return GameDetail.fromJson(json);
  });

  @override
  Future<BoardMoveResolution> resolveBoardMove({
    required String gameId,
    required String fen,
    required String source,
    required String target,
    required int firstCandidatePly,
  }) async {
    final nativeGameId = gameId.toNativeUtf8();
    final nativeFen = fen.toNativeUtf8();
    final nativeSource = source.toNativeUtf8();
    final nativeTarget = target.toNativeUtf8();
    try {
      final json =
          _readJson(
                _resolveBoardMove(
                  _handle,
                  nativeGameId,
                  nativeFen,
                  nativeSource,
                  nativeTarget,
                  firstCandidatePly,
                ),
              )!
              as Map<String, Object?>;
      return BoardMoveResolution.fromJson(json);
    } finally {
      malloc.free(nativeGameId);
      malloc.free(nativeFen);
      malloc.free(nativeSource);
      malloc.free(nativeTarget);
    }
  }

  @override
  Future<GameSummary> importPgn(String pgn) => _withNativeString(pgn, (value) {
    final json = _readJson(_importPgn(_handle, value))! as Map<String, Object?>;
    return GameSummary.fromJson(json);
  });

  @override
  Future<GameSummary> importFen({
    required String fen,
    required String name,
  }) async {
    final nativeFen = fen.toNativeUtf8();
    final nativeName = name.toNativeUtf8();
    try {
      final json =
          _readJson(_importFen(_handle, nativeFen, nativeName))!
              as Map<String, Object?>;
      return GameSummary.fromJson(json);
    } finally {
      malloc.free(nativeFen);
      malloc.free(nativeName);
    }
  }

  @override
  Future<AnalysisSnapshot> startAnalysis(String gameId) =>
      _analysisCall(gameId, _startAnalysis);

  @override
  Future<AnalysisSnapshot> analysisStatus(String gameId) =>
      _analysisCall(gameId, _analysisStatus);

  @override
  Future<AnalysisSnapshot> startMoveRefinement(String gameId, int ply) =>
      _withNativeString(gameId, (value) {
        final json =
            _readJson(_startMoveRefinement(_handle, value, ply))!
                as Map<String, Object?>;
        return AnalysisSnapshot.fromJson(json);
      });

  @override
  Future<AnalysisSnapshot> moveAnalysisStatus(String gameId, int ply) =>
      _withNativeString(gameId, (value) {
        final json =
            _readJson(_moveAnalysisStatus(_handle, value, ply))!
                as Map<String, Object?>;
        return AnalysisSnapshot.fromJson(json);
      });

  @override
  Future<void> cancelAnalysis(String gameId) => _withNativeString(
    gameId,
    (value) => _checkStatus(_cancelAnalysis(_handle, value)),
  );

  @override
  Future<void> deleteAnalysis(String gameId) => _withNativeString(
    gameId,
    (value) => _checkStatus(_deleteAnalysis(_handle, value)),
  );

  @override
  Future<void> clearEngineCache() async =>
      _checkStatus(_clearEngineCache(_handle));

  @override
  Future<VariationAnalysisSnapshot> startVariationAnalysis({
    required String fen,
    required String uci,
    int? depth,
    int? multiPv,
    int? threads,
    int? hashMb,
  }) async {
    final hasOverrides =
        depth != null || multiPv != null || threads != null || hashMb != null;
    if (hasOverrides &&
        (depth == null ||
            multiPv == null ||
            threads == null ||
            hashMb == null)) {
      throw ArgumentError(
        'Sideline engine overrides require depth, multiPv, threads and hashMb.',
      );
    }
    final nativeFen = fen.toNativeUtf8();
    final nativeUci = uci.toNativeUtf8();
    try {
      final pointer = hasOverrides
          ? _startVariationAnalysisWithSettings(
              _handle,
              nativeFen,
              nativeUci,
              depth!,
              multiPv!,
              threads!,
              hashMb!,
            )
          : _startVariationAnalysis(_handle, nativeFen, nativeUci);
      final json = _readJson(pointer)! as Map<String, Object?>;
      return VariationAnalysisSnapshot.fromJson(json);
    } finally {
      malloc.free(nativeFen);
      malloc.free(nativeUci);
    }
  }

  @override
  Future<VariationAnalysisSnapshot> variationAnalysisStatus(String jobId) =>
      _withNativeString(jobId, (value) {
        final json =
            _readJson(_variationAnalysisStatus(_handle, value))!
                as Map<String, Object?>;
        return VariationAnalysisSnapshot.fromJson(json);
      });

  @override
  Future<void> cancelVariationAnalysis(String jobId) => _withNativeString(
    jobId,
    (value) => _checkStatus(_cancelVariationAnalysis(_handle, value)),
  );

  @override
  Future<void> setGameFavorite(String gameId, bool enabled) =>
      _withNativeString(
        gameId,
        (value) =>
            _checkStatus(_setGameFavorite(_handle, value, enabled ? 1 : 0)),
      );

  @override
  Future<List<FavoriteCollection>> favoriteCollections() async =>
      _readList(_favoriteCollections(_handle), FavoriteCollection.fromJson);

  @override
  Future<FavoriteCollection> createFavoriteCollection(String name) =>
      _withNativeString(name, (value) {
        final json =
            _readJson(_createFavoriteCollection(_handle, value))!
                as Map<String, Object?>;
        return FavoriteCollection.fromJson(json);
      });

  @override
  Future<void> renameFavoriteCollection(
    String collectionId,
    String name,
  ) async {
    final nativeCollection = collectionId.toNativeUtf8();
    final nativeName = name.toNativeUtf8();
    try {
      _checkStatus(
        _renameFavoriteCollection(_handle, nativeCollection, nativeName),
      );
    } finally {
      malloc.free(nativeCollection);
      malloc.free(nativeName);
    }
  }

  @override
  Future<void> deleteFavoriteCollection(String collectionId) =>
      _withNativeString(
        collectionId,
        (value) => _checkStatus(_deleteFavoriteCollection(_handle, value)),
      );

  @override
  Future<void> setGameFavoriteCollection(
    String gameId,
    String? collectionId,
  ) async {
    final nativeGame = gameId.toNativeUtf8();
    final nativeCollection = (collectionId ?? '').toNativeUtf8();
    try {
      _checkStatus(
        _setGameFavoriteCollection(_handle, nativeGame, nativeCollection),
      );
    } finally {
      malloc.free(nativeGame);
      malloc.free(nativeCollection);
    }
  }

  @override
  Future<void> setGameDownloaded(String gameId, bool enabled) =>
      _withNativeString(
        gameId,
        (value) =>
            _checkStatus(_setGameDownloaded(_handle, value, enabled ? 1 : 0)),
      );

  @override
  Future<void> deleteLocalGame(String gameId) => _withNativeString(
    gameId,
    (value) => _checkStatus(_deleteLocalGame(_handle, value)),
  );

  @override
  Future<void> clearCachedMonth(String profileId, String month) async {
    final nativeProfile = profileId.toNativeUtf8();
    final nativeMonth = month.toNativeUtf8();
    try {
      _checkStatus(_clearCachedMonth(_handle, nativeProfile, nativeMonth));
    } finally {
      malloc.free(nativeProfile);
      malloc.free(nativeMonth);
    }
  }

  Future<ProviderOverview> _waitProviderJob(String jobId) async {
    _activeProviderJobId = jobId;
    try {
      while (true) {
        final status = await _withNativeString(jobId, (value) {
          return _readJson(_providerJobStatus(_handle, value))!
              as Map<String, Object?>;
        });
        if (status['finished'] as bool? ?? false) {
          final result = status['result'] as Map<String, Object?>?;
          if (status['state'] == 'error' || result == null) {
            throw CoreGatewayException(
              _providerError(
                status['errorKind'] as String?,
                status['errorMessage'] as String?,
              ),
            );
          }
          return ProviderOverview.fromJson(result);
        }
        await Future<void>.delayed(const Duration(milliseconds: 120));
      }
    } finally {
      if (_activeProviderJobId == jobId) _activeProviderJobId = null;
    }
  }

  String _providerError(String? kind, String? detail) => switch (kind) {
    'offline' => 'Offline – gespeicherte Daten werden angezeigt.',
    'timeout' => 'Zeitüberschreitung beim Anbieter. Bitte erneut versuchen.',
    'dns' => 'Der Anbieter konnte nicht gefunden werden (DNS).',
    'tls' => 'Die sichere Verbindung zum Anbieter ist fehlgeschlagen.',
    'notFound' || 'not_found' => 'Benutzer nicht gefunden.',
    'gone' => 'Dieses Profil ist dauerhaft nicht verfügbar.',
    'rateLimited' || 'rate_limited' =>
      'Der Anbieter begrenzt Anfragen. Bitte später erneut versuchen.',
    'server' => 'Der Anbieter ist momentan nicht erreichbar.',
    'invalidResponse' ||
    'invalid_response' => 'Der Anbieter hat ungültige Daten geliefert.',
    'cancelled' => 'Die Anfrage wurde abgebrochen.',
    _ => detail?.isNotEmpty == true ? detail! : 'Unbekannter Anbieterfehler.',
  };

  Future<AnalysisSnapshot> _analysisCall(
    String gameId,
    _StringArgDart function,
  ) => _withNativeString(gameId, (value) {
    final json = _readJson(function(_handle, value))! as Map<String, Object?>;
    return AnalysisSnapshot.fromJson(json);
  });

  T? _readJson<T>(Pointer<Utf8> pointer) {
    if (pointer == nullptr) {
      throw _nativeException();
    }
    try {
      return jsonDecode(pointer.toDartString()) as T?;
    } finally {
      _freeString(pointer);
    }
  }

  List<T> _readList<T>(
    Pointer<Utf8> pointer,
    T Function(Map<String, Object?> json) fromJson,
  ) {
    final values = _readJson<List<Object?>>(pointer)!;
    return values
        .cast<Map<String, Object?>>()
        .map(fromJson)
        .toList(growable: false);
  }

  Future<T> _withNativeString<T>(
    String input,
    T Function(Pointer<Utf8>) operation,
  ) async {
    final native = input.toNativeUtf8();
    try {
      return operation(native);
    } finally {
      malloc.free(native);
    }
  }

  CoreGatewayException _nativeException([int? status]) {
    final nativeStatus =
        status ?? (_handle == nullptr ? 5 : _lastStatus(_handle));
    final message = _handle == nullptr
        ? 'Native Kchess core is not available'
        : _lastError(_handle).toDartString();
    return CoreGatewayException(
      message.isEmpty ? 'Unknown native Kchess error' : message,
      code: CoreErrorCode.fromNative(nativeStatus),
    );
  }

  void _checkStatus(int status) {
    if (status != 0) {
      throw _nativeException(status);
    }
  }

  @override
  void dispose() {
    if (_handle != nullptr) {
      final providerJob = _activeProviderJobId;
      if (providerJob != null) {
        final native = providerJob.toNativeUtf8();
        try {
          _cancelProviderJob(_handle, native);
        } finally {
          malloc.free(native);
        }
      }
      _destroy(_handle);
      _handle = nullptr;
    }
  }
}
