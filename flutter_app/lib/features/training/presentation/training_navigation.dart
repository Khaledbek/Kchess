// -----------------------------------------------------------------------------
// Section: Training navigation scope
// -----------------------------------------------------------------------------

import 'package:flutter/widgets.dart';

import '../models/opening_training_request.dart';

/// Lets screens inside the home shell switch to the training tab.
///
/// The statistics tab uses it for its `[Trainieren ➔]` deep link: the shell owns
/// the selected navigation index, so the request travels up through this scope
/// instead of the statistics widgets reaching into the shell's state.
class TrainingNavigator extends InheritedWidget {
  const TrainingNavigator({
    required this.openTraining,
    required super.child,
    super.key,
  });

  /// Selects the training destination. Passing an [OpeningTrainingRequest]
  /// additionally pre-populates the opening lab with that line.
  final void Function({OpeningTrainingRequest? opening}) openTraining;

  /// Null outside the home shell — a screen pumped on its own in a test, for
  /// instance. Callers hide their deep link in that case.
  static TrainingNavigator? maybeOf(BuildContext context) =>
      context.dependOnInheritedWidgetOfExactType<TrainingNavigator>();

  @override
  bool updateShouldNotify(TrainingNavigator oldWidget) =>
      openTraining != oldWidget.openTraining;
}
