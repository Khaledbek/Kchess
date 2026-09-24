import 'package:flutter/widgets.dart';

import '../features/coach/models/coach_ui_models.dart';

typedef CoachClientActionHandler = Future<void> Function(
  BuildContext context,
  CoachClientAction action,
);

class CoachClientActionScope extends InheritedWidget {
  const CoachClientActionScope({
    required this.onAction,
    required super.child,
    super.key,
  });

  final CoachClientActionHandler onAction;

  static CoachClientActionScope? maybeOf(BuildContext context) =>
      context.dependOnInheritedWidgetOfExactType<CoachClientActionScope>();

  @override
  bool updateShouldNotify(CoachClientActionScope oldWidget) =>
      oldWidget.onAction != onAction;
}
