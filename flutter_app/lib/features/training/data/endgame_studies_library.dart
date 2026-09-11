import 'dart:convert';

import 'package:flutter/services.dart';

import '../models/endgame_study.dart';

/// Loads and caches the 86 classical endgame studies from Kling & Horwitz (1851)
/// stored in `assets/endgame_studies.json`.
class EndgameStudiesLibrary {
  EndgameStudiesLibrary._();

  static EndgameStudyCatalogue? _cached;

  /// Returns the cached catalogue if available, or [EndgameStudyCatalogue.empty].
  static EndgameStudyCatalogue get current =>
      _cached ?? EndgameStudyCatalogue.empty;

  /// Loads the catalogue from the bundled asset. Caches the result in memory.
  static Future<EndgameStudyCatalogue> load({AssetBundle? bundle}) async {
    if (_cached != null) return _cached!;
    final assetBundle = bundle ?? rootBundle;
    final jsonString = await assetBundle.loadString(
      'assets/endgame_studies.json',
    );
    final data = jsonDecode(jsonString) as Map<String, Object?>;
    final catalogue = EndgameStudyCatalogue.fromJson(data);
    _cached = catalogue;
    return catalogue;
  }

  /// Finds the next study in the same section, or null if it's the last one.
  static EndgameStudy? nextStudyInSection(
    EndgameStudySection section,
    EndgameStudy study,
  ) {
    final index = section.studies.indexWhere((s) => s.id == study.id);
    if (index >= 0 && index + 1 < section.studies.length) {
      return section.studies[index + 1];
    }
    return null;
  }

  /// Resets the cached catalogue (primarily for testing).
  static void resetCache() {
    _cached = null;
  }
}
