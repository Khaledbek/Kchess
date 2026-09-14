import 'package:flutter/foundation.dart';
import 'package:flutter/scheduler.dart';

/// Lightweight, read-only instrumentation for the Flutter side of app startup.
///
/// This object deliberately records timings only. It must never make startup,
/// scheduling, caching, or domain decisions from the collected values.
class AppStartupDiagnostics {
  AppStartupDiagnostics._();

  static final AppStartupDiagnostics instance = AppStartupDiagnostics._();

  static const int _maximumFrameSamples = 120;
  static const double _slowFrameThresholdMs = 1000.0 / 60.0;

  final Stopwatch _clock = Stopwatch();
  final Map<String, int> _marks = <String, int>{};
  final Map<String, int> _ffiGatewayDurations = <String, int>{};
  final Map<String, int> _controllerDurations = <String, int>{};

  bool _started = false;
  int _controllerInitializeCalls = 0;
  bool? _controllerInitializeSucceeded;
  int _frameSamples = 0;
  int _slowBuildFrames = 0;
  int _slowRasterFrames = 0;
  double _totalBuildMs = 0;
  double _totalRasterMs = 0;
  double _worstBuildMs = 0;
  double _worstRasterMs = 0;

  void startMain() {
    if (!_started) {
      _clock.start();
      _started = true;
    }
    mark('mainEnteredMs');
  }

  int get elapsedMs => _started ? _clock.elapsedMilliseconds : 0;

  void mark(String name) {
    if (!_started) return;
    _marks.putIfAbsent(name, () => elapsedMs);
  }

  void recordFfiGatewayDuration(String name, int durationMs) {
    _ffiGatewayDurations.putIfAbsent(name, () => durationMs);
  }

  bool beginControllerInitialization() {
    _controllerInitializeCalls += 1;
    if (_controllerInitializeCalls != 1) return false;
    mark('controllerInitializeStartedMs');
    return true;
  }

  void recordControllerDuration(String name, int durationMs) {
    _controllerDurations.putIfAbsent(name, () => durationMs);
  }

  void completeControllerInitialization({
    required int totalMs,
    required bool succeeded,
  }) {
    if (_controllerInitializeSucceeded != null) return;
    _controllerInitializeSucceeded = succeeded;
    _controllerDurations['totalMs'] = totalMs;
    mark(succeeded ? 'controllerReadyMs' : 'controllerFailedMs');
  }

  void recordFrameTimings(List<FrameTiming> timings) {
    for (final timing in timings) {
      if (_frameSamples >= _maximumFrameSamples) return;
      final buildMs = timing.buildDuration.inMicroseconds / 1000.0;
      final rasterMs = timing.rasterDuration.inMicroseconds / 1000.0;
      _frameSamples += 1;
      _totalBuildMs += buildMs;
      _totalRasterMs += rasterMs;
      if (buildMs > _worstBuildMs) _worstBuildMs = buildMs;
      if (rasterMs > _worstRasterMs) _worstRasterMs = rasterMs;
      if (buildMs > _slowFrameThresholdMs) _slowBuildFrames += 1;
      if (rasterMs > _slowFrameThresholdMs) _slowRasterFrames += 1;
    }
  }

  bool get frameSamplingComplete => _frameSamples >= _maximumFrameSamples;

  Map<String, Object?> snapshot() {
    final averageBuildMs =
        _frameSamples == 0 ? 0.0 : _totalBuildMs / _frameSamples;
    final averageRasterMs =
        _frameSamples == 0 ? 0.0 : _totalRasterMs / _frameSamples;
    return <String, Object?>{
      'schema': 'flutter.startup.v1',
      'buildMode': kDebugMode ? 'debug' : (kProfileMode ? 'profile' : 'release'),
      'platform': defaultTargetPlatform.name,
      'elapsedSinceMainMs': elapsedMs,
      'marks': Map<String, int>.from(_marks),
      'ffiGateway': <String, Object?>{
        ..._ffiGatewayDurations,
        'totalMs': _ffiGatewayDurations.values.fold<int>(0, (a, b) => a + b),
      },
      'controllerInitialize': <String, Object?>{
        'calls': _controllerInitializeCalls,
        'succeeded': _controllerInitializeSucceeded,
        ..._controllerDurations,
      },
      'frameTiming': <String, Object?>{
        'samples': _frameSamples,
        'sampleLimit': _maximumFrameSamples,
        'samplingComplete': frameSamplingComplete,
        'slowFrameThresholdMs': _slowFrameThresholdMs,
        'slowBuildFrames': _slowBuildFrames,
        'slowRasterFrames': _slowRasterFrames,
        'averageBuildMs': averageBuildMs,
        'averageRasterMs': averageRasterMs,
        'worstBuildMs': _worstBuildMs,
        'worstRasterMs': _worstRasterMs,
      },
    };
  }
}
