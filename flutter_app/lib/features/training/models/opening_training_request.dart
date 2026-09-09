/// A line the user picked in the statistics tab and wants to train.
///
/// Carried from the statistics deep link into the opening lab, so the trainer
/// opens on that family instead of an empty picker.
class OpeningTrainingRequest {
  const OpeningTrainingRequest({
    required this.openingName,
    this.eco = '',
    this.color = 'unknown',
  });

  final String openingName;

  /// ECO code of the family, e.g. `C65`. Empty when the row has none.
  final String eco;

  /// Side the user played the line with: `white` | `black` | `unknown`.
  final String color;

  /// e.g. "C65 · Ruy Lopez" — the ECO is dropped when the row has none.
  String get label => eco.isEmpty ? openingName : '$eco · $openingName';
}
